#ifndef FC_TEST_INPUT_DPMI_H
#define FC_TEST_INPUT_DPMI_H

typedef struct {
  unsigned short offset16, segment;
} __dpmi_raddr;

typedef union {
  struct {
    unsigned short di, di_hi, si, si_hi, bp, bp_hi, reserved, reserved_hi;
    unsigned short bx, bx_hi, dx, dx_hi, cx, cx_hi, ax, ax_hi;
    unsigned short flags, es, ds, fs, gs, ip, cs, sp, ss;
  } x;
} __dpmi_regs;

extern unsigned short __dpmi_error;
int __dpmi_get_real_mode_interrupt_vector(int vector, __dpmi_raddr *address);
int __dpmi_int(int vector, __dpmi_regs *regs);
#endif
