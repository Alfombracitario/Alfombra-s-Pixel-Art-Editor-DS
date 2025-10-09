//este código no fué hecho por mi aunque pienso aprender ASM en un futuro

    .text
    .align 2
    .global memcpy_fast_arm9
    .type memcpy_fast_arm9, %function

// void memcpy_fast_arm9(const void* src, void* dst, unsigned int bytes);
// r0 = src, r1 = dst, r2 = bytes
memcpy_fast_arm9:
    push    {r4-r11, lr}        // preserva registros usados

    // si bytes == 0 -> return
    cmp     r2, #0
    beq     .mf_done

    // Copia por bloques de 32 bytes: usamos r4-r11 (8 registros * 4 = 32 bytes)
.mf_loop_32:
    cmp     r2, #32
    blt     .mf_words

    // LDMIA r0!, {r4-r11}
    ldmia   r0!, {r4-r11}
    // STMIA r1!, {r4-r11}
    stmia   r1!, {r4-r11}
    sub     r2, r2, #32
    b       .mf_loop_32

// Ahora quedan < 32 bytes. Copiamos por palabras (4 bytes)
.mf_words:
    cmp     r2, #4
    blt     .mf_bytes

.mf_loop_4:
    cmp     r2, #4
    blt     .mf_bytes
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    sub     r2, r2, #4
    b       .mf_loop_4

// Resto bytes (0..3)
.mf_bytes:
    cmp     r2, #0
    beq     .mf_done

    // r2 = 1..3
    // copia byte a byte
.mf_byte_loop:
    ldrb    r3, [r0], #1
    strb    r3, [r1], #1
    subs    r2, r2, #1
    bne     .mf_byte_loop

.mf_done:
    pop     {r4-r11, pc}
