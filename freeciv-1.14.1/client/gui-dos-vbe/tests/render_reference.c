#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render_pattern.h"

int main(int argc, char **argv)
{
  struct dos_vbe_framebuffer fb = { 0 };
  unsigned int width;
  unsigned int height;
  unsigned int x;
  unsigned int y;
  FILE *file;

  if (argc != 3 || (strcmp(argv[1], "640") && strcmp(argv[1], "800"))) {
    fprintf(stderr, "Usage: render-reference 640|800 output.ppm\n");
    return EXIT_FAILURE;
  }
  width = (unsigned int)atoi(argv[1]);
  height = width == 640 ? 480 : 600;
  if (dos_vbe_framebuffer_init(&fb, width, height, 16) != 0
      || dos_render_test_pattern(&fb) != 0) {
    dos_vbe_framebuffer_destroy(&fb);
    return EXIT_FAILURE;
  }
  file = fopen(argv[2], "wb");
  if (!file) {
    perror("render-reference output");
    dos_vbe_framebuffer_destroy(&fb);
    return EXIT_FAILURE;
  }
  fprintf(file, "P6\n%u %u\n255\n", width, height);
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      size_t offset = (size_t)y * fb.stride + x * 2U;
      unsigned int pixel = fb.pixels[offset] | ((unsigned int)fb.pixels[offset + 1] << 8);
      unsigned char rgb[3];
      unsigned int r = (pixel >> 11) & 31U;
      unsigned int g = (pixel >> 5) & 63U;
      unsigned int b = pixel & 31U;
      rgb[0] = (r << 3) | (r >> 2);
      rgb[1] = (g << 2) | (g >> 4);
      rgb[2] = (b << 3) | (b >> 2);
      if (fwrite(rgb, 1, 3, file) != 3) {
        perror("render-reference pixel write");
        fclose(file);
        dos_vbe_framebuffer_destroy(&fb);
        return EXIT_FAILURE;
      }
    }
  }
  dos_vbe_framebuffer_destroy(&fb);
  if (fclose(file) != 0) {
    perror("render-reference close");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
