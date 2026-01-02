[bits 32]

[extern isr_handler]

; Common ISR code (Exceptions)
isr_common_stub:
    pusha           ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    mov ax, ds      ; Lower 16-bits of eax = ds.
    push eax        ; Save the data segment descriptor

    mov ax, 0x10    ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp        ; Pass pointer to registers_t
    call isr_handler
    add esp, 4      ; Clean up stack

    pop eax         ; reload the original data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; Pops edi,esi,ebp...
    add esp, 8      ; Cleans up the pushed error code and pushed ISR number
    iret            ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

; Common IRQ code (Hardware Interrupts)
irq_common_stub:
    pusha           ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    mov ax, ds      ; Lower 16-bits of eax = ds.
    push eax        ; Save the data segment descriptor

    mov ax, 0x10    ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp        ; Pass pointer to registers_t
    extern irq_handler
    call irq_handler
    add esp, 4

    pop eax         ; reload the original data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; Pops edi,esi,ebp...
    add esp, 8      ; Cleans up the pushed error code and pushed ISR number
    iret

; Macros for ISRs
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        push dword 0      ; Push dummy error code (32-bit)
        push dword %1     ; Push interrupt number (32-bit)
        jmp isr_common_stub
%endmacro

; Macros for IRQs
%macro IRQ 2
    global isr%1
    isr%1:
        push dword 0
        push dword %2
        jmp irq_common_stub
%endmacro

%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        ; Error code already pushed by CPU
        push dword %1 ; Push interrupt number (32-bit)
        jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; IRQs
IRQ 32, 32
IRQ 33, 33
IRQ 34, 34
IRQ 35, 35
IRQ 36, 36
IRQ 37, 37
IRQ 38, 38
IRQ 39, 39
IRQ 40, 40
IRQ 41, 41
IRQ 42, 42
IRQ 43, 43
IRQ 44, 44
IRQ 45, 45
IRQ 46, 46
IRQ 47, 47

; System Call (INT 0x80)
ISR_NOERRCODE 128
; Spurious APIC
ISR_NOERRCODE 255
