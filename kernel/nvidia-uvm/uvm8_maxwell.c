/*******************************************************************************
    Copyright (c) 2016-2019 NVIDIA Corporation

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
#include "uvm8_gpu.h"
#include "uvm8_mem.h"

void uvm_hal_maxwell_arch_init_properties(uvm_gpu_t *gpu)
{
    gpu->big_page.swizzling = false;

    gpu->tlb_batch.va_invalidate_supported = false;

    // 128 GB should be enough for all current RM allocations and leaves enough
    // space for UVM internal mappings.
    // A single top level PDE covers 64 or 128 MB on Maxwell so 128 GB is fine to use.
    gpu->rm_va_base = 0;
    gpu->rm_va_size = 128ull * 1024 * 1024 * 1024;

    gpu->uvm_mem_va_base = 768ull * 1024 * 1024 * 1024;
    gpu->uvm_mem_va_size = UVM_MEM_VA_SIZE;

    // We don't have a compelling use case in UVM-Lite for direct peer
    // migrations between GPUs, so don't bother setting them up.
    gpu->peer_copy_mode = UVM_GPU_PEER_COPY_MODE_UNSUPPORTED;

    gpu->max_channel_va = 1ULL << 40;

    // Maxwell can only map sysmem with 4K pages
    gpu->can_map_sysmem_with_large_pages = false;

    // Maxwell cannot place GPFIFO in vidmem
    gpu->gpfifo_in_vidmem_supported = false;

    gpu->replayable_faults_supported = false;

    gpu->non_replayable_faults_supported = false;

    gpu->access_counters_supported = false;

    gpu->fault_cancel_va_supported = false;

    gpu->scoped_atomics_supported = false;

    gpu->has_pulse_based_interrupts = false;




}
