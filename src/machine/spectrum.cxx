/*
    ZX Spectrum 48K emulator driver for Tickle
    TAP support via real EAR emulation only
*/

#include "spectrum.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Hardware constants
// -----------------------------------------------------------------------------
enum {
    ScreenWidth           = 320,      // 256 + borders
    ScreenHeight          = 240,      // 192 + borders
    ScreenColors          = 16,
    VideoFrequency        = 50,
    CpuClock              = 3500000,
    CpuCyclesPerFrame     = CpuClock / VideoFrequency,

    SpectrumWidth         = 256,
    SpectrumHeight        = 192,

    BorderLeft            = 32,
    BorderTop             = 24,

    BeeperAmplitude       = 6000,

    Sna48kSize            = 49179,

    // Standard ROM timings
    TapePilotPulse        = 2168,
    TapeSync1Pulse        = 667,
    TapeSync2Pulse        = 735,
    TapeBit0Pulse         = 855,
    TapeBit1Pulse         = 1710,
    TapePilotHeaderPulses = 8063,
    TapePilotDataPulses   = 3223,

    // Inter-block / end-of-tape pauses (stable level, no extra edges)
    TapeInterBlockPause   = CpuClock / 4,   // ~250 ms
    TapeEndPause          = CpuClock / 2    // ~500 ms
};

enum {
    Ef48Rom
};

// -----------------------------------------------------------------------------
// Driver registration
// -----------------------------------------------------------------------------
static TMachineInfo ZX48kInfo = {
    "zx48k", "ZX Spectrum 48K", "Sinclair Research", 1982,
    ScreenWidth, ScreenHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler reg1( &ZX48kInfo, ZX48k::createInstance );

// -----------------------------------------------------------------------------
// Tape helpers
// -----------------------------------------------------------------------------
void ZX48kBoard::stopTapPlayback()
{
    tape_playing_ = false;
    tape_state_ = TapeIdle;
    tape_cycles_to_edge_ = 0;
    tape_pilot_pulses_left_ = 0;
    tape_block_len_ = 0;
    tape_block_ptr_ = 0;
    tape_byte_index_ = 0;
    tape_bit_mask_ = 0;
    tape_half_pulse_ = 0;
    // do not force an immediate level change here
}

bool ZX48kBoard::armTapPlayback()
{
    if( tap_data_ == 0 || tap_size_ == 0 ) {
        return false;
    }

    stopTapPlayback();
    tape_armed_ = true;
    return true;
}

bool ZX48kBoard::rewindAndPlayTap()
{
    if( tap_data_ == 0 || tap_size_ == 0 ) {
        return false;
    }

    tap_pos_ = 0;
    stopTapPlayback();

    // F11 should really behave as REWIND + PLAY
    tape_armed_ = true;
    return startNextTapBlock();
}

bool ZX48kBoard::parseNextTapBlock( const unsigned char ** blk, unsigned * block_len )
{
    if( blk == 0 || block_len == 0 ) {
        return false;
    }

    if( tap_data_ == 0 || tap_pos_ >= tap_size_ ) {
        return false;
    }

    if( tap_pos_ + 2 > tap_size_ ) {
        return false;
    }

    unsigned len = tap_data_[tap_pos_] | (((unsigned)tap_data_[tap_pos_ + 1]) << 8);
    unsigned off = tap_pos_ + 2;

    if( len < 2 || off + len > tap_size_ ) {
        return false;
    }

    *blk = tap_data_ + off;
    *block_len = len;
    return true;
}

bool ZX48kBoard::startNextTapBlock()
{
    if( tap_data_ == 0 || tap_pos_ >= tap_size_ ) {
        return false;
    }

    if( tap_pos_ + 2 > tap_size_ ) {
        stopTapPlayback();
        tap_pos_ = tap_size_;
        return false;
    }

    unsigned block_len = tap_data_[tap_pos_] | (((unsigned)tap_data_[tap_pos_ + 1]) << 8);
    unsigned block_off = tap_pos_ + 2;

    if( block_len < 2 || block_off + block_len > tap_size_ ) {
        stopTapPlayback();
        tap_pos_ = tap_size_;
        return false;
    }

    tape_block_ptr_ = tap_data_ + block_off;
    tape_block_len_ = block_len;
    tape_byte_index_ = 0;
    tape_bit_mask_ = 0x80;
    tape_half_pulse_ = 0;

    // Header block uses long pilot, data block uses short pilot
    tape_pilot_pulses_left_ = (tape_block_ptr_[0] == 0x00) ? TapePilotHeaderPulses : TapePilotDataPulses;

    tape_state_ = TapePilot;
    tape_playing_ = true;
    tape_cycles_to_edge_ = TapePilotPulse;

    // Initial polarity does not matter for the ROM loader
    ear_level_ = 0;

    // Advance tape position: this block is now "current"
    tap_pos_ = block_off + block_len;

    return true;
}

void ZX48kBoard::advanceTap( unsigned cycles )
{
    while( tape_playing_ && cycles > 0 ) {
        if( cycles < tape_cycles_to_edge_ ) {
            tape_cycles_to_edge_ -= cycles;
            cycles = 0;
            break;
        }

        cycles -= tape_cycles_to_edge_;

        switch( tape_state_ ) {
            case TapePilot:
                ear_level_ ^= 1;
                if( tape_pilot_pulses_left_ > 0 ) {
                    tape_pilot_pulses_left_--;
                }

                if( tape_pilot_pulses_left_ > 0 ) {
                    tape_cycles_to_edge_ = TapePilotPulse;
                }
                else {
                    tape_state_ = TapeSync1;
                    tape_cycles_to_edge_ = TapeSync1Pulse;
                }
                break;

            case TapeSync1:
                ear_level_ ^= 1;
                tape_state_ = TapeSync2;
                tape_cycles_to_edge_ = TapeSync2Pulse;
                break;

            case TapeSync2:
                ear_level_ ^= 1;
                tape_state_ = TapeData;
                tape_byte_index_ = 0;
                tape_bit_mask_ = 0x80;
                tape_half_pulse_ = 0;
                tape_cycles_to_edge_ =
                    (tape_block_ptr_[tape_byte_index_] & tape_bit_mask_) ? TapeBit1Pulse : TapeBit0Pulse;
                break;

            case TapeData:
            {
                ear_level_ ^= 1;

                unsigned pulse_len =
                    (tape_block_ptr_[tape_byte_index_] & tape_bit_mask_) ? TapeBit1Pulse : TapeBit0Pulse;

                if( tape_half_pulse_ == 0 ) {
                    tape_half_pulse_ = 1;
                    tape_cycles_to_edge_ = pulse_len;
                }
                else {
                    tape_half_pulse_ = 0;
                    tape_bit_mask_ >>= 1;

                    if( tape_bit_mask_ == 0 ) {
                        tape_bit_mask_ = 0x80;
                        tape_byte_index_++;

                        if( tape_byte_index_ >= tape_block_len_ ) {
                            // end of current block -> stable pause
                            tape_state_ = TapePause;
                            tape_cycles_to_edge_ = (tap_pos_ < tap_size_) ? TapeInterBlockPause : TapeEndPause;
                            break;
                        }
                    }

                    tape_cycles_to_edge_ =
                        (tape_block_ptr_[tape_byte_index_] & tape_bit_mask_) ? TapeBit1Pulse : TapeBit0Pulse;
                }
                break;
            }

            case TapePause:
                // Keep EAR stable during pause
                if( tap_pos_ < tap_size_ ) {
                    if( ! startNextTapBlock() ) {
                        stopTapPlayback();
                        tape_armed_ = false;
                    }
                }
                else {
                    // End of tape after trailing silence
                    stopTapPlayback();
                    tape_armed_ = false;
                }
                break;

            case TapeIdle:
            default:
                stopTapPlayback();
                tape_armed_ = false;
                break;
        }
    }
}

// -----------------------------------------------------------------------------
// ZX48kBoard
// -----------------------------------------------------------------------------
ZX48kBoard::ZX48kBoard()
{
    cpu_ = new ZX48kCpu( *this );

    memset( rom_, 0x00, sizeof(rom_) );
    memset( ram_, 0x00, sizeof(ram_) );

    border_color_  = 7;
    port_fe_latch_ = 0;
    beeper_level_  = 0;
    mic_level_     = 0;
    ear_level_     = 0;
    frame_counter_ = 0;

    frame_start_beeper_level_ = 0;
    beeper_event_count_ = 0;

    tap_data_ = 0;
    tap_size_ = 0;
    tap_pos_ = 0;
    tape_block_ptr_ = 0;
    tape_block_len_ = 0;
    tape_byte_index_ = 0;
    tape_bit_mask_ = 0;
    tape_half_pulse_ = 0;
    tape_pilot_pulses_left_ = 0;
    tape_cycles_to_edge_ = 0;
    tape_playing_ = false;
    tape_armed_ = false;
    tape_state_ = TapeIdle;

    clearKeyboard();
    reset();
}

ZX48kBoard::~ZX48kBoard()
{
    delete [] tap_data_;
    delete cpu_;
}

void ZX48kBoard::clearKeyboard()
{
    for( int i=0; i<8; i++ ) {
        key_row_[i] = 0x1F; // all released (active low)
    }
}

void ZX48kBoard::setKeyState( int row, int bit, bool pressed )
{
    if( row < 0 || row >= 8 || bit < 0 || bit >= 5 ) {
        return;
    }

    if( pressed ) {
        key_row_[row] &= ~(1 << bit);
    }
    else {
        key_row_[row] |= (1 << bit);
    }
}

void ZX48kBoard::beginAudioFrame()
{
    frame_start_beeper_level_ = beeper_level_;
    beeper_event_count_ = 0;
    cpu_->setCycles( 0 );
}

void ZX48kBoard::logBeeperEdge( unsigned cycle, unsigned char level )
{
    if( beeper_event_count_ >= MaxBeeperEvents ) {
        return;
    }

    if( cycle > CpuCyclesPerFrame ) {
        cycle = CpuCyclesPerFrame;
    }

    beeper_event_cycle_[beeper_event_count_] = cycle;
    beeper_event_level_[beeper_event_count_] = level;
    beeper_event_count_++;
}

void ZX48kBoard::reset()
{
    cpu_->reset();

    border_color_  = 7;
    port_fe_latch_ = 0;
    beeper_level_  = 0;
    mic_level_     = 0;
    ear_level_     = 0;
    frame_counter_ = 0;

    frame_start_beeper_level_ = 0;
    beeper_event_count_ = 0;

    // Keep tape loaded, rewind it, and stop playback
    tap_pos_ = 0;
    stopTapPlayback();
    tape_armed_ = false;
    ear_level_ = 0;

    clearKeyboard();
}

bool ZX48kBoard::loadSna48k( const unsigned char * buf, unsigned len )
{
    if( buf == 0 || len != Sna48kSize ) {
        return false;
    }

    cpu_->reset();

    memcpy( ram_, buf + 27, 0xC000 );

    cpu_->I  = buf[0];

    cpu_->L1 = buf[1];
    cpu_->H1 = buf[2];

    cpu_->E1 = buf[3];
    cpu_->D1 = buf[4];

    cpu_->C1 = buf[5];
    cpu_->B1 = buf[6];

    cpu_->F1 = buf[7];
    cpu_->A1 = buf[8];

    cpu_->L  = buf[9];
    cpu_->H  = buf[10];

    cpu_->E  = buf[11];
    cpu_->D  = buf[12];

    cpu_->C  = buf[13];
    cpu_->B  = buf[14];

    cpu_->IY = ((unsigned)buf[15]) | (((unsigned)buf[16]) << 8);
    cpu_->IX = ((unsigned)buf[17]) | (((unsigned)buf[18]) << 8);

    unsigned char iff2 = buf[19];
    cpu_->R  = buf[20];

    cpu_->F  = buf[21];
    cpu_->A  = buf[22];

    cpu_->SP = ((unsigned)buf[23]) | (((unsigned)buf[24]) << 8);

    cpu_->setInterruptModePublic( buf[25] & 0x03 );

    border_color_  = buf[26] & 0x07;
    port_fe_latch_ = border_color_;
    mic_level_     = 0;
    ear_level_     = 0;
    frame_counter_ = 0;

    // Recover PC from stack
    if( cpu_->SP < 0x4000 || cpu_->SP >= 0xFFFF ) {
        return false;
    }

    {
        unsigned sp_off = cpu_->SP - 0x4000;
        unsigned pcl = ram_[sp_off];
        unsigned pch = ram_[sp_off + 1];
        cpu_->PC = pcl | (pch << 8);
        cpu_->SP += 2;
    }

    // Approximate IFF recovery
    {
        unsigned savePC = cpu_->PC;
        unsigned scratch = 0x5B00; // safe RAM area on 48K Spectrum
        unsigned off = scratch - 0x4000;

        unsigned char b0 = ram_[off + 0];
        unsigned char b1 = ram_[off + 1];

        if( iff2 & 0x04 ) {
            ram_[off + 0] = 0xFB; // EI
            ram_[off + 1] = 0x00; // NOP
            cpu_->PC = scratch;
            cpu_->step();
            cpu_->step();
        }
        else {
            ram_[off + 0] = 0xF3; // DI
            cpu_->PC = scratch;
            cpu_->step();
        }

        ram_[off + 0] = b0;
        ram_[off + 1] = b1;

        cpu_->PC = savePC;
    }

    beeper_event_count_ = 0;
    frame_start_beeper_level_ = beeper_level_;
    clearKeyboard();

    return true;
}

bool ZX48kBoard::loadTap( const unsigned char * buf, unsigned len )
{
    delete [] tap_data_;
    tap_data_ = 0;
    tap_size_ = 0;
    tap_pos_  = 0;

    stopTapPlayback();
    tape_armed_ = false;
    ear_level_ = 0;

    if( buf == 0 || len == 0 ) {
        return false;
    }

    tap_data_ = new unsigned char[len];
    memcpy( tap_data_, buf, len );
    tap_size_ = len;
    tap_pos_  = 0;

    return true;
}

// Disabled in EAR-only mode
bool ZX48kBoard::handleTapTrap()
{
    return false;
}

void ZX48kBoard::run()
{
    beginAudioFrame();

    while( cpu_->getCycles() < CpuCyclesPerFrame ) {
        unsigned before = cpu_->getCycles();

        cpu_->step();

        unsigned after = cpu_->getCycles();
        if( after > before && tape_playing_ ) {
            advanceTap( after - before );
        }
    }

    // Standard Spectrum maskable interrupt, once per frame.
    cpu_->interrupt( 0xFF );

    frame_counter_++;
}

unsigned char ZX48kBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr < 0x4000 ) {
        return rom_[addr];
    }

    return ram_[addr - 0x4000];
}

void ZX48kBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr < 0x4000 ) {
        return;
    }

    ram_[addr - 0x4000] = value;
}

unsigned char ZX48kBoard::readPort( unsigned port )
{
    // ULA responds to any even port.
    if( (port & 0x01) == 0 ) {
        unsigned char result = 0xFF;
        unsigned char hi = 0xFF;

        if( cpu_ != 0 ) {
            unsigned pc = cpu_->PC & 0xFFFF;

            // Default for IN r,(C) / IN A,(C): high byte comes from B
            hi = cpu_->B;

            // Heuristic for IN A,(n) (opcode DB nn):
            // depending on when the callback happens, PC may already point
            // 1 or 2 bytes after the opcode fetch. Check both possibilities.
            if( pc >= 1 ) {
                unsigned char p1 = readByte( (pc - 1) & 0xFFFF );
                if( p1 == 0xDB ) {
                    hi = cpu_->A;
                }
            }

            if( pc >= 2 ) {
                unsigned char p2 = readByte( (pc - 2) & 0xFFFF );
                if( p2 == 0xDB ) {
                    hi = cpu_->A;
                }
            }
        }

        for( int row=0; row<8; row++ ) {
            if( ((hi >> row) & 0x01) == 0 ) {
                result &= (unsigned char)(0xE0 | key_row_[row]);
            }
        }

        // EAR input on bit 6
        if( ear_level_ ) {
            result |= 0x40;
        }
        else {
            result &= (unsigned char)~0x40;
        }

        // Keep unrelated upper bits high
        result |= 0xA0;

        return result;
    }

    return 0xFF;
}

void ZX48kBoard::writePort( unsigned port, unsigned char value )
{
    if( (port & 0x01) == 0 ) {
        unsigned char new_beeper = (value >> 4) & 0x01;

        port_fe_latch_ = value;
        border_color_  = value & 0x07;
        mic_level_     = (value >> 3) & 0x01;

        if( new_beeper != beeper_level_ ) {
            logBeeperEdge( cpu_->getCycles(), new_beeper );
            beeper_level_ = new_beeper;
        }
    }
}

// -----------------------------------------------------------------------------
// ZX48k machine
// -----------------------------------------------------------------------------
ZX48k::ZX48k( ZX48kBoard * board )
{
    main_board_ = board;
    createScreen( ScreenWidth, ScreenHeight, ScreenColors );
    registerDriver( ZX48kInfo );
}

ZX48k::~ZX48k()
{
    delete main_board_;
}

bool ZX48k::initialize( TMachineDriverInfo * info )
{
    resourceHandler()->add( Ef48Rom, "48.rom", 0x4000, efROM, main_board_->rom_ );
    resourceHandler()->addToMachineDriverInfo( info );

    // Standard 8 colors + bright set
    palette()->setColor(  0, TPalette::encodeColor(0x00,0x00,0x00) );
    palette()->setColor(  1, TPalette::encodeColor(0x00,0x00,0xD7) );
    palette()->setColor(  2, TPalette::encodeColor(0xD7,0x00,0x00) );
    palette()->setColor(  3, TPalette::encodeColor(0xD7,0x00,0xD7) );
    palette()->setColor(  4, TPalette::encodeColor(0x00,0xD7,0x00) );
    palette()->setColor(  5, TPalette::encodeColor(0x00,0xD7,0xD7) );
    palette()->setColor(  6, TPalette::encodeColor(0xD7,0xD7,0x00) );
    palette()->setColor(  7, TPalette::encodeColor(0xD7,0xD7,0xD7) );

    palette()->setColor(  8, TPalette::encodeColor(0x00,0x00,0x00) );
    palette()->setColor(  9, TPalette::encodeColor(0x00,0x00,0xFF) );
    palette()->setColor( 10, TPalette::encodeColor(0xFF,0x00,0x00) );
    palette()->setColor( 11, TPalette::encodeColor(0xFF,0x00,0xFF) );
    palette()->setColor( 12, TPalette::encodeColor(0x00,0xFF,0x00) );
    palette()->setColor( 13, TPalette::encodeColor(0x00,0xFF,0xFF) );
    palette()->setColor( 14, TPalette::encodeColor(0xFF,0xFF,0x00) );
    palette()->setColor( 15, TPalette::encodeColor(0xFF,0xFF,0xFF) );

    return true;
}

bool ZX48k::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    if( id == EfSpectrumSnapshot ) {
        return main_board_->loadSna48k( buf, len );
    }

    if( id == EfSpectrumTape ) {
        return main_board_->loadTap( buf, len );
    }

    return 0 == resourceHandler()->handle( id, buf, len );
}

bool ZX48k::handleInputEvent( unsigned device, unsigned param, void * data )
{
    (void) data;

    if( device == idSpectrumKey ) {
        bool pressed = (param & 0x01) != 0;
        int row = (param >> 8) & 0x0F;
        int bit = (param >> 4) & 0x0F;
        main_board_->setKeyState( row, bit, pressed );
        return true;
    }

    if( device == idSpectrumKeyCombo ) {
        bool pressed = (param & 0x01) != 0;
        int row1 = (param >> 24) & 0x0F;
        int bit1 = (param >> 20) & 0x0F;
        int row2 = (param >> 16) & 0x0F;
        int bit2 = (param >> 12) & 0x0F;

        main_board_->setKeyState( row1, bit1, pressed );
        main_board_->setKeyState( row2, bit2, pressed );
        return true;
    }

    if( device == idSpectrumTapeControl ) {
        if( param & 0x01 ) {
            return main_board_->rewindAndPlayTap();
        }
        return true;
    }

    return TStandardMachine::handleInputEvent( device, param, data );
}

void ZX48k::reset()
{
    main_board_->reset();
}

void ZX48k::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    main_board_->run();
    renderAudio( frame, samplesPerFrame, samplingRate );
    frame->setVideo( renderVideo() );
}

void ZX48k::renderAudio( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    (void) samplingRate;

    if( samplesPerFrame == 0 ) {
        return;
    }

    TMixerBuffer * mixer_buffer = frame->getMixer()->getBuffer( chMono, samplesPerFrame, 3 );
    int * data = mixer_buffer->data();

    for( unsigned i=0; i<samplesPerFrame; i++ ) {
        data[i] = 0;
    }

    unsigned prev_cycle = 0;
    unsigned char level = main_board_->frame_start_beeper_level_;

    for( unsigned e=0; e<main_board_->beeper_event_count_; e++ ) {
        unsigned event_cycle = main_board_->beeper_event_cycle_[e];
        if( event_cycle > CpuCyclesPerFrame ) {
            event_cycle = CpuCyclesPerFrame;
        }

        unsigned s0 = (prev_cycle  * samplesPerFrame) / CpuCyclesPerFrame;
        unsigned s1 = (event_cycle * samplesPerFrame) / CpuCyclesPerFrame;
        int amp = level ? +BeeperAmplitude : -BeeperAmplitude;

        for( unsigned s=s0; s<s1 && s<samplesPerFrame; s++ ) {
            data[s] += amp;
        }

        prev_cycle = event_cycle;
        level = main_board_->beeper_event_level_[e];
    }

    // Tail segment
    {
        unsigned s0 = (prev_cycle * samplesPerFrame) / CpuCyclesPerFrame;
        int amp = level ? +BeeperAmplitude : -BeeperAmplitude;

        for( unsigned s=s0; s<samplesPerFrame; s++ ) {
            data[s] += amp;
        }
    }
}

unsigned short ZX48k::pixelAddress( int x, int y )
{
    return (unsigned short)(
        0x4000 +
        ((y & 0xC0) << 5) +
        ((y & 0x07) << 8) +
        ((y & 0x38) << 2) +
        (x >> 3)
    );
}

unsigned short ZX48k::attrAddress( int x, int y )
{
    return (unsigned short)(0x5800 + ((y >> 3) * 32) + (x >> 3));
}

TBitmapIndexed * ZX48k::renderVideo()
{
    TBitBlock * bits = screen()->bits();

    bits->fill( (unsigned char)(main_board_->border_color_ & 0x07) );

    bool flash_phase = ((main_board_->frame_counter_ >> 4) & 1) != 0;

    for( int y=0; y<SpectrumHeight; y++ ) {
        for( int x=0; x<SpectrumWidth; x += 8 ) {
            unsigned short paddr = pixelAddress( x, y );
            unsigned short aaddr = attrAddress( x, y );

            unsigned char pix  = main_board_->readByte( paddr );
            unsigned char attr = main_board_->readByte( aaddr );

            unsigned char ink   = attr & 0x07;
            unsigned char paper = (attr >> 3) & 0x07;
            bool bright         = (attr & 0x40) != 0;
            bool flash          = (attr & 0x80) != 0;

            if( flash && flash_phase ) {
                unsigned char t = ink;
                ink = paper;
                paper = t;
            }

            for( int b=0; b<8; b++ ) {
                bool pixel_set = (pix & (0x80 >> b)) != 0;
                unsigned char c = pixel_set ? ink : paper;
                unsigned char ci = (unsigned char)((bright ? 8 : 0) + (c & 0x07));

                bits->setPixel(
                    BorderLeft + x + b,
                    BorderTop + y,
                    ci
                );
            }
        }
    }

    return screen();
}