/* RGB565 colors for the DOS renderer. */
#ifndef FC__COLORS_H
#define FC__COLORS_H

#include "colors_g.h"

unsigned int dos_vbe_rgb(unsigned int red, unsigned int green,
                         unsigned int blue);
unsigned int dos_vbe_standard_color(enum color_std color);

#endif  /* FC__COLORS_H */
