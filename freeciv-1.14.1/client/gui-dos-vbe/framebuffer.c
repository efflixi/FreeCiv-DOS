#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "framebuffer.h"
#include "bitmap_font.h"

static int invalid(const char *message)
{
  fprintf(stderr, "DOS VBE framebuffer: %s.\n", message);
  return -1;
}

static int dimensions(unsigned int width, unsigned int height,
                      unsigned int bpp, unsigned int pitch, size_t *size)
{
  if (!width || !height || width > INT_MAX || height > INT_MAX
      || bpp != 16U || width > UINT_MAX / 2U
      || pitch < width * 2U || (pitch & 1U)
      || height > UINT_MAX / pitch || (size_t)height > SIZE_MAX / pitch) {
    return invalid("invalid RGB565 dimensions or pitch");
  }
  *size = (size_t)pitch * height;
  return 0;
}

int dos_vbe_framebuffer_validate(const struct dos_vbe_framebuffer *fb)
{
  size_t size;

  if (!fb || !fb->pixels) {
    return invalid("missing buffer");
  }
  if (dimensions(fb->width, fb->height, fb->bpp, fb->stride, &size) < 0) {
    return -1;
  }
  if (fb->size_bytes != size) {
    return invalid("allocation size does not match buffer metadata");
  }
  if (fb->dirty != 0 && fb->dirty != 1) {
    return invalid("invalid dirty state");
  }
  if (fb->dirty && (!fb->dirty_width || !fb->dirty_height
                   || fb->dirty_x >= fb->width || fb->dirty_y >= fb->height
                   || fb->dirty_width > fb->width - fb->dirty_x
                   || fb->dirty_height > fb->height - fb->dirty_y)) {
    return invalid("invalid dirty bounds");
  }
  return 0;
}

int dos_vbe_framebuffer_init(struct dos_vbe_framebuffer *fb,
                            unsigned int width, unsigned int height,
                            unsigned int bpp)
{
  if (width > UINT_MAX / 2U) {
    return invalid("width overflows pitch");
  }
  return dos_vbe_framebuffer_init_pitch(fb, width, height, bpp, width * 2U);
}

int dos_vbe_framebuffer_init_pitch(struct dos_vbe_framebuffer *fb,
                                  unsigned int width, unsigned int height,
                                  unsigned int bpp, unsigned int pitch)
{
  unsigned char *pixels;
  size_t size;

  if (!fb) {
    return invalid("missing initialization target");
  }
  if (dimensions(width, height, bpp, pitch, &size) < 0) {
    return -1;
  }
  pixels = calloc(size, 1U);
  if (!pixels) {
    return invalid("unable to allocate backbuffer");
  }
  dos_vbe_framebuffer_destroy(fb);
  fb->width = width;
  fb->height = height;
  fb->bpp = bpp;
  fb->stride = pitch;
  fb->pixels = pixels;
  fb->size_bytes = size;
  return 0;
}

static unsigned int read_pixel(const struct dos_vbe_framebuffer *fb,
                               unsigned int x, unsigned int y)
{
  size_t offset = (size_t)y * fb->stride + (size_t)x * 2U;
  return fb->pixels[offset] | ((unsigned int)fb->pixels[offset + 1U] << 8);
}

static void write_pixel(struct dos_vbe_framebuffer *fb,
                        unsigned int x, unsigned int y, unsigned int color)
{
  size_t offset = (size_t)y * fb->stride + (size_t)x * 2U;
  unsigned int right, bottom;

  if (read_pixel(fb, x, y) == color) {
    return;
  }
  fb->pixels[offset] = (unsigned char)color;
  fb->pixels[offset + 1U] = (unsigned char)(color >> 8);
  if (!fb->dirty) {
    fb->dirty = 1;
    fb->dirty_x = x;
    fb->dirty_y = y;
    fb->dirty_width = fb->dirty_height = 1U;
    return;
  }
  right = fb->dirty_x + fb->dirty_width;
  bottom = fb->dirty_y + fb->dirty_height;
  if (x < fb->dirty_x) {
    fb->dirty_x = x;
  }
  if (y < fb->dirty_y) {
    fb->dirty_y = y;
  }
  if (x >= right) {
    right = x + 1U;
  }
  if (y >= bottom) {
    bottom = y + 1U;
  }
  fb->dirty_width = right - fb->dirty_x;
  fb->dirty_height = bottom - fb->dirty_y;
}

static int drawing_input(struct dos_vbe_framebuffer *fb, unsigned int color)
{
  if (dos_vbe_framebuffer_validate(fb) < 0) {
    return -1;
  }
  return color <= 0xffffU ? 0 : invalid("color is not packed RGB565");
}

static void fill_clipped(struct dos_vbe_framebuffer *fb,
                         int64_t x, int64_t y, int64_t width, int64_t height,
                         unsigned int color)
{
  int64_t left = x, top = y, right = x + width;
  int64_t bottom = y + height, px, py;

  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > fb->width) right = fb->width;
  if (bottom > fb->height) bottom = fb->height;
  if (right <= left || bottom <= top) return;
  for (py = top; py < bottom; ++py) {
    for (px = left; px < right; ++px) {
      write_pixel(fb, (unsigned int)px, (unsigned int)py, color);
    }
  }
}

int dos_vbe_framebuffer_fill_rect(struct dos_vbe_framebuffer *fb,
                                 int x, int y, int width, int height,
                                 unsigned int color)
{
  if (drawing_input(fb, color) < 0) return -1;
  if (width < 0 || height < 0) {
    return invalid("negative rectangle dimensions");
  }
  fill_clipped(fb, x, y, width, height, color);
  return 0;
}

int dos_vbe_framebuffer_fill(struct dos_vbe_framebuffer *fb,
                            int x, int y, int width, int height,
                            unsigned int color)
{
  return dos_vbe_framebuffer_fill_rect(fb, x, y, width, height, color);
}

void dos_vbe_framebuffer_clear(struct dos_vbe_framebuffer *fb,
                              unsigned int color)
{
  if (drawing_input(fb, color) < 0) {
    return;
  }
  (void)dos_vbe_framebuffer_fill_rect(fb, 0, 0, (int)fb->width,
                                    (int)fb->height, color);
}

void dos_vbe_framebuffer_put_pixel(struct dos_vbe_framebuffer *fb,
                                  unsigned int x, unsigned int y,
                                  unsigned int color)
{
  if (drawing_input(fb, color) < 0 || x >= fb->width || y >= fb->height) {
    return;
  }
  write_pixel(fb, x, y, color);
}

static unsigned int outcode(int64_t x, int64_t y, int64_t xmax, int64_t ymax)
{
  return (x < 0 ? 1U : x > xmax ? 2U : 0U)
    | (y < 0 ? 4U : y > ymax ? 8U : 0U);
}

/* Coordinate differences are at most UINT32_MAX. Their unsigned product fits
 * uint64_t even when signed multiplication would overflow for INT_MIN/MAX.
 */
static int64_t ratio(int64_t a, int64_t b, int64_t divisor)
{
  uint64_t ua = (uint64_t)(a < 0 ? -a : a);
  uint64_t ub = (uint64_t)(b < 0 ? -b : b);
  uint64_t ud = (uint64_t)(divisor < 0 ? -divisor : divisor);
  int64_t result = (int64_t)(ua * ub / ud);
  return ((a < 0) ^ (b < 0) ^ (divisor < 0)) ? -result : result;
}

int dos_vbe_framebuffer_line(struct dos_vbe_framebuffer *fb,
                            int x0, int y0, int x1, int y1,
                            unsigned int color)
{
  int64_t ax = x0, ay = y0, bx = x1, by = y1, x, y;
  int64_t xmax, ymax, dx, dy, sx, sy, error, twice;
  unsigned int a, b, code;

  if (drawing_input(fb, color) < 0) {
    return -1;
  }
  xmax = (int64_t)fb->width - 1;
  ymax = (int64_t)fb->height - 1;
  for (;;) {
    a = outcode(ax, ay, xmax, ymax);
    b = outcode(bx, by, xmax, ymax);
    if (!(a | b)) break;
    if (a & b) return 0;
    code = a ? a : b;
    if (code & (4U | 8U)) {
      y = (code & 4U) ? 0 : ymax;
      x = ax + ratio(bx - ax, y - ay, by - ay);
    } else {
      x = (code & 1U) ? 0 : xmax;
      y = ay + ratio(by - ay, x - ax, bx - ax);
    }
    if (code == a) {
      ax = x;
      ay = y;
    } else {
      bx = x;
      by = y;
    }
  }
  dx = bx > ax ? bx - ax : ax - bx;
  dy = by > ay ? ay - by : by - ay;
  sx = ax < bx ? 1 : -1;
  sy = ay < by ? 1 : -1;
  error = dx + dy;
  for (;;) {
    write_pixel(fb, (unsigned int)ax, (unsigned int)ay, color);
    if (ax == bx && ay == by) break;
    twice = 2 * error;
    if (twice >= dy) {
      error += dy;
      ax += sx;
    }
    if (twice <= dx) {
      error += dx;
      ay += sy;
    }
  }
  return 0;
}

static int clip_axis(int64_t *dst, int64_t *src, int64_t *count,
                     unsigned int dst_limit, unsigned int src_limit)
{
  int64_t start = 0, end = *count;

  if (-*dst > start) start = -*dst;
  if (-*src > start) start = -*src;
  if ((int64_t)dst_limit - *dst < end) end = (int64_t)dst_limit - *dst;
  if ((int64_t)src_limit - *src < end) end = (int64_t)src_limit - *src;
  if (end <= start) return 0;
  *dst += start;
  *src += start;
  *count = end - start;
  return 1;
}

int dos_vbe_framebuffer_blit(struct dos_vbe_framebuffer *dst, int dx, int dy,
                            const struct dos_vbe_framebuffer *src,
                            int sx, int sy, int width, int height,
                            int transparent, unsigned int key)
{
  int64_t tx = dx, ty = dy, fx = sx, fy = sy, w = width, h = height;
  int64_t x, y, xi = 1, yi = 1, xstart = 0, ystart = 0;
  unsigned int color;

  if (dos_vbe_framebuffer_validate(dst) < 0
      || dos_vbe_framebuffer_validate(src) < 0) return -1;
  if (width < 0 || height < 0 || (transparent != 0 && transparent != 1)
      || (transparent && key > 0xffffU)) {
    return invalid("invalid blit dimensions or transparency");
  }
  if (dst->pixels == src->pixels
      && (dst->width != src->width || dst->height != src->height
          || dst->stride != src->stride)) {
    return invalid("aliased buffers have different layouts");
  }
  if (!clip_axis(&tx, &fx, &w, dst->width, src->width)
      || !clip_axis(&ty, &fy, &h, dst->height, src->height)) return 0;
  if (dst->pixels == src->pixels) {
    if (ty > fy) {
      yi = -1;
      ystart = h - 1;
    }
    if (tx > fx) {
      xi = -1;
      xstart = w - 1;
    }
  }
  for (y = ystart; y >= 0 && y < h; y += yi) {
    for (x = xstart; x >= 0 && x < w; x += xi) {
      color = read_pixel(src, (unsigned int)(fx + x), (unsigned int)(fy + y));
      if (!transparent || color != key) {
        write_pixel(dst, (unsigned int)(tx + x), (unsigned int)(ty + y), color);
      }
    }
  }
  return 0;
}

int dos_vbe_framebuffer_icon(struct dos_vbe_framebuffer *dst, int x, int y,
                            const struct dos_vbe_framebuffer *icon,
                            int transparent, unsigned int key)
{
  if (dos_vbe_framebuffer_validate(icon) < 0) return -1;
  return dos_vbe_framebuffer_blit(dst, x, y, icon, 0, 0,
                                 (int)icon->width, (int)icon->height,
                                 transparent, key);
}

int dos_vbe_framebuffer_text(struct dos_vbe_framebuffer *fb, int x, int y,
                            const char *text, unsigned int color,
                            unsigned int scale)
{
  int64_t cursor_x = x, cursor_y = y, px, py;
  unsigned int row, col;
  const unsigned char *glyph;

  if (drawing_input(fb, color) < 0) return -1;
  if (!text) return invalid("missing text");
  if (!scale || scale > (unsigned int)INT_MAX / 8U) {
    return invalid("invalid bitmap font scale");
  }
  while (*text) {
    unsigned int character = (unsigned char)*text++;
    if (character == '\n') {
      cursor_x = x;
      cursor_y += (int64_t)scale * 8;
      if (cursor_y >= fb->height) break;
      continue;
    }
    if (cursor_x < fb->width && cursor_x + (int64_t)scale * 5 > 0
        && cursor_y < fb->height && cursor_y + (int64_t)scale * 7 > 0) {
      glyph = dos_vbe_bitmap_glyph(character);
      for (row = 0; row < DOS_VBE_FONT_HEIGHT; ++row) {
        for (col = 0; col < DOS_VBE_FONT_WIDTH; ++col) {
          if (!(glyph[row] & (1U << (DOS_VBE_FONT_WIDTH - 1U - col)))) continue;
          px = cursor_x + (int64_t)col * scale;
          py = cursor_y + (int64_t)row * scale;
          fill_clipped(fb, px, py, scale, scale, color);
        }
      }
    }
    /* Once off the right edge, hold the cursor there until newline. */
    if (cursor_x < fb->width) cursor_x += (int64_t)scale * 6;
  }
  return 0;
}

int dos_vbe_framebuffer_dirty_peek(const struct dos_vbe_framebuffer *fb,
                                  struct dos_vbe_dirty_rect *rect)
{
  if (dos_vbe_framebuffer_validate(fb) < 0) return -1;
  if (!rect) return invalid("missing dirty rectangle output");
  memset(rect, 0, sizeof(*rect));
  if (!fb->dirty) return 0;
  rect->x = fb->dirty_x;
  rect->y = fb->dirty_y;
  rect->width = fb->dirty_width;
  rect->height = fb->dirty_height;
  return 1;
}

int dos_vbe_framebuffer_dirty_clear(struct dos_vbe_framebuffer *fb)
{
  if (dos_vbe_framebuffer_validate(fb) < 0) return -1;
  fb->dirty = 0;
  fb->dirty_x = fb->dirty_y = fb->dirty_width = fb->dirty_height = 0U;
  return 0;
}

int dos_vbe_framebuffer_encode_rgb565(unsigned int bpp,
                                     unsigned int red_size,
                                     unsigned int red_position,
                                     unsigned int green_size,
                                     unsigned int green_position,
                                     unsigned int blue_size,
                                     unsigned int blue_position,
                                     unsigned int reserved_size,
                                     unsigned int red, unsigned int green,
                                     unsigned int blue, unsigned int *color)
{
  if (!color || bpp != 16U || red_size != 5U || red_position != 11U
      || green_size != 6U || green_position != 5U || blue_size != 5U
      || blue_position != 0U || reserved_size != 0U
      || red > 255U || green > 255U || blue > 255U) {
    return invalid("incompatible RGB565 masks or RGB888 color");
  }
  *color = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
  return 0;
}

void dos_vbe_framebuffer_destroy(struct dos_vbe_framebuffer *fb)
{
  if (fb) {
    free(fb->pixels);
    memset(fb, 0, sizeof(*fb));
  }
}
