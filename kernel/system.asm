global timer_interrupt, system_call
extern do_timer, sys_call_table
align 4

system_call:
    push ds
    push es
    push fs
    push edi
    push esi
    push edx
    push ecx
    push ebx


    mov edx,0x10
    mov ds,dx
    mov es,dx

    call [sys_call_table + eax*4]
    push eax

ret_from_sys_call:
    pop eax
    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi
    pop fs
    pop es
    pop ds
iret

timer_interrupt:
    push ds
    push es
    push fs
    push edx
    push ecx
    push ebx
    push eax

    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ax,0x17
    mov fs,ax

    mov al,0x20
    out 0x20,al
;    out 0xA0,al

    call do_timer

    pop eax
    pop ebx
    pop ecx
    pop edx
    pop fs
    pop es
    pop ds
    iret