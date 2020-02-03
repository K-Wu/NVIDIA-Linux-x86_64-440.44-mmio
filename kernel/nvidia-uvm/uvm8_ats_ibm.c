/*******************************************************************************
    Copyright (c) 2018-2019 NVIDIA Corporation

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be
        included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "uvm8_api.h"
#include "uvm8_lock.h"
#include "uvm8_kvmalloc.h"
#include "uvm8_va_space.h"
#include "uvm8_va_space_mm.h"
#include "uvm8_ats_ibm.h"
#include "uvm_common.h"

#if UVM_KERNEL_SUPPORTS_IBM_ATS()

void uvm_ats_ibm_register_lock(uvm_va_space_t *va_space)
{
    uvm_mutex_lock(&va_space->mm_state.ats_reg_unreg_lock);
}

void uvm_ats_ibm_register_unlock(uvm_va_space_t *va_space)
{
    uvm_mutex_unlock(&va_space->mm_state.ats_reg_unreg_lock);
}

// This function is called under two circumstances:
// 1) By the kernel when the mm is about to be torn down
// 2) By the last pnv_npu2_destroy_context in a VA space
//
// We are guaranteed that this function is called by at least one of those
// paths. We are not guaranteed to be called by both paths, but it is possible
// that they are called concurrently.
static void npu_release(struct npu_context *npu_context, void *va_mm)
{
    uvm_va_space_mm_t *va_space_mm = (uvm_va_space_mm_t *)va_mm;
    UVM_ASSERT(g_uvm_global.ats.enabled);

    // There are some subtleties identifying whether we're on the mm teardown
    // path or the GPU VA space unregister path. uvm_va_space_mm_shutdown will
    // figure that out.
    //
    // The requirement for this callback are that, once we return:
    // 1) GPUs will not issue any more translated ATS memory accesses under this
    //    mm_struct
    // 2) GPUs will not issue any more ATRs under that mm_struct
    // 3) pnv_npu2_handle_fault will no longer be called on this npu_context
    //
    // uvm_va_space_mm_shutdown provides all of those guarantees.
    uvm_va_space_mm_shutdown(va_space_mm);
}

static void npu_release_entry(struct npu_context *npu_context, void *va_mm)
{
    UVM_ENTRY_VOID(npu_release(npu_context, va_mm));
}

NV_STATUS uvm_ats_ibm_register_gpu_va_space(uvm_gpu_va_space_t *gpu_va_space)
{
    uvm_va_space_t *va_space = gpu_va_space->va_space;
    struct npu_context *npu_context;
    NV_STATUS status;

    if (!gpu_va_space->ats.enabled)
        return NV_OK;

    UVM_ASSERT(g_uvm_global.ats.enabled);
    UVM_ASSERT(uvm_gpu_va_space_state(gpu_va_space) == UVM_GPU_VA_SPACE_STATE_ACTIVE);
    uvm_assert_mmap_sem_locked_write(&current->mm->mmap_sem);
    uvm_assert_rwsem_locked_write(&va_space->lock);
    uvm_assert_mutex_locked(&va_space->mm_state.ats_reg_unreg_lock);

    // We use the va_space_mm as the callback arg to pnv_npu2_init_context, so
    // we have to register it first to make sure it's created. This thread holds
    // a reference on current->mm, so we're safe to use the mm here even before
    // pnv_npu2_init_context takes its reference.
    status = uvm_va_space_mm_register(va_space);
    if (status != NV_OK)
        return status;

    // We're holding both the VA space lock and mmap_sem on this path so we
    // can't call uvm_va_space_mm_unregister if we hit some error. Tell the
    // caller to do it if that becomes necessary.
    gpu_va_space->did_va_space_mm_register = true;

    // The callback values are shared by all devices under the npu, so we must
    // pass the same values to each one.
    //
    // Note that the callback cannot be invoked until we're done with this
    // ioctl, since the only paths which invoke the callback are GPU VA space
    // unregister and mm teardown. The GPU VA space can't be unregistered while
    // we hold the VA space lock, and the mm can't be torn down while it's
    // active on this thread.
    npu_context = pnv_npu2_init_context(gpu_va_space->gpu->pci_dev,
                                        (MSR_DR | MSR_PR | MSR_HV),
                                        npu_release_entry,
                                        gpu_va_space->va_space->mm_state.va_space_mm);
    if (IS_ERR(npu_context)) {
        int err = PTR_ERR(npu_context);

        // We'll get -EINVAL if the callback value (va_space_mm) differs from
        // the one already registered to the npu_context associated with this
        // mm. That can only happen when multiple VA spaces attempt registration
        // within the same process, which is disallowed and should return
        // NV_ERR_NOT_SUPPORTED.
        if (err == -EINVAL)
            return NV_ERR_NOT_SUPPORTED;
        return errno_to_nv_status(err);
    }

    gpu_va_space->ats.npu_context = npu_context;
    return NV_OK;
}

void uvm_ats_ibm_unregister_gpu_va_space(uvm_gpu_va_space_t *gpu_va_space)
{
    uvm_va_space_t *va_space = gpu_va_space->va_space;
    uvm_va_space_mm_t *va_space_mm;

    if (!gpu_va_space->did_va_space_mm_register) {
        UVM_ASSERT(!gpu_va_space->ats.npu_context);
        return;
    }

    UVM_ASSERT(gpu_va_space->ats.enabled);
    UVM_ASSERT(va_space);

    uvm_ats_ibm_register_lock(va_space);

    // Calling pnv_npu2_destroy_context may invoke uvm_va_space_mm_shutdown,
    // which may operate on this va_space_mm. We have to make sure the
    // va_space_mm remains valid until mm_shutdown is done by calling
    // uvm_va_space_mm_unregister.
    va_space_mm = uvm_va_space_mm_unregister(va_space);

    if (gpu_va_space->ats.npu_context) {
        UVM_ASSERT(uvm_gpu_va_space_state(gpu_va_space) == UVM_GPU_VA_SPACE_STATE_DEAD);

        // This call may in turn call back into npu_release, which may take
        // mmap_sem and the VA space lock. That sequence is the reason we can't
        // be holding those locks on this path.
        pnv_npu2_destroy_context(gpu_va_space->ats.npu_context, gpu_va_space->gpu->pci_dev);
        gpu_va_space->ats.npu_context = NULL;
    }

    uvm_ats_ibm_register_unlock(va_space);
    uvm_va_space_mm_drop(va_space_mm);
}

NV_STATUS uvm_ats_ibm_service_fault(uvm_gpu_va_space_t *gpu_va_space,
                                    NvU64 fault_addr,
                                    uvm_fault_access_type_t access_type)
{
    unsigned long flags;
    uintptr_t addr;
    unsigned long fault_status = 0;
    int err;

    UVM_ASSERT(g_uvm_global.ats.enabled);
    UVM_ASSERT(gpu_va_space->ats.enabled);

    // TODO: Bug 2103669: Service more than a single fault at a time
    flags = (unsigned long)((access_type >= UVM_FAULT_ACCESS_TYPE_WRITE) ? NPU2_WRITE : 0);
    addr = (uintptr_t)fault_addr;

    err = pnv_npu2_handle_fault(gpu_va_space->ats.npu_context, &addr, &flags, &fault_status, 1);
    if (err == -EFAULT) {
        // pnv_npu2_handle_fault returns -EFAULT when one of the VAs couldn't be
        // serviced. We have to inspect the per-page fault_status field for the
        // specific error.

        // TODO: Bug 2103669: If we service more than a single fault at a
        //       time and there's an error on at least one of the pages,
        //       we'll have to pick which error to use.
        return errno_to_nv_status(fault_status);
    }

    return errno_to_nv_status(err);
}

#endif // UVM_KERNEL_SUPPORTS_IBM_ATS
