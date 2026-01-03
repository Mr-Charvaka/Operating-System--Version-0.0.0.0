[bits 32]
global _start
global stack_top
extern main

VIRTUAL_BASE equ 0xC0000000
PDE_INDEX equ (VIRTUAL_BASE >> 22)

; Map 128MB during boot (32 page tables) - expanded for larger kernel support
NUM_BOOT_TABLES equ 32
NUM_BOOT_ENTRIES equ (NUM_BOOT_TABLES * 1024)

section .text
_start:
    ; Setup PDEs for each boot page table
    %assign i 0
    %rep NUM_BOOT_TABLES
        mov eax, (BootPageTables - VIRTUAL_BASE + i * 4096)
        or eax, 0x3
        mov [BootPageDirectory - VIRTUAL_BASE + i * 4], eax
        mov [BootPageDirectory - VIRTUAL_BASE + (PDE_INDEX + i) * 4], eax
        %assign i i+1
    %endrep
    
    ; Set CR3
    mov eax, (BootPageDirectory - VIRTUAL_BASE)
    mov cr3, eax
    
    ; Enable Paging
    mov eax, cr0
    or eax, 0x80000000 
    mov cr0, eax
    
    lea eax, [higher_half]
    jmp eax

higher_half:
    mov esp, stack_top
    call main

    cli
.halt:
    hlt
    jmp .halt

section .data
align 4096
BootPageDirectory:
    times 1024 dd 0

align 4096
BootPageTables:
    ; Identity map 64MB (16 tables * 1024 entries * 4KB = 64MB)
    %assign i 0
    %rep NUM_BOOT_ENTRIES
        dd (i << 12) | 0x3
        %assign i i+1
    %endrep

section .bss
align 16
stack_bottom:
    resb 65536 ; 64KB stack (increased for more headroom)
stack_top:
