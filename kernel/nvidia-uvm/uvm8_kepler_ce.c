#include <linux/kernel.h>
/*******************************************************************************
    Copyright (c) 2015-2019 NVIDIA Corporation

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

#include "uvm8_hal.h"
#include "uvm8_push.h"
#include "cla0b5.h"
#include "cla06f.h"

void uvm_hal_kepler_ce_init(uvm_push_t *push)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    // Notably this sends SET_OBJECT with the CE class on subchannel 0 instead
    // of the recommended by HW subchannel 4 (subchannel 4 is recommended to
    // match CE usage on GRCE). For the UVM driver using subchannel 0 has the
    // benefit of also verifying that we ended up on the right PBDMA though as
    // SET_OBJECT with CE class on subchannel 0 would fail on GRCE.
    NV_PUSH_1U(A06F, SET_OBJECT, uvm_push_get_gpu(push)->rm_info.ceClass);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_offset_out(uvm_push_t *push, NvU64 offset_out)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NV_PUSH_2U(A0B5, OFFSET_OUT_UPPER, HWVALUE(A0B5, OFFSET_OUT_UPPER, UPPER, NvOffset_HI32(offset_out)),
                     OFFSET_OUT_LOWER, HWVALUE(A0B5, OFFSET_OUT_LOWER, VALUE, NvOffset_LO32(offset_out)));
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_offset_in_out(uvm_push_t *push, NvU64 offset_in, NvU64 offset_out)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NV_PUSH_4U(A0B5, OFFSET_IN_UPPER,  HWVALUE(A0B5, OFFSET_IN_UPPER,  UPPER, NvOffset_HI32(offset_in)),
                     OFFSET_IN_LOWER,  HWVALUE(A0B5, OFFSET_IN_LOWER,  VALUE, NvOffset_LO32(offset_in)),
                     OFFSET_OUT_UPPER, HWVALUE(A0B5, OFFSET_OUT_UPPER, UPPER, NvOffset_HI32(offset_out)),
                     OFFSET_OUT_LOWER, HWVALUE(A0B5, OFFSET_OUT_LOWER, VALUE, NvOffset_LO32(offset_out)));
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

// Perform an appropriate membar before a semaphore operation. Returns whether
// the semaphore operation should include a flush.
static bool kepler_membar_before_semaphore(uvm_push_t *push)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_gpu_t *gpu;

    if (uvm_push_get_and_reset_flag(push, UVM_PUSH_FLAG_CE_NEXT_MEMBAR_NONE)) {
        // No MEMBAR requested, don't use a flush.
        return false;
    }

    if (!uvm_push_get_and_reset_flag(push, UVM_PUSH_FLAG_CE_NEXT_MEMBAR_GPU)) {
        // By default do a MEMBAR SYS and for that we can just use flush on the
        // semaphore operation.
        return true;
    }

    // MEMBAR GPU requested, do it on the HOST and skip the CE flush as CE
    // doesn't have this capability.
    gpu = uvm_push_get_gpu(push);
    gpu->host_hal->wait_for_idle(push);
    gpu->host_hal->membar_gpu(push);

    return false;
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_semaphore_release(uvm_push_t *push, uvm_gpu_semaphore_t *semaphore, NvU32 payload)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_gpu_t *gpu = uvm_push_get_gpu(push);
    NvU64 semaphore_va = uvm_gpu_semaphore_get_gpu_va(semaphore, gpu);
    NvU32 flush_value;
    bool use_flush;

    use_flush = kepler_membar_before_semaphore(push);

    if (use_flush)
        flush_value = HWCONST(A0B5, LAUNCH_DMA, FLUSH_ENABLE, TRUE);
    else
        flush_value = HWCONST(A0B5, LAUNCH_DMA, FLUSH_ENABLE, FALSE);

    NV_PUSH_3U(A0B5, SET_SEMAPHORE_A, NvOffset_HI32(semaphore_va),
                     SET_SEMAPHORE_B, NvOffset_LO32(semaphore_va),
                     SET_SEMAPHORE_PAYLOAD, payload);

    NV_PUSH_1U(A0B5, LAUNCH_DMA, flush_value |
       HWCONST(A0B5, LAUNCH_DMA, DATA_TRANSFER_TYPE, NONE) |
       HWCONST(A0B5, LAUNCH_DMA, SEMAPHORE_TYPE, RELEASE_ONE_WORD_SEMAPHORE));
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_semaphore_reduction_inc(uvm_push_t *push, NvU64 semaphore_va, NvU32 payload)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NvU32 flush_value;
    bool use_flush;

    use_flush = kepler_membar_before_semaphore(push);

    if (use_flush)
        flush_value = HWCONST(A0B5, LAUNCH_DMA, FLUSH_ENABLE, TRUE);
    else
        flush_value = HWCONST(A0B5, LAUNCH_DMA, FLUSH_ENABLE, FALSE);

    // Semaphore reductions are only supported on GK110+
    UVM_ASSERT_MSG(uvm_gpu_is_gk110_plus(uvm_push_get_gpu(push)), "GPU %s\n", uvm_push_get_gpu(push)->name);

    NV_PUSH_3U(A0B5, SET_SEMAPHORE_A, NvOffset_HI32(semaphore_va),
                     SET_SEMAPHORE_B, NvOffset_LO32(semaphore_va),
                     SET_SEMAPHORE_PAYLOAD, payload);

    NV_PUSH_1U(A0B5, LAUNCH_DMA, flush_value |
       HWCONST(A0B5, LAUNCH_DMA, DATA_TRANSFER_TYPE, NONE) |
       HWCONST(A0B5, LAUNCH_DMA, SEMAPHORE_TYPE, RELEASE_ONE_WORD_SEMAPHORE) |
       HWCONST(A0B5, LAUNCH_DMA, SEMAPHORE_REDUCTION, INC) |
       HWCONST(A0B5, LAUNCH_DMA, SEMAPHORE_REDUCTION_SIGN, UNSIGNED) |
       HWCONST(A0B5, LAUNCH_DMA, SEMAPHORE_REDUCTION_ENABLE, TRUE));
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_semaphore_timestamp(uvm_push_t *push, NvU64 gpu_va)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NvU32 flush_value;
    bool use_flush;

    use_flush = kepler_membar_before_semaphore(push);

    if (use_flush)
        flush_value = HWCONST(A0B5, LAUNCH_DMA, FLUSH_ENABLE, TRUE);
    else
        flush_value = HWCONST(A0B5, LAUNCH_DMA, FLUSH_ENABLE, FALSE);

    NV_PUSH_3U(A0B5, SET_SEMAPHORE_A, NvOffset_HI32(gpu_va),
                     SET_SEMAPHORE_B, NvOffset_LO32(gpu_va),
                     SET_SEMAPHORE_PAYLOAD, 0xdeadbeef);

    NV_PUSH_1U(A0B5, LAUNCH_DMA, flush_value |
       HWCONST(A0B5, LAUNCH_DMA, DATA_TRANSFER_TYPE, NONE) |
       HWCONST(A0B5, LAUNCH_DMA, SEMAPHORE_TYPE, RELEASE_FOUR_WORD_SEMAPHORE));
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

static void kepler_membar_after_transfer(uvm_push_t *push)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_gpu_t *gpu;

    if (uvm_push_get_and_reset_flag(push, UVM_PUSH_FLAG_CE_NEXT_MEMBAR_NONE)) {
        return;
    }

    // Flush on transfers only works when paired with a semaphore release. Use a
    // host WFI + MEMBAR.
    // http://nvbugs/1709888
    gpu = uvm_push_get_gpu(push);
    gpu->host_hal->wait_for_idle(push);

    if (uvm_push_get_and_reset_flag(push, UVM_PUSH_FLAG_CE_NEXT_MEMBAR_GPU))
        gpu->host_hal->membar_gpu(push);
    else
        gpu->host_hal->membar_sys(push);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

static NvU32 ce_aperture(uvm_aperture_t aperture)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    BUILD_BUG_ON(HWCONST(A0B5, SET_SRC_PHYS_MODE, TARGET, LOCAL_FB) != HWCONST(A0B5, SET_DST_PHYS_MODE, TARGET, LOCAL_FB));
    BUILD_BUG_ON(HWCONST(A0B5, SET_SRC_PHYS_MODE, TARGET, COHERENT_SYSMEM) != HWCONST(A0B5, SET_DST_PHYS_MODE, TARGET, COHERENT_SYSMEM));

    UVM_ASSERT_MSG(aperture == UVM_APERTURE_VID || aperture == UVM_APERTURE_SYS, "aperture 0x%x\n", aperture);

    if (aperture == UVM_APERTURE_SYS)
        return HWCONST(A0B5, SET_SRC_PHYS_MODE, TARGET, COHERENT_SYSMEM);
    else
        return HWCONST(A0B5, SET_SRC_PHYS_MODE, TARGET, LOCAL_FB);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

// Push SET_{SRC,DST}_PHYS mode if needed and return LAUNCH_DMA_{SRC,DST}_TYPE flags
NvU32 uvm_hal_kepler_ce_phys_mode(uvm_push_t *push, uvm_gpu_address_t dst, uvm_gpu_address_t src)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NvU32 launch_dma_src_dst_type = 0;

    if (src.is_virtual)
        launch_dma_src_dst_type |= HWCONST(A0B5, LAUNCH_DMA, SRC_TYPE, VIRTUAL);
    else
        launch_dma_src_dst_type |= HWCONST(A0B5, LAUNCH_DMA, SRC_TYPE, PHYSICAL);

    if (dst.is_virtual)
        launch_dma_src_dst_type |= HWCONST(A0B5, LAUNCH_DMA, DST_TYPE, VIRTUAL);
    else
        launch_dma_src_dst_type |= HWCONST(A0B5, LAUNCH_DMA, DST_TYPE, PHYSICAL);

    if (!src.is_virtual && !dst.is_virtual) {
        NV_PUSH_2U(A0B5, SET_SRC_PHYS_MODE, ce_aperture(src.aperture),
                         SET_DST_PHYS_MODE, ce_aperture(dst.aperture));
    }
    else if (!src.is_virtual) {
        NV_PUSH_1U(A0B5, SET_SRC_PHYS_MODE, ce_aperture(src.aperture));
    }
    else if (!dst.is_virtual) {
        NV_PUSH_1U(A0B5, SET_DST_PHYS_MODE, ce_aperture(dst.aperture));
    }

    return launch_dma_src_dst_type;
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_memcopy(uvm_push_t *push, uvm_gpu_address_t dst, uvm_gpu_address_t src, size_t size)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    // If >4GB copies ever become an important use case, this function should
    // use multi-line transfers so we don't have to iterate (bug 1766588).
    static const size_t max_single_copy_size = 0xFFFFFFFF;
    uvm_gpu_t *gpu = uvm_push_get_gpu(push);

    NvU32 pipelined_value;
    NvU32 launch_dma_src_dst_type;
    bool first_operation = true;

    launch_dma_src_dst_type = gpu->ce_hal->phys_mode(push, dst, src);

    do {
        NvU32 copy_this_time = (NvU32)min(size, max_single_copy_size);

        if (first_operation && uvm_push_get_and_reset_flag(push, UVM_PUSH_FLAG_CE_NEXT_PIPELINED))
            pipelined_value = HWCONST(A0B5, LAUNCH_DMA, DATA_TRANSFER_TYPE, PIPELINED);
        else
            pipelined_value = HWCONST(A0B5, LAUNCH_DMA, DATA_TRANSFER_TYPE, NON_PIPELINED);

        gpu->ce_hal->offset_in_out(push, src.address, dst.address);

        NV_PUSH_1U(A0B5, LINE_LENGTH_IN, copy_this_time);

        NV_PUSH_1U(A0B5, LAUNCH_DMA,
           HWCONST(A0B5, LAUNCH_DMA, SRC_MEMORY_LAYOUT, PITCH) |
           HWCONST(A0B5, LAUNCH_DMA, DST_MEMORY_LAYOUT, PITCH) |
           HWCONST(A0B5, LAUNCH_DMA, MULTI_LINE_ENABLE, FALSE) |
           HWCONST(A0B5, LAUNCH_DMA, REMAP_ENABLE, FALSE) |
           HWCONST(A0B5, LAUNCH_DMA, FLUSH_ENABLE, FALSE) |
           launch_dma_src_dst_type |
           pipelined_value);

        dst.address += copy_this_time;
        src.address += copy_this_time;
        size -= copy_this_time;
        first_operation = false;
    } while (size > 0);

    kepler_membar_after_transfer(push);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_memcopy_v_to_v(uvm_push_t *push, NvU64 dst_va, NvU64 src_va, size_t size)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_hal_kepler_ce_memcopy(push, uvm_gpu_address_virtual(dst_va), uvm_gpu_address_virtual(src_va), size);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

// Push SET_DST_PHYS mode if needed and return LAUNCH_DMA_DST_TYPE flags
static NvU32 memset_push_phys_mode(uvm_push_t *push, uvm_gpu_address_t dst)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    if (dst.is_virtual)
        return HWCONST(A0B5, LAUNCH_DMA, DST_TYPE, VIRTUAL);

    NV_PUSH_1U(A0B5, SET_DST_PHYS_MODE, ce_aperture(dst.aperture));
    return HWCONST(A0B5, LAUNCH_DMA, DST_TYPE, PHYSICAL);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

static void memset_common(uvm_push_t *push, uvm_gpu_address_t dst, size_t size, size_t memset_element_size)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    // If >4GB memsets ever become an important use case, this function should
    // use multi-line transfers so we don't have to iterate (bug 1766588).
    static const size_t max_single_memset_size = 0xFFFFFFFF;

    NvU32 pipelined_value;
    bool first_operation = true;
    NvU32 launch_dma_dst_type;

    launch_dma_dst_type = memset_push_phys_mode(push, dst);

    do {
        NvU32 memset_this_time = (NvU32)min(size, max_single_memset_size);

        if (first_operation && uvm_push_get_and_reset_flag(push, UVM_PUSH_FLAG_CE_NEXT_PIPELINED))
            pipelined_value = HWCONST(A0B5, LAUNCH_DMA, DATA_TRANSFER_TYPE, PIPELINED);
        else
            pipelined_value = HWCONST(A0B5, LAUNCH_DMA, DATA_TRANSFER_TYPE, NON_PIPELINED);

        uvm_push_get_gpu(push)->ce_hal->offset_out(push, dst.address);

        NV_PUSH_1U(A0B5, LINE_LENGTH_IN, memset_this_time);

        NV_PUSH_1U(A0B5, LAUNCH_DMA,
           HWCONST(A0B5, LAUNCH_DMA, SRC_MEMORY_LAYOUT, PITCH) |
           HWCONST(A0B5, LAUNCH_DMA, DST_MEMORY_LAYOUT, PITCH) |
           HWCONST(A0B5, LAUNCH_DMA, MULTI_LINE_ENABLE, FALSE) |
           HWCONST(A0B5, LAUNCH_DMA, REMAP_ENABLE, TRUE) |
           HWCONST(A0B5, LAUNCH_DMA, FLUSH_ENABLE, FALSE) |
           launch_dma_dst_type |
           pipelined_value);

        dst.address += memset_this_time * memset_element_size;
        size -= memset_this_time;
        first_operation = false;
    } while (size > 0);

    kepler_membar_after_transfer(push);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_memset_1(uvm_push_t *push, uvm_gpu_address_t dst, NvU8 value, size_t size)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NV_PUSH_2U(A0B5, SET_REMAP_CONST_B,    (NvU32)value,
                     SET_REMAP_COMPONENTS,
       HWCONST(A0B5, SET_REMAP_COMPONENTS, DST_X,               CONST_B) |
       HWCONST(A0B5, SET_REMAP_COMPONENTS, COMPONENT_SIZE,      ONE)     |
       HWCONST(A0B5, SET_REMAP_COMPONENTS, NUM_DST_COMPONENTS,  ONE));

    memset_common(push, dst, size, 1);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_memset_4(uvm_push_t *push, uvm_gpu_address_t dst, NvU32 value, size_t size)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    UVM_ASSERT_MSG(size % 4 == 0, "size: %zd\n", size);

    size /= 4;

    NV_PUSH_2U(A0B5, SET_REMAP_CONST_B,    value,
                     SET_REMAP_COMPONENTS,
       HWCONST(A0B5, SET_REMAP_COMPONENTS, DST_X,               CONST_B) |
       HWCONST(A0B5, SET_REMAP_COMPONENTS, COMPONENT_SIZE,      FOUR)    |
       HWCONST(A0B5, SET_REMAP_COMPONENTS, NUM_DST_COMPONENTS,  ONE));

    memset_common(push, dst, size, 4);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_memset_8(uvm_push_t *push, uvm_gpu_address_t dst, NvU64 value, size_t size)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    UVM_ASSERT_MSG(size % 8 == 0, "size: %zd\n", size);

    size /= 8;

    NV_PUSH_3U(A0B5, SET_REMAP_CONST_A, (NvU32)value,
                     SET_REMAP_CONST_B, (NvU32)(value >> 32),
                     SET_REMAP_COMPONENTS,
       HWCONST(A0B5, SET_REMAP_COMPONENTS, DST_X,               CONST_A) |
       HWCONST(A0B5, SET_REMAP_COMPONENTS, DST_Y,               CONST_B) |
       HWCONST(A0B5, SET_REMAP_COMPONENTS, COMPONENT_SIZE,      FOUR)    |
       HWCONST(A0B5, SET_REMAP_COMPONENTS, NUM_DST_COMPONENTS,  TWO));

    memset_common(push, dst, size, 8);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_kepler_ce_memset_v_4(uvm_push_t *push, NvU64 dst_va, NvU32 value, size_t size)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_hal_kepler_ce_memset_4(push, uvm_gpu_address_virtual(dst_va), value, size);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}
