/* _NVRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by NVIDIA Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 *
 * _NVRM_COPYRIGHT_END_
 */

#ifndef _NV_PCI_H_
#define _NV_PCI_H_

#include <linux/pci.h>
#include "nv-linux.h"

#if defined(NV_DEV_IS_PCI_PRESENT)
#define nv_dev_is_pci(dev) dev_is_pci(dev)
#else
#define nv_dev_is_pci(dev) (true)
#endif

int nv_pci_register_driver(void);
void nv_pci_unregister_driver(void);
int nv_pci_count_devices(nvidia_stack_t *sp);
NvU8 nv_find_pci_capability(struct pci_dev *, NvU8);
int nvidia_dev_get_pci_info(const NvU8 *, struct pci_dev **, NvU64 *, NvU64 *);
nv_linux_state_t * find_pci(NvU32, NvU8, NvU8, NvU8);

#endif
