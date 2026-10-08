/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dpmi.h>
#include "civclient.h"
#include "clinet.h"
#include "gui_main.h"
#include "vbe_init.h"
#include "input.h"
#include "phase6_scene.h"

int main(int argc, char **argv)
{
  int result = EXIT_FAILURE, bridge = 0, scene = 0;
  unsigned int width = 800;
  __dpmi_regs regs;
  if (argc == 2 && !strcmp(argv[1], "--bridge")) bridge = 1;
  else if (argc == 2 && !strcmp(argv[1], "640")) width = 640;
  else if (argc != 1 && !(argc == 2 && !strcmp(argv[1], "800"))) {
    fprintf(stderr, "Usage: INPUTCHK [640|800|--bridge] > writable.log\n");
    return EXIT_FAILURE;
  }
  if (dup2(fileno(stdout), fileno(stderr)) < 0) {
    perror("INPUTCHK redirection");
    return EXIT_FAILURE;
  }
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  memset(&regs, 0, sizeof(regs)); regs.x.ax = 0x0f00;
  if (__dpmi_int(0x10, &regs) != 0 || (regs.h.al & 0x7fU) != 3) {
    fprintf(stderr, "FAIL start in text mode 3\n");
    return EXIT_FAILURE;
  }
  if (vbe_set_mode(width, width == 640 ? 480 : 600, 16) != 0) return EXIT_FAILURE;
  ui_init();
  dos_vbe_gui_set_trace(1);
  if (bridge) {
    char error[256];
    set_client_state(CLIENT_PRE_GAME_STATE);
    if (connect_to_local_game("DOSInput", error, sizeof(error)) != 0) {
      fprintf(stderr, "FAIL real bridge: %s\n", error);
      goto done;
    }
    printf("READY real pregame join/packet bridge; no game start\n");
  } else {
    if (dos_phase6_scene_init() != 0) goto done;
    scene = 1;
    if (dos_phase6_scene_render() != 0) goto done;
    printf("READY actual-state map input fixture; no authoritative gameplay\n");
  }
  printf("INPUT mouse=%d width=%u\n", dos_input_mouse_available(), width);
  ui_main(0, NULL);
  if (dos_vbe_gui_result() != EXIT_SUCCESS) goto done;
  if (bridge) {
    if (aconnection.used) {
      fprintf(stderr, "FAIL bridge not disconnected at safe boundary\n");
      goto done;
    }
    printf("PASS bridge disconnected at safe boundary\n");
  }
  printf("PASS persistent event loop returned after explicit confirmed quit\n");
  result = EXIT_SUCCESS;
done:
  if (scene) dos_phase6_scene_free();
  if (bridge) client_game_free();
  dos_vbe_gui_shutdown();
  if (dos_vbe_gui_result() != EXIT_SUCCESS || dos_vbe_display_active()) result = EXIT_FAILURE;
  memset(&regs, 0, sizeof(regs)); regs.x.ax = 0x0f00;
  if (__dpmi_int(0x10, &regs) != 0 || (regs.h.al & 0x7fU) != 3) {
    fprintf(stderr, "FAIL text-mode restoration\n");
    result = EXIT_FAILURE;
  }
  printf("EXIT %s\n", result == EXIT_SUCCESS ? "SUCCESS" : "FAILURE");
  if (fclose(stdout) != 0) result = EXIT_FAILURE;
  return result;
}
