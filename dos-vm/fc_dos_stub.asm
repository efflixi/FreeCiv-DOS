; Minimal DOS startup stub for the Freeciv DOS port
; Assembles to a .COM file that prints a startup banner and exits cleanly.
; This is a Phase 5 milestone artifact: real DOS executable under DOS 6.22.

BITS 16
ORG 100h

start:
    mov dx, banner
    mov ah, 09h
    int 21h

    mov ax, 4C00h
    int 21h

banner:
    db "Freeciv DOS phase 5 startup stub active.$"
