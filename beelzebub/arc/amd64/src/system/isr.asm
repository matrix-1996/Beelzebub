global IsrGates
global InterruptHandlers
global InterruptEnders

%assign i 0
%rep 256
    global IsrStub %+ i
    %assign i i+1
%endrep

global IsrStubsBegin
global IsrStubsEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

section .text
bits 64

DEFAULT REL

align 16

IsrCommonStub:
    ;   Upon entry, the lower byte of RCX will contain the interrupt vector.
    ;   We zero-extend that value into the whole register.
    movzx   rcx, cl

    push    rax
    push    rbx
    push    rdx
    push    rbp
    push    rdi
    push    rsi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    ;   Store general-purpose registers, except RCX.

    xor     rax, rax
    mov     ax, ds
    push    rax
    ;   Save data segment.

    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    ;   Make sure the data segments are the kernel's, before accessing data below.

    mov     rdi, rsp
    ;   Stack pointer as first parameter (IsrState *)

    mov     rbx, InterruptHandlers
    mov     rdx, qword [rbx + rcx * 8]
    ;   Handler pointer.

    mov     rbx, InterruptEnders
    mov     rsi, qword [rbx + rcx * 8]
    ;   Ender pointer.

    test    rdx, rdx
    jz      .skip
    ;   A null handler means the call is skipped.

    ;   At this point, the arguments given are the following:
    ;   1. RDI = State pointer
    ;   2. RSI = Ender pointer
    ;   3. RDX = Handler pointer
    ;   4. RCX = Vector
    call    rdx                            ;   Call handler

.skip:
    pop     rax 
    mov     es, ax
    mov     ds, ax
    ;   Simply restore the data segments.

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rsi
    pop     rdi
    pop     rbp
    pop     rdx
    pop     rbx
    pop     rax
    ;   Restore general-purpose registers, except RCX.

    pop     rcx
    ;   Pop RCX

    add     rsp, 8
    ;   "Pop" error code.

    iretq

%macro ISR_NOERRCODE 1
    IsrStub%1:
        cli
        push    qword 0
        push    rcx
        mov     cl, %1
        jmp     IsrCommonStub
    align 16
%endmacro

%macro ISR_ERRCODE 1
    IsrStub%1:
        cli
        push    rcx
        mov     cl, %1
        jmp     IsrCommonStub
    align 16
%endmacro

align 16

IsrStubsBegin:

    ISR_NOERRCODE 0
    ISR_NOERRCODE 1
    ISR_NOERRCODE 2
    ISR_NOERRCODE 3
    ISR_NOERRCODE 4
    ISR_NOERRCODE 5
    ISR_NOERRCODE 6
    ISR_NOERRCODE 7
    ISR_ERRCODE   8
    ISR_NOERRCODE 9
    ISR_ERRCODE   10
    ISR_ERRCODE   11
    ISR_ERRCODE   12
    ISR_ERRCODE   13
    ISR_ERRCODE   14
    ISR_NOERRCODE 15
    ISR_NOERRCODE 16
    ISR_NOERRCODE 17
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

    %assign i 32
    %rep 224
        ISR_NOERRCODE i
        %assign i i+1
    %endrep

IsrStubsEnd:
    nop ;   Dummy.

DEFAULT ABS

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

section .data

align 16

IsrGates:
    %assign i 0
    %rep 256
        dq IsrStub %+ i
        %assign i i+1
    %endrep

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

section .bss

align 16

InterruptHandlers:
    resb 8 * 256
InterruptEnders:
    resb 8 * 256
