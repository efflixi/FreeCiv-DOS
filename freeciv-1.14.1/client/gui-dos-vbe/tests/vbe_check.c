#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <unistd.h>
#include <dpmi.h>

#include "vbe_init.h"

static FILE *log_file;

static int finish(int result)
{
  __dpmi_regs regs;
  int restored = vbe_shutdown_display();

  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x0f00;
  if (__dpmi_int(0x10, &regs) != 0 || (regs.h.al & 0x7fU) != 3U) {
    fprintf(log_file, "FAIL original text mode not restored\n");
    result = EXIT_FAILURE;
  }
  if (restored != 0 || vbe_shutdown_display() != 0) {
    fprintf(log_file, "FAIL display restoration/resource release\n");
    result = EXIT_FAILURE;
  }
  fprintf(log_file, "EXIT %s: observed BIOS text mode %u; "
          "repeated shutdown checked\n",
          result == EXIT_SUCCESS ? "SUCCESS" : "FAILURE", regs.h.al & 0x7fU);
  if (fclose(log_file) != 0) {
    perror("VBECHECK log close");
    result = EXIT_FAILURE;
  }
  printf("VBECHECK %s; see log for display/restoration results.\n",
         result == EXIT_SUCCESS ? "passed" : "failed");
  return result;
}

int main(int argc, char **argv)
{
  static const unsigned int bars[] = {
    0xf800, 0x07e0, 0x001f, 0xffff, 0x0000, 0xffe0, 0x07ff, 0xf81f
  };
  struct dos_vbe_mode_info info;
  unsigned int mode;
  unsigned int width = 800;
  unsigned int height = 600;
  unsigned int x;
  unsigned int y;
  int batch = 0;
  int i;
  const char *log_path = "C:\\FREECIV\\VBE.LOG";
  __dpmi_regs regs;

  for (i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--640")) {
      width = 640;
      height = 480;
    } else if (!strcmp(argv[i], "--batch")) {
      batch = 1;
    } else if (!strcmp(argv[i], "--log") && i + 1 < argc) {
      log_path = argv[++i];
    } else {
      fprintf(stderr, "Usage: VBECHECK [--640] [--batch] [--log path]\n");
      return EXIT_FAILURE;
    }
  }
  log_file = fopen(log_path, "w");
  if (!log_file) {
    perror("VBECHECK log open");
    return EXIT_FAILURE;
  }
  setvbuf(log_file, NULL, _IONBF, 0);
  if (dup2(fileno(log_file), fileno(stderr)) < 0) {
    perror("VBECHECK diagnostic redirection");
    fclose(log_file);
    return EXIT_FAILURE;
  }
  setvbuf(stderr, NULL, _IONBF, 0);
  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x0f00;
  if (__dpmi_int(0x10, &regs) != 0 || (regs.h.al & 0x7fU) != 3U) {
    fprintf(log_file, "FAIL diagnostic must start in text mode 3\n");
    fclose(log_file);
    return EXIT_FAILURE;
  }
  fprintf(log_file, "INIT VBECHECK: real BIOS/DPMI display only; not gameplay\n");
  if (vbe_set_mode(width, height, 16) != 0
      || dos_vbe_current_mode(&info, &mode) != 0) {
    fprintf(log_file, "FAIL real display initialization\n");
    return finish(EXIT_FAILURE);
  }
  fprintf(log_file, "PASS MODE 0x%04x %ux%u bpp=%u pitch=%u attributes=0x%04x "
          "physical=0x%08lx RGB=%u@%u/%u@%u/%u@%u\n",
          mode, info.x_resolution, info.y_resolution, info.bits_per_pixel,
          info.bytes_per_scanline, info.mode_attributes,
          (unsigned long)info.physical_base_pointer,
          info.red_mask_size, info.red_field_position,
          info.green_mask_size, info.green_field_position,
          info.blue_mask_size, info.blue_field_position);
  for (y = 0; y < info.y_resolution; ++y) {
    for (x = 0; x < info.x_resolution; ++x) {
      unsigned int color = bars[x * 8U / info.x_resolution];
      if (y >= info.y_resolution / 2U) {
        color = ((x / 16U + y / 16U) & 1U) ? 0xffffU : 0;
      }
      dos_vbe_framebuffer_put_pixel(&dos_vbe_front_buffer, x, y, color);
    }
  }
  if (dos_vbe_present() != 0 || dos_vbe_verify_video() != 0
      || vbe_init_display() != 0) {
    fprintf(log_file, "FAIL presentation/readback/idempotent initialization\n");
    return finish(EXIT_FAILURE);
  }
  fprintf(log_file, "READY visible RGB bars and checkerboard; complete "
          "pitch*height LFB readback passed; idempotent init passed\n");
  if (!batch) {
    fprintf(log_file, "WAIT Q to restore original text mode and release display\n");
    while (1) {
      if (kbhit()) {
        int key = getch();
        if (key == 'q' || key == 'Q') {
          break;
        }
      }
      usleep(10000);
    }
    fprintf(log_file, "PASS Q received\n");
  }
  return finish(EXIT_SUCCESS);
}
