#ifndef FC__DOS_VBE_INIT_H
#define FC__DOS_VBE_INIT_H

#include <stdint.h>
#include "framebuffer.h"

#define DOS_VBE_MODE_640X480X16 0x0111U
#define DOS_VBE_MODE_800X600X16 0x0114U
#define DOS_VBE_MODE_1024X768X16 0x0117U
#define DOS_VBE_CONTROLLER_BYTES 512U
#define DOS_VBE_MODE_BYTES 256U

/* BIOS buffers are byte arrays, never host/compiler-layout C structures. */
struct dos_vbe_controller_block {
  unsigned char bytes[DOS_VBE_CONTROLLER_BYTES];
};
struct dos_vbe_mode_block {
  unsigned char bytes[DOS_VBE_MODE_BYTES];
};
typedef char dos_vbe_controller_size_check[
  sizeof(struct dos_vbe_controller_block) == 512 ? 1 : -1];
typedef char dos_vbe_mode_size_check[
  sizeof(struct dos_vbe_mode_block) == 256 ? 1 : -1];

struct dos_vbe_mode_info {
  unsigned int mode_attributes;
  unsigned int bytes_per_scanline;
  unsigned int x_resolution;
  unsigned int y_resolution;
  unsigned int number_of_planes;
  unsigned int bits_per_pixel;
  unsigned int memory_model;
  unsigned int red_mask_size;
  unsigned int red_field_position;
  unsigned int green_mask_size;
  unsigned int green_field_position;
  unsigned int blue_mask_size;
  unsigned int blue_field_position;
  unsigned int reserved_mask_size;
  uint32_t physical_base_pointer;
};

extern struct dos_vbe_framebuffer dos_vbe_front_buffer;

int dos_vbe_detect(void);
unsigned int dos_vbe_mode_for_resolution(unsigned int width,
                                        unsigned int height,
                                        unsigned int bpp);
int dos_vbe_get_mode_info(unsigned int mode, struct dos_vbe_mode_info *info);
const char *dos_vbe_mode_name(unsigned int mode);
int vbe_init_display(void);
int vbe_set_mode(unsigned int width, unsigned int height, unsigned int bpp);
int vbe_shutdown_display(void);
int dos_vbe_display_active(void);
int dos_vbe_current_mode(struct dos_vbe_mode_info *info, unsigned int *mode);
int dos_vbe_present(void);
int dos_vbe_verify_video(void);
size_t dos_vbe_last_present_bytes(void);
unsigned long dos_vbe_display_generation(void);

#endif
