# Lonely Binary GFX

The drawing layer shared by every Lonely Binary display: TFT over SPI, VGA over
the parallel bus, and e-paper.

You normally do not install this on purpose. You install the library for the
screen you bought — `Lonely Binary Display`, `Lonely Binary VGA`,
`Lonely Binary EPaper` — and this comes with it, already wired to your panel.

```cpp
#include <LonelyBinaryVGA.h>          // or LonelyBinaryDisplay.h, or ...EPaper.h

LB_VGA vga(LB_VGA_320x240);

void setup() {
  vga.begin();
  vga.fillScreen(LB_BLACK);
  vga.drawString("Hello", 10, 10);
  vga.flush();
}
void loop() {}
```

## Why it exists

Before this, the product line spoke three different graphics APIs: the TFT
library used Arduino_GFX, the VGA library used LovyanGFX, e-paper used GxEPD2 on
top of Adafruit_GFX. A sketch could not be moved between our own products, and
this function could not be written at all:

```cpp
void drawGauge(LB_Canvas &c, float value);   // TFT, VGA and e-paper, one body
```

That function is the whole point. Everything else follows from it.

Three other things fall out of owning the layer:

- **One API to learn, one to document.** Tutorials, `llms.txt` and support
  answers stop being per-product.
- **No GPL in the e-paper line.** GxEPD2 is GPL-3.0, which cannot be vendored
  into an MIT library and raises questions about shipping firmware. We already
  write our own e-paper panel drivers, so the dependency bought very little.
- **No third-party graphics dependency at all.** Every available engine carries
  a competitor's copyright notice — Arduino_GFX's licence file is verbatim
  Adafruit's, and LovyanGFX records incorporated Adafruit_GFX and TFT_eSPI code.

## The contract

[`canvas.yaml`](canvas.yaml) is the source of truth, frozen at version 1. It
defines every operation, both language spellings, and the reasoning behind each
decision — including the ones decided against. This header set is reviewed
against it; where they disagree, the YAML is right.

The documentation and `llms.txt` are generated from it. The C++ header is not:
thirty signatures for a single target does not justify a generator.

## Colour

Every call takes an `lb_color_t` — logically 24-bit RGB — and each panel
quantises it to what it can actually show.

```cpp
vga.fillRect(0, 0, 100, 50, LB_RED);
vga.fillRect(0, 0, 100, 50, LB_RGB(255, 128, 0));
```

That is what makes one sketch run everywhere: a TFT shows it directly, a
16-colour VGA nearest-matches it, a black-and-white e-paper thresholds it. The
quantisation happens **once per shape, never per pixel**, so it costs nothing
measurable.

For palette-cycling effects, or when you own the palette, `LB_INDEX(i)` names an
entry directly and skips the matching.

## Always call `flush()`

| Device | What `flush()` costs |
|---|---|
| TFT, straight to the bus | nothing, the pixels already went |
| TFT with a framebuffer | a few milliseconds |
| VGA | nothing, the scan-out interrupt reads the buffer |
| **e-paper** | **1.5 s mono, 20–45 s colour. It blocks.** |

It is a no-op where it is not needed, and writing it every time is the only way
a portable sketch can exist. `supportsPartial()` tells you whether
`flush(LB_FLUSH_PARTIAL)` will do anything; where it will not, it silently does
a full refresh — which on a colour e-paper takes tens of seconds and looks
exactly like a hang, so ask first if that matters to you.

## Do not write per-pixel loops

True of every graphics library, and worst on the sub-byte formats where one
pixel is a read-modify-write of half a byte. Measured on VGA: 0.92 µs per pixel,
282 ms for a full 640×480 screen — and most of that is per-call overhead, not
the pixel. Use `fillRect`, `drawString` and `drawJpg`, which fill whole bytes.

## Writing a device library

Implement `LB_Panel` — geometry, pixel format, buffer, stride, `flush()` — and
subclass `LB_Canvas` holding that panel as a member. The device owns the buffer;
the canvas only draws into memory it was handed. On VGA the scan-out interrupt
reads that exact memory, and a TFT without PSRAM has no buffer at all, so it
could not be the other way round.

## Licence

MIT. See [LICENSE](LICENSE).
