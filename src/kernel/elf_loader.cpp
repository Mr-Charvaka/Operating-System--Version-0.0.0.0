#include "../drivers/serial.h"
#include "../include/elf.h"
#include "../include/string.h"
#include "../include/vfs.h"
#include "../kernel/heap.h" // Added for kfree
#include "../kernel/memory.h"
#include "../kernel/pmm.h"
#include "../kernel/vm.h"

// ... imports ...

uint32_t load_elf(const char *filename, uint32_t *top_address) {
  serial_log("ELF: Loading file via VFS...");
  serial_log(filename);

  vfs_node_t *node = finddir_vfs(vfs_root, (char *)filename);
  if (node == 0) {
    serial_log("ELF ERROR: File not found in VFS.");
    return 0;
  }

  uint8_t *buffer = (uint8_t *)kmalloc(node->length + 512);
  read_vfs(node, 0, node->length, buffer);

  Elf32_Ehdr *ehdr = (Elf32_Ehdr *)buffer;

  // Validate Magic
  if (memcmp(ehdr->e_ident, "\x7f\x45\x4c\x46", 4) != 0) {
    serial_log("ELF ERROR: Invalid Magic.");
    kfree(buffer); // Good practice
    return 0;
  }

  serial_log("ELF: Valid Magic found.");
  serial_log_hex("ELF: Entry Point: ", ehdr->e_entry);
  uint32_t entry_point = ehdr->e_entry;

  uint32_t max_addr = 0;

  // Parse Program Headers
  Elf32_Phdr *phdr = (Elf32_Phdr *)(buffer + ehdr->e_phoff);
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
      serial_log_hex("ELF: Loading segment at ", phdr[i].p_vaddr);
      serial_log_hex("ELF: Segment size: ", phdr[i].p_memsz);

      // Ensure Pages are mapped!
      // This is crucial for per-process directories where the user range is
      // initially empty.
      uint32_t start_addr = phdr[i].p_vaddr;
      uint32_t end_addr = start_addr + phdr[i].p_memsz;

      if (end_addr > max_addr) {
        max_addr = end_addr;
      }

      uint32_t start_page = start_addr & 0xFFFFF000;
      uint32_t end_page = (end_addr + 0xFFF) & 0xFFFFF000;

      for (uint32_t page = start_page; page < end_page; page += 4096) {
        if (vm_get_phys(page) == 0) {
          // Allocate new frame and map it
          // User Mode | RW | Present = 7
          vm_map_page((uint32_t)pmm_alloc_block(), page, 7);
        }
      }

      // Safe to copy now
      memcpy((void *)phdr[i].p_vaddr, buffer + phdr[i].p_offset,
             phdr[i].p_filesz);

      // Zero out BSS
      if (phdr[i].p_memsz > phdr[i].p_filesz) {
        memset((void *)(phdr[i].p_vaddr + phdr[i].p_filesz), 0,
               phdr[i].p_memsz - phdr[i].p_filesz);
      }
    }
  }

  if (top_address) {
    // Align to next page boundary for safety
    *top_address = (max_addr + 0xFFF) & 0xFFFFF000;
  }

  kfree(buffer); // Clean up
  serial_log_hex("ELF: Returning entry point ", entry_point);
  return entry_point;
}
