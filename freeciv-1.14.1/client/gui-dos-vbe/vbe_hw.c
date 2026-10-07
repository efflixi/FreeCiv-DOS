#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vbe_hw.h"

#ifdef __DJGPP__
#include <dpmi.h>
#include <sys/movedata.h>
#include <sys/segments.h>

static int dpmi_failure(const char *operation)
{
  fprintf(stderr, "DOS VBE: DPMI %s failed (0x%04x).\n",
          operation, __dpmi_error);
  return -1;
}

int dos_vbe_hw_runtime(void)
{
  __dpmi_version_ret version;

  if (__dpmi_get_version(&version) != 0) {
    return dpmi_failure("version query");
  }
  if (!(version.flags & 1) || version.cpu < 3) {
    fprintf(stderr, "DOS VBE: a 32-bit DPMI host and 386+ CPU are required.\n");
    return -1;
  }
  return 0;
}

int dos_vbe_hw_interrupt(struct dos_vbe_regs *regs)
{
  __dpmi_regs bios;

  memset(&bios, 0, sizeof(bios));
  bios.x.ax = regs->ax;
  bios.x.bx = regs->bx;
  bios.x.cx = regs->cx;
  bios.x.dx = regs->dx;
  bios.x.es = regs->es;
  bios.x.di = regs->di;
  if (__dpmi_int(0x10, &bios) != 0) {
    return dpmi_failure("real-mode INT 10h");
  }
  regs->ax = bios.x.ax;
  regs->bx = bios.x.bx;
  regs->cx = bios.x.cx;
  regs->dx = bios.x.dx;
  regs->es = bios.x.es;
  regs->di = bios.x.di;
  return 0;
}

int dos_vbe_hw_alloc_dos(size_t size, struct dos_vbe_dos_buffer *buffer)
{
  int selector;
  int segment;

  if (!size || size > 65536UL || buffer->size) {
    fprintf(stderr, "DOS VBE: invalid conventional-buffer allocation.\n");
    return -1;
  }
  segment = __dpmi_allocate_dos_memory((size + 15U) / 16U, &selector);
  if (segment < 0) {
    return dpmi_failure("conventional memory allocation");
  }
  buffer->segment = segment;
  buffer->selector = selector;
  buffer->size = size;
  return 0;
}

int dos_vbe_hw_free_dos(struct dos_vbe_dos_buffer *buffer)
{
  if (buffer->size) {
    if (__dpmi_free_dos_memory(buffer->selector) != 0) {
      return dpmi_failure("conventional memory release");
    }
    memset(buffer, 0, sizeof(*buffer));
  }
  return 0;
}

void dos_vbe_hw_read_dos(uint32_t address, size_t size, void *data)
{
  dosmemget(address, size, data);
}

void dos_vbe_hw_write_dos(uint32_t address, size_t size, const void *data)
{
  dosmemput(data, size, address);
}

int dos_vbe_hw_map(uint32_t physical, size_t size,
                   struct dos_vbe_mapping *mapping)
{
  __dpmi_meminfo memory;
  __dpmi_meminfo device;
  size_t page_offset = physical & 4095U;
  int rights;

  if (!size || size > UINT32_MAX - page_offset - 4095U
      || size > UINT32_MAX - physical || mapping->mapped) {
    fprintf(stderr, "DOS VBE: invalid physical video mapping.\n");
    return -1;
  }
  memset(&memory, 0, sizeof(memory));
  memory.size = (size + page_offset + 4095U) & ~(size_t)4095U;
  if (__dpmi_allocate_memory(&memory) != 0) {
    return dpmi_failure("managed video address-space allocation");
  }
  mapping->address = memory.address;
  mapping->handle = memory.handle;
  mapping->size = size;
  mapping->offset = page_offset;
  mapping->mapped = 1;
  mapping->selector = -1;
  /* CWSDPMI r7 has 0508h, but not the 0801h physical-unmapping service.
   * A managed block makes device mappings releasable through 0502h. */
  device = memory;
  device.address = 0;
  device.size = memory.size / 4096U;
  if (__dpmi_map_device_in_memory_block(&device, physical - page_offset) != 0) {
    dpmi_failure("device mapping (0508h extension required)");
    dos_vbe_hw_unmap(mapping);
    return -1;
  }
  mapping->selector = __dpmi_allocate_ldt_descriptors(1);
  if (mapping->selector < 0) {
    dpmi_failure("video selector allocation");
    dos_vbe_hw_unmap(mapping);
    return -1;
  }
  rights = __dpmi_get_descriptor_access_rights(_my_ds());
  if (rights < 0
      || __dpmi_set_segment_base_address(mapping->selector,
                                         mapping->address) != 0
      || __dpmi_set_segment_limit(mapping->selector,
                                  memory.size - 1U) != 0
      || __dpmi_set_descriptor_access_rights(mapping->selector,
                                             rights) != 0) {
    dpmi_failure("video selector setup");
    dos_vbe_hw_unmap(mapping);
    return -1;
  }
  return 0;
}

int dos_vbe_hw_unmap(struct dos_vbe_mapping *mapping)
{
  if (!mapping->mapped) {
    return 0;
  }
  if (mapping->selector >= 0) {
    if (__dpmi_free_ldt_descriptor(mapping->selector) != 0) {
      return dpmi_failure("video selector release");
    }
    mapping->selector = -1;
  }
  if (__dpmi_free_memory(mapping->handle) != 0) {
    return dpmi_failure("managed video block release");
  }
  memset(mapping, 0, sizeof(*mapping));
  return 0;
}

void dos_vbe_hw_write_video(const struct dos_vbe_mapping *mapping,
                           size_t offset, const void *data, size_t size)
{
  movedata(_my_ds(), (unsigned int)data, mapping->selector,
           mapping->offset + offset, size);
}

void dos_vbe_hw_read_video(const struct dos_vbe_mapping *mapping,
                          size_t offset, void *data, size_t size)
{
  movedata(mapping->selector, mapping->offset + offset,
           _my_ds(), (unsigned int)data, size);
}
#else
static int unavailable(void)
{
  fprintf(stderr, "DOS VBE: hardware services require a DJGPP/DPMI build.\n");
  return -1;
}

int dos_vbe_hw_runtime(void) { return unavailable(); }
int dos_vbe_hw_interrupt(struct dos_vbe_regs *regs)
{ (void)regs; return unavailable(); }
int dos_vbe_hw_alloc_dos(size_t size, struct dos_vbe_dos_buffer *buffer)
{ (void)size; (void)buffer; return unavailable(); }
int dos_vbe_hw_free_dos(struct dos_vbe_dos_buffer *buffer)
{ (void)buffer; return unavailable(); }
void dos_vbe_hw_read_dos(uint32_t address, size_t size, void *data)
{ (void)address; (void)size; (void)data; unavailable(); }
void dos_vbe_hw_write_dos(uint32_t address, size_t size, const void *data)
{ (void)address; (void)size; (void)data; unavailable(); }
int dos_vbe_hw_map(uint32_t physical, size_t size,
                   struct dos_vbe_mapping *mapping)
{ (void)physical; (void)size; (void)mapping; return unavailable(); }
int dos_vbe_hw_unmap(struct dos_vbe_mapping *mapping)
{ (void)mapping; return unavailable(); }
void dos_vbe_hw_write_video(const struct dos_vbe_mapping *mapping,
                           size_t offset, const void *data, size_t size)
{ (void)mapping; (void)offset; (void)data; (void)size; unavailable(); }
void dos_vbe_hw_read_video(const struct dos_vbe_mapping *mapping,
                          size_t offset, void *data, size_t size)
{ (void)mapping; (void)offset; (void)data; (void)size; unavailable(); }
#endif
