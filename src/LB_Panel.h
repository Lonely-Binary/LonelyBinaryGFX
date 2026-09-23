#pragma once
/*
 * LB_Panel.h - what a device library provides, and what everything else consumes.
 *
 * Contract: canvas.yaml, section 2 and decision D2.
 *
 *                    LB_Panel  (buffer / size / format / flush)
 *                       ^                        ^
 *                   LB_Canvas                 LB_LVGL
 *              (immediate drawing)         (retained GUI)
 *
 * Implemented by LonelyBinaryDisplay (TFT), LonelyBinaryVGA, LonelyBinaryEPaper.
 * It lives in LB_GFX rather than in a library of its own (D2): the interface is
 * a couple of hundred bytes of header, and the linker drops whatever drawing
 * code a given sketch never calls, so an LVGL-only sketch pays nothing for it.
 *
 * !! The device owns the buffer, never the canvas (D6) !!
 *   VGA's scan-out interrupt reads that exact memory, and a TFT on a board with
 *   no PSRAM has no buffer at all and goes straight to the bus. A canvas draws
 *   into memory it was handed; it cannot exist without a device behind it.
 *   Consequence: an off-screen canvas, for compositing or double buffering, is
 *   a v1.1 feature and does not fall out of v1 for free.
 */
#include <stdint.h>
#include "LB_Color.h"

/* How pixels are stored. Sub-byte formats pack low-to-high with the LEFT (even)
 * pixel in the HIGH bits - the layout LovyanGFX uses, kept because the VGA
 * scan-out ISR is already verified against it. */
enum lb_format_t : uint8_t
{
  LB_FMT_RGB565 = 0, /* 16 bpp. TFT, and VGA's 320x240 mode                  */
  LB_FMT_PAL4,       /*  4 bpp, 16-entry palette. VGA's 640x480 mode         */
  LB_FMT_PAL2,       /*  2 bpp, 4 fixed inks. Colour e-paper (D7)            */
  LB_FMT_PAL1,       /*  1 bpp, 2 inks. Black-and-white e-paper              */
};

static inline uint8_t lb_format_bpp(lb_format_t f)
{
  switch (f)
  {
  case LB_FMT_RGB565: return 16;
  case LB_FMT_PAL4:   return 4;
  case LB_FMT_PAL2:   return 2;
  default:            return 1;
  }
}

static inline bool lb_format_is_paletted(lb_format_t f)
{
  return f != LB_FMT_RGB565;
}

enum lb_flush_t : uint8_t
{
  LB_FLUSH_FULL = 0,
  LB_FLUSH_PARTIAL,
};

class LB_Panel
{
public:
  virtual ~LB_Panel() {}

  /* Native geometry, at rotation 0. The canvas applies rotation itself, so a
   * panel never has to think about it. */
  virtual int16_t panelWidth() const = 0;
  virtual int16_t panelHeight() const = 0;

  virtual lb_format_t format() const = 0;

  /*
   * The pixel buffer, or nullptr for a device that writes straight to the bus
   * with no RAM copy (a TFT on a board without PSRAM).
   *
   * !! Every caller must handle nullptr !!  On such a device the canvas falls
   * back to asking the panel to paint spans directly - slower, but it is the
   * difference between "works everywhere" and "needs PSRAM".
   */
  virtual uint8_t *buffer() const = 0;

  /* Bytes per row. NOT always width * bpp / 8: sub-byte formats round a row up
   * to a whole byte, so a 4bpp row of 3 pixels still occupies 2 bytes. */
  virtual uint32_t stride() const = 0;

  /*
   * Push what has been drawn. Behaves differently on every device - see
   * canvas.yaml - and on e-paper it BLOCKS FOR SECONDS (measured on our own
   * panels: 1.5 s mono, 20-45 s colour).
   *
   * The contract is that a portable sketch always calls it; where it is not
   * needed it is a no-op.
   *
   * LB_FLUSH_PARTIAL falls back to a full refresh, silently, on a panel that
   * cannot do partial (D4). Silent because returning an error would push the
   * problem onto every caller and make portable code impossible; the danger is
   * that the fallback then blocks for tens of seconds and looks exactly like a
   * hang, which is why supportsPartial() exists and why the per-panel blocking
   * time is documented rather than discovered.
   */
  virtual void flush(lb_flush_t mode = LB_FLUSH_FULL) = 0;

  virtual bool supportsPartial() const { return false; }

  /* Deep sleep. Required on e-paper, where leaving the panel powered damages it
   * over time; a no-op elsewhere. On e-paper this DISCARDS the previous-frame
   * memory, so partial refresh is unavailable until the next full refresh. */
  virtual void sleep() {}

  /* ---- palette, for paletted panels ---- */

  /* 0 on a direct-colour panel. 16 for PAL4, 4 for PAL2, 2 for PAL1. */
  virtual uint16_t paletteSize() const { return 0; }

  /*
   * Returns false where the palette is read-only, which is the case for colour
   * e-paper (D7): those are fixed inks - black, white, red, yellow - not a
   * palette anyone can redefine. Returning false is better than accepting the
   * call and doing nothing.
   */
  virtual bool setPaletteColor(uint8_t, uint8_t, uint8_t, uint8_t) { return false; }

  /* paletteSize() entries of 0x00RRGGBB, or nullptr. The canvas reads this to
   * nearest-match a colour; the panel keeps whatever hardware-side copy it
   * needs (VGA's scan-out ISR, for instance, holds an RGB565 version). */
  virtual const lb_color_t *palette() const { return nullptr; }

  /*
   * Paint a horizontal span directly, for panels with no buffer().
   * Only called when buffer() is nullptr; the default is deliberately empty so
   * that a buffered panel does not have to implement it.
   */
  virtual void writeSpan(int16_t, int16_t, int16_t, lb_color_t) {}

  /*
   * True when the panel applies rotation itself - a TFT does it with one MADCTL
   * bit, for free. The canvas then leaves coordinates alone and just forwards
   * the rotation.
   *
   * False (the default) means the canvas rotates in software, and there the
   * cost is not symmetric: at rotation 0 and 2 a horizontal fill is still a
   * horizontal run of memory, but at 1 and 3 it becomes a vertical one and has
   * to be written pixel by pixel. Rotating a VGA or e-paper canvas by 90
   * degrees therefore makes fills substantially slower. That is worth knowing
   * before designing a portrait layout on a landscape panel.
   */
  virtual bool hardwareRotation() const { return false; }
  virtual void setPanelRotation(uint8_t) {}
};
