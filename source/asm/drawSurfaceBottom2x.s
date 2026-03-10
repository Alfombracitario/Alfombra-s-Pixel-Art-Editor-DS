.section .text
.align 2
.arm

.global drawSurfaceBottom2x
.type drawSurfaceBottom2x, %function
drawSurfaceBottom2x:
    push    {r4-r11, lr}

    mov     r7, #64          @ r7 = contador de filas

outer_rows_loop:
    @ r1 apunta al inicio del bloque destino para la fila actual
    mov     r5, r0           @ r5 = src_ptr (trabajo sobre r5)
    mov     r12, r1          @ r12 = write_ptr (empezamos a escribir desde r1)

    @ Bucle interno para leer 32 píxeles y escalar horizontalmente x4
    mov r6, #64
inner_x_loop:
    ldrh    r8, [r5], #2     @ r8 = *src_ptr++; leer u16
    strh    r8, [r12], #2    @ escribir 1ª copia
    strh    r8, [r12], #2    @ 2ª copia
    subs r6, #1
    bne inner_x_loop

    @ Después de procesar los 32 píxeles, tenemos 1 fila escalada horizontalmente
    @ Ahora copiar esta fila 3 veces más (escalado vertical x4)

    mov     r4, r1           @ r4 = inicio de la fila recién escrita
    mov     r9, r1           @ r9 = destino actual para copia

vertical_copy_loop:
    add     r9, r3           @ r9 += dstStride (siguiente fila destino)
    mov     r10, r4          @ r10 = origen (fila base)
    mov     r11, #256      @ r11 = bytes a copiar (32 píxeles * 4 copias * 2 bytes = 256)

copy_loop: 
    ldr     r8, [r10], #4    @ Cargar 2 píxeles (4 bytes)
    str     r8, [r9], #4     @ Almacenar 2 píxeles
    subs    r11, r11, #4     @ Decrementar contador de bytes
    bne     copy_loop

    @ Preparar siguiente iteración de filas
    add     r0, r0, r2       @ srcBase += srcStride
    add     r1, r1, #1024  @ dstBase += 4 * dstStride (avanzar 4 filas)

    subs    r7, r7, #1       @ Decrementar contador de filas
    bne     outer_rows_loop

    pop     {r4-r11, pc}     @ Restaurar registros y retornar