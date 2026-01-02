#ifndef PCI_H
#define PCI_H

#include "../include/types.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

void pci_check_buses();
uint32_t pci_get_bga_bar0(); // Special helper for our goal

#endif
