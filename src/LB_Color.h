#pragma once
/*
 * LB_Color.h - one colour type for every Lonely Binary display.
 *
 * Contract: canvas.yaml, section 3.
 *
 * Every drawing call takes an lb_color_t. It is logically 24-bit RGB, and each
 * canvas quantises it to whatever its panel can actually show: straight through
 * on a TFT, nearest-match on a 16-colour VGA, threshold on a black-and-white
 * e-paper.
 *
 * The alternative - a colour type per device - means a sketch written for a TFT
 * does not compile for e-paper, which defeats the point of having one family.
 * Taking RGB and quantising means one sketch runs everywhere and simply looks
 * coarser on coarser hardware.
 *
 * The quantisation happens ONCE PER SHAPE, never per pixel: fillRect() resolves
 * the colour before it touches a single byte, so a nearest-match against a
 * 16-entry palette costs nothing measurable.
 */
#include <stdint.h>

/*
 * 0x00RRGGBB, or a palette index when the top bit is set.
 *
 * The tag bit is why this is uint32_t and not uint24-ish: a paletted canvas has
 * to be able to tell "the colour #101010" from "palette entry 0x101010 & 0xFF",
 * and an in-band tag is the only way to do that without a second argument on
 * every call.
 */
typedef uint32_t lb_color_t;

#define LB_COLOR_INDEX_FLAG 0x80000000u

/* Build a colour from components. constexpr, so the named constants below cost
 * nothing at runtime. */
static inline constexpr lb_color_t LB_RGB(uint8_t r, uint8_t g, uint8_t b)
{
  return ((lb_color_t)r << 16) | ((lb_color_t)g << 8) | (lb_color_t)b;
}

/*
 * The escape hatch, for paletted canvases only: "palette entry i", with no
 * matching and no quantisation. This is what palette-cycling effects use, and
 * what code that owns the palette uses.
 *
 * On a direct-colour canvas an indexed colour is meaningless. Rather than
 * silently drawing some arbitrary shade, the canvas resolves it to LB_MAGENTA
 * so the mistake is visible on screen rather than subtle.
 */
static inline constexpr lb_color_t LB_INDEX(uint8_t i)
{
  return LB_COLOR_INDEX_FLAG | (lb_color_t)i;
}

static inline constexpr bool lb_color_is_index(lb_color_t c)
{
  return (c & LB_COLOR_INDEX_FLAG) != 0;
}

static inline constexpr uint8_t lb_color_r(lb_color_t c) { return (uint8_t)(c >> 16); }
static inline constexpr uint8_t lb_color_g(lb_color_t c) { return (uint8_t)(c >> 8); }
static inline constexpr uint8_t lb_color_b(lb_color_t c) { return (uint8_t)c; }

/*
 * A deliberately short list.
 *
 * These have to still mean something after quantisation to two inks on a
 * black-and-white e-paper. A long catalogue of subtle shades does not survive
 * that, and offering one would promise something the hardware cannot keep.
 */
static constexpr lb_color_t LB_BLACK     = LB_RGB(0, 0, 0);
static constexpr lb_color_t LB_WHITE     = LB_RGB(255, 255, 255);
static constexpr lb_color_t LB_RED       = LB_RGB(255, 0, 0);
static constexpr lb_color_t LB_GREEN     = LB_RGB(0, 255, 0);
static constexpr lb_color_t LB_BLUE      = LB_RGB(0, 0, 255);
static constexpr lb_color_t LB_YELLOW    = LB_RGB(255, 255, 0);
static constexpr lb_color_t LB_CYAN      = LB_RGB(0, 255, 255);
static constexpr lb_color_t LB_MAGENTA   = LB_RGB(255, 0, 255);
static constexpr lb_color_t LB_ORANGE    = LB_RGB(255, 140, 0);
static constexpr lb_color_t LB_GRAY      = LB_RGB(128, 128, 128);
static constexpr lb_color_t LB_DARKGRAY  = LB_RGB(64, 64, 64);
static constexpr lb_color_t LB_LIGHTGRAY = LB_RGB(192, 192, 192);

/* ---- conversions used by the quantisers ---- */

static inline constexpr uint16_t lb_to_rgb565(lb_color_t c)
{
  return (uint16_t)(((lb_color_r(c) & 0xF8) << 8) |
                    ((lb_color_g(c) & 0xFC) << 3) |
                    (lb_color_b(c) >> 3));
}

/*
 * Perceptual weights, not a plain average. Pure blue and pure yellow have
 * wildly different apparent brightness, and on a two-ink e-paper the difference
 * decides whether text is readable at all.
 */
static inline constexpr uint8_t lb_luma(lb_color_t c)
{
  return (uint8_t)((lb_color_r(c) * 77 + lb_color_g(c) * 150 + lb_color_b(c) * 29) >> 8);
}
