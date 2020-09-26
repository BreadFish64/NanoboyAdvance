#include <cstring>

#include "ppu.hpp"

namespace nba::core {

void PPU::CheckVerticalCounterIRQ() {
    auto& dispstat = mmio.dispstat;
    auto vcount_flag_new = dispstat.vcount_setting == mmio.vcount;
    if (dispstat.vcount_irq_enable && !dispstat.vcount_flag && vcount_flag_new) {
        irq_controller->Raise(InterruptSource::VCount);
    }
    dispstat.vcount_flag = vcount_flag_new;
}

void PPU::Reset() {
    std::memset(pram, 0, 0x00400);
    std::memset(oam, 0, 0x00400);
    std::memset(vram, 0, 0x18000);

    mmio.dispcnt.Reset();
    mmio.dispstat.Reset();
    mmio.vcount = 0;

    for (int i = 0; i < 4; i++) {
        mmio.bgcnt[i].Reset();
        mmio.bghofs[i] = 0;
        mmio.bgvofs[i] = 0;
    }

    for (int i = 0; i < 2; i++) {
        mmio.bgx[i].Reset();
        mmio.bgy[i].Reset();
        mmio.bgpa[i] = 0x100;
        mmio.bgpb[i] = 0;
        mmio.bgpc[i] = 0;
        mmio.bgpd[i] = 0x100;
    }

    mmio.winh[0].Reset();
    mmio.winh[1].Reset();
    mmio.winv[0].Reset();
    mmio.winv[1].Reset();
    mmio.winin.Reset();
    mmio.winout.Reset();

    mmio.mosaic.Reset();

    mmio.eva = 0;
    mmio.evb = 0;
    mmio.evy = 0;
    mmio.bldcnt.Reset();
}

} // namespace nba::core
