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

} // namespace nba::core
