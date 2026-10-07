#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include "vbe_init.h"

static unsigned int standard_colors[COLOR_STD_LAST];
static int palette_ready;

unsigned int dos_vbe_rgb(unsigned int red, unsigned int green,
                         unsigned int blue)
{
  unsigned int color;
  if (dos_vbe_framebuffer_encode_rgb565(16, 5, 11, 6, 5, 5, 0, 0,
                                       red, green, blue, &color) != 0) {
    fprintf(stderr, "DOS VBE: invalid RGB color component.\n");
    exit(EXIT_FAILURE);
  }
  return color;
}

unsigned int dos_vbe_standard_color(enum color_std color)
{
  if (color < 0 || color >= COLOR_STD_LAST) {
    fprintf(stderr, "DOS VBE: invalid standard color index.\n");
    exit(EXIT_FAILURE);
  }
  if (!palette_ready) {
    init_color_system();
  }
  return standard_colors[color];
}

enum Display_color_type get_visual(void)
{
  return COLOR_DISPLAY;
}

void free_color_system(void)
{
  /* The standard palette is static; display resources belong to VBE. */
  palette_ready = 0;
}


void
init_color_system(void)
{
  static const unsigned char rgb[COLOR_STD_LAST][3] = {
    {0,0,0}, {255,255,255}, {255,0,0}, {255,255,0}, {0,255,200},
    {0,200,0}, {0,0,200}, {86,86,86}, {128,0,0}, {128,255,255},
    {255,0,0}, {255,0,128}, {0,0,128}, {255,0,255}, {255,128,0},
    {255,255,128}, {255,128,128}, {0,0,255}, {0,255,0}, {0,128,128},
    {0,64,64}, {198,198,198}
  };
  struct dos_vbe_mode_info info;
  unsigned int mode;
  unsigned int i;

  if (dos_vbe_current_mode(&info, &mode) != 0
      || info.bits_per_pixel != 16U || info.red_mask_size != 5U
      || info.red_field_position != 11U || info.green_mask_size != 6U
      || info.green_field_position != 5U || info.blue_mask_size != 5U
      || info.blue_field_position != 0U || info.reserved_mask_size != 0U) {
    color_error();
  }
  for (i = 0; i < COLOR_STD_LAST; ++i) {
    standard_colors[i] = dos_vbe_rgb(rgb[i][0], rgb[i][1], rgb[i][2]);
  }
  palette_ready = 1;
}

void
color_error(void)
{
  fprintf(stderr, "DOS VBE: renderer requires a validated RGB565 display.\n");
  exit(EXIT_FAILURE);
}
