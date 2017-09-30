/**
  * Copyright (C) 2017 flerovium^-^ (Frederic Meyer)
  *
  * This file is part of NanoboyAdvance.
  *
  * NanoboyAdvance is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * NanoboyAdvance is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with NanoboyAdvance. If not, see <http://www.gnu.org/licenses/>.
  */

#pragma once

#include <string>
#include <vector>
#include "save.hpp"

namespace Core {
    enum EEPROMSize {
        EEPROM_4K  = 0,
        EEPROM_64K = 1
    };

    class EEPROM : public Save {
    public:
        EEPROM(std::string save_file, EEPROMSize size);
       ~EEPROM();

        void reset();
        auto read8(u32 address) -> u8;
        void write8(u32 address, u8 value);

    private:
        void resetSerialBuffer();

        enum State {
            AcceptCommand = 1 << 0,
            ReadMode      = 1 << 1,
            WriteMode     = 1 << 2,
            GetAddress    = 1 << 3,
            EnableRead    = 1 << 4,
            DummyNibble   = 1 << 5,
            GetWriteData  = 1 << 6,
            EatDummy      = 1 << 7
        };

        static constexpr int s_addr_bits[2] = { 6, 14 };

        u8* memory {nullptr};

        u64 serialBuffer;
        int transmittedBits;
        int currentAddress;

        State state;
        EEPROMSize size;
    };
}