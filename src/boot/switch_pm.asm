[bits 16]
switch_to_pm:
    cli ; 1. must switch of interrupts until we have
        ; set-up the protected mode interrupt vector
        ; otherwise interrupts will run riot

    lgdt [gdt_descriptor] ; 2. Load the GDT descriptor

    mov eax, cr0
    or eax, 0x1 ; 3. set 32-bit mode bit in cr0
    mov cr0, eax

    jmp CODE_SEG:init_pm ; 4. far jump by using a different segment

[bits 32]
init_pm:
    mov ax, DATA_SEG ; 5. update segment registers
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x1E000000 ; 6. update the stack to 480MB (Safe from Heap and BSS)
    mov esp, ebp

    call BEGIN_PM ; 7. Call some well-known label
