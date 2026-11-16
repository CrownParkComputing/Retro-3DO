#include "demo_rom.h"

#include "demo_rom_data.h"
#include "product.h"

namespace retro3do {

static_assert(kDemoRomBytes_len > 32,
              "The demo ROM must contain its ARM exception vectors");
static_assert(kDemoRomBytes_len <= 1024u * 1024u,
              "The demo ROM must fit the 3DO ROM window");

const DemoRom& builtin_demo_rom() {
    static const DemoRom rom{
        kDemoRomBytes,
        kDemoRomBytes_len,
        RETRO3DO_PRODUCT_NAME " Original Hardware Demo",
        "a745e8adc47ca1fa74e446743eb3a1413da079f822020597be1a794affeeb88d",
    };
    return rom;
}

}  // namespace retro3do
