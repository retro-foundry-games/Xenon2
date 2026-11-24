# Xenon 2 SDL3

Build the desktop port with `make sdl`. Run it with the two disk images,
`xenon2-disk1.adf` and `xenon2-disk2.adf`, in the folder passed to `--disks`.
Disk 1 can be the PNA raw-track release or the BS1/Deejay99 AmigaDOS release.
Add `--interpolate` to enable smooth motion.

```sh
build/sdl/xenon2-sdl --disks /path/to/disks --interpolate
```

Build for the browser with `nix build .#web`. Choose the two disk images in
the launcher and enable **Smooth motion** before starting. Browser presentation
follows the display's refresh callbacks. Both modes use the Gods pixel
resampler, with exact pixel copies at integer scales and sharp boundaries at
fractional scales. The 320 × 256 raster fits the available display pixels,
including on high-DPI screens and at browser zoom.

Smooth motion interpolates between the completed gameplay flips in
`swap_screens` ($2034). It uses the tile backdrop and parallax strip
(`draw_background`, $17D6), starfield (`stars_draw`, $233A), and the masked
draws for objects and projectiles. The panel uses the original copper output.
The machine runs at 50 Hz and retains its original waits between flips.
The presented motion trails by one completed gameplay frame.

Run `make test-sdl-presentation` to check the compositor and upscaling
without disk images.

## License

This is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version. See [LICENSE](LICENSE).

`src/sdl/third_party/ZXSC-LICENSE.txt` is the license (MIT) of the pixel scaling code that `src/sdl/pixel_scale.*` builds on. Xenon 2 Megablast itself, its graphics, music and data are not covered and are not distributed here.
