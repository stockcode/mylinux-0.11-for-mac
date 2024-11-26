section .text
bits 32
global _start,gdt,idt,pg_dir
extern user_stack_top
extern kernel_start
pg_dir:
_start:
        mov ax,0x10
        mov ds,ax
        mov es,ax
        mov ss,ax
        mov esp,[user_stack_top]
        lgdt [gdt_desc]
        jmp 8:go

go:        
        mov ax,0x10
        mov ds,ax
        mov es,ax
        mov ss,ax
        mov ax,0x1b
        mov gs,ax
        mov esp,[user_stack_top]
;check A20
.1:
        xor eax,eax
        inc eax
        mov [0],eax
        cmp [0x100000],eax
        je .1
        call setup_idt
        jmp after_page_table
setup_idt:
        mov eax,0x00080000
        mov edx,default_idthandler
        mov ax,dx
        mov dx,0x0e00
        mov ecx,256
        mov edi,idt
rp_sidt:
        mov [edi],eax
        mov [edi+4],edx
        add edi,8
        loop rp_sidt
        lidt [idt_desc]
        ret
        times 0x1000-($-$$) db 0
pg0:
        times 0x1000 db 0
pg1:
        times 0x1000 db 0
pg2:
        times 0x1000 db 0                        
pg3:
        times 0x1000 db 0        
after_page_table:
        call setup_paging
        call kernel_start
        jmp $

default_idthandler:

        iret
setup_paging:
        xor eax,eax
        mov ecx,1024*5
        xor edi,edi
        rep
        stosd
        mov dword[pg_dir],pg0+7
        mov dword[pg_dir+4],pg1+7
        mov dword[pg_dir+8],pg2+7
        mov dword[pg_dir+12],pg3+7
        mov eax,0xfff000+7
        mov edi,pg3+4092
        std
.1:
        stosd
        sub eax,0x1000
        cmp eax,0x1000        
        jae .1

        mov eax,pg_dir
        mov cr3,eax

        mov eax,cr0
        or eax,0x80000000
        mov cr0, eax

        ret
align 4
dw 0
idt_desc:
        dw 256*8-1
        dd idt
dw 0
gdt_desc:        
        dw 256*8-1
        dd gdt
align 8
idt:
        times 256 dq 0

gdt:
    dw  0,0,0,0     ;dummy

    dw  0FFFh      ;16Mb - limit=2047 (4096*4096)=16Mb
    dw  0000h       ; base address=0
    dw  9A00h       ; code read/exec
    dw  00C0h       ;   granularity=4096, 386

    dw  0FFFh      ;16Mb - limit=2047 (4096*4096)=16Mb
    dw  0000h       ; base address=0
    dw  9200h       ; data read/exec
    dw  00C0h       ;   granularity=4096, 386

    ;video
    dw 0x7fff
    dw 0x8000
    dw 0xf20b
    dw 0x0040

    times 252 dq 0