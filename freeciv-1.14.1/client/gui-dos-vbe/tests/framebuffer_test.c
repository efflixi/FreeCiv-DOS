#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framebuffer.h"
#include "bitmap_font.h"

#define WIDTH 9U
#define HEIGHT 8U
#define PITCH 24U
#define GUARD 32U
#define SENTINEL 0xa5

static int fail_allocation;

void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size);

void *__wrap_calloc(size_t count, size_t size)
{
  if (fail_allocation) {
    fail_allocation = 0;
    return NULL;
  }
  return __real_calloc(count, size);
}

struct guarded_buffer {
  struct dos_vbe_framebuffer fb;
  unsigned char *allocation;
};

static unsigned int pixel(const struct dos_vbe_framebuffer *fb,
                          unsigned int x, unsigned int y)
{
  size_t offset = (size_t)y * fb->stride + x * 2U;
  return fb->pixels[offset] | ((unsigned int)fb->pixels[offset + 1U] << 8);
}

static void seed(struct dos_vbe_framebuffer *fb, unsigned int x,
                 unsigned int y, unsigned int color)
{
  size_t offset = (size_t)y * fb->stride + x * 2U;
  fb->pixels[offset] = (unsigned char)color;
  fb->pixels[offset + 1U] = (unsigned char)(color >> 8);
}

static void guarded_init(struct guarded_buffer *buffer)
{
  unsigned int y;
  memset(buffer, 0, sizeof(*buffer));
  buffer->allocation = malloc(PITCH * HEIGHT + GUARD * 2U);
  assert(buffer->allocation);
  memset(buffer->allocation, SENTINEL, PITCH * HEIGHT + GUARD * 2U);
  buffer->fb.width = WIDTH;
  buffer->fb.height = HEIGHT;
  buffer->fb.bpp = 16;
  buffer->fb.stride = PITCH;
  buffer->fb.size_bytes = PITCH * HEIGHT;
  buffer->fb.pixels = buffer->allocation + GUARD;
  for (y = 0; y < HEIGHT; ++y) {
    memset(buffer->fb.pixels + y * PITCH, 0, WIDTH * 2U);
  }
  assert(dos_vbe_framebuffer_validate(&buffer->fb) == 0);
}

static void guards_check(const struct guarded_buffer *buffer)
{
  unsigned int i, y;
  for (i = 0; i < GUARD; ++i) {
    assert(buffer->allocation[i] == SENTINEL);
    assert(buffer->allocation[GUARD + PITCH * HEIGHT + i] == SENTINEL);
  }
  for (y = 0; y < HEIGHT; ++y) {
    for (i = WIDTH * 2U; i < PITCH; ++i) {
      assert(buffer->fb.pixels[y * PITCH + i] == SENTINEL);
    }
  }
}

static void guarded_free(struct guarded_buffer *buffer)
{
  guards_check(buffer);
  free(buffer->allocation);
}

static void clean(struct dos_vbe_framebuffer *fb)
{
  struct dos_vbe_dirty_rect rect = { 1, 1, 1, 1 };
  assert(dos_vbe_framebuffer_dirty_clear(fb) == 0);
  assert(dos_vbe_framebuffer_dirty_peek(fb, &rect) == 0);
  assert(!rect.x && !rect.y && !rect.width && !rect.height);
}

static void dirty(const struct dos_vbe_framebuffer *fb,
                  unsigned int x, unsigned int y,
                  unsigned int width, unsigned int height)
{
  struct dos_vbe_dirty_rect rect;
  assert(dos_vbe_framebuffer_dirty_peek(fb, &rect) == 1);
  assert(rect.x == x && rect.y == y);
  assert(rect.width == width && rect.height == height);
}

static void allocation_test(void)
{
  struct dos_vbe_framebuffer fb = { 0 };
  struct dos_vbe_framebuffer before;
  before = fb;
  fail_allocation = 1;
  assert(dos_vbe_framebuffer_init(&fb, 640, 480, 16) == -1);
  assert(memcmp(&before, &fb, sizeof(fb)) == 0);
  assert(dos_vbe_framebuffer_init_pitch(&fb, 9, 8, 16, 24) == 0);
  assert(fb.size_bytes == 192);
  assert(pixel(&fb, 8, 7) == 0);
  clean(&fb);
  dos_vbe_framebuffer_put_pixel(&fb, 8, 7, 0xffff);
  before = fb;
  fail_allocation = 1;
  assert(dos_vbe_framebuffer_init(&fb, 640, 480, 16) == -1);
  assert(memcmp(&before, &fb, sizeof(fb)) == 0);
  assert(pixel(&fb, 8, 7) == 0xffff);
  dirty(&fb, 8, 7, 1, 1);
  assert(dos_vbe_framebuffer_init_pitch(&fb, 0, 1, 16, 2) == -1);
  assert(memcmp(&before, &fb, sizeof(fb)) == 0);
  assert(dos_vbe_framebuffer_init(&fb, 800, 600, 16) == 0);
  assert(fb.width == 800 && fb.height == 600 && fb.size_bytes == 960000);
  clean(&fb);
  dos_vbe_framebuffer_destroy(&fb);
  dos_vbe_framebuffer_destroy(&fb);
  assert(!fb.pixels && !fb.size_bytes && !fb.width && !fb.dirty);
  dos_vbe_framebuffer_destroy(NULL);
}

static void fill_dirty_test(void)
{
  struct guarded_buffer buffer;
  struct dos_vbe_framebuffer *fb;
  unsigned int x, y;
  guarded_init(&buffer);
  fb = &buffer.fb;
  clean(fb);
  assert(dos_vbe_framebuffer_fill(fb, -2, -1, 5, 3, 0x1234) == 0);
  dirty(fb, 0, 0, 3, 2);
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      assert(pixel(fb, x, y) == ((x < 3 && y < 2) ? 0x1234U : 0U));
    }
  }
  clean(fb);
  assert(dos_vbe_framebuffer_fill_rect(fb, -2, -1, 5, 3, 0x1234) == 0);
  assert(!fb->dirty);
  dos_vbe_framebuffer_put_pixel(fb, 8, 7, 0xf800);
  dos_vbe_framebuffer_put_pixel(fb, 2, 3, 0x07e0);
  dirty(fb, 2, 3, 7, 5);
  clean(fb);
  assert(dos_vbe_framebuffer_fill_rect(fb, INT_MIN, INT_MIN,
                                     INT_MAX, INT_MAX, 1) == 0);
  assert(dos_vbe_framebuffer_fill_rect(fb, INT_MAX, INT_MAX,
                                     INT_MAX, INT_MAX, 1) == 0);
  assert(dos_vbe_framebuffer_fill_rect(fb, 1, 1, 0, 8, 1) == 0);
  dos_vbe_framebuffer_put_pixel(fb, UINT_MAX, UINT_MAX, 1);
  assert(!fb->dirty);
  assert(dos_vbe_framebuffer_fill_rect(fb, 8, 7, INT_MAX, INT_MAX, 1) == 0);
  dirty(fb, 8, 7, 1, 1);
  dos_vbe_framebuffer_clear(fb, 0xffff);
  dirty(fb, 0, 0, WIDTH, HEIGHT);
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) assert(pixel(fb, x, y) == 0xffff);
  }
  clean(fb);
  dos_vbe_framebuffer_clear(fb, 0xffff);
  assert(!fb->dirty);
  guarded_free(&buffer);
}

static void line_test(void)
{
  struct guarded_buffer buffer;
  struct dos_vbe_framebuffer *fb;
  unsigned int i, x, y, a, b, c, d;
  unsigned int shallow[9] = { 0, 0, 1, 1, 2, 2, 2, 3, 3 };
  int endpoints[10] = {
    INT_MIN, INT_MIN + 1, -20, -1, 0, 1, 7, 8, 20, INT_MAX
  };
  int octants[8][2] = {
    { 8, 5 }, { 5, 7 }, { 3, 7 }, { 0, 5 },
    { 0, 1 }, { 3, 0 }, { 5, 0 }, { 8, 1 }
  };
  guarded_init(&buffer);
  fb = &buffer.fb;
  assert(dos_vbe_framebuffer_line(fb, INT_MIN, 2, INT_MAX, 2, 1) == 0);
  for (x = 0; x < WIDTH; ++x) assert(pixel(fb, x, 2) == 1);
  dirty(fb, 0, 2, WIDTH, 1);
  clean(fb);
  assert(dos_vbe_framebuffer_line(fb, 3, INT_MAX, 3, INT_MIN, 2) == 0);
  for (y = 0; y < HEIGHT; ++y) assert(pixel(fb, 3, y) == 2);
  dirty(fb, 3, 0, 1, HEIGHT);
  dos_vbe_framebuffer_clear(fb, 0);
  clean(fb);
  assert(dos_vbe_framebuffer_line(fb, INT_MIN, INT_MIN,
                                INT_MAX, INT_MAX, 3) == 0);
  for (i = 0; i < HEIGHT; ++i) assert(pixel(fb, i, i) == 3);
  dirty(fb, 0, 0, HEIGHT, HEIGHT);
  dos_vbe_framebuffer_clear(fb, 0);
  clean(fb);
  assert(dos_vbe_framebuffer_line(fb, INT_MIN, INT_MAX,
                                INT_MAX, INT_MIN, 1) == 0);
  assert(!fb->dirty);
  assert(dos_vbe_framebuffer_line(fb, -1, INT_MIN, -1, INT_MAX, 1) == 0);
  assert(dos_vbe_framebuffer_line(fb, 2, -4, 7, -1, 1) == 0);
  assert(!fb->dirty);
  assert(dos_vbe_framebuffer_line(fb, 8, 7, 8, 7, 7) == 0);
  dirty(fb, 8, 7, 1, 1);
  clean(fb);
  assert(dos_vbe_framebuffer_line(fb, 8, 7, 8, 7, 7) == 0);
  assert(!fb->dirty);
  for (i = 0; i < 8; ++i) {
    dos_vbe_framebuffer_clear(fb, 0);
    clean(fb);
    assert(dos_vbe_framebuffer_line(fb, 4, 3,
                                  octants[i][0], octants[i][1], 1) == 0);
    assert(pixel(fb, 4, 3) == 1);
    assert(pixel(fb, (unsigned int)octants[i][0],
                 (unsigned int)octants[i][1]) == 1);
    assert(fb->dirty);
    guards_check(&buffer);
  }
  dos_vbe_framebuffer_clear(fb, 0);
  clean(fb);
  assert(dos_vbe_framebuffer_line(fb, 0, 0, 8, 3, 1) == 0);
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      assert(pixel(fb, x, y) == (y == shallow[x] ? 1U : 0U));
    }
  }
  /* Cross-product of extreme, nearby and interior endpoints must terminate
   * with every byte outside the visible area intact.
   */
  for (a = 0; a < 10; ++a) {
    for (b = 0; b < 10; ++b) {
      for (c = 0; c < 10; ++c) {
        for (d = 0; d < 10; ++d) {
          assert(dos_vbe_framebuffer_line(fb, endpoints[a], endpoints[b],
                                        endpoints[c], endpoints[d], 5) == 0);
          assert(dos_vbe_framebuffer_validate(fb) == 0);
          guards_check(&buffer);
          clean(fb);
        }
      }
    }
  }
  guarded_free(&buffer);
}

static void blit_case(int dx, int dy, int sx, int sy,
                      int width, int height, int transparent, int self)
{
  struct guarded_buffer destination, source;
  struct dos_vbe_framebuffer *dst, *src;
  unsigned int snapshot[HEIGHT][WIDTH], expected[HEIGHT][WIDTH];
  unsigned int x, y, minx = WIDTH, miny = HEIGHT, maxx = 0, maxy = 0;
  int xx, yy, changed = 0;
  int64_t tx, ty, fx, fy;
  guarded_init(&destination);
  guarded_init(&source);
  dst = &destination.fb;
  src = self ? dst : &source.fb;
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      seed(dst, x, y, y * WIDTH + x);
      if (!self) seed(src, x, y, (y * WIDTH + x) % 5U);
      snapshot[y][x] = pixel(src, x, y);
      expected[y][x] = pixel(dst, x, y);
    }
  }
  for (yy = 0; yy < height; ++yy) {
    for (xx = 0; xx < width; ++xx) {
      tx = (int64_t)dx + xx; ty = (int64_t)dy + yy;
      fx = (int64_t)sx + xx; fy = (int64_t)sy + yy;
      if (tx >= 0 && tx < WIDTH && ty >= 0 && ty < HEIGHT
          && fx >= 0 && fx < WIDTH && fy >= 0 && fy < HEIGHT
          && (!transparent || snapshot[fy][fx] != 0)) {
        expected[ty][tx] = snapshot[fy][fx];
      }
    }
  }
  assert(dos_vbe_framebuffer_blit(dst, dx, dy, src, sx, sy,
                                width, height, transparent, 0) == 0);
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      unsigned int original = y * WIDTH + x;
      assert(pixel(dst, x, y) == expected[y][x]);
      if (expected[y][x] != original) {
        changed = 1;
        if (x < minx) minx = x;
        if (y < miny) miny = y;
        if (x > maxx) maxx = x;
        if (y > maxy) maxy = y;
      }
    }
  }
  if (changed) dirty(dst, minx, miny, maxx - minx + 1U, maxy - miny + 1U);
  else assert(!dst->dirty);
  if (!self) assert(!src->dirty);
  guarded_free(&destination);
  guarded_free(&source);
}

static void blit_test(void)
{
  struct guarded_buffer destination, source;
  int dx, dy, self, transparent;
  for (self = 0; self <= 1; ++self) {
    for (transparent = 0; transparent <= 1; ++transparent) {
      for (dy = -3; dy <= 3; ++dy) {
        for (dx = -3; dx <= 3; ++dx) {
          blit_case(dx, dy, 0, 0, 9, 8, transparent, self);
          blit_case(dx, dy, 2, 1, 6, 6, transparent, self);
          blit_case(dx, dy, -2, -1, 8, 7, transparent, self);
        }
      }
    }
  }
  guarded_init(&destination);
  guarded_init(&source);
  dos_vbe_framebuffer_clear(&source.fb, 0x1234);
  assert(dos_vbe_framebuffer_icon(&destination.fb, -8, -7,
                                &source.fb, 0, UINT_MAX) == 0);
  assert(pixel(&destination.fb, 0, 0) == 0x1234);
  dirty(&destination.fb, 0, 0, 1, 1);
  clean(&destination.fb);
  assert(dos_vbe_framebuffer_blit(&destination.fb, INT_MIN, INT_MIN,
                                &source.fb, INT_MAX, INT_MAX,
                                INT_MAX, INT_MAX, 0, 0) == 0);
  assert(dos_vbe_framebuffer_blit(&destination.fb, INT_MAX, INT_MAX,
                                &source.fb, INT_MIN, INT_MIN,
                                INT_MAX, INT_MAX, 1, 0) == 0);
  assert(dos_vbe_framebuffer_blit(&destination.fb, 0, 0, &source.fb,
                                0, 0, 0, 0, 1, 0) == 0);
  assert(!destination.fb.dirty);
  guarded_free(&destination);
  guarded_free(&source);
}

static void text_test(void)
{
  struct dos_vbe_framebuffer fb = { 0 };
  struct guarded_buffer buffer;
  char text[2] = { 0, 0 };
  char all_ascii[99];
  size_t at = 0;
  unsigned int ch, x, y, bits, expected;
  const unsigned char *glyph;
  assert(dos_vbe_framebuffer_init(&fb, 640, 480, 16) == 0);
  for (ch = DOS_VBE_FONT_FIRST; ch <= DOS_VBE_FONT_LAST; ++ch) {
    dos_vbe_framebuffer_clear(&fb, 0);
    clean(&fb);
    text[0] = (char)ch;
    assert(dos_vbe_framebuffer_text(&fb, 2, 3, text, 0xffff, 2) == 0);
    glyph = dos_vbe_bitmap_glyph(ch);
    bits = 0;
    for (y = 0; y < 14; ++y) {
      for (x = 0; x < 10; ++x) {
        expected = (glyph[y / 2U] & (1U << (4U - x / 2U))) ? 0xffffU : 0U;
        assert(pixel(&fb, x + 2U, y + 3U) == expected);
        bits += expected != 0;
      }
    }
    assert((bits != 0) == (ch != ' '));
    assert((fb.dirty != 0) == (ch != ' '));
    clean(&fb);
    assert(dos_vbe_framebuffer_text(&fb, 2, 3, text, 0xffff, 2) == 0);
    assert(!fb.dirty);
  }
  dos_vbe_framebuffer_clear(&fb, 0);
  assert(dos_vbe_framebuffer_text(&fb, 0, 0, "A\nB", 1, 2) == 0);
  assert(pixel(&fb, 2, 0) == 1);
  assert(pixel(&fb, 0, 16) == 1);
  assert(pixel(&fb, 12, 0) == 0);
  assert(dos_vbe_bitmap_glyph(0) == dos_vbe_bitmap_glyph('?'));
  assert(dos_vbe_bitmap_glyph(255) == dos_vbe_bitmap_glyph('?'));
  assert(dos_vbe_framebuffer_init(&fb, 800, 600, 16) == 0);
  for (ch = DOS_VBE_FONT_FIRST; ch <= DOS_VBE_FONT_LAST; ++ch) {
    all_ascii[at++] = (char)ch;
    if ((ch - DOS_VBE_FONT_FIRST + 1U) % 40U == 0) all_ascii[at++] = '\n';
  }
  all_ascii[at] = '\0';
  assert(dos_vbe_framebuffer_text(&fb, 0, 0, all_ascii, 0xffff, 2) == 0);
  assert(fb.dirty && fb.dirty_y == 0 && fb.dirty_height == 46);
  dos_vbe_framebuffer_destroy(&fb);
  guarded_init(&buffer);
  assert(dos_vbe_framebuffer_text(&buffer.fb, -3, -5, "Text", 0xffff, 2) == 0);
  assert(buffer.fb.dirty);
  clean(&buffer.fb);
  assert(dos_vbe_framebuffer_text(&buffer.fb, INT_MAX, INT_MAX, "A\nB", 1, 2) == 0);
  assert(dos_vbe_framebuffer_text(&buffer.fb, INT_MIN, INT_MIN, "A\nB", 1, 2) == 0);
  assert(dos_vbe_framebuffer_text(&buffer.fb, 0, 0, "", 1, 2) == 0);
  assert(!buffer.fb.dirty);
  assert(dos_vbe_framebuffer_text(&buffer.fb, -1, -1, "A", 1, 1) == 0);
  assert(buffer.fb.dirty);
  clean(&buffer.fb);
  assert(dos_vbe_framebuffer_text(&buffer.fb, INT_MIN, INT_MIN, "A\nB", 1,
                                (unsigned int)INT_MAX / 8U) == 0);
  guarded_free(&buffer);
}

static void color_test(void)
{
  unsigned int color = 123U, channel;
  assert(dos_vbe_framebuffer_encode_rgb565(16, 5, 11, 6, 5, 5, 0, 0,
                                         255, 0, 0, &color) == 0);
  assert(color == 0xf800);
  assert(dos_vbe_framebuffer_encode_rgb565(16, 5, 11, 6, 5, 5, 0, 0,
                                         0, 255, 0, &color) == 0);
  assert(color == 0x07e0);
  assert(dos_vbe_framebuffer_encode_rgb565(16, 5, 11, 6, 5, 5, 0, 0,
                                         0, 0, 255, &color) == 0);
  assert(color == 0x001f);
  for (channel = 0; channel <= 255; ++channel) {
    assert(dos_vbe_framebuffer_encode_rgb565(16, 5, 11, 6, 5, 5, 0, 0,
                                           channel, channel, channel, &color) == 0);
    assert(color == (((channel >> 3) << 11)
                     | ((channel >> 2) << 5) | (channel >> 3)));
  }
  color = 123U;
  assert(dos_vbe_framebuffer_encode_rgb565(15, 5, 10, 5, 5, 5, 0, 0,
                                         1, 2, 3, &color) == -1);
  assert(dos_vbe_framebuffer_encode_rgb565(16, 5, 0, 6, 5, 5, 11, 0,
                                         1, 2, 3, &color) == -1);
  assert(dos_vbe_framebuffer_encode_rgb565(16, 5, 11, 6, 5, 5, 0, 1,
                                         1, 2, 3, &color) == -1);
  assert(dos_vbe_framebuffer_encode_rgb565(16, 5, 11, 6, 5, 5, 0, 0,
                                         256, 2, 3, &color) == -1);
  assert(dos_vbe_framebuffer_encode_rgb565(16, 5, 11, 6, 5, 5, 0, 0,
                                         1, 2, 3, NULL) == -1);
  assert(color == 123U);
}

static void invalid_test(void)
{
  struct dos_vbe_framebuffer fb = { 0 }, bad;
  struct dos_vbe_dirty_rect rect;
  struct guarded_buffer buffer;
  assert(dos_vbe_framebuffer_init(NULL, 1, 1, 16) == -1);
  assert(dos_vbe_framebuffer_init(&fb, UINT_MAX, 1, 16) == -1);
  assert(dos_vbe_framebuffer_init(&fb, 1, UINT_MAX, 16) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&fb, (unsigned int)INT_MAX + 1U,
                                      1, 16, UINT_MAX - 1U) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&fb, 1, (unsigned int)INT_MAX + 1U,
                                      16, 2) == -1);
  assert(dos_vbe_framebuffer_init(&fb, 1, 0, 16) == -1);
  assert(dos_vbe_framebuffer_init(&fb, 1, 1, 8) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&fb, 1, 1, 16, 0) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&fb, 1, 1, 16, 3) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&fb, 9, 1, 16, 16) == -1);
  assert(dos_vbe_framebuffer_init_pitch(&fb, 1, INT_MAX, 16, 4) == -1);
  assert(dos_vbe_framebuffer_validate(NULL) == -1);
  assert(dos_vbe_framebuffer_validate(&fb) == -1);
  assert(dos_vbe_framebuffer_line(&fb, 0, 0, 1, 1, 1) == -1);
  dos_vbe_framebuffer_clear(NULL, 0);
  dos_vbe_framebuffer_put_pixel(NULL, 0, 0, 0);
  guarded_init(&buffer);
  bad = buffer.fb;
  bad.size_bytes--;
  assert(dos_vbe_framebuffer_validate(&bad) == -1);
  assert(dos_vbe_framebuffer_fill_rect(&bad, 0, 0, 1, 1, 1) == -1);
  bad = buffer.fb;
  bad.height++;
  assert(dos_vbe_framebuffer_validate(&bad) == -1);
  bad = buffer.fb;
  bad.dirty = 1;
  bad.dirty_x = UINT_MAX;
  assert(dos_vbe_framebuffer_validate(&bad) == -1);
  bad.dirty = 2;
  assert(dos_vbe_framebuffer_validate(&bad) == -1);
  bad = buffer.fb;
  bad.width--;
  assert(dos_vbe_framebuffer_blit(&buffer.fb, 0, 0, &bad, 0, 0, 1, 1, 0, 0) == -1);
  assert(dos_vbe_framebuffer_fill_rect(&buffer.fb, 0, 0, -1, 1, 1) == -1);
  assert(dos_vbe_framebuffer_fill_rect(&buffer.fb, 0, 0, 1, -1, 1) == -1);
  assert(dos_vbe_framebuffer_line(&buffer.fb, 0, 0, 1, 1, 0x10000) == -1);
  assert(dos_vbe_framebuffer_blit(&buffer.fb, 0, 0, &buffer.fb,
                                0, 0, -1, 1, 0, 0) == -1);
  assert(dos_vbe_framebuffer_blit(&buffer.fb, 0, 0, &buffer.fb,
                                0, 0, 1, 1, 2, 0) == -1);
  assert(dos_vbe_framebuffer_blit(&buffer.fb, 0, 0, &buffer.fb,
                                0, 0, 1, 1, 1, UINT_MAX) == -1);
  assert(dos_vbe_framebuffer_text(&buffer.fb, 0, 0, NULL, 1, 2) == -1);
  assert(dos_vbe_framebuffer_text(&buffer.fb, 0, 0, "A", 1, 0) == -1);
  assert(dos_vbe_framebuffer_text(&buffer.fb, 0, 0, "A", 1, UINT_MAX) == -1);
  assert(dos_vbe_framebuffer_dirty_peek(&buffer.fb, NULL) == -1);
  assert(dos_vbe_framebuffer_dirty_peek(NULL, &rect) == -1);
  assert(dos_vbe_framebuffer_dirty_clear(NULL) == -1);
  assert(!buffer.fb.dirty && pixel(&buffer.fb, 0, 0) == 0);
  guarded_free(&buffer);
}

int main(void)
{
  allocation_test();
  fill_dirty_test();
  line_test();
  blit_test();
  text_test();
  color_test();
  invalid_test();
  puts("framebuffer: allocation, clipping, 10000 extreme lines,"
       " 588 snapshot blits, ASCII,"
       " masks, dirty lifecycle and guards passed (ASan/UBSan)");
  return 0;
}
