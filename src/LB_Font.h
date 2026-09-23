#pragma once
/*
 * LB_Font.h - the bitmap font format.
 *
 * Deliberately the simplest thing that works: one byte per 8 pixels of width,
 * rows top to bottom, glyphs in ASCII order. The decoder in LB_Canvas.cpp is a
 * dozen lines, which matters more than compactness - this is the code that runs
 * for every character of every label on every screen.
 *
 * Fonts are generated from BDF by tools/gen_fonts.py into LB_Fonts.cpp.
 *
 * !! UTF-8 goes in even though v1 ships no CJK font (D5) !!
 *   drawString decodes UTF-8 and draws a placeholder box for any codepoint the
 *   font lacks. That way adding Chinese later is additive, rather than breaking
 *   every sketch that ever passed a non-ASCII string.
 */
#include <stdint.h>

struct LB_Font
{
  uint8_t width;        /* cell width in pixels                    */
  uint8_t height;       /* cell height in pixels                   */
  uint8_t bytesPerRow;  /* (width + 7) / 8                         */
  uint16_t first;       /* first codepoint present                 */
  uint16_t last;        /* last codepoint present                  */
  const uint8_t *data;  /* (last-first+1) * height * bytesPerRow   */
};

extern const LB_Font LB_Font5x8;   /* small, for dense readouts            */
extern const LB_Font LB_Font8x16;  /* the workhorse. 80x30 on 640x480      */
