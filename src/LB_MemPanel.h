#pragma once
/*
 * LB_MemPanel - a panel that is just memory.
 *
 * Draw into RAM with the same API as a real display, then do what you like with
 * the result: copy it to a device's framebuffer, checksum it, dump it over
 * serial. There is no hardware behind it, so flush() does nothing.
 *
 *     LB_MemPanel back(320, 240, LB_FMT_RGB565, true);  // true = PSRAM
 *     LB_Canvas c(back);
 *     c.fillScreen(LB_BLACK);
 *     c.drawJpg(photo, len, 0, 0);          // slow, but nobody is watching
 *     memcpy(vga.panel().buffer(), back.buffer(), back.sizeBytes());
 *
 * canvas.yaml D6 put off-screen canvases in v1.1, on the grounds that the
 * device owns the buffer and one does not fall out of v1 for free. This is the
 * narrow version of that: a panel you point at your own memory. It is fifty
 * lines and it unblocks two real jobs - page flipping, and testing the drawing
 * code with no display attached at all - so it ships early. What is still v1.1
 * is compositing: no pushing one canvas into another, no transparency.
 *
 * !! Choose PSRAM deliberately !!
 *   A back buffer in PSRAM is read once per flip and is fine there. A DISPLAY
 *   framebuffer in PSRAM is read tens of megabytes per second, forever, and
 *   will not work. See the VGA library for what that looks like.
 */
#include <stdlib.h>
#include "LB_Panel.h"

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

class LB_MemPanel : public LB_Panel
{
public:
  LB_MemPanel(int16_t w, int16_t h, lb_format_t fmt, bool psram = false)
      : _w(w), _h(h), _f(fmt)
  {
    _stride = ((uint32_t)w * lb_format_bpp(fmt) + 7) / 8;
#if defined(ESP32)
    _buf = (uint8_t *)heap_caps_calloc(
        1, _stride * h,
        psram ? MALLOC_CAP_SPIRAM : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
    (void)psram;
    _buf = (uint8_t *)calloc(1, _stride * h);
#endif
    if (lb_format_is_paletted(fmt))
      for (int i = 0; i < 16; i++) _pal[i] = LB_RGB(i * 17, i * 17, i * 17);
  }
  ~LB_MemPanel() override { free(_buf); }

  bool ok() const { return _buf != nullptr; }
  size_t sizeBytes() const { return (size_t)_stride * _h; }

  int16_t panelWidth() const override { return _w; }
  int16_t panelHeight() const override { return _h; }
  lb_format_t format() const override { return _f; }
  uint8_t *buffer() const override { return _buf; }
  uint32_t stride() const override { return _stride; }
  void flush(lb_flush_t = LB_FLUSH_FULL) override {}

  uint16_t paletteSize() const override { return lb_format_is_paletted(_f) ? 16 : 0; }
  const lb_color_t *palette() const override
  {
    return lb_format_is_paletted(_f) ? _pal : nullptr;
  }
  bool setPaletteColor(uint8_t i, uint8_t r, uint8_t g, uint8_t b) override
  {
    if (i > 15 || !lb_format_is_paletted(_f)) return false;
    _pal[i] = LB_RGB(r, g, b);
    return true;
  }

private:
  int16_t _w, _h;
  lb_format_t _f;
  uint32_t _stride = 0;
  uint8_t *_buf = nullptr;
  lb_color_t _pal[16] = {0};
};
