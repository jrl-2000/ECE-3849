    	.thumb				; select the Thumb-2 instruction set
		.text				; assemble into the C code section
		.global block_copy32; allow other modules to see this label
block_copy32 .asmfunc		; lets the debugger find the function
; r0 = destination pointer
; r1 = source pointer
; r2 = number of 32-byte blocks to copy
        push  {r4-r10}      ; save work registers on the stack
        cmp   r2, #0
        beq   done1         ; if r2 == 0, return
loop1   ldm   r1!, {r3-r10} ; load 8 words, update src pointer
        stm   r0!, {r3-r10} ; store 8 words, update dst pointer
        subs  r2, #1        ; update block count
        bne   loop1         ; loop if remaining count != 0
done1   pop   {r4-r10}      ; restore work registers
        bx    lr            ; return
    	.endasmfunc
    	
    	.global block_copy4
    	.align 4
;block_copy4 .asmfunc
; r0 = dst = destination pointer
; r1 = src = source pointer
; r2 = count = number of 4-byte words to copy
;        add   r12, r0, r2, lsl #2 ; r12 = stop = src + count;
;        b     test1               ; jump to loop test
;        .align 4
;loop2   ldr   r3, [r1], #4        ; load word, update src pointer
;        str   r3, [r0], #4        ; store word, update dst pointer
;test1   cmp   r0, r12             ; compare src to stop
;        bne   loop2               ; if (src != stop) continue loop
;        bx    lr                  ; return
;    	.endasmfunc
    	
;    	.global block_copy4
;   	.align 4
;block_copy4 .asmfunc
; r0 = destination pointer
; r1 = source pointer
; r2 = number of 4-byte words to copy
;        cmp   r2, #0
;        beq   done2         ; if r2 == 0, return
;loop2   ldr   r3, [r1], #4  ; load word, update src pointer
;        str   r3, [r0], #4  ; store word, update dst pointer
;        subs  r2, #1        ; update word count, set flags
;        bne   loop2         ; loop if remaining count != 0
;done2   bx    lr            ; return
;    	.endasmfunc

block_copy4 .asmfunc
; Arguments:
; r0 = dst = destination pointer
; r1 = src = source pointer
; r2 = count = number of 4-byte words to copy
; Local variables:
; r3 = offset = i * 4  (does not follow the C code exactly)
; r12 = temp

;        mov   r3, #0         ; offset = 0;
;        b     test1          ; jump to loop test
;        .align 4
;loop2   ldr   r12, [r1, r3]  ; load word from source
;        str   r12, [r0, r3]  ; store word to destination
;        add   r3, r3, #4     ; offset += 4;
;test1   cmp   r3, r2, lsl #2 ; compare offset to (count * 4)
;        blo   loop2          ; if (offset < count * 4), continue loop
;        bx    lr             ; return

; block_copy4
; Arguments:
; r0 = dst = destination pointer
; r1 = src = source pointer
; r2 = count = number of 4-byte words to copy
; Local variables:
; r2 = number of bytes to copy
; r3 = offset = i * 4  (does not follow the C code exactly)
; r12 = temp
        lsl   r2, r2, #2     ; r2 = count * 4; // # of bytes to copy
        mov   r3, #0         ; offset = 0;

loop2   cmp   r3, r2
        bhs   done2          ; if (offset >= total bytes), return

        ldr   r12, [r1, r3]  ; load word from source
        str   r12, [r0, r3]  ; store word to destination
        add   r3, r3, #4     ; offset += 4;
        b     loop2          ; continue loop
        
done2   bx lr
        .endasmfunc



        
        