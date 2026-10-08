#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <unistd.h>
#include <dpmi.h>

#include "phase6_scene.h"
#include "vbe_init.h"
#include "mapview.h"

int main(int argc, char **argv)
{
  unsigned int width = 800;
  int result = EXIT_FAILURE;
  __dpmi_regs regs;

  if (argc == 2 && !strcmp(argv[1], "640")) {
    width = 640;
  } else if (argc != 1 && !(argc == 2 && !strcmp(argv[1], "800"))) {
    fprintf(stderr, "Usage: MAPCHECK [640|800] > writable.log\n");
    return EXIT_FAILURE;
  }
  if (dup2(fileno(stdout), fileno(stderr)) < 0) {
    perror("MAPCHECK diagnostic redirection");
    return EXIT_FAILURE;
  }
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x0f00;
  if (__dpmi_int(0x10, &regs) != 0 || (regs.h.al & 0x7fU) != 3U) {
    fprintf(stderr, "FAIL must start in DOS text mode 3\n");
    return EXIT_FAILURE;
  }
  printf("MAPCHECK: real tileset/client-state fixture; NOT gameplay\n");
  if (vbe_set_mode(width, width == 640 ? 480 : 600, 16) != 0
      || dos_phase6_scene_init() != 0
      || dos_phase6_scene_render() != 0
      || dos_vbe_present() != 0 || dos_vbe_verify_video() != 0) {
    fprintf(stderr, "FAIL resource/map rendering or full readback\n");
    goto done;
  }
  printf("PASS resource/map fixture and full readback %ux%u\n",
         dos_vbe_front_buffer.width, dos_vbe_front_buffer.height);
  update_map_canvas_visible();
  if (dos_vbe_last_present_bytes() != 0 || dos_vbe_verify_video() != 0) {
    fprintf(stderr, "FAIL repeated composed map presentation\n");
    goto done;
  }
  printf("PASS repeated map bytes=0\n");
  printf("READY map/HUD/overview fixture; Q restores DOS\n");
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
  dos_phase6_scene_free();
  if (vbe_shutdown_display() != 0 || dos_vbe_display_active()
      || vbe_shutdown_display() != 0) {
    fprintf(stderr, "FAIL display teardown\n");
    result = EXIT_FAILURE;
  }
  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x0f00;
  if (__dpmi_int(0x10, &regs) != 0 || (regs.h.al & 0x7fU) != 3U) {
    fprintf(stderr, "FAIL original text mode not restored\n");
    result = EXIT_FAILURE;
  }
  printf("EXIT %s\n", result == EXIT_SUCCESS ? "SUCCESS" : "FAILURE");
  if (fclose(stdout) != 0) {
    perror("MAPCHECK log close");
    result = EXIT_FAILURE;
  }
  return result;
}
