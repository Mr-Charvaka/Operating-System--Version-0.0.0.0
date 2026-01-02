[org 0x7c00]
    jmp short start
    nop

    ; FAT16 BPB (BIOS Parameter Block)
    bsOemName               db "MSWIN4.1"
    bpbBytesPerSec          dw 512
    bpbSecPerClust          db 1
    bpbResSectors           dw 4096       ; RESERVED for Kernel (2MB safe zone)
    bpbNumFATs              db 2
    bpbRootEntCnt           dw 512
    bpbTotSec16             dw 0
    bpbMedia                db 0xF8
    bpbFATSz16              dw 256        ; 256 sectors per FAT (covering 32MB)
    bpbSecPerTrk            dw 63         ; Maximize sectors per track
    bpbNumHeads             dw 16         ; More heads
    bpbHiddSec              dd 0
    bpbTotSec32             dd 65536      ; 32MB Total Sectors
    bsDrvNum                db 0x80
    bsReserved1             db 0
    bsBootSig               db 0x29
    bsVolID                 dd 0x12345678
    bsVolLab                db "MYOS       "
    bsFilSysType            db "FAT16   "

start:
KERNEL_OFFSET equ 0xC000 ; Match linker.ld address (Low Memory)

    mov [BOOT_DRIVE], dl ; BIOS stores our boot drive in DL

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov bp, 0x9000
    mov sp, bp

    mov bx, MSG_REAL_MODE
    call print_string

    call enable_a20  ; Enable A20 line (good practice)
    call load_kernel ; Load our kernel

    ; Removed BIOS VGA switch - We use BGA later
    call switch_to_pm ; Note that we never return from here.

    jmp $

%include "print.asm"
%include "disk.asm"
%include "gdt.asm"
%include "print_pm.asm"
%include "switch_pm.asm"

[bits 16]
enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

[bits 16]
load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print_string

    ; Set ES:BX to load to KERNEL_OFFSET (0xC000)
    ; 0x0C00:0x0000 = 0xC000
    mov ax, 0x0C00
    mov es, ax
    xor bx, bx

    mov cx, 600  ; Read 600 sectors (300KB) - enough for kernel
    mov byte [ABS_S], 2 ; Start at sector 2
    mov byte [ABS_H], 0
    mov byte [ABS_C], 0

.read_one:
    push cx
    mov al, 1
    mov cl, [ABS_S]
    mov dh, [ABS_H]
    mov ch, [ABS_C]
    mov dl, [BOOT_DRIVE]
    call disk_read_sector

    add bx, 512
    jnc .skip_es
    mov ax, es
    add ax, 0x1000 ; Move to next 64KB block (increment segment by 4KB)
    mov es, ax
.skip_es:

    inc byte [ABS_S]
    cmp byte [ABS_S], 63 ; Max 63 sectors/track
    jbe .done_inc
    
    mov byte [ABS_S], 1
    inc byte [ABS_H]
    cmp byte [ABS_H], 16 ; Max 16 heads
    jb .done_inc
    
    mov byte [ABS_H], 0
    inc byte [ABS_C]

.done_inc:
    ; Print a dot every 16 sectors to show progress without spamming
    test cx, 0x0F
    jnz .skip_dot
    mov ah, 0x0E
    mov al, '.'
    int 0x10
.skip_dot:
    pop cx
    loop .read_one
    ret

ABS_S db 0
ABS_H db 0
ABS_C db 0

[bits 32]
; This is where we arrive after switching to and initialising protected mode.
BEGIN_PM:
    mov ebx, MSG_PROT_MODE
    call print_string_pm

    call KERNEL_OFFSET ; Now jump to the entry point of our loaded kernel code

    jmp $ ; Hang.

; Global variables
BOOT_DRIVE db 0
MSG_REAL_MODE db "Started in 16-bit Real Mode", 0
MSG_PROT_MODE db "Successfully landed in 32-bit Protected Mode", 0
MSG_LOAD_KERNEL db "Loading kernel into memory.", 0

; Bootsector padding
times 510-($-$$) db 0
dw 0xaa55
