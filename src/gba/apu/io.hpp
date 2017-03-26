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

#ifdef APU_INCLUDE

enum Side {
    SIDE_LEFT  = 0,
    SIDE_RIGHT = 1
};

enum DMANumber {
    DMA_A = 0,
    DMA_B = 1
};

enum SweepDirection {
    SWEEP_INC = 0,
    SWEEP_DEC = 1
};
    
enum EnvelopeDirection {
    ENV_INC = 1,
    ENV_DEC = 0
};

struct IO {
    FIFO fifo[2];
    
    struct VolumeEnvelope {
        int time;
        int initial;
        int direction;
    };
    
    struct ToneChannel {
        
        struct Sweep {
            int time;
            int shift;
            int direction;
        } sweep;
        
        VolumeEnvelope envelope;
        
        int frequency;
        int wave_duty;
        int sound_length;
        bool apply_length;
        
        // not visible to the CPU
        struct Internal {
            int sample;
            int volume;
            int frequency;
            
            struct Cycles {
                int sweep;
                int length;
                int envelope;
            } cycles;
        } internal;
        
        void reset();
        auto read(int offset) -> u8;
        void write(int offset, u8 value);
    } tone[2];
    
    struct WaveChannel {
        
    } wave;
    
    struct NoiseChannel {
        
    } noise;
    
    struct Control {
        bool master_enable;
        FIFO* fifo[2];
        
        struct PSG {
            int  volume;    // 0=25% 1=50% 2=100% 3=forbidden
            int  master[2]; // 0..255, one for each side
            bool enable[2][4];
        } psg;
        
        struct DMA {
            int  volume; // 0=50%, 1=100%
            bool enable[2];
            int  timer_num;
        } dma[2];
        
        void reset();
        auto read(int offset) -> u8;
        void write(int offset, u8 value);
    } control;
    
    struct BIAS {
        int level;
        int resolution;
        
        void reset();
        auto read(int offset) -> u8;
        void write(int offset, u8 value);
    } bias;
} m_io;

#endif