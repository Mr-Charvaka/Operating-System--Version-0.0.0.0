#ifndef PAGING_H
#define PAGING_H

#include "../include/isr.h"
#include "../include/types.h"

void init_paging();
void switch_page_directory(uint32_t *new_dir);
void paging_map(uint32_t phys, uint32_t virt, uint32_t flags);
extern uint32_t *kernel_directory;

#endif
