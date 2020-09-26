#pragma once

#include <emulator/core/hw/interrupt.hpp>
#include <emulator/core/hw/ppu/registers.hpp>

namespace nba::core {

class PPU {
public:
    PPU(InterruptController* irq_controller) : irq_controller{irq_controller} {};

    virtual void Reset() = 0;
    virtual void HookPRAM(std::uint32_t address, std::uint32_t size){};
    virtual void HookVRAM(std::uint32_t address, std::uint32_t size){};
    virtual void HookOAM(std::uint32_t address, std::uint32_t size){};
    virtual void HookMMIO(std::uint32_t address){};

    std::uint8_t pram[0x00400];
    std::uint8_t oam[0x00400];
    std::uint8_t vram[0x18000];

    struct MMIO {
        DisplayControl dispcnt;
        DisplayStatus dispstat;

        std::uint8_t vcount;

        BackgroundControl bgcnt[4]{0, 1, 2, 3};

        std::uint16_t bghofs[4];
        std::uint16_t bgvofs[4];

        ReferencePoint bgx[2], bgy[2];
        std::int16_t bgpa[2];
        std::int16_t bgpb[2];
        std::int16_t bgpc[2];
        std::int16_t bgpd[2];

        WindowRange winh[2];
        WindowRange winv[2];
        WindowLayerSelect winin;
        WindowLayerSelect winout;

        Mosaic mosaic;

        BlendControl bldcnt;
        int eva;
        int evb;
        int evy;
    } mmio;

    void CheckVerticalCounterIRQ();

protected:

    InterruptController* irq_controller{};
};

} // namespace nba::core
