#pragma once
/*
 * LB_Canvas.h - the drawing API every Lonely Binary display shares.
 *
 * Contract: canvas.yaml v1. That file is the source of truth; this header is
 * reviewed against it. If the two disagree, canvas.yaml is right.
 *
 * A device library subclasses this and holds its panel as a member, so the
 * customer writes one object and calls drawing straight on it:
 *
 *     LB_VGA vga(LB_VGA_320x240);
 *     vga.begin();
 *     vga.fillScreen(LB_BLACK);
 *     vga.drawString("Hello", 10, 10);
 *     vga.flush();
 *
 * !! Always call flush() !!
 *   It is a no-op on a TFT writing straight to the bus and on VGA, a few
 *   milliseconds on a TFT with a framebuffer, and SECONDS on e-paper. Writing
 *   it every time is the only way one sketch can run on all three.
 *
 * !! Do not write per-pixel loops !!
 *   True of every graphics library, and worst on the sub-byte formats where a
 *   single pixel is a read-modify-write of half a byte. Measured on VGA through
 *   LovyanGFX: 0.92 us per pixel, 282 ms for a full 640x480 screen, and most of
 *   that is per-call overhead rather than the pixel itself. Use fillRect,
 *   drawString and drawJpg, which fill whole bytes at a time.
 */
#include <stdint.h>
#include "LB_Color.h"
#include "LB_Panel.h"
#include "LB_Font.h"

class LB_Canvas
{
public:
  explicit LB_Canvas(LB_Panel &panel) : _panel(panel) {}
  virtual ~LB_Canvas() {}

  /* ── surface ─────────────────────────────────────────────────────────── */

  /* In the CURRENT rotation. Several panels ship rotated, so this is not the
   * glass size, and it changes when the rotation does. Never hard-code it. */
  int16_t width() const;
  int16_t height() const;

  void setRotation(uint8_t rotation);
  uint8_t rotation() const { return _rotation; }

  void fillScreen(lb_color_t color);

  void flush(lb_flush_t mode = LB_FLUSH_FULL) { _panel.flush(mode); }
  bool supportsPartial() const { return _panel.supportsPartial(); }
  void sleep() { _panel.sleep(); }

  /* ── pixels and lines ────────────────────────────────────────────────── */
  void drawPixel(int16_t x, int16_t y, lb_color_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, lb_color_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, lb_color_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, lb_color_t color);

  /* ── rectangles ──────────────────────────────────────────────────────── */
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, lb_color_t color);

  /* The most-used call in the whole API - across the 13 VGA examples it is used
   * more than every other primitive combined. Its speed is the library's
   * reputation, which is why the sub-byte path fills byte-aligned runs with
   * memset and only touches nibbles at the two ragged ends. */
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, lb_color_t color);

  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, lb_color_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, lb_color_t color);

  /* ── circles and triangles ───────────────────────────────────────────── */
  void drawCircle(int16_t cx, int16_t cy, int16_t r, lb_color_t color);
  void fillCircle(int16_t cx, int16_t cy, int16_t r, lb_color_t color);
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, lb_color_t color);
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, lb_color_t color);

  /* ── text ────────────────────────────────────────────────────────────── */
  void setFont(const LB_Font *font);
  const LB_Font *font() const { return _font; }

  /* One argument leaves the background alone. Two fills the glyph box, which is
   * what stops a changing number from leaving fragments of the previous one. */
  void setTextColor(lb_color_t fg);
  void setTextColor(lb_color_t fg, lb_color_t bg);

  /* Integer scaling only. A bitmap font at 2.5x looks like a mistake. */
  void setTextSize(uint8_t scale);

  /* UTF-8 in, always - even though v1 ships no CJK font. A font without a glyph
   * draws a placeholder box, so adding Chinese later is additive rather than a
   * breaking change (D5). Returns the x the cursor advanced to. */
  int16_t drawString(const char *str, int16_t x, int16_t y);

  /* Measure before you place. Never nudge an x coordinate until it looks right. */
  int16_t textWidth(const char *str) const;
  int16_t fontHeight() const;

  /* ── images ──────────────────────────────────────────────────────────── */

  /* Raw pixels, quantised like any other colour. On e-paper a photo comes out
   * dithered to two or four inks - the honest result, not a failure. */
  void drawRGB565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data);

  /* Baseline JPEG only. TJpgDec is vendored - the same decoder LovyanGFX uses,
   * one self-contained C file built for embedding. A progressive JPEG fails
   * SILENTLY and leaves a blank area, so the asset tool checks the SOF marker.
   * Measured on VGA: 59 ms for a full 320x240 screen, about 3.5 frames. */
  bool drawJpg(const uint8_t *data, uint32_t len, int16_t x, int16_t y, float scale = 1.0f);

  /* ── palette (paletted panels only) ──────────────────────────────────── */

  /* Changing the palette recolours everything already drawn without touching a
   * pixel. Measured on VGA 640x480: rotating 14 entries takes 5 us against
   * 282 ms to repaint the same picture. Returns false where the palette is
   * read-only, i.e. colour e-paper's fixed inks (D7). */
  bool setPaletteColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

  uint16_t paletteSize() const { return _panel.paletteSize(); }

  /* Escape hatch for the underlying device. Use it for anything this API does
   * not cover; the tutorials deliberately do not teach it. */
  LB_Panel &panel() { return _panel; }

protected:
  /*
   * Quantise once per shape, never per pixel (canvas.yaml section 3).
   * Direct-colour panels convert; paletted panels nearest-match. An indexed
   * colour on a direct-colour panel resolves to magenta rather than to
   * something plausible, so the mistake shows up on screen instead of hiding.
   */
  uint32_t resolve(lb_color_t color) const;

  /* Rotation is applied here, so no primitive and no panel has to think about
   * it. Everything below this point works in panel coordinates. */
  void rotate(int16_t &x, int16_t &y) const;

  /* The one place that writes pixels. Every filled primitive decomposes into
   * spans and goes through here, which is why there is exactly one piece of
   * per-format code to get right - and to benchmark. */
  void span(int16_t x, int16_t y, int16_t w, uint32_t raw);

  /* A single pixel, already in PANEL coordinates and already resolved. Used by
   * drawPixel and by the rotated-span path, both of which are off the fast
   * path anyway. */
  void writeRaw(int16_t px, int16_t py, uint32_t raw);

  LB_Panel &_panel;
  uint8_t _rotation = 0;

  const LB_Font *_font = &LB_Font8x16;
  lb_color_t _textFg = LB_WHITE;
  lb_color_t _textBg = 0;
  bool _textBgOpaque = false;
  uint8_t _textSize = 1;
};
