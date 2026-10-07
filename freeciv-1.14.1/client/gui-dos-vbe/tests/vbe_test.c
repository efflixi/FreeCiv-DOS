#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vbe_hw.h"
#include "vbe_init.h"

static unsigned char conventional[0x100000];
static unsigned char mode_blocks[3][256];
static unsigned char *mapped_video;
static unsigned int bios_mode;
static unsigned int saved_bios_mode;
static unsigned int sets;
static unsigned int restores;
static unsigned int allocations;
static unsigned int mappings;
static unsigned int next_segment;
static int version;
static int fail_controller;
static int fail_signature;
static int bad_pointer;
static int missing_terminator;
static int fail_mode_query;
static int fail_set;
static int wrong_current;
static int fail_save;
static int fail_restore;
static int fail_mapping;
static int corrupt_video;
static int fail_alloc;
static int fail_calloc;
static int fail_free;
static int fail_unmap;
static unsigned int last_requested_mode;
static size_t bytes_written;
static unsigned int video_writes;

void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size);

void *__wrap_calloc(size_t count, size_t size)
{
  if (fail_calloc) {
    return NULL;
  }
  return __real_calloc(count, size);
}

static void put16(unsigned char *bytes, unsigned int value)
{
  bytes[0] = value & 255;
  bytes[1] = value >> 8;
}

static void put32(unsigned char *bytes, uint32_t value)
{
  put16(bytes, value & 65535);
  put16(bytes + 2, value >> 16);
}

static void make_mode(unsigned char *block, unsigned int width,
                      unsigned int height)
{
  memset(block, 0, 256);
  put16(block, 0x99);
  put16(block + 16, width * 2 + 16);
  put16(block + 18, width);
  put16(block + 20, height);
  block[24] = 1;
  block[25] = 16;
  block[27] = 6;
  block[31] = 5;
  block[32] = 11;
  block[33] = 6;
  block[34] = 5;
  block[35] = 5;
  put32(block + 40, 0xe0000000UL);
  put16(block + 50, width * 2 + 32);
  memcpy(block + 54, block + 31, 8);
}

static void clean(void)
{
  assert(vbe_shutdown_display() == 0);
  assert(!dos_vbe_display_active());
  assert(!dos_vbe_front_buffer.pixels);
  assert(allocations == 0);
  assert(mappings == 0);
  assert(bios_mode == 3);
}

static void reset(void)
{
  clean();
  memset(conventional, 0, sizeof(conventional));
  make_mode(mode_blocks[0], 800, 600);
  make_mode(mode_blocks[1], 640, 480);
  make_mode(mode_blocks[2], 800, 600);
  put16(conventional + 0x90000, 0x150);
  put16(conventional + 0x90002, 0x151);
  put16(conventional + 0x90004, 0x152);
  put16(conventional + 0x90006, 0xffff);
  next_segment = 0x2000;
  version = 0x200;
  fail_controller = fail_signature = bad_pointer = missing_terminator = 0;
  fail_mode_query = fail_set = wrong_current = fail_save = fail_restore = 0;
  fail_mapping = corrupt_video = fail_alloc = fail_calloc = 0;
  fail_free = fail_unmap = 0;
  sets = restores = 0;
  bytes_written = video_writes = 0;
}

int dos_vbe_hw_runtime(void) { return 0; }

int dos_vbe_hw_interrupt(struct dos_vbe_regs *regs)
{
  unsigned char *data = conventional + ((unsigned int)regs->es << 4);
  unsigned int function = regs->ax;
  unsigned int index;

  regs->ax = 0x004f;
  switch (function) {
  case 0x4f00:
    assert(memcmp(data, "VBE2", 4) == 0);
    if (fail_controller) {
      regs->ax = 0x014f;
      break;
    }
    memcpy(data, fail_signature ? "BAD!" : "VESA", 4);
    put16(data + 4, version);
    put16(data + 14, 0);
    put16(data + 16, bad_pointer ? 0 : 0x9000);
    put16(data + 18, 64);
    if (missing_terminator) {
      for (index = 0; index < 512; ++index) {
        put16(conventional + 0x90000 + index * 2, 0x150);
      }
    }
    break;
  case 0x4f01:
    assert(regs->cx >= 0x150 && regs->cx <= 0x152);
    if (fail_mode_query) {
      regs->ax = 0x014f;
    } else {
      memcpy(data, mode_blocks[regs->cx - 0x150], 256);
    }
    break;
  case 0x4f02:
    ++sets;
    last_requested_mode = regs->bx;
    bios_mode = regs->bx;
    if ((regs->bx & 0x3fff) >= 0x100) {
      memset(conventional + 0xb8000, 0xff, 32768);
    }
    if (fail_set && (regs->bx & 0x3fff) == 0x150) {
      regs->ax = 0x014f;
    }
    break;
  case 0x4f03:
    regs->bx = wrong_current && bios_mode != 3 ? 0x150 : bios_mode;
    break;
  case 0x0f00:
    regs->ax = bios_mode;
    break;
  case 0x4f04:
    assert(regs->cx == 0xf);
    if (regs->dx == 0) {
      regs->bx = 16;
    } else if (regs->dx == 1) {
      assert(regs->bx == 0);
      if (fail_save) {
        regs->ax = 0x014f;
      } else {
        saved_bios_mode = bios_mode;
      }
    } else {
      assert(regs->dx == 2);
      ++restores;
      if (fail_restore) {
        regs->ax = 0x014f;
      } else {
        bios_mode = saved_bios_mode;
      }
    }
    break;
  case 3:
    bios_mode = 3;
    break;
  default:
    assert(!"unexpected BIOS function");
  }
  return 0;
}

int dos_vbe_hw_alloc_dos(size_t size, struct dos_vbe_dos_buffer *buffer)
{
  if (fail_alloc) {
    return -1;
  }
  assert(buffer->size == 0);
  buffer->segment = next_segment;
  buffer->selector = next_segment;
  buffer->size = size;
  next_segment += (size + 15) / 16;
  assert(next_segment < 0x8000);
  ++allocations;
  return 0;
}

int dos_vbe_hw_free_dos(struct dos_vbe_dos_buffer *buffer)
{
  if (buffer->size) {
    if (fail_free) {
      return -1;
    }
    --allocations;
    memset(buffer, 0, sizeof(*buffer));
  }
  return 0;
}

void dos_vbe_hw_read_dos(uint32_t address, size_t size, void *data)
{
  assert(address + size <= sizeof(conventional));
  memcpy(data, conventional + address, size);
}

void dos_vbe_hw_write_dos(uint32_t address, size_t size, const void *data)
{
  assert(address + size <= sizeof(conventional));
  memcpy(conventional + address, data, size);
}

int dos_vbe_hw_map(uint32_t physical, size_t size,
                   struct dos_vbe_mapping *mapping)
{
  assert(physical == 0xe0000000UL);
  if (fail_mapping) {
    return -1;
  }
  assert(!mapped_video);
  mapped_video = malloc(size);
  assert(mapped_video);
  mapping->size = size;
  mapping->mapped = 1;
  ++mappings;
  return 0;
}

int dos_vbe_hw_unmap(struct dos_vbe_mapping *mapping)
{
  if (mapping->mapped) {
    if (fail_unmap) {
      return -1;
    }
    free(mapped_video);
    mapped_video = NULL;
    memset(mapping, 0, sizeof(*mapping));
    --mappings;
  }
  return 0;
}

void dos_vbe_hw_write_video(const struct dos_vbe_mapping *mapping,
                           size_t offset, const void *data, size_t size)
{
  assert(mapping->mapped && offset + size <= mapping->size);
  bytes_written += size;
  ++video_writes;
  memcpy(mapped_video + offset, data, size);
}

void dos_vbe_hw_read_video(const struct dos_vbe_mapping *mapping,
                          size_t offset, void *data, size_t size)
{
  assert(mapping->mapped && offset + size <= mapping->size);
  memcpy(data, mapped_video + offset, size);
  if (corrupt_video) {
    ((unsigned char *)data)[0] ^= 1;
  }
}

static void no_modes(void)
{
  memcpy(mode_blocks[2], mode_blocks[0], 256);
  memcpy(mode_blocks[1], mode_blocks[0], 256);
  assert(vbe_init_display() == -1);
  assert(sets == 0);
  clean();
}

int main(void)
{
  struct dos_vbe_mode_info info;
  struct dos_vbe_framebuffer buffer = { 0 };
  unsigned int mode;
  unsigned int before;

  bios_mode = 3;
  reset();
  assert(dos_vbe_detect());
  assert(dos_vbe_mode_for_resolution(800, 600, 16) == 0x150);
  assert(dos_vbe_mode_for_resolution(640, 480, 16) == 0x151);
  assert(dos_vbe_mode_for_resolution(800, 600, 8) == 0);
  conventional[0xb8000] = 'D';
  conventional[0xbffff] = 'S';
  assert(vbe_init_display() == 0);
  assert(dos_vbe_current_mode(&info, &mode) == 0);
  assert(mode == 0x150 && info.bytes_per_scanline == 1616);
  assert(last_requested_mode == 0x4150);
  before = sets;
  assert(vbe_init_display() == 0 && sets == before);
  bytes_written = video_writes = 0;
  dos_vbe_framebuffer_put_pixel(&dos_vbe_front_buffer, 2, 3, 0xffff);
  dos_vbe_framebuffer_put_pixel(&dos_vbe_front_buffer, 3, 4, 0xffff);
  assert(dos_vbe_present() == 0);
  assert(bytes_written == 8 && dos_vbe_last_present_bytes() == 8);
  assert(video_writes == 2 && dos_vbe_verify_video() == 0);
  before = video_writes;
  assert(dos_vbe_present() == 0 && dos_vbe_last_present_bytes() == 0);
  assert(video_writes == before);
  dos_vbe_framebuffer_put_pixel(&dos_vbe_front_buffer, 2, 3, 0xffff);
  assert(dos_vbe_present() == 0 && dos_vbe_last_present_bytes() == 0);
  dos_vbe_framebuffer_put_pixel(&dos_vbe_front_buffer, 4, 5, 0xffff);
  before = video_writes;
  --dos_vbe_front_buffer.size_bytes;
  assert(dos_vbe_present() == -1 && dos_vbe_verify_video() == -1);
  assert(video_writes == before && dos_vbe_front_buffer.dirty);
  ++dos_vbe_front_buffer.size_bytes;
  assert(dos_vbe_present() == 0 && dos_vbe_last_present_bytes() == 2);
  dos_vbe_framebuffer_clear(&dos_vbe_front_buffer, 0xf800);
  dos_vbe_framebuffer_put_pixel(&dos_vbe_front_buffer, 799, 599, 0x07e0);
  assert(dos_vbe_present() == 0 && dos_vbe_verify_video() == 0);
  assert(mapped_video[(599 * 1616 + 799 * 2)] == 0xe0);
  assert(mapped_video[(599 * 1616 + 799 * 2) + 1] == 0x07);
  assert(vbe_set_mode(640, 480, 16) == 0);
  assert(restores == 1 && dos_vbe_front_buffer.width == 640);
  clean();
  assert(conventional[0xb8000] == 'D' && conventional[0xbffff] == 'S');
  clean();

  reset();
  version = 0x300;
  assert(vbe_init_display() == 0);
  assert(dos_vbe_front_buffer.stride == 1632);
  clean();

  reset(); fail_controller = 1; assert(!dos_vbe_detect()); clean();
  reset(); fail_signature = 1; assert(!dos_vbe_detect()); clean();
  reset(); version = 0x100; assert(!dos_vbe_detect()); clean();
  reset(); bad_pointer = 1; assert(!dos_vbe_detect()); clean();
  reset(); missing_terminator = 1; assert(!dos_vbe_detect()); clean();
  reset(); fail_alloc = 1; assert(!dos_vbe_detect()); clean();
  reset(); fail_mode_query = 1; assert(vbe_init_display() == -1); clean();
  reset(); mode_blocks[0][0] &= ~0x80; no_modes();
  reset(); mode_blocks[0][0] &= ~1; no_modes();
  reset(); mode_blocks[0][25] = 8; no_modes();
  reset(); mode_blocks[0][27] = 4; no_modes();
  reset(); put16(mode_blocks[0] + 16, 1598); no_modes();
  reset(); put16(mode_blocks[0] + 16, 1601); no_modes();
  reset(); mode_blocks[0][33] = 5; no_modes();
  reset(); mode_blocks[0][32] = 10; no_modes();
  reset(); put32(mode_blocks[0] + 40, 0); no_modes();
  reset(); put32(mode_blocks[0] + 40, 0xfffff000UL); no_modes();
  reset(); put16(mode_blocks[0] + 16, 65534); no_modes();
  reset(); version = 0x300; put16(mode_blocks[0] + 50, 0); no_modes();

  reset();
  fail_set = 1;
  assert(vbe_init_display() == 0);
  assert(last_requested_mode == 0x4152 && restores == 1);
  clean();
  reset();
  mode_blocks[0][0] &= ~0x80;
  mode_blocks[2][0] &= ~0x80;
  assert(vbe_init_display() == 0 && dos_vbe_front_buffer.width == 640);
  clean();
  reset(); fail_mapping = 1; assert(vbe_init_display() == -1); clean();
  reset(); fail_calloc = 1; assert(vbe_init_display() == -1); clean();
  reset(); fail_save = 1; assert(vbe_init_display() == -1); clean();
  reset(); wrong_current = 1; assert(vbe_init_display() == -1); clean();
  reset(); corrupt_video = 1; assert(vbe_init_display() == -1); clean();

  reset();
  assert(vbe_init_display() == 0);
  fail_restore = 1;
  assert(vbe_shutdown_display() == -1);
  assert(bios_mode == 3 && allocations == 1 && mappings == 0);
  fail_restore = 0;
  clean();
  reset();
  assert(vbe_init_display() == 0);
  fail_free = fail_unmap = 1;
  assert(vbe_shutdown_display() == -1);
  assert(allocations == 1 && mappings == 1);
  fail_free = fail_unmap = 0;
  clean();

  reset();
  assert(vbe_set_mode(320, 200, 16) == -1);
  assert(vbe_set_mode(640, 480, 32) == -1);
  assert(dos_vbe_present() == -1);
  assert(dos_vbe_verify_video() == -1);
  assert(dos_vbe_current_mode(&info, &mode) == -1);
  assert(dos_vbe_get_mode_info(0, &info) == -1);
  assert(dos_vbe_get_mode_info(0x150, NULL) == -1);
  assert(dos_vbe_framebuffer_init(&buffer, UINT_MAX, 1, 16) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&buffer, 1, UINT_MAX, 16, 4) == -1);
  assert(dos_vbe_framebuffer_init(&buffer, 1, 1, 8) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&buffer, 1, 1, 16, 0) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&buffer, 1, 1, 16, 3) == -1);
  clean();
  puts("PASS VBE BIOS metadata, enumeration, pitch/masks, presentation, fallback,"
       " allocation/failure cleanup, repeated lifecycle and state recovery");
  return 0;
}
