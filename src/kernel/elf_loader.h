#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "../include/elf.h"

#include "../include/types.h"

uint32_t load_elf(const char *filename, uint32_t *top_address);

#endif
