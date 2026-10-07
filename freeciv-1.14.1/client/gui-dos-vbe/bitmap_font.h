/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FC__DOS_VBE_BITMAP_FONT_H
#define FC__DOS_VBE_BITMAP_FONT_H

#define DOS_VBE_FONT_FIRST 32U
#define DOS_VBE_FONT_LAST 126U
#define DOS_VBE_FONT_WIDTH 5U
#define DOS_VBE_FONT_HEIGHT 7U
#define DOS_VBE_FONT_SCALE 2U
#define DOS_VBE_FONT_ADVANCE 12U
#define DOS_VBE_FONT_LINE_HEIGHT 16U

/* Seven rows per glyph, most significant of the five bits is the left edge.
 * This original, hand-authored resource is GPL-2.0-or-later, like Freeciv.
 * Unsupported bytes return the question-mark glyph.
 */
const unsigned char *dos_vbe_bitmap_glyph(unsigned int character);

#endif
