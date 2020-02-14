#include <linux/kernel.h>
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

#include "uvm_linux.h"
#include "uvm8_perf_heuristics.h"
#include "uvm8_perf_thrashing.h"
#include "uvm8_perf_prefetch.h"
#include "uvm8_gpu_access_counters.h"
#include "uvm8_va_space.h"

NV_STATUS uvm_perf_heuristics_init()
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NV_STATUS status;

    status = uvm_perf_thrashing_init();
    if (status != NV_OK)
        return status;

    status = uvm_perf_prefetch_init();
    if (status != NV_OK)
        return status;

    status = uvm_perf_access_counters_init();
    if (status != NV_OK)
        return status;

    return NV_OK;
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_perf_heuristics_exit()
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_perf_access_counters_exit();
    uvm_perf_prefetch_exit();
    uvm_perf_thrashing_exit();
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

NV_STATUS uvm_perf_heuristics_add_gpu(uvm_gpu_t *gpu)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_assert_mutex_locked(&g_uvm_global.global_lock);

    return uvm_perf_thrashing_add_gpu(gpu);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_perf_heuristics_remove_gpu(uvm_gpu_t *gpu)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_assert_mutex_locked(&g_uvm_global.global_lock);

    uvm_perf_thrashing_remove_gpu(gpu);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

NV_STATUS uvm_perf_heuristics_load(uvm_va_space_t *va_space)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    NV_STATUS status;

    status = uvm_perf_thrashing_load(va_space);
    if (status != NV_OK)
        return status;
    status = uvm_perf_prefetch_load(va_space);
    if (status != NV_OK)
        return status;
    status = uvm_perf_access_counters_load(va_space);
    if (status != NV_OK)
        return status;

    return NV_OK;
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

NV_STATUS uvm_perf_heuristics_register_gpu(uvm_va_space_t *va_space, uvm_gpu_t *gpu)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_assert_rwsem_locked_write(&va_space->lock);

    return uvm_perf_thrashing_register_gpu(va_space, gpu);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_perf_heuristics_stop(uvm_va_space_t *va_space)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_assert_lockable_order(UVM_LOCK_ORDER_VA_SPACE);

    // Prefetch heuristics don't need a stop operation for now
    uvm_perf_thrashing_stop(va_space);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}

void uvm_perf_heuristics_unload(uvm_va_space_t *va_space)
{pr_info("UVM entering %s in %s(LINE:%s) dumping stack\n",__func__,__FILE__,__LINE__);dump_stack();pr_info("UVM entering %s in %s(LINE:%s) dumped stack\n",__func__,__FILE__,__LINE__);
    uvm_assert_rwsem_locked_write(&va_space->lock);

    uvm_perf_access_counters_unload(va_space);
    uvm_perf_prefetch_unload(va_space);
    uvm_perf_thrashing_unload(va_space);
pr_info("UVM leaving %s in %s(LINE:%s)\n",__func__,__FILE__,__LINE__);}
