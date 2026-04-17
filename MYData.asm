; 
;
; Target: x86-64 Linux
; Optimization: -O3 -march=native -mtune=native
;
; Registers:
;   SCR = RAX   (Accumulator)
;   SCT = RBX   (Base)
;   SCX = RCX   (Counter)
;   SCZ = RDX   (Data)
;   SCN = R8    (Numeric)
;
;   iIIIIi
;   IIIiii
;   IIiiiI
;   IIIIIi
;   IIIiII
;   IIIIII
; ════════════════════════════════════════════════════════════════════
global _start
section .data
    Hello_from_NHP_Compiler_v3_0_ db 'Hello from NHP Compiler v3.0!',0
    Hello_from_NHP_Compiler_v3_0__len equ $ - Hello_from_NHP_Compiler_v3_0_
    _newline db 10
section .bss
    _input_buf resb 256
    _output_buf resb 256
    _var_space resb 0
section .text
_start:
    ; === Execute_system ===
    ; Print 'Hello from NHP Compiler v3.0!'
    mov  rax, 1
    mov  rdi, 1
    mov  rsi, Hello_from_NHP_Compiler_v3_0_
    mov  rdx, Hello_from_NHP_Compiler_v3_0__len
    syscall
    ; === end Execute_system ===
    ; ════ Program Exit ════
    mov  rax, 60    ; sys_exit
    xor  rdi, rdi   ; status 0
    syscall
