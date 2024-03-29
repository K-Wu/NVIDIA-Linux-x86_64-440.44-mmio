#include <linux/kernel.h>
/*******************************************************************************
    Copyright (c) 2017 NVIDIA Corporation

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
#include "uvm8_user_channel.h"
#include "clc46f.h"

void uvm_hal_turing_host_semaphore_release(uvm_push_t *push, uvm_gpu_semaphore_t *semaphore, NvU32 payload)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_gpu_t *gpu = uvm_push_get_gpu(push);
    NvU64 semaphore_va = uvm_gpu_semaphore_get_gpu_va(semaphore, gpu);
    NV_PUSH_5U(C46F, SEM_ADDR_LO,    NvOffset_LO32(semaphore_va),
                     SEM_ADDR_HI,    HWVALUE(C46F, SEM_ADDR_HI, OFFSET, NvOffset_HI32(semaphore_va)),
                     SEM_PAYLOAD_LO, payload,
                     SEM_PAYLOAD_HI, 0,
                     SEM_EXECUTE,    HWCONST(C46F, SEM_EXECUTE, OPERATION, RELEASE) |
                                     HWCONST(C46F, SEM_EXECUTE, PAYLOAD_SIZE, 32BIT) |
                                     HWCONST(C46F, SEM_EXECUTE, RELEASE_TIMESTAMP, DIS) |
                                     HWCONST(C46F, SEM_EXECUTE, RELEASE_WFI, DIS));
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_turing_host_semaphore_acquire(uvm_push_t *push, uvm_gpu_semaphore_t *semaphore, NvU32 payload)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_gpu_t *gpu = uvm_push_get_gpu(push);
    NvU64 semaphore_va = uvm_gpu_semaphore_get_gpu_va(semaphore, gpu);
    NV_PUSH_5U(C46F, SEM_ADDR_LO,    NvOffset_LO32(semaphore_va),
                     SEM_ADDR_HI,    HWVALUE(C46F, SEM_ADDR_HI, OFFSET, NvOffset_HI32(semaphore_va)),
                     SEM_PAYLOAD_LO, payload,
                     SEM_PAYLOAD_HI, 0,
                     SEM_EXECUTE,    HWCONST(C46F, SEM_EXECUTE, OPERATION, ACQ_CIRC_GEQ) |
                                     HWCONST(C46F, SEM_EXECUTE, PAYLOAD_SIZE, 32BIT) |
                                     HWCONST(C46F, SEM_EXECUTE, ACQUIRE_SWITCH_TSG, EN));
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_hal_turing_host_clear_faulted_channel_method(uvm_push_t *push,
                                                      uvm_user_channel_t *user_channel,
                                                      const uvm_fault_buffer_entry_t *fault)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NvU32 clear_type_value = 0;

    UVM_ASSERT(user_channel->gpu->has_clear_faulted_channel_method);

    if (fault->fault_source.mmu_engine_type == UVM_MMU_ENGINE_TYPE_HOST) {
        clear_type_value = HWCONST(C46F, CLEAR_FAULTED, TYPE, PBDMA_FAULTED);
    }
    else if (fault->fault_source.mmu_engine_type == UVM_MMU_ENGINE_TYPE_CE) {
        clear_type_value = HWCONST(C46F, CLEAR_FAULTED, TYPE, ENG_FAULTED);
    }
    else {
        UVM_ASSERT_MSG(false, "Unsupported MMU engine type %s\n",
                       uvm_mmu_engine_type_string(fault->fault_source.mmu_engine_type));
    }

    NV_PUSH_1U(C46F, CLEAR_FAULTED, HWVALUE(C46F, CLEAR_FAULTED, HANDLE, user_channel->clear_faulted_token) |
                                    clear_type_value);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}
