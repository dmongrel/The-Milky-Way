# Third-party notices

The Milky Way builds three pieces of third-party work into `the-milky-way.scr`: the Michroma
typeface, the volk Vulkan loader and NASA/ESA Hubble images.

## Michroma

Copyright 2011 The Michroma Project Authors (https://github.com/googlefonts/Michroma-font)

Licensed under the SIL Open Font License, Version 1.1. The full licence is
`assets/fonts/Michroma-OFL.txt`. It is embedded in the `.scr` as a resource next to the font,
because OFL section 2 requires the licence to travel with the font, and a licence left in the
repository would not reach anyone who has only the screen saver.

The font sets every on-screen overlay. The file is version 1.100,
taken unmodified from `google/fonts@main/ofl/michroma`:

    file    assets/fonts/Michroma-Regular.ttf
    sha256  b62301163788bc5b7f8fcac0b74b184e34e1827e577b499ecb724da065098f87
    bytes   64344

The font has not been subsetted, renamed or otherwise modified. It declares no Reserved Font Name.

## volk

Copyright (c) 2018-2026 Arseny Kapoulkine (https://github.com/zeux/volk). MIT License; the
licence text is `third_party/volk/LICENSE.md`. volk is vendored unmodified and compiled into the
`.scr`. It loads `vulkan-1.dll` at run time.

## NASA/ESA Hubble imagery

The background sky embeds two Hubble images. STScI treats Hubble imagery as public domain unless
noted, and asks for credit (https://hubblesite.org/copyright). Both originals, with their
sources, are kept in the comparison's shared `The Milky Way References/imagery/`.
`tools/prepare_sky.py` builds the embedded copies from them.

- `assets/sky/andromeda.jpg`: the Andromeda Galaxy, from the PHAST+PHAT mosaic (STScI 2025-005),
  cropped around the nucleus, faded to black inside the mosaic's edges, and resized to 2048 px.
  Credit: NASA, ESA, Benjamin F. Williams (UWashington), Zhuo Chen (UWashington), L. Clifton
  Johnson (Northwestern); Image Processing: Joseph DePasquale (STScI).
- `assets/sky/deep-field.jpg`: the Hubble eXtreme Deep Field (STScI 2012-37), unmodified.
  Credit: NASA, ESA, G. Illingworth, D. Magee, and P. Oesch (University of California, Santa
  Cruz), R. Bouwens (Leiden University), and the HUDF09 Team.
