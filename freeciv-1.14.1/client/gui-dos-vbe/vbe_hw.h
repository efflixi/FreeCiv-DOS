#ifndef FC__DOS_VBE_HW_H
#define FC__DOS_VBE_HW_H

#include <stddef.h>
#include <stdint.h>

struct dos_vbe_regs {
  uint16_t ax, bx, cx, dx, es, di;
};

struct dos_vbe_dos_buffer {
  unsigned int segment;
  int selector;
  size_t size;
};

struct dos_vbe_mapping {
  uint32_t address;
  uint32_t handle;
  size_t size;
  size_t offset;
  int selector;
  int mapped;
};

int dos_vbe_hw_runtime(void);
int dos_vbe_hw_interrupt(struct dos_vbe_regs *regs);
int dos_vbe_hw_alloc_dos(size_t size, struct dos_vbe_dos_buffer *buffer);
int dos_vbe_hw_free_dos(struct dos_vbe_dos_buffer *buffer);
void dos_vbe_hw_read_dos(uint32_t address, size_t size, void *data);
void dos_vbe_hw_write_dos(uint32_t address, size_t size, const void *data);
int dos_vbe_hw_map(uint32_t physical, size_t size,
                   struct dos_vbe_mapping *mapping);
int dos_vbe_hw_unmap(struct dos_vbe_mapping *mapping);
void dos_vbe_hw_write_video(const struct dos_vbe_mapping *mapping,
                           size_t offset, const void *data, size_t size);
void dos_vbe_hw_read_video(const struct dos_vbe_mapping *mapping,
                          size_t offset, void *data, size_t size);

#endif
