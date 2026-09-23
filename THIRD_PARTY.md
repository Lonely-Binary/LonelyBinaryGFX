# Third-party code and fonts

Everything here is permissively licensed and cleared for commercial, closed
products. Nothing is GPL, and nothing carries a competitor's copyright — which
was one of the reasons for writing this library rather than adopting an existing
engine. Retain these notices when redistributing.

## TJpgDec — `src/tjpgd/`

Baseline JPEG decoder, R0.03 (2021), by ChaN.

`tjpgd.c` and `tjpgd.h` are vendored **unmodified**. `tjpgdcnf.h` — the
configuration header the author intends you to edit — has two values changed:
`JD_FORMAT` 0 → 1 (RGB565 output, which every panel here wants) and
`JD_FASTDECODE` 0 → 1 (the 32-bit code path; 0 is the 8/16-bit setting).
The header records both changes and why.

> Copyright (C) 2021, ChaN, all right reserved.
>
> * The TJpgDec module is a free software and there is NO WARRANTY.
> * No restriction on use. You can use, modify and redistribute it for
>   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
> * Redistributions of source code must retain the above copyright notice.

Source: <http://elm-chan.org/fsw/tjpgd/>

Baseline JPEG only. A progressive JPEG fails **silently** and leaves a blank
area, which is why the asset tooling checks the SOF marker (0xC0 baseline,
0xC2 progressive) on every conversion.

## Spleen — `src/LB_Fonts.cpp`

Bitmap fonts, by Frederic Cambus. BSD 2-Clause. Converted from the upstream BDF
files by `tools/gen_fonts.py`; the glyph data is unmodified.

> Copyright (c) 2018-2026, Frederic Cambus
> All rights reserved.
>
> Redistribution and use in source and binary forms, with or without
> modification, are permitted provided that redistributions retain the above
> copyright notice, this list of conditions and the following disclaimer.

Source: <https://github.com/fcambus/spleen>

## Planned: efont — CJK

When Chinese ships, the glyphs will come from efont (The Electronic Font Open
Laboratory), **BSD 3-Clause**.

That choice is deliberate. The other obvious CJK bitmap fonts — GNU Unifont and
WenQuanYi — are GPL with a font-embedding exception, and that exception is
written about embedding a font in a *document*. Whether firmware counts as a
document is a grey area, and not one worth entering when a BSD-licensed font of
equal quality exists.
