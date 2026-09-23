/*
 * LB_Canvas.cpp - the shared drawing implementation.
 *
 * Contract: canvas.yaml v1.
 *
 * Structure, and the reason for it: every filled primitive decomposes into
 * horizontal spans and goes through span(). That is the ONLY function that
 * knows about pixel formats, so there is exactly one piece of per-format code
 * to get right, to benchmark, and to look at when something is slow.
 *
 * The 4bpp path in span() is the one that was benchmarked against LovyanGFX
 * before any of this was written (Internal/LonelyBinaryVGA/代码/FillBench):
 *   byte-identical output, and 0.95x the speed on random rectangles, 0.93x on
 *   full-screen fills. Five to seven percent slower than an engine that has
 *   been optimised for years, which says the limit is memory bandwidth rather
 *   than cleverness - memset is already as good as it gets.
 */
#include "LB_Canvas.h"
#include <string.h>
#include <stdlib.h>

/* ────────────────────────────── geometry ─────────────────────────────── */

int16_t LB_Canvas::width() const
{
  if (_panel.hardwareRotation()) return _panel.panelWidth();
  return (_rotation & 1) ? _panel.panelHeight() : _panel.panelWidth();
}

int16_t LB_Canvas::height() const
{
  if (_panel.hardwareRotation()) return _panel.panelHeight();
  return (_rotation & 1) ? _panel.panelWidth() : _panel.panelHeight();
}

void LB_Canvas::setRotation(uint8_t r)
{
  _rotation = r & 3;
  if (_panel.hardwareRotation()) _panel.setPanelRotation(_rotation);
}

/* Canvas coordinates to panel coordinates. Only called when the panel does not
 * rotate for us. */
void LB_Canvas::rotate(int16_t &x, int16_t &y) const
{
  const int16_t pw = _panel.panelWidth();
  const int16_t ph = _panel.panelHeight();
  int16_t nx = x, ny = y;
  switch (_rotation)
  {
  case 1: nx = (int16_t)(pw - 1 - y); ny = x; break;
  case 2: nx = (int16_t)(pw - 1 - x); ny = (int16_t)(ph - 1 - y); break;
  case 3: nx = y; ny = (int16_t)(ph - 1 - x); break;
  default: break;
  }
  x = nx;
  y = ny;
}

/* ────────────────────────────── colour ───────────────────────────────── */

uint32_t LB_Canvas::resolve(lb_color_t color) const
{
  const lb_format_t f = _panel.format();

  if (lb_color_is_index(color))
  {
    if (!lb_format_is_paletted(f))
    {
      /* An index means nothing here. Draw it as magenta so the mistake is
       * visible on screen rather than quietly becoming some plausible shade. */
      return lb_to_rgb565(LB_MAGENTA);
    }
    const uint16_t n = _panel.paletteSize();
    const uint32_t i = color & 0xFF;
    return n ? (i % n) : 0;
  }

  if (f == LB_FMT_RGB565) return lb_to_rgb565(color);

  /* Paletted: nearest match. Resolved once per shape, never per pixel, so a
   * linear scan over at most sixteen entries costs nothing measurable. */
  const lb_color_t *pal = _panel.palette();
  const uint16_t n = _panel.paletteSize();
  if (!pal || !n) return 0;

  const int16_t r = lb_color_r(color), g = lb_color_g(color), b = lb_color_b(color);
  uint32_t best = 0;
  int32_t bestD = INT32_MAX;
  for (uint16_t i = 0; i < n; i++)
  {
    /* Weighted, not plain Euclidean. An unweighted distance picks visibly wrong
     * inks on a four-colour e-paper, where the choices are far apart. */
    const int32_t dr = r - (int16_t)lb_color_r(pal[i]);
    const int32_t dg = g - (int16_t)lb_color_g(pal[i]);
    const int32_t db = b - (int16_t)lb_color_b(pal[i]);
    const int32_t d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
    if (d < bestD) { bestD = d; best = i; }
  }
  return best;
}

/* ────────────────────────── the only pixel writer ────────────────────── */

void LB_Canvas::span(int16_t x, int16_t y, int16_t w, uint32_t raw)
{
  if (w <= 0) return;

  /* Clip in canvas space first, so every caller can be sloppy. */
  const int16_t cw = width(), ch = height();
  if (y < 0 || y >= ch) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > cw) w = cw - x;
  if (w <= 0) return;

  const bool soft = !_panel.hardwareRotation();

  /* At rotation 1 and 3 a horizontal span in canvas space is a VERTICAL run in
   * panel memory, so the fast path does not apply and it goes pixel by pixel.
   * Documented in LB_Panel::hardwareRotation(); it is the price of rotating a
   * framebuffer panel in software. */
  if (soft && (_rotation & 1))
  {
    for (int16_t i = 0; i < w; i++)
    {
      int16_t px = (int16_t)(x + i), py = y;
      rotate(px, py);
      writeRaw(px, py, raw);
    }
    return;
  }

  int16_t px = x, py = y;
  if (soft)
  {
    /* Rotation 2 mirrors the run; map its left end and start from there. */
    int16_t ex = (int16_t)(x + w - 1), ey = y;
    rotate(px, py);
    rotate(ex, ey);
    if (ex < px) px = ex;
    py = ey;
  }

  uint8_t *buf = _panel.buffer();
  if (!buf)
  {
    /* No framebuffer: a TFT on a board without PSRAM. Hand the run to the
     * panel, which pushes it over the bus. */
    _panel.writeSpan(px, py, w, (lb_color_t)raw);
    return;
  }

  uint8_t *row = buf + (size_t)py * _panel.stride();

  switch (_panel.format())
  {
  case LB_FMT_RGB565:
  {
    uint16_t *p = (uint16_t *)row + px;
    const uint16_t v = (uint16_t)raw;
    for (int16_t i = 0; i < w; i++) p[i] = v;
    break;
  }

  case LB_FMT_PAL4:
  {
    /* Benchmarked path. Two pixels per byte, LEFT pixel in the HIGH nibble.
     * Fill the byte-aligned middle with memset and touch a nibble only at the
     * two ragged ends - writing this per-pixel instead is 57x slower. */
    const uint8_t c = (uint8_t)(raw & 0x0F);
    const uint8_t both = (uint8_t)((c << 4) | c);
    const int xEnd = px + w;
    const int byteFirst = (px + 1) >> 1;
    const int byteLast = xEnd >> 1;

    if (px & 1) row[px >> 1] = (uint8_t)((row[px >> 1] & 0xF0) | c);
    if (byteLast > byteFirst) memset(row + byteFirst, both, (size_t)(byteLast - byteFirst));
    if (xEnd & 1) row[xEnd >> 1] = (uint8_t)((row[xEnd >> 1] & 0x0F) | (uint8_t)(c << 4));
    break;
  }

  case LB_FMT_PAL2:
  case LB_FMT_PAL1:
  {
    /* Deliberately the simple version.
     *
     * These are e-paper formats, where a single flush() costs 1.5 to 45
     * SECONDS. Shaving microseconds off the fill is invisible next to that, and
     * a generic bit-twiddling loop is much easier to prove correct than four
     * hand-tuned ones. If an e-paper panel ever turns out to be fill-bound,
     * this is where to look - but measure before assuming it. */
    const uint8_t bpp = lb_format_bpp(_panel.format());
    const uint8_t perByte = (uint8_t)(8 / bpp);
    const uint8_t mask = (uint8_t)((1u << bpp) - 1);
    const uint8_t v = (uint8_t)(raw & mask);
    for (int16_t i = 0; i < w; i++)
    {
      const int xi = px + i;
      uint8_t *p = row + xi / perByte;
      const uint8_t shift = (uint8_t)(8 - bpp - (xi % perByte) * bpp);
      *p = (uint8_t)((*p & ~(mask << shift)) | (v << shift));
    }
    break;
  }
  }
}

/* One pixel, already in panel coordinates. Only used by the rotated-span path
 * and by drawPixel, so it can afford to be the slow way round. */
void LB_Canvas::writeRaw(int16_t px, int16_t py, uint32_t raw)
{
  if (px < 0 || py < 0 || px >= _panel.panelWidth() || py >= _panel.panelHeight()) return;
  uint8_t *buf = _panel.buffer();
  if (!buf) { _panel.writeSpan(px, py, 1, (lb_color_t)raw); return; }

  uint8_t *row = buf + (size_t)py * _panel.stride();
  const lb_format_t f = _panel.format();
  if (f == LB_FMT_RGB565) { ((uint16_t *)row)[px] = (uint16_t)raw; return; }

  const uint8_t bpp = lb_format_bpp(f);
  const uint8_t perByte = (uint8_t)(8 / bpp);
  const uint8_t mask = (uint8_t)((1u << bpp) - 1);
  uint8_t *p = row + px / perByte;
  const uint8_t shift = (uint8_t)(8 - bpp - (px % perByte) * bpp);
  *p = (uint8_t)((*p & ~(mask << shift)) | ((raw & mask) << shift));
}

/* ─────────────────────────── primitives ──────────────────────────────── */

void LB_Canvas::fillScreen(lb_color_t color)
{
  const uint32_t raw = resolve(color);
  const int16_t h = height(), w = width();
  for (int16_t y = 0; y < h; y++) span(0, y, w, raw);
}

void LB_Canvas::drawPixel(int16_t x, int16_t y, lb_color_t color)
{
  if (x < 0 || y < 0 || x >= width() || y >= height()) return;
  int16_t px = x, py = y;
  if (!_panel.hardwareRotation()) rotate(px, py);
  writeRaw(px, py, resolve(color));
}

void LB_Canvas::drawFastHLine(int16_t x, int16_t y, int16_t w, lb_color_t color)
{
  span(x, y, w, resolve(color));
}

void LB_Canvas::drawFastVLine(int16_t x, int16_t y, int16_t h, lb_color_t color)
{
  const uint32_t raw = resolve(color);
  for (int16_t i = 0; i < h; i++) span(x, (int16_t)(y + i), 1, raw);
}

void LB_Canvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, lb_color_t color)
{
  const uint32_t raw = resolve(color); /* once per shape, never per pixel */
  for (int16_t i = 0; i < h; i++) span(x, (int16_t)(y + i), w, raw);
}

void LB_Canvas::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, lb_color_t color)
{
  if (w <= 0 || h <= 0) return;
  const uint32_t raw = resolve(color);
  span(x, y, w, raw);
  span(x, (int16_t)(y + h - 1), w, raw);
  for (int16_t i = 1; i < h - 1; i++)
  {
    span(x, (int16_t)(y + i), 1, raw);
    span((int16_t)(x + w - 1), (int16_t)(y + i), 1, raw);
  }
}

void LB_Canvas::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, lb_color_t color)
{
  /* Horizontal and vertical lines are extremely common - axes, grids, borders -
   * and going through the span path instead of Bresenham is worth the branch. */
  if (y0 == y1) { span(x0 < x1 ? x0 : x1, y0, (int16_t)(abs(x1 - x0) + 1), resolve(color)); return; }
  if (x0 == x1) { drawFastVLine(x0, y0 < y1 ? y0 : y1, (int16_t)(abs(y1 - y0) + 1), color); return; }

  const uint32_t raw = resolve(color);
  int16_t dx = (int16_t)abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int16_t dy = (int16_t)-abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int32_t err = dx + dy;
  for (;;)
  {
    span(x0, y0, 1, raw);
    if (x0 == x1 && y0 == y1) break;
    const int32_t e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 = (int16_t)(x0 + sx); }
    if (e2 <= dx) { err += dx; y0 = (int16_t)(y0 + sy); }
  }
}

void LB_Canvas::drawCircle(int16_t cx, int16_t cy, int16_t r, lb_color_t color)
{
  if (r < 0) return;
  const uint32_t raw = resolve(color);
  int16_t x = 0, y = r;
  int16_t d = (int16_t)(1 - r);
  while (x <= y)
  {
    const int16_t pts[8][2] = {
        {(int16_t)(cx + x), (int16_t)(cy + y)}, {(int16_t)(cx - x), (int16_t)(cy + y)},
        {(int16_t)(cx + x), (int16_t)(cy - y)}, {(int16_t)(cx - x), (int16_t)(cy - y)},
        {(int16_t)(cx + y), (int16_t)(cy + x)}, {(int16_t)(cx - y), (int16_t)(cy + x)},
        {(int16_t)(cx + y), (int16_t)(cy - x)}, {(int16_t)(cx - y), (int16_t)(cy - x)}};
    for (int i = 0; i < 8; i++) span(pts[i][0], pts[i][1], 1, raw);
    if (d < 0) d = (int16_t)(d + 2 * x + 3);
    else { d = (int16_t)(d + 2 * (x - y) + 5); y--; }
    x++;
  }
}

void LB_Canvas::fillCircle(int16_t cx, int16_t cy, int16_t r, lb_color_t color)
{
  if (r < 0) return;
  const uint32_t raw = resolve(color);
  /* One span per scan line rather than eight symmetric points - a filled circle
   * is a stack of horizontal runs, which is exactly what span() is fast at. */
  for (int16_t dy = (int16_t)-r; dy <= r; dy++)
  {
    const int32_t dx = (int32_t)(0.5f + __builtin_sqrtf((float)(r * r - dy * dy)));
    span((int16_t)(cx - dx), (int16_t)(cy + dy), (int16_t)(2 * dx + 1), raw);
  }
}

void LB_Canvas::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, lb_color_t color)
{
  if (w <= 0 || h <= 0) return;
  if (r > w / 2) r = (int16_t)(w / 2);
  if (r > h / 2) r = (int16_t)(h / 2);
  const uint32_t raw = resolve(color);
  span((int16_t)(x + r), y, (int16_t)(w - 2 * r), raw);
  span((int16_t)(x + r), (int16_t)(y + h - 1), (int16_t)(w - 2 * r), raw);
  for (int16_t i = r; i < h - r; i++)
  {
    span(x, (int16_t)(y + i), 1, raw);
    span((int16_t)(x + w - 1), (int16_t)(y + i), 1, raw);
  }
  /* Four quarter arcs, walked once and mirrored. */
  int16_t cx = 0, cy = r, d = (int16_t)(1 - r);
  while (cx <= cy)
  {
    const int16_t off[4][2] = {{cx, cy}, {cy, cx}, {cx, cy}, {cy, cx}};
    for (int q = 0; q < 2; q++)
    {
      const int16_t ox = off[q][0], oy = off[q][1];
      span((int16_t)(x + r - ox), (int16_t)(y + r - oy), 1, raw);
      span((int16_t)(x + w - 1 - r + ox), (int16_t)(y + r - oy), 1, raw);
      span((int16_t)(x + r - ox), (int16_t)(y + h - 1 - r + oy), 1, raw);
      span((int16_t)(x + w - 1 - r + ox), (int16_t)(y + h - 1 - r + oy), 1, raw);
    }
    if (d < 0) d = (int16_t)(d + 2 * cx + 3);
    else { d = (int16_t)(d + 2 * (cx - cy) + 5); cy--; }
    cx++;
  }
}

void LB_Canvas::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, lb_color_t color)
{
  if (w <= 0 || h <= 0) return;
  if (r > w / 2) r = (int16_t)(w / 2);
  if (r > h / 2) r = (int16_t)(h / 2);
  const uint32_t raw = resolve(color);
  for (int16_t i = 0; i < h; i++)
  {
    int16_t inset = 0;
    if (i < r)
    {
      const int16_t dy = (int16_t)(r - i);
      inset = (int16_t)(r - (int16_t)(0.5f + __builtin_sqrtf((float)(r * r - dy * dy))));
    }
    else if (i >= h - r)
    {
      const int16_t dy = (int16_t)(i - (h - 1 - r));
      inset = (int16_t)(r - (int16_t)(0.5f + __builtin_sqrtf((float)(r * r - dy * dy))));
    }
    span((int16_t)(x + inset), (int16_t)(y + i), (int16_t)(w - 2 * inset), raw);
  }
}

void LB_Canvas::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             int16_t x2, int16_t y2, lb_color_t color)
{
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

void LB_Canvas::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             int16_t x2, int16_t y2, lb_color_t color)
{
  /* Sort by y, then walk scan lines emitting one span each. */
  if (y0 > y1) { int16_t t; t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y1 > y2) { int16_t t; t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if (y0 > y1) { int16_t t; t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y0 == y2) return; /* degenerate: a horizontal line, nothing to fill */

  const uint32_t raw = resolve(color);
  for (int16_t y = y0; y <= y2; y++)
  {
    const bool upper = (y < y1);
    const int16_t ya = upper ? y0 : y1, yb = upper ? y1 : y2;
    const int16_t xa = upper ? x0 : x1, xb = upper ? x1 : x2;
    const int16_t den = (int16_t)(yb - ya);
    const int32_t sa = den ? (int32_t)(xb - xa) * (y - ya) / den : 0;
    const int32_t sb = (int32_t)(x2 - x0) * (y - y0) / (y2 - y0);
    int16_t left = (int16_t)(xa + sa), right = (int16_t)(x0 + sb);
    if (left > right) { const int16_t t = left; left = right; right = t; }
    span(left, y, (int16_t)(right - left + 1), raw);
  }
}

/* ─────────────────────────────── images ──────────────────────────────── */

void LB_Canvas::drawRGB565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data)
{
  /* No span fast path here: every pixel can be a different colour, so this is
   * genuinely per-pixel work whatever we do. On a paletted panel each pixel
   * also costs a nearest-match, which is why a photo on 16-colour VGA or on
   * e-paper is noticeably slower than the same photo on a TFT. */
  for (int16_t j = 0; j < h; j++)
    for (int16_t i = 0; i < w; i++)
    {
      const uint16_t c = data[(size_t)j * w + i];
      const lb_color_t rgb = LB_RGB((uint8_t)((c >> 8) & 0xF8),
                                    (uint8_t)((c >> 3) & 0xFC),
                                    (uint8_t)((c << 3) & 0xF8));
      drawPixel((int16_t)(x + i), (int16_t)(y + j), rgb);
    }
}

/* ────────────────────────────── palette ──────────────────────────────── */

bool LB_Canvas::setPaletteColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
  return _panel.setPaletteColor(index, r, g, b);
}

/* ─────────────────────────────── text ────────────────────────────────── */

void LB_Canvas::setFont(const LB_Font *font) { if (font) _font = font; }
void LB_Canvas::setTextColor(lb_color_t fg) { _textFg = fg; _textBgOpaque = false; }
void LB_Canvas::setTextColor(lb_color_t fg, lb_color_t bg)
{
  _textFg = fg; _textBg = bg; _textBgOpaque = true;
}
void LB_Canvas::setTextSize(uint8_t scale) { _textSize = scale ? scale : 1; }

int16_t LB_Canvas::fontHeight() const
{
  return (int16_t)(_font ? _font->height * _textSize : 0);
}

/*
 * Minimal UTF-8 decoder: returns the codepoint and advances the pointer.
 *
 * It is here in v1 even though v1 ships only ASCII fonts (D5). Accepting UTF-8
 * from day one is what makes adding Chinese later additive instead of a
 * breaking change - a sketch that passes a UTF-8 string keeps working, it just
 * starts rendering real glyphs instead of placeholder boxes.
 *
 * Malformed input yields 0xFFFD and consumes one byte, so a bad string can
 * never spin forever.
 */
static uint32_t utf8_next(const char **s)
{
  const uint8_t *p = (const uint8_t *)*s;
  uint8_t c = *p;
  if (c < 0x80) { *s += 1; return c; }
  uint32_t cp;
  int extra;
  if ((c & 0xE0) == 0xC0)      { cp = c & 0x1F; extra = 1; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
  else                         { *s += 1; return 0xFFFD; }
  for (int i = 1; i <= extra; i++)
  {
    if ((p[i] & 0xC0) != 0x80) { *s += 1; return 0xFFFD; }
    cp = (cp << 6) | (p[i] & 0x3F);
  }
  *s += extra + 1;
  return cp;
}

int16_t LB_Canvas::textWidth(const char *str) const
{
  if (!str || !_font) return 0;
  int16_t n = 0;
  const char *p = str;
  while (*p) { utf8_next(&p); n++; }
  return (int16_t)(n * _font->width * _textSize);
}

int16_t LB_Canvas::drawString(const char *str, int16_t x, int16_t y)
{
  if (!str || !_font) return x;

  const uint32_t fg = resolve(_textFg);
  const uint32_t bg = resolve(_textBg);
  const uint8_t gw = _font->width, gh = _font->height, bpr = _font->bytesPerRow;
  const uint8_t s = _textSize;
  const uint16_t glyphBytes = (uint16_t)gh * bpr;

  const char *p = str;
  while (*p)
  {
    const uint32_t cp = utf8_next(&p);

    /* A codepoint this font does not have draws a hollow box. Silently skipping
     * it would make a missing font look like a missing string, which is a much
     * harder bug to report. */
    const bool have = (cp >= _font->first && cp <= _font->last);
    const uint8_t *g = have ? _font->data + (size_t)(cp - _font->first) * glyphBytes : nullptr;

    for (uint8_t row = 0; row < gh; row++)
    {
      /* Accumulate a run of same-coloured pixels and emit it as one span,
       * rather than calling into span() once per pixel. On a 4bpp panel that is
       * the difference between memset and a nibble read-modify-write per pixel. */
      int16_t runStart = -1;
      uint32_t runColor = 0;
      for (uint8_t col = 0; col <= gw; col++)
      {
        bool on = false;
        if (col < gw)
        {
          if (have) on = (g[row * bpr + (col >> 3)] >> (7 - (col & 7))) & 1;
          else      on = (row == 0 || row == gh - 1 || col == 0 || col == gw - 1);
        }
        const bool paint = (col < gw) && (on || _textBgOpaque);
        const uint32_t c = on ? fg : bg;

        if (paint && runStart >= 0 && c == runColor) continue;
        if (runStart >= 0)
        {
          const int16_t x0 = (int16_t)(x + runStart * s);
          const int16_t w0 = (int16_t)((col - runStart) * s);
          for (uint8_t sy = 0; sy < s; sy++)
            span(x0, (int16_t)(y + row * s + sy), w0, runColor);
          runStart = -1;
        }
        if (paint) { runStart = col; runColor = c; }
      }
    }
    x = (int16_t)(x + gw * s);
  }
  return x;
}

/* ─────────────────────────────── JPEG ────────────────────────────────── */

extern "C" {
#include "tjpgd/tjpgd.h"
}

namespace {
struct JpgCtx
{
  const uint8_t *data;
  uint32_t len, pos;
  LB_Canvas *canvas;
  int16_t x, y;
};

size_t jpgIn(JDEC *jd, uint8_t *buf, size_t n)
{
  JpgCtx *c = (JpgCtx *)jd->device;
  const uint32_t avail = c->len - c->pos;
  if (n > avail) n = avail;
  if (buf) memcpy(buf, c->data + c->pos, n); /* buf == nullptr means "skip" */
  c->pos += n;
  return n;
}

int jpgOut(JDEC *jd, void *bitmap, JRECT *r)
{
  JpgCtx *c = (JpgCtx *)jd->device;
  c->canvas->drawRGB565((int16_t)(c->x + r->left), (int16_t)(c->y + r->top),
                        (int16_t)(r->right - r->left + 1),
                        (int16_t)(r->bottom - r->top + 1),
                        (const uint16_t *)bitmap);
  return 1;
}
} /* namespace */

bool LB_Canvas::drawJpg(const uint8_t *data, uint32_t len, int16_t x, int16_t y, float scale)
{
  if (!data || !len) return false;

  /*
   * !! Baseline JPEG only !!
   *   TJpgDec cannot read a progressive JPEG. It does not crash - it returns an
   *   error and leaves the area untouched, which looks exactly like "nothing
   *   was drawn". The asset tooling checks the SOF marker (0xC0 baseline,
   *   0xC2 progressive) so that failure never reaches a customer.
   *
   * !! Scale is 1, 1/2, 1/4 or 1/8 - nothing in between !!
   *   The float argument keeps the call site readable, but the decoder only
   *   descales by powers of two. 0.5 is exact; 0.3 rounds DOWN to 1/4, which is
   *   smaller than asked for rather than larger, so a layout never overflows
   *   its box.
   */
  uint8_t sc = 0;
  if (scale <= 0.125f) sc = 3;
  else if (scale <= 0.25f) sc = 2;
  else if (scale <= 0.5f) sc = 1;

  /* The work area goes on the heap, not the stack: TJpgDec wants about 3 KB and
   * the Arduino loop task only has 8 KB to begin with. */
  const size_t poolSize = 3600;
  void *pool = malloc(poolSize);
  if (!pool) return false;

  JpgCtx ctx = {data, len, 0, this, x, y};
  JDEC jd;
  bool ok = false;
  if (jd_prepare(&jd, jpgIn, pool, poolSize, &ctx) == JDR_OK)
    ok = (jd_decomp(&jd, jpgOut, sc) == JDR_OK);

  free(pool);
  return ok;
}
