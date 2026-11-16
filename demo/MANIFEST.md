# Built-in ROM manifest

| Field | Value |
|---|---|
| Display name | "<product> Original Hardware Demo" -- the product name comes from RETRO3DO_PRODUCT_NAME, so this is "Spryte ..." on iOS and "Retro-3DO ..." elsewhere |
| Copyright owner | Crown Park Computing |
| Source | `demo/retro3do-demo.s` |
| Authored image size | 1,794 bytes |
| Authored SHA-256 | `f019b2a87431cfaec321dd894201f64b36ce964684661bb3eaf18d49fe1e0a4b` |
| Loader image size | 1,048,576 bytes, zero-padded |
| Loader SHA-256 | `a745e8adc47ca1fa74e446743eb3a1413da079f822020597be1a794affeeb88d` |
| Licence | MIT |

The generated C++ byte array is `src/core/demo_rom_data.h`. It contains only
the assembled bytes from the source named above. The remaining bytes in the
3DO ROM window are supplied as zeroes by the ordinary Retro-3DO BIOS loader.

No Portfolio headers, libraries, source, object code, retail BIOS, game code,
artwork, music, or third-party ROM material is present in this image.
