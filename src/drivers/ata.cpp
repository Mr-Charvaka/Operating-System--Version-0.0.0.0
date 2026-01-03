#include "ata.h"
#include "../include/io.h"
#include "serial.h"

void ata_wait_bsy() {
  while (inb(ATA_STATUS) & ATA_SR_BSY)
    ;
}

void ata_wait_drq() {
  while (!(inb(ATA_STATUS) & ATA_SR_DRQ))
    ;
}

void ata_read_sector(uint32_t lba, uint8_t *buffer) {
  asm volatile("cli");
  ata_wait_bsy();

  outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); // Select Master Drive
  outb(ATA_ERROR, 0x00);                             // Null byte
  outb(ATA_SECTOR_CNT, 1);                           // Sector Count = 1
  outb(ATA_LBA_LO, (uint8_t)lba);
  outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
  outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
  outb(ATA_COMMAND, ATA_CMD_READ_PIO);

  ata_wait_bsy();
  ata_wait_drq();

  // Read 256 words (512 bytes)
  uint16_t *buf16 = (uint16_t *)buffer;
  for (int i = 0; i < 256; i++) {
    buf16[i] = inw(ATA_DATA);
  }
  asm volatile("sti");
}

void ata_write_sector(uint32_t lba, uint8_t *buffer) {
  ata_wait_bsy();

  outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_ERROR, 0x00);
  outb(ATA_SECTOR_CNT, 1);
  outb(ATA_LBA_LO, (uint8_t)lba);
  outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
  outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
  outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

  ata_wait_bsy();
  ata_wait_drq();

  // Write 256 words
  uint16_t *buf16 = (uint16_t *)buffer;
  for (int i = 0; i < 256; i++) {
    outw(ATA_DATA, buf16[i]);
  }

  // Flush Cache
  outb(ATA_COMMAND, 0xE7);
  ata_wait_bsy();
}
