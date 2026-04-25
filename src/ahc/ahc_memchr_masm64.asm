.CODE
PUBLIC ahc_memchr_masm64
ahc_memchr_masm64 PROC
  mov r10, rcx
  movzx eax, dl
  test r8, r8
  jz  ahc_mchr0
L1:
  movzx r11d, byte ptr [r10]
  cmp   r11b, al
  je    ahc_mchrok
  inc   r10
  dec   r8
  jne   L1
ahc_mchr0:
  xor rax, rax
  ret
ahc_mchrok:
  mov rax, r10
  ret
ahc_memchr_masm64 ENDP
END
