#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dpmi.h>
#include <sys/movedata.h>
#include <sys/segments.h>
#include "vbe_hw.h"

#define BLOCK_BASE 0x10000000UL
#define BLOCK_HANDLE 0x12345678UL
#define VIDEO_SELECTOR 0x88
#define DATA_SELECTOR 0x20
#define DOS_SELECTOR 0x90
#define DOS_SEGMENT 0x2000
#define DATA_RIGHTS 0xc093

enum operation {
  VERSION, INTERRUPT, DOS_ALLOC, DOS_FREE, BLOCK_ALLOC, DEVICE_MAP,
  SELECTOR_ALLOC, RIGHTS_GET, BASE_SET, LIMIT_SET, RIGHTS_SET,
  SELECTOR_FREE, BLOCK_FREE, OPERATION_COUNT
};

int __dpmi_error = 0x8001;
static unsigned int calls[OPERATION_COUNT];
static enum operation trace[64];
static size_t trace_size;
static int failing[OPERATION_COUNT];
static unsigned short version_flags;
static unsigned char version_cpu;
static unsigned long block_size, physical_address;
static int block_live, selector_live, dos_live;
static unsigned long selector_base, selector_limit;
static int dos_paragraphs;
static unsigned char conventional[0x100000];
static unsigned char video[32768];
static void *host_pointer;
static size_t video_offset;
static unsigned int transfers;
static struct dos_vbe_regs bios_input, bios_output;

static int call(enum operation operation)
{
  assert(trace_size < sizeof(trace) / sizeof(trace[0]));
  trace[trace_size++] = operation;
  calls[operation]++;
  return failing[operation] ? -1 : 0;
}

static void reset(void)
{
  assert(!block_live && !selector_live && !dos_live);
  memset(calls, 0, sizeof(calls));
  memset(failing, 0, sizeof(failing));
  memset(video, 0, sizeof(video));
  trace_size = 0;
  transfers = 0;
  version_flags = 1;
  version_cpu = 3;
  block_size = 0;
  selector_base = selector_limit = 0;
}

int __dpmi_get_version(__dpmi_version_ret *version)
{
  if (call(VERSION)) {
    return -1;
  }
  memset(version, 0, sizeof(*version));
  version->flags = version_flags;
  version->cpu = version_cpu;
  return 0;
}

int __dpmi_int(int vector, __dpmi_regs *regs)
{
  __dpmi_regs expected;

  assert(vector == 0x10);
  memset(&expected, 0, sizeof(expected));
  expected.x.ax = bios_input.ax;
  expected.x.bx = bios_input.bx;
  expected.x.cx = bios_input.cx;
  expected.x.dx = bios_input.dx;
  expected.x.es = bios_input.es;
  expected.x.di = bios_input.di;
  assert(memcmp(regs, &expected, sizeof(expected)) == 0);
  regs->x.ax = bios_output.ax;
  regs->x.bx = bios_output.bx;
  regs->x.cx = bios_output.cx;
  regs->x.dx = bios_output.dx;
  regs->x.es = bios_output.es;
  regs->x.di = bios_output.di;
  return call(INTERRUPT);
}

int __dpmi_allocate_dos_memory(int paragraphs, int *selector)
{
  assert(!dos_live);
  dos_paragraphs = paragraphs;
  if (call(DOS_ALLOC)) {
    return -1;
  }
  dos_live = 1;
  *selector = DOS_SELECTOR;
  return DOS_SEGMENT;
}

int __dpmi_free_dos_memory(int selector)
{
  assert(dos_live && selector == DOS_SELECTOR);
  if (call(DOS_FREE)) {
    return -1;
  }
  dos_live = 0;
  return 0;
}

void dosmemget(unsigned long address, size_t size, void *data)
{
  assert(address <= sizeof(conventional));
  assert(size <= sizeof(conventional) - address);
  memcpy(data, conventional + address, size);
}

void dosmemput(const void *data, size_t size, unsigned long address)
{
  assert(address <= sizeof(conventional));
  assert(size <= sizeof(conventional) - address);
  memcpy(conventional + address, data, size);
}

int __dpmi_allocate_memory(__dpmi_meminfo *memory)
{
  assert(!block_live);
  assert(memory->handle == 0 && memory->address == 0);
  block_size = memory->size;
  assert(block_size > 0 && block_size % 4096 == 0);
  if (call(BLOCK_ALLOC)) {
    return -1;
  }
  memory->address = BLOCK_BASE;
  memory->handle = BLOCK_HANDLE;
  block_live = 1;
  return 0;
}

int __dpmi_map_device_in_memory_block(__dpmi_meminfo *memory,
                                     unsigned long physical)
{
  assert(block_live);
  assert(memory->handle == BLOCK_HANDLE);
  assert(memory->address == 0);
  assert(memory->size == block_size / 4096);
  assert(physical == physical_address);
  return call(DEVICE_MAP);
}

int __dpmi_free_memory(unsigned long handle)
{
  assert(block_live && !selector_live && handle == BLOCK_HANDLE);
  if (call(BLOCK_FREE)) {
    return -1;
  }
  block_live = 0;
  return 0;
}

int __dpmi_allocate_ldt_descriptors(int count)
{
  assert(block_live && !selector_live && count == 1);
  if (call(SELECTOR_ALLOC)) {
    return -1;
  }
  selector_live = 1;
  return VIDEO_SELECTOR;
}

int __dpmi_free_ldt_descriptor(int selector)
{
  assert(block_live && selector_live && selector == VIDEO_SELECTOR);
  if (call(SELECTOR_FREE)) {
    return -1;
  }
  selector_live = 0;
  return 0;
}

unsigned short _my_ds(void)
{
  return DATA_SELECTOR;
}

int __dpmi_get_descriptor_access_rights(int selector)
{
  assert(selector_live && selector == DATA_SELECTOR);
  return call(RIGHTS_GET) ? -1 : DATA_RIGHTS;
}

int __dpmi_set_segment_base_address(int selector, unsigned long address)
{
  assert(selector_live && selector == VIDEO_SELECTOR);
  assert(address == BLOCK_BASE);
  selector_base = address;
  return call(BASE_SET);
}

int __dpmi_set_segment_limit(int selector, unsigned long limit)
{
  assert(selector_live && selector == VIDEO_SELECTOR);
  assert(limit == block_size - 1);
  selector_limit = limit;
  return call(LIMIT_SET);
}

int __dpmi_set_descriptor_access_rights(int selector, int rights)
{
  assert(selector_live && selector == VIDEO_SELECTOR && rights == DATA_RIGHTS);
  return call(RIGHTS_SET);
}

void movedata(unsigned int source_selector, unsigned int source_offset,
              unsigned int destination_selector,
              unsigned int destination_offset, size_t size)
{
  assert(selector_live && selector_base == BLOCK_BASE);
  assert(video_offset <= selector_limit);
  assert(size <= selector_limit + 1 - video_offset);
  assert(video_offset + size <= sizeof(video));
  /* DJGPP offsets are 32-bit. Check the ABI token, but copy with the registered
   * native pointer rather than dereferencing its truncated host representation. */
  if (source_selector == DATA_SELECTOR) {
    assert(source_offset == (unsigned int)(uintptr_t)host_pointer);
    assert(destination_selector == VIDEO_SELECTOR);
    assert(destination_offset == video_offset);
    memcpy(video + destination_offset, host_pointer, size);
  } else {
    assert(source_selector == VIDEO_SELECTOR && source_offset == video_offset);
    assert(destination_selector == DATA_SELECTOR);
    assert(destination_offset == (unsigned int)(uintptr_t)host_pointer);
    memcpy(host_pointer, video + source_offset, size);
  }
  transfers++;
}

static void test_runtime_and_bios(void)
{
  struct dos_vbe_regs regs;

  reset();
  assert(dos_vbe_hw_runtime() == 0);
  version_flags = 0;
  assert(dos_vbe_hw_runtime() == -1);
  version_flags = 2;
  assert(dos_vbe_hw_runtime() == -1);
  version_flags = 1;
  version_cpu = 2;
  assert(dos_vbe_hw_runtime() == -1);
  version_cpu = 6;
  version_flags = 0xffff;
  assert(dos_vbe_hw_runtime() == 0);
  failing[VERSION] = 1;
  assert(dos_vbe_hw_runtime() == -1);
  bios_input.ax = 0x4f01;
  bios_input.bx = 0x1234;
  bios_input.cx = 0xfedc;
  bios_input.dx = 0xabcd;
  bios_input.es = 0x2000;
  bios_input.di = 0xfffe;
  bios_output.ax = 0x004f;
  bios_output.bx = 0x4321;
  bios_output.cx = 0xcdef;
  bios_output.dx = 0xdcba;
  bios_output.es = 0x3000;
  bios_output.di = 0x7654;
  regs = bios_input;
  assert(dos_vbe_hw_interrupt(&regs) == 0);
  assert(memcmp(&regs, &bios_output, sizeof(regs)) == 0);
  failing[INTERRUPT] = 1;
  regs = bios_input;
  assert(dos_vbe_hw_interrupt(&regs) == -1);
  assert(memcmp(&regs, &bios_input, sizeof(regs)) == 0);
}

static void test_dos(void)
{
  struct dos_vbe_dos_buffer buffer, saved, zero;
  unsigned char input[17], output[17];
  size_t i;

  reset();
  memset(&buffer, 0, sizeof(buffer));
  memset(&zero, 0, sizeof(zero));
  assert(dos_vbe_hw_free_dos(&buffer) == 0 && calls[DOS_FREE] == 0);
  assert(dos_vbe_hw_alloc_dos(0, &buffer) == -1);
  assert(dos_vbe_hw_alloc_dos(65537, &buffer) == -1);
  assert(calls[DOS_ALLOC] == 0);
  failing[DOS_ALLOC] = 1;
  assert(dos_vbe_hw_alloc_dos(17, &buffer) == -1);
  assert(memcmp(&buffer, &zero, sizeof(buffer)) == 0);
  failing[DOS_ALLOC] = 0;
  assert(dos_vbe_hw_alloc_dos(17, &buffer) == 0);
  assert(dos_paragraphs == 2 && buffer.size == 17);
  assert(buffer.segment == DOS_SEGMENT && buffer.selector == DOS_SELECTOR);
  saved = buffer;
  assert(dos_vbe_hw_alloc_dos(1, &buffer) == -1);
  assert(calls[DOS_ALLOC] == 2);
  for (i = 0; i < sizeof(input); i++) {
    input[i] = (unsigned char)(i * 7 + 3);
  }
  dos_vbe_hw_write_dos(DOS_SEGMENT * 16UL + 15, sizeof(input), input);
  assert(memcmp(conventional + DOS_SEGMENT * 16UL + 15,
                input, sizeof(input)) == 0);
  dos_vbe_hw_read_dos(DOS_SEGMENT * 16UL + 15, sizeof(output), output);
  assert(memcmp(input, output, sizeof(input)) == 0);
  failing[DOS_FREE] = 1;
  assert(dos_vbe_hw_free_dos(&buffer) == -1);
  assert(memcmp(&buffer, &saved, sizeof(buffer)) == 0);
  failing[DOS_FREE] = 0;
  assert(dos_vbe_hw_free_dos(&buffer) == 0);
  assert(memcmp(&buffer, &zero, sizeof(buffer)) == 0);
  assert(dos_vbe_hw_free_dos(&buffer) == 0 && calls[DOS_FREE] == 2);
  assert(dos_vbe_hw_alloc_dos(65536, &buffer) == 0);
  assert(dos_paragraphs == 4096);
  assert(dos_vbe_hw_free_dos(&buffer) == 0);
  assert(dos_vbe_hw_alloc_dos(1, &buffer) == 0 && dos_paragraphs == 1);
  assert(dos_vbe_hw_free_dos(&buffer) == 0);
}

static void map_success(uint32_t physical, size_t size, size_t expected_size)
{
  struct dos_vbe_mapping mapping, zero;
  unsigned char input[19], output[19];
  size_t i, offset, transfer_size;

  reset();
  memset(&mapping, 0, sizeof(mapping));
  memset(&zero, 0, sizeof(zero));
  physical_address = physical & ~4095UL;
  assert(dos_vbe_hw_map(physical, size, &mapping) == 0);
  assert(block_size == expected_size && mapping.size == size);
  assert(mapping.address == BLOCK_BASE && mapping.handle == BLOCK_HANDLE);
  assert(mapping.selector == VIDEO_SELECTOR && mapping.mapped);
  assert(trace_size == 7);
  for (i = 0; i < 7; i++) {
    assert(trace[i] == (enum operation)(BLOCK_ALLOC + i));
  }
  assert(dos_vbe_hw_map(physical, size, &mapping) == -1);
  assert(calls[BLOCK_ALLOC] == 1);
  for (i = 0; i < sizeof(input); i++) {
    input[i] = (unsigned char)(i * 13 + 11);
  }
  transfer_size = size < sizeof(input) ? size : sizeof(input);
  for (i = 0; i < 2; i++) {
    offset = i == 0 ? 0 : size - transfer_size;
    video_offset = (physical & 4095U) + offset;
    host_pointer = input;
    dos_vbe_hw_write_video(&mapping, offset, input, transfer_size);
    assert(memcmp(video + video_offset, input, transfer_size) == 0);
    memset(output, 0, sizeof(output));
    host_pointer = output;
    dos_vbe_hw_read_video(&mapping, offset, output, transfer_size);
    assert(memcmp(input, output, transfer_size) == 0);
  }
  assert(transfers == 4);
  assert(dos_vbe_hw_unmap(&mapping) == 0);
  assert(trace[7] == SELECTOR_FREE && trace[8] == BLOCK_FREE);
  assert(memcmp(&mapping, &zero, sizeof(mapping)) == 0);
  assert(dos_vbe_hw_unmap(&mapping) == 0);
  assert(calls[SELECTOR_FREE] == 1 && calls[BLOCK_FREE] == 1);
}

static void test_invalid_maps(void)
{
  struct dos_vbe_mapping mapping;

  reset();
  memset(&mapping, 0, sizeof(mapping));
  assert(dos_vbe_hw_unmap(&mapping) == 0);
  assert(dos_vbe_hw_map(0xe0000000U, 0, &mapping) == -1);
  assert(dos_vbe_hw_map(0, UINT32_MAX, &mapping) == -1);
  assert(dos_vbe_hw_map(UINT32_MAX - 15U, 16, &mapping) == -1);
  assert(dos_vbe_hw_map(4095, UINT32_MAX - 4095U, &mapping) == -1);
  if (sizeof(size_t) > sizeof(uint32_t)) {
    assert(dos_vbe_hw_map(0, (size_t)UINT32_MAX + 1, &mapping) == -1);
  }
  assert(trace_size == 0);
}

static void test_setup_failures(void)
{
  struct dos_vbe_mapping mapping, zero;
  enum operation operation;
  size_t i;

  for (operation = BLOCK_ALLOC; operation <= RIGHTS_SET; operation++) {
    reset();
    memset(&mapping, 0, sizeof(mapping));
    memset(&zero, 0, sizeof(zero));
    physical_address = 0xe0000000UL;
    failing[operation] = 1;
    assert(dos_vbe_hw_map(physical_address + 123, 5000, &mapping) == -1);
    assert(!block_live && !selector_live);
    assert(memcmp(&mapping, &zero, sizeof(mapping)) == 0);
    for (i = BLOCK_ALLOC; i <= RIGHTS_SET; i++) {
      assert(calls[i] == (i <= (size_t)operation ? 1U : 0U));
    }
    assert(calls[BLOCK_FREE] == (operation != BLOCK_ALLOC ? 1U : 0U));
    assert(calls[SELECTOR_FREE] == (operation >= RIGHTS_GET ? 1U : 0U));
    if (operation >= RIGHTS_GET) {
      assert(trace[trace_size - 2] == SELECTOR_FREE);
    }
    if (operation != BLOCK_ALLOC) {
      assert(trace[trace_size - 1] == BLOCK_FREE);
    }
    assert(dos_vbe_hw_unmap(&mapping) == 0);
    failing[operation] = 0;
    assert(dos_vbe_hw_map(physical_address + 123, 5000, &mapping) == 0);
    assert(dos_vbe_hw_unmap(&mapping) == 0);
  }
}

static void test_cleanup_retries(void)
{
  struct dos_vbe_mapping mapping, saved, zero;
  enum operation operation;

  for (operation = SELECTOR_FREE; operation <= BLOCK_FREE; operation++) {
    reset();
    memset(&mapping, 0, sizeof(mapping));
    memset(&zero, 0, sizeof(zero));
    physical_address = 0xe0000000UL;
    assert(dos_vbe_hw_map(physical_address + 4095, 5000, &mapping) == 0);
    saved = mapping;
    failing[operation] = 1;
    assert(dos_vbe_hw_unmap(&mapping) == -1);
    assert(mapping.mapped && mapping.handle == saved.handle);
    assert(mapping.address == saved.address && mapping.size == saved.size);
    assert(block_live);
    if (operation == SELECTOR_FREE) {
      assert(selector_live && mapping.selector == VIDEO_SELECTOR);
      assert(calls[BLOCK_FREE] == 0);
    } else {
      assert(!selector_live && mapping.selector == -1);
    }
    failing[operation] = 0;
    assert(dos_vbe_hw_unmap(&mapping) == 0);
    assert(memcmp(&mapping, &zero, sizeof(mapping)) == 0);
    assert(calls[operation] == 2);
    assert(calls[operation == BLOCK_FREE ? SELECTOR_FREE : BLOCK_FREE] == 1);
    assert(dos_vbe_hw_unmap(&mapping) == 0);
  }

  /* Unsupported 0508h must fail explicitly, retaining a failed cleanup for
   * retry instead of falling back to the unavailable physical-unmap API. */
  reset();
  memset(&mapping, 0, sizeof(mapping));
  failing[DEVICE_MAP] = failing[BLOCK_FREE] = 1;
  assert(dos_vbe_hw_map(physical_address + 123, 5000, &mapping) == -1);
  assert(mapping.mapped && mapping.selector == -1 && block_live);
  assert(calls[SELECTOR_ALLOC] == 0);
  failing[BLOCK_FREE] = 0;
  assert(dos_vbe_hw_unmap(&mapping) == 0);
  assert(!block_live && calls[BLOCK_FREE] == 2);

  reset();
  memset(&mapping, 0, sizeof(mapping));
  failing[RIGHTS_SET] = failing[SELECTOR_FREE] = 1;
  assert(dos_vbe_hw_map(physical_address + 123, 5000, &mapping) == -1);
  assert(mapping.mapped && selector_live && block_live);
  assert(calls[BLOCK_FREE] == 0);
  failing[SELECTOR_FREE] = 0;
  assert(dos_vbe_hw_unmap(&mapping) == 0);
  assert(!block_live && !selector_live);
}

int main(void)
{
  test_runtime_and_bios();
  test_dos();
  test_invalid_maps();
  map_success(0xe0000000U, 1, 4096);
  map_success(0xe0000000U, 4096, 4096);
  map_success(0xe0000000U, 4097, 8192);
  map_success(0xe000007bU, 3973, 4096);
  map_success(0xe000007bU, 3974, 8192);
  map_success(0xe0000fffU, 1, 4096);
  map_success(0xe0000fffU, 2, 8192);
  map_success(0xe0000fffU, 5000, 12288);
  test_setup_failures();
  test_cleanup_retries();
  reset();
  puts("DPMI hardware behavior tests passed (real vbe_hw.c, host stubs).");
  return 0;
}
