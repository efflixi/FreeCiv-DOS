#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <unistd.h>
#include <dpmi.h>

#include "render_pattern.h"
#include "vbe_init.h"

static int check_local_updates(void)
{
  struct dos_vbe_framebuffer *fb = &dos_vbe_front_buffer;
  unsigned int x;
  unsigned int y;
  unsigned int left = fb->width - 12;
  unsigned int top = fb->height - 12;

  for (y = top; y < top + 4; ++y) {
    for (x = left; x < left + 4; ++x) {
      dos_vbe_framebuffer_put_pixel(fb, x, y, 0xffff);
    }
  }
  if (dos_vbe_present() != 0 || dos_vbe_last_present_bytes() != 32
      || dos_vbe_verify_video() != 0) {
    fprintf(stderr, "FAIL local update/readback\n");
    return -1;
  }
  printf("PASS local bytes=%lu\n", (unsigned long)dos_vbe_last_present_bytes());
  if (dos_vbe_present() != 0 || dos_vbe_last_present_bytes() != 0) {
    fprintf(stderr, "FAIL repeated presentation\n");
    return -1;
  }
  printf("PASS repeated bytes=0\n");
  for (y = top; y < top + 4; ++y) {
    for (x = left; x < left + 4; ++x) {
      dos_vbe_framebuffer_put_pixel(fb, x, y, 0x0841);
    }
  }
  if (dos_vbe_present() != 0 || dos_vbe_verify_video() != 0) {
    fprintf(stderr, "FAIL restore local background\n");
    return -1;
  }
  dos_vbe_framebuffer_put_pixel(fb, left + 1, top + 1, 0xffff);
  dos_vbe_framebuffer_put_pixel(fb, left + 3, top + 2, 0xffff);
  if (dos_vbe_present() != 0 || dos_vbe_last_present_bytes() != 12
      || dos_vbe_verify_video() != 0) {
    fprintf(stderr, "FAIL coalesced updates\n");
    return -1;
  }
  printf("PASS coalesced bytes=12\n");
  dos_vbe_framebuffer_put_pixel(fb, left + 1, top + 1, 0x0841);
  dos_vbe_framebuffer_put_pixel(fb, left + 3, top + 2, 0x0841);
  if (dos_vbe_present() != 0 || dos_vbe_verify_video() != 0) {
    fprintf(stderr, "FAIL final readback\n");
    return -1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  unsigned int width = 800;
  int result = EXIT_FAILURE;
  __dpmi_regs regs;

  if (argc == 2 && strcmp(argv[1], "640") == 0) {
    width = 640;
  } else if (argc != 1 && !(argc == 2 && strcmp(argv[1], "800") == 0)) {
    fprintf(stderr, "Usage: RNDCHECK [640|800]\n");
    return EXIT_FAILURE;
  }
  if (dup2(fileno(stdout), fileno(stderr)) < 0) {
    perror("RNDCHECK stderr redirection");
    return EXIT_FAILURE;
  }
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x0f00;
  if (__dpmi_int(0x10, &regs) != 0 || (regs.h.al & 0x7fU) != 3U) {
    fprintf(stderr, "FAIL diagnostic must start in text mode 3\n");
    return EXIT_FAILURE;
  }
  printf("RENDER CHECK start width=%u\n", width);
  if (vbe_set_mode(width, width == 640 ? 480 : 600, 16) != 0) {
    goto done;
  }
  if (dos_render_test_pattern(&dos_vbe_front_buffer) != 0
      || dos_vbe_present() != 0 || dos_vbe_verify_video() != 0) {
    fprintf(stderr, "FAIL initial pattern/readback\n");
    goto done;
  }
  printf("PASS initial readback %ux%u\n", dos_vbe_front_buffer.width,
         dos_vbe_front_buffer.height);
  if (check_local_updates() != 0) {
    goto done;
  }
  printf("READY original ASCII font/primitives; press Q\n");
  while (!kbhit()) {
    usleep(10000);
  }
  {
    int key = getch();
    if (key != 'q' && key != 'Q') {
      fprintf(stderr, "FAIL expected Q\n");
      goto done;
    }
  }
  printf("PASS Q received\n");
  result = EXIT_SUCCESS;
done:
  if (vbe_shutdown_display() != 0 || dos_vbe_display_active()
      || vbe_shutdown_display() != 0) {
    fprintf(stderr, "FAIL display resources still owned\n");
    result = EXIT_FAILURE;
  }
  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x0f00;
  if (__dpmi_int(0x10, &regs) != 0 || (regs.h.al & 0x7fU) != 3U) {
    fprintf(stderr, "FAIL original BIOS text mode not restored\n");
    result = EXIT_FAILURE;
  }
  printf("EXIT %s\n", result == EXIT_SUCCESS ? "SUCCESS" : "FAILURE");
  if (fclose(stdout) != 0) {
    perror("RNDCHECK log close");
    result = EXIT_FAILURE;
  }
  return result;
}
