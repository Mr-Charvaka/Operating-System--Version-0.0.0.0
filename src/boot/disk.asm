[bits 16]
; load DH sectors to ES:BX from drive DL starting from sector CL, head DH_head, cyl CH
; Actually let's simplify for the caller
; AL = sectors to read, DL = drive, ES:BX = destination
; CL = sector, CH = cylinder, DH_head = head
disk_read_sector:
    pusha
    
    mov ah, 0x02
    ; AL is already sectors
    ; CL is sector
    ; CH is cylinder
    ; DH is head (from caller)
    ; DL is drive (from caller)
    
    int 0x13
    jc .error
    
    popa
    ret

.error:
    mov bx, .err_msg
    call print_string
    jmp $
.err_msg db "Disk Error!", 0

; NEW load_kernel logic in boot.asm will call this
