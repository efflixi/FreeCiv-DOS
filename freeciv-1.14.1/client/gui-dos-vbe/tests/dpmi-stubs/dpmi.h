#ifndef FC_TEST_DPMI_H
#define FC_TEST_DPMI_H

#include <stdint.h>

typedef struct {
  unsigned char major, minor;
  unsigned short flags;
  unsigned char cpu, master_pic, slave_pic;
} __dpmi_version_ret;

typedef union {
  struct {
    uint32_t edi, esi, ebp, reserved, ebx, edx, ecx, eax;
    unsigned short flags, es, ds, fs, gs, ip, cs, sp, ss;
  } d;
  struct {
    unsigned short di, di_hi, si, si_hi, bp, bp_hi, reserved, reserved_hi;
    unsigned short bx, bx_hi, dx, dx_hi, cx, cx_hi, ax, ax_hi;
    unsigned short flags, es, ds, fs, gs, ip, cs, sp, ss;
  } x;
} __dpmi_regs;

typedef struct {
  unsigned long handle, size, address;
} __dpmi_meminfo;

extern int __dpmi_error;
int __dpmi_get_version(__dpmi_version_ret *version);
int __dpmi_int(int vector, __dpmi_regs *regs);
int __dpmi_allocate_dos_memory(int paragraphs, int *selector);
int __dpmi_free_dos_memory(int selector);
int __dpmi_allocate_memory(__dpmi_meminfo *memory);
int __dpmi_map_device_in_memory_block(__dpmi_meminfo *memory,
                                     unsigned long physical);
int __dpmi_free_memory(unsigned long handle);
int __dpmi_allocate_ldt_descriptors(int count);
int __dpmi_free_ldt_descriptor(int selector);
int __dpmi_get_descriptor_access_rights(int selector);
int __dpmi_set_segment_base_address(int selector, unsigned long address);
int __dpmi_set_segment_limit(int selector, unsigned long limit);
int __dpmi_set_descriptor_access_rights(int selector, int rights);

/* Deliberately no 0800h/0801h physical-map/unmap declarations or definitions. */
#endif
