#include <stdio.h>
#include "render_pattern.h"

int dos_render_test_pattern(struct dos_vbe_framebuffer *fb)
{
  static const unsigned int bars[] = {
    0xf800, 0x07e0, 0x001f, 0xffff, 0x0000, 0xffe0, 0x07ff, 0xf81f
  };
  struct dos_vbe_framebuffer icon = { 0 };
  unsigned int i;
  unsigned int x;
  unsigned int y;
  int result = -1;

  if (dos_vbe_framebuffer_validate(fb) != 0
      || fb->width < 640 || fb->height < 480) {
    fprintf(stderr, "RENDER CHECK: pattern requires at least 640x480.\n");
    return -1;
  }
  if (dos_vbe_framebuffer_fill(fb, 0, 0, fb->width, fb->height, 0x0841) != 0) {
    return -1;
  }
  for (i = 0; i < 8; ++i) {
    if (dos_vbe_framebuffer_fill(fb, i * (fb->width / 8), 0,
                                 fb->width / 8, 32, bars[i]) != 0) {
      return -1;
    }
  }
  if (dos_vbe_framebuffer_text(fb, 8, 48, "FREECIV DOS - ORIGINAL 5x7 ASCII", 0xffff, 2)
      || dos_vbe_framebuffer_text(fb, 8, 80, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0xffe0, 2)
      || dos_vbe_framebuffer_text(fb, 8, 104, "abcdefghijklmnopqrstuvwxyz", 0x07ff, 2)
      || dos_vbe_framebuffer_text(fb, 8, 128, "0123456789 !\"#$%&'()*+,-./", 0xffff, 2)
      || dos_vbe_framebuffer_text(fb, 8, 152, ":;<=>?@[\\]^_`{|}~", 0xffff, 2)
      || dos_vbe_framebuffer_text(fb, 8, 192, "FILL / CLIPPED LINE / BLIT / ICON", 0xffff, 2)
      || dos_vbe_framebuffer_fill(fb, -20, 224, 140, 64, 0x07e0)
      || dos_vbe_framebuffer_line(fb, -500, 240, 500, 272, 0xffff)
      || dos_vbe_framebuffer_line(fb, 130, 220, 230, 300, 0xf800)
      || dos_vbe_framebuffer_line(fb, 130, 300, 230, 220, 0x07ff)
      || dos_vbe_framebuffer_text(fb, 8, fb->height - 42,
                                  "DIRTY: 4x4=32 BYTES, REPEAT=0", 0xffff, 2)
      || dos_vbe_framebuffer_init_pitch(&icon, 16, 16, 16, 40)) {
    goto done;
  }
  for (y = 0; y < 16; ++y) {
    for (x = 0; x < 16; ++x) {
      if (x == y || x + y == 15 || (x >= 6 && x < 10 && y >= 6 && y < 10)) {
        dos_vbe_framebuffer_put_pixel(&icon, x, y, 0xffe0);
      }
    }
  }
  if (dos_vbe_framebuffer_icon(fb, 260, 240, &icon, 1, 0)
      || dos_vbe_framebuffer_blit(fb, 300, 240, &icon, 0, 0, 16, 16, 0, 0)
      || dos_vbe_framebuffer_blit(fb, 304, 244, fb, 300, 240, 16, 16, 1, 0)) {
    goto done;
  }
  result = 0;
done:
  dos_vbe_framebuffer_destroy(&icon);
  return result;
}
