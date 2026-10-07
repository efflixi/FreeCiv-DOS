#ifndef FC__DOS_VBE_FRAMEBUFFER_H
#define FC__DOS_VBE_FRAMEBUFFER_H

#include <stddef.h>

struct dos_vbe_framebuffer {
  unsigned int width;
  unsigned int height;
  unsigned int bpp;
  unsigned int stride;
  unsigned char *pixels;
  size_t size_bytes;
  int dirty;
  unsigned int dirty_x;
  unsigned int dirty_y;
  unsigned int dirty_width;
  unsigned int dirty_height;
};

struct dos_vbe_dirty_rect {
  unsigned int x, y, width, height;
};

/* Initialize a zero-initialized object. Reinitialization is failure-atomic.
 * Buffers own their pixels; do not shallow-copy them or mutate their metadata.
 * Coordinates are signed, dimensions nonnegative, colors packed RGB565.
 * Drawing clips to visible pixels and never touches scanline padding.
 * New operations return 0 on success (including fully clipped draws), -1 on
 * invalid input. Dirty peek returns 1 for a rectangle, 0 if clean, -1 on error.
 */
int dos_vbe_framebuffer_validate(const struct dos_vbe_framebuffer *fb);
int dos_vbe_framebuffer_fill(struct dos_vbe_framebuffer *fb,
                            int x, int y, int width, int height,
                            unsigned int color);
int dos_vbe_framebuffer_fill_rect(struct dos_vbe_framebuffer *fb,
                                 int x, int y, int width, int height,
                                 unsigned int color);
int dos_vbe_framebuffer_line(struct dos_vbe_framebuffer *fb,
                            int x0, int y0, int x1, int y1,
                            unsigned int color);
/* Source and destination are clipped together. Self-overlap has snapshot
 * semantics, including transparent pixels; key is used only if transparent.
 */
int dos_vbe_framebuffer_blit(struct dos_vbe_framebuffer *dst, int dx, int dy,
                            const struct dos_vbe_framebuffer *src,
                            int sx, int sy, int width, int height,
                            int transparent, unsigned int key);
int dos_vbe_framebuffer_icon(struct dos_vbe_framebuffer *dst, int x, int y,
                            const struct dos_vbe_framebuffer *icon,
                            int transparent, unsigned int key);
/* Original 5x7 ASCII font, 6x8 cells, transparent background. Scale 2 gives
 * readable 10x14 glyphs in 12x16 cells. Scale must be 1..INT_MAX/8.
 * Newline starts another row; nonprintable bytes other than newline use '?'.
 */
int dos_vbe_framebuffer_text(struct dos_vbe_framebuffer *fb, int x, int y,
                            const char *text, unsigned int color,
                            unsigned int scale);
int dos_vbe_framebuffer_dirty_peek(const struct dos_vbe_framebuffer *fb,
                                  struct dos_vbe_dirty_rect *rect);
int dos_vbe_framebuffer_dirty_clear(struct dos_vbe_framebuffer *fb);
/* Accept only 16-bit RGB565 (5@11, 6@5, 5@0, no reserved mask). */
int dos_vbe_framebuffer_encode_rgb565(unsigned int bpp,
                                     unsigned int red_size,
                                     unsigned int red_position,
                                     unsigned int green_size,
                                     unsigned int green_position,
                                     unsigned int blue_size,
                                     unsigned int blue_position,
                                     unsigned int reserved_size,
                                     unsigned int red, unsigned int green,
                                     unsigned int blue, unsigned int *color);

int dos_vbe_framebuffer_init(struct dos_vbe_framebuffer *fb,
                              unsigned int width,
                              unsigned int height,
                              unsigned int bpp);
int dos_vbe_framebuffer_init_pitch(struct dos_vbe_framebuffer *fb,
                                   unsigned int width,
                                   unsigned int height,
                                   unsigned int bpp,
                                   unsigned int pitch);
void dos_vbe_framebuffer_clear(struct dos_vbe_framebuffer *fb,
                              unsigned int color);
void dos_vbe_framebuffer_put_pixel(struct dos_vbe_framebuffer *fb,
                                  unsigned int x,
                                  unsigned int y,
                                  unsigned int color);
void dos_vbe_framebuffer_destroy(struct dos_vbe_framebuffer *fb);

#endif
