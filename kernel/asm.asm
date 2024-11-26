section .text
extern do_divide_error,do_debug,do_nmi,do_int3,do_overflow,do_bounds,do_invalid_op,do_device_not_available,do_coprocessor_error,do_double_fault,do_invalid_TSS,do_segment_not_present,do_stack_segment,do_general_protection
global error_code,divide_error,debug,nmi,int3,overflow,bounds,invalid_op,device_not_available,coprocessor_segment_overrun,double_fault,invalid_TSS,segment_not_present,stack_segment,general_protection
error_code:
    xchg eax, dword [esp+4]
    xchg ebx, dword [esp]
    push ecx
    push edx
    push esi
    push edi
    push ebp
    push ds
    push es
    push fs

    push eax
    lea eax, [esp+44]
    push eax
    mov ax,0x10
    mov ds,ax
    mov es,ax

    call ebx
    add esp,0

    pop fs
    pop es
    pop ds
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax

iret

;int 0
divide_error:
    push 0
    push do_divide_error
    jmp error_code

;int 1
debug:
    push 0
    push do_debug
    jmp error_code

;int 2
nmi:
    push 0
    push do_nmi
    jmp error_code

;int 3
int3:
    push 0
    push do_int3
    jmp error_code

;int 4
overflow:
    push 0
    push do_overflow
    jmp error_code

;int 5
bounds:
    push 0
    push do_bounds
    jmp error_code

;int 6
invalid_op:
    push 0
    push do_invalid_op
    jmp error_code

;int 7
device_not_available:
    push 0
    push do_device_not_available
    jmp error_code

;int 9

coprocessor_segment_overrun:
    push 0
    push do_coprocessor_error
    jmp error_code

;int 8
double_fault:
    push do_double_fault
    jmp error_code

;int 10
invalid_TSS:
    push do_invalid_TSS
    jmp error_code

;int 11
segment_not_present:
    push do_segment_not_present
    jmp error_code

;int 12
stack_segment:
    push do_stack_segment
    jmp error_code

;int 13
general_protection:
    push do_general_protection
    jmp error_code