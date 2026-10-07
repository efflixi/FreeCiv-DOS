#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extender_compat.h"
#include "vbe_hw.h"
#include "vbe_init.h"

#define MAX_VBE_MODES 512U
#define VBE_STATE_FLAGS 0x000fU

/* Historical name: this is the offscreen buffer, not physical video memory. */
struct dos_vbe_framebuffer dos_vbe_front_buffer;

static unsigned int controller_version;
static uint32_t video_bytes;
static unsigned int modes[MAX_VBE_MODES];
static unsigned int mode_count;
static struct dos_vbe_dos_buffer query_buffer;
static struct dos_vbe_dos_buffer state_buffer;
static struct dos_vbe_mapping video;
static struct dos_vbe_mode_info current_info;
static unsigned int current_mode;
static unsigned int original_mode;
static int original_is_vbe;
static unsigned char original_text_memory[32768];
static uint32_t original_text_address;
static int state_saved;
static int state_dirty;
static int active;
static int exit_registered;
static size_t last_present_bytes;
static unsigned long display_generation;
static int full_sync;

static unsigned int read16(const unsigned char *bytes)
{
  return bytes[0] | ((unsigned int)bytes[1] << 8);
}

static uint32_t read32(const unsigned char *bytes)
{
  return (uint32_t)read16(bytes) | ((uint32_t)read16(bytes + 2) << 16);
}

static int bios_call(struct dos_vbe_regs *regs, const char *operation)
{
  if (dos_vbe_hw_interrupt(regs) != 0) {
    return -1;
  }
  if (regs->ax != 0x004fU) {
    fprintf(stderr, "DOS VBE: BIOS %s failed (AX=0x%04x).\n",
            operation, regs->ax);
    return -1;
  }
  return 0;
}

static int restore_state(void)
{
  struct dos_vbe_regs regs;

  if (!state_dirty) {
    return 0;
  }
  /* State restore alone does not reset a VBE adapter's mode/LFB enable bits. */
  memset(&regs, 0, sizeof(regs));
  if (original_is_vbe) {
    regs.ax = 0x4f02;
    regs.bx = original_mode;
    if (bios_call(&regs, "original mode set") != 0) {
      return -1;
    }
  } else {
    regs.ax = original_mode;
    if (dos_vbe_hw_interrupt(&regs) != 0) {
      return -1;
    }
  }
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4f04;
  regs.cx = VBE_STATE_FLAGS;
  regs.dx = 2;
  regs.es = state_buffer.segment;
  if (bios_call(&regs, "display-state restore") != 0) {
    /* Recovery restores the mode, but does not claim full register/DAC recovery. */
    memset(&regs, 0, sizeof(regs));
    if (original_is_vbe) {
      regs.ax = 0x4f02;
      regs.bx = original_mode;
      if (bios_call(&regs, "original-mode recovery") != 0) {
        return -1;
      }
    } else {
      regs.ax = original_mode;
      if (dos_vbe_hw_interrupt(&regs) != 0) {
        return -1;
      }
    }
    fprintf(stderr, "DOS VBE: original mode recovered; full state restore "
            "failed. Saved state retained for retry.\n");
    return -1;
  }
  if (original_text_address) {
    dos_vbe_hw_write_dos(original_text_address, sizeof(original_text_memory),
                         original_text_memory);
  }
  state_dirty = 0;
  return 0;
}

int vbe_shutdown_display(void)
{
  int result = 0;

  active = 0;
  last_present_bytes = 0;
  full_sync = 0;
  if (restore_state() != 0) {
    result = -1;
  }
  if (dos_vbe_hw_unmap(&video) != 0) {
    result = -1;
  }
  dos_vbe_framebuffer_destroy(&dos_vbe_front_buffer);
  if (!state_dirty) {
    if (dos_vbe_hw_free_dos(&state_buffer) != 0) {
      result = -1;
    } else {
      state_saved = 0;
    }
  }
  if (dos_vbe_hw_free_dos(&query_buffer) != 0) {
    result = -1;
  }
  memset(&current_info, 0, sizeof(current_info));
  current_mode = 0;
  return result;
}

static void exit_display(void)
{
  if (vbe_shutdown_display() != 0) {
    fprintf(stderr, "DOS VBE: display cleanup at exit failed.\n");
  }
}

int dos_vbe_detect(void)
{
  struct dos_vbe_controller_block block;
  struct dos_vbe_regs regs;
  uint32_t address;
  unsigned char entry[2];
  unsigned int i;
  int result = -1;

  controller_version = 0;
  mode_count = 0;
  video_bytes = 0;
  if (dos_extender_init() != 0
      || dos_vbe_hw_free_dos(&query_buffer) != 0
      || dos_vbe_hw_alloc_dos(sizeof(block), &query_buffer) != 0) {
    return 0;
  }
  memset(&block, 0, sizeof(block));
  memcpy(block.bytes, "VBE2", 4);
  dos_vbe_hw_write_dos(query_buffer.segment << 4, sizeof(block), &block);
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4f00;
  regs.es = query_buffer.segment;
  if (bios_call(&regs, "controller query") != 0) {
    goto done;
  }
  dos_vbe_hw_read_dos(query_buffer.segment << 4, sizeof(block), &block);
  if (memcmp(block.bytes, "VESA", 4) != 0
      || read16(block.bytes + 4) < 0x0200U
      || read16(block.bytes + 18) == 0) {
    fprintf(stderr, "DOS VBE: invalid signature/version/video memory; "
            "VESA 2.0+ is required.\n");
    goto done;
  }
  address = ((uint32_t)read16(block.bytes + 16) << 4)
            + read16(block.bytes + 14);
  if (address < 0x400U || address > 0xffffeU) {
    fprintf(stderr, "DOS VBE: invalid real-mode mode-list pointer.\n");
    goto done;
  }
  for (i = 0; i < MAX_VBE_MODES; ++i, address += 2U) {
    if (address > 0xffffeU) {
      break;
    }
    dos_vbe_hw_read_dos(address, sizeof(entry), entry);
    if (read16(entry) == 0xffffU) {
      if (!mode_count) {
        fprintf(stderr, "DOS VBE: controller returned an empty mode list.\n");
        goto done;
      }
      controller_version = read16(block.bytes + 4);
      video_bytes = (uint32_t)read16(block.bytes + 18) * 65536UL;
      result = 0;
      goto done;
    }
    if (read16(entry) >= 0x0100U && read16(entry) < 0x4000U) {
      modes[mode_count++] = read16(entry);
    }
  }
  fprintf(stderr, "DOS VBE: unterminated/oversized real-mode mode list.\n");
done:
  if (dos_vbe_hw_free_dos(&query_buffer) != 0) {
    result = -1;
  }
  if (result != 0) {
    controller_version = 0;
    mode_count = 0;
    video_bytes = 0;
  }
  return result == 0;
}

int dos_vbe_get_mode_info(unsigned int mode, struct dos_vbe_mode_info *info)
{
  struct dos_vbe_mode_block block;
  struct dos_vbe_regs regs;
  unsigned int mask_offset = 31U;
  int result = -1;

  if (!info || mode < 0x100U || mode >= 0x4000U) {
    fprintf(stderr, "DOS VBE: invalid mode query argument.\n");
    return -1;
  }
  memset(info, 0, sizeof(*info));
  if (!controller_version && !dos_vbe_detect()) {
    return -1;
  }
  if (dos_vbe_hw_free_dos(&query_buffer) != 0
      || dos_vbe_hw_alloc_dos(sizeof(block), &query_buffer) != 0) {
    return -1;
  }
  memset(&block, 0, sizeof(block));
  dos_vbe_hw_write_dos(query_buffer.segment << 4, sizeof(block), &block);
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4f01;
  regs.cx = mode;
  regs.es = query_buffer.segment;
  if (bios_call(&regs, "mode query") != 0) {
    goto done;
  }
  dos_vbe_hw_read_dos(query_buffer.segment << 4, sizeof(block), &block);
  info->mode_attributes = read16(block.bytes);
  info->bytes_per_scanline = read16(block.bytes + 16);
  info->x_resolution = read16(block.bytes + 18);
  info->y_resolution = read16(block.bytes + 20);
  info->number_of_planes = block.bytes[24];
  info->bits_per_pixel = block.bytes[25];
  info->memory_model = block.bytes[27];
  info->physical_base_pointer = read32(block.bytes + 40);
  if (controller_version >= 0x0300U) {
    info->bytes_per_scanline = read16(block.bytes + 50);
    mask_offset = 54U;
  }
  info->red_mask_size = block.bytes[mask_offset];
  info->red_field_position = block.bytes[mask_offset + 1];
  info->green_mask_size = block.bytes[mask_offset + 2];
  info->green_field_position = block.bytes[mask_offset + 3];
  info->blue_mask_size = block.bytes[mask_offset + 4];
  info->blue_field_position = block.bytes[mask_offset + 5];
  info->reserved_mask_size = block.bytes[mask_offset + 6];
  result = 0;
done:
  if (dos_vbe_hw_free_dos(&query_buffer) != 0) {
    result = -1;
  }
  if (result != 0) {
    memset(info, 0, sizeof(*info));
  }
  return result;
}

static int usable_mode(const struct dos_vbe_mode_info *info)
{
  uint32_t size;

  if ((info->mode_attributes & 0x0099U) != 0x0099U
      || info->number_of_planes != 1U || info->bits_per_pixel != 16U
      || info->memory_model != 6U || !info->x_resolution
      || !info->y_resolution || info->x_resolution > UINT_MAX / 2U
      || info->bytes_per_scanline < info->x_resolution * 2U
      || (info->bytes_per_scanline & 1U)
      || info->y_resolution > UINT32_MAX / info->bytes_per_scanline) {
    return 0;
  }
  size = (uint32_t)info->bytes_per_scanline * info->y_resolution;
  return size <= video_bytes
         && info->physical_base_pointer >= 0x100000UL
         && size <= UINT32_MAX - info->physical_base_pointer
         && info->red_mask_size == 5U && info->red_field_position == 11U
         && info->green_mask_size == 6U && info->green_field_position == 5U
         && info->blue_mask_size == 5U && info->blue_field_position == 0U
         && info->reserved_mask_size == 0U;
}

unsigned int dos_vbe_mode_for_resolution(unsigned int width,
                                        unsigned int height,
                                        unsigned int bpp)
{
  unsigned int i;
  struct dos_vbe_mode_info info;

  if (!controller_version && !dos_vbe_detect()) {
    return 0;
  }
  for (i = 0; i < mode_count; ++i) {
    if (dos_vbe_get_mode_info(modes[i], &info) == 0
        && info.x_resolution == width && info.y_resolution == height
        && info.bits_per_pixel == bpp && usable_mode(&info)) {
      return modes[i];
    }
  }
  fprintf(stderr, "DOS VBE: no validated %ux%ux%u RGB565 linear mode.\n",
          width, height, bpp);
  return 0;
}

const char *dos_vbe_mode_name(unsigned int mode)
{
  switch (mode) {
  case DOS_VBE_MODE_640X480X16: return "standard 640x480x16 candidate";
  case DOS_VBE_MODE_800X600X16: return "standard 800x600x16 candidate";
  case DOS_VBE_MODE_1024X768X16: return "standard 1024x768x16 candidate";
  default: return "BIOS-enumerated mode";
  }
}

static int save_state(void)
{
  struct dos_vbe_regs regs;
  size_t size;

  if (state_saved) {
    return 0;
  }
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x0f00;
  if (dos_vbe_hw_interrupt(&regs) != 0) {
    return -1;
  }
  original_mode = regs.ax & 0x7fU;
  original_is_vbe = 0;
  original_text_address = 0;
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4f03;
  if (dos_vbe_hw_interrupt(&regs) != 0) {
    return -1;
  }
  if (regs.ax == 0x004fU && (regs.bx & 0x3fffU) >= 0x100U) {
    original_mode = regs.bx & 0x7fffU;
    original_is_vbe = 1;
  }
  if (!original_is_vbe && (original_mode <= 3U || original_mode == 7U)) {
    original_text_address = original_mode == 7U ? 0xb0000UL : 0xb8000UL;
    dos_vbe_hw_read_dos(original_text_address, sizeof(original_text_memory),
                        original_text_memory);
  }
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4f04;
  regs.cx = VBE_STATE_FLAGS;
  if (bios_call(&regs, "display-state size query") != 0) {
    return -1;
  }
  size = (size_t)regs.bx * 64U;
  if (!size || size > 65536UL) {
    fprintf(stderr, "DOS VBE: invalid BIOS state-buffer size.\n");
    return -1;
  }
  if (dos_vbe_hw_alloc_dos(size, &state_buffer) != 0) {
    return -1;
  }
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4f04;
  regs.cx = VBE_STATE_FLAGS;
  regs.dx = 1;
  regs.es = state_buffer.segment;
  if (bios_call(&regs, "display-state save") != 0) {
    dos_vbe_hw_free_dos(&state_buffer);
    return -1;
  }
  state_saved = 1;
  return 0;
}

static int try_mode(unsigned int mode, const struct dos_vbe_mode_info *info)
{
  struct dos_vbe_regs regs;
  size_t size = (size_t)info->bytes_per_scanline * info->y_resolution;

  if (dos_vbe_framebuffer_init_pitch(&dos_vbe_front_buffer,
                                     info->x_resolution, info->y_resolution,
                                     info->bits_per_pixel,
                                     info->bytes_per_scanline) != 0
      || dos_vbe_hw_map(info->physical_base_pointer, size, &video) != 0) {
    return -1;
  }
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4f02;
  regs.bx = mode | 0x4000U;
  state_dirty = 1;
  if (bios_call(&regs, "linear graphics mode set") != 0) {
    return -1;
  }
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4f03;
  if (bios_call(&regs, "current mode verification") != 0
      || (regs.bx & 0x7fffU) != (mode | 0x4000U)) {
    fprintf(stderr, "DOS VBE: BIOS did not select the requested linear mode.\n");
    return -1;
  }
  current_mode = mode;
  current_info = *info;
  active = 1;
  full_sync = 1;
  if (dos_vbe_present() != 0 || dos_vbe_verify_video() != 0) {
    return -1;
  }
  ++display_generation;
  return 0;
}

int vbe_set_mode(unsigned int width, unsigned int height, unsigned int bpp)
{
  static const unsigned int widths[] = { 1024U, 800U, 640U };
  static const unsigned int heights[] = { 768U, 600U, 480U };
  struct dos_vbe_mode_info info;
  unsigned int resolution;
  unsigned int i;
  int requested = 0;

  for (resolution = 0; resolution < 3; ++resolution) {
    if (width == widths[resolution] && height == heights[resolution]) {
      requested = 1;
      break;
    }
  }
  if (!requested || bpp != 16U) {
    fprintf(stderr, "DOS VBE: unsupported requested display profile.\n");
    return -1;
  }
  if (active && current_info.x_resolution == width
      && current_info.y_resolution == height) {
    return 0;
  }
  if (vbe_shutdown_display() != 0 || !dos_vbe_detect()) {
    return -1;
  }
  if (!exit_registered) {
    if (atexit(exit_display) != 0) {
      fprintf(stderr, "DOS VBE: unable to register display-exit cleanup.\n");
      return -1;
    }
    exit_registered = 1;
  }
  if (save_state() != 0) {
    vbe_shutdown_display();
    return -1;
  }
  for (; resolution < 3; ++resolution) {
    for (i = 0; i < mode_count; ++i) {
      if (dos_vbe_get_mode_info(modes[i], &info) != 0
          || info.x_resolution != widths[resolution]
          || info.y_resolution != heights[resolution] || !usable_mode(&info)) {
        continue;
      }
      if (try_mode(modes[i], &info) == 0) {
        fprintf(stderr, "DOS VBE: BIOS mode 0x%04x, %ux%ux16, pitch %u, "
                "RGB565 LFB 0x%08lx initialized.\n",
                current_mode, info.x_resolution, info.y_resolution,
                info.bytes_per_scanline,
                (unsigned long)info.physical_base_pointer);
        return 0;
      }
      active = 0;
      if (restore_state() != 0 || dos_vbe_hw_unmap(&video) != 0) {
        vbe_shutdown_display();
        return -1;
      }
      dos_vbe_framebuffer_destroy(&dos_vbe_front_buffer);
      fprintf(stderr, "DOS VBE: rejected failed mode 0x%04x; "
              "trying another validated candidate.\n", modes[i]);
    }
  }
  fprintf(stderr, "DOS VBE: no usable RGB565 linear framebuffer mode; "
          "VESA 2.0+ LFB with BIOS state save/restore is required. "
          "Banked-only hardware is unsupported.\n");
  vbe_shutdown_display();
  return -1;
}

int vbe_init_display(void)
{
  if (active) {
    return 0;
  }
  return vbe_set_mode(800U, 600U, 16U);
}

int dos_vbe_display_active(void)
{
  return active;
}

unsigned long dos_vbe_display_generation(void)
{
  return display_generation;
}

int dos_vbe_current_mode(struct dos_vbe_mode_info *info, unsigned int *mode)
{
  if (!active || !info || !mode) {
    fprintf(stderr, "DOS VBE: no active display for metadata query.\n");
    return -1;
  }
  *info = current_info;
  *mode = current_mode;
  return 0;
}

static int validate_backbuffer(void)
{
  if (!active || !video.mapped
      || dos_vbe_framebuffer_validate(&dos_vbe_front_buffer) != 0
      || dos_vbe_front_buffer.stride != current_info.bytes_per_scanline
      || dos_vbe_front_buffer.width != current_info.x_resolution
      || dos_vbe_front_buffer.height != current_info.y_resolution
      || dos_vbe_front_buffer.bpp != 16U) {
    fprintf(stderr, "DOS VBE: presentation requires a valid active display.\n");
    return -1;
  }
  if (dos_vbe_front_buffer.size_bytes > video.size) {
    fprintf(stderr, "DOS VBE: backbuffer exceeds its video mapping.\n");
    return -1;
  }
  return 0;
}

size_t dos_vbe_last_present_bytes(void)
{
  return last_present_bytes;
}

int dos_vbe_present(void)
{
  struct dos_vbe_dirty_rect rect;
  unsigned int y;
  int dirty;

  last_present_bytes = 0;
  if (validate_backbuffer() != 0) {
    return -1;
  }
  dirty = dos_vbe_framebuffer_dirty_peek(&dos_vbe_front_buffer, &rect);
  if (dirty < 0) {
    return -1;
  }
  if (full_sync) {
    dos_vbe_hw_write_video(&video, 0, dos_vbe_front_buffer.pixels,
                           dos_vbe_front_buffer.size_bytes);
    last_present_bytes = dos_vbe_front_buffer.size_bytes;
    full_sync = 0;
  } else if (dirty) {
    size_t count = (size_t)rect.width * 2U;
    for (y = rect.y; y < rect.y + rect.height; ++y) {
      size_t offset = (size_t)y * dos_vbe_front_buffer.stride + rect.x * 2U;
      dos_vbe_hw_write_video(&video, offset,
                             dos_vbe_front_buffer.pixels + offset, count);
      last_present_bytes += count;
    }
  }
  return dos_vbe_framebuffer_dirty_clear(&dos_vbe_front_buffer);
}

int dos_vbe_verify_video(void)
{
  unsigned char bytes[256];
  size_t size;
  size_t offset;
  size_t count;

  if (validate_backbuffer() != 0) {
    return -1;
  }
  size = dos_vbe_front_buffer.size_bytes;
  for (offset = 0; offset < size; offset += count) {
    count = size - offset < sizeof(bytes) ? size - offset : sizeof(bytes);
    dos_vbe_hw_read_video(&video, offset, bytes, count);
    if (memcmp(bytes, dos_vbe_front_buffer.pixels + offset, count) != 0) {
      fprintf(stderr, "DOS VBE: mapped video readback mismatch at %lu.\n",
              (unsigned long)offset);
      return -1;
    }
  }
  return 0;
}
