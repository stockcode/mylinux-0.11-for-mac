BOOTSEG equ 0x07c0
INITSEG equ 0x9000
SETUPSEG equ 0x9020
SETUPLEN equ 4
SYSSEG  equ 0x1000
SYSSIZE equ 0x3000
SYSEND equ SYSSEG+SYSSIZE
section .text
    ;ds:si -> es:di,cx
    mov ax, BOOTSEG
    mov ds,ax
    mov ax, INITSEG
    mov es,ax
    mov cx,256
    xor si,si
    xor di,di
    rep
    movsw
    jmp INITSEG:go
go:
    mov ax,INITSEG
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x0FF00
load_setup:
    mov dx,0x0000
    mov cx,0x0002
    mov bx,0x0200
    mov ax, 0x0200+SETUPLEN    
    int 0x13
    jnc ok_load_setup
    mov dx,0x0000
    mov ax,0x0000
    int 0x13
    jmp load_setup

ok_load_setup:    
;get the parametor of the dis,especially the sectors of a track

    mov dl,0x00
    mov ax,0x0800    
    int 0x13
    mov ch,0x00
    mov [sectors],cx
    mov ax, INITSEG
    mov es,ax

;Print some inane message

    mov ah,0x03 ; read cursor pos
    xor bh,bh
    int 0x10

    mov cx,0x30
    mov bx, 0x0007
    mov bp, msg1
    mov ax,0x1301
    int 0x10

; ok, we've written the message, now
; we want to load the system (at 0x10000)

    mov ax,SYSSEG
    mov es,ax
    call read_it
    call kill_motor
    jmp SETUPSEG:0    

read_it:
    mov ax,es
    test ax,0x0fff
die:    jne die
    xor bx, bx
rp_read:
    mov ax,es
    cmp ax,SYSEND
    jb ok_read1
    ret
ok_read1:
    mov ax,[sectors]
    sub ax,[sread]
    mov cx,ax
    shl cx,9
    add cx,bx
    jnc ok_read2
    je ok_read2
    xor ax,ax
    sub ax,bx
    shr ax,9
ok_read2:
    call read_track
    mov cx,ax
    add ax,[sread]
    cmp ax,[sectors]
    jne ok_read3
    mov ax,1
    sub ax,[head]
    jne ok_read4
    inc word[track]
ok_read4:
    mov [head],ax
    xor ax,ax
ok_read3:
    mov [sread], ax
    shl cx,9
    add bx,cx
    jnc rp_read
    mov ax,es
    add ax,0x1000
    mov es,ax
    xor bx,bx
    jmp rp_read
read_track:
    push ax
    push bx
    push cx
    push dx
    mov dx,[track]
    mov cx,[sread]
    inc cx    
    mov ch,dl
    mov dx,[head]
    mov dh,dl
    mov dl,00
    and dx,0x0100
    mov ah,02
    int 0x13
    jc bad_rt
    pop dx
    pop cx
    pop bx
    pop ax
    ret
bad_rt:
    mov ah,00
    mov dl,00
    int 0x13
    pop dx
    pop cx
    pop bx
    pop ax
    jmp read_track

;/*
; * This procedure turns off the floppy drive motor, so
; * that we enter the kernel in a known state, and
; * don't have to worry about it later.
; */
kill_motor:
	push	dx
	mov	dx, 0x3f2
	mov	al, 0x00
	out dx,al
	pop	dx
	ret

head dw 0
track dw 0
sread dw 1+SETUPLEN
sectors dw 0
msg1:
    db 'IceCityOS is booting ...'
times 510-($-$$) db 0
dw 0xaa55