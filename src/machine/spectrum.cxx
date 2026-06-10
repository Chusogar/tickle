/*
    ZX Spectrum 48K emulator driver for Tickle
*/

#include "spectrum.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Hardware constants
// -----------------------------------------------------------------------------
enum {
    ScreenWidth          = 320,      // 256 + borders
    ScreenHeight         = 240,      // 192 + borders
    ScreenColors         = 16,
    VideoFrequency       = 50,
    CpuClock             = 3500000,
    CpuCyclesPerFrame    = CpuClock / VideoFrequency,

    SpectrumWidth        = 256,
    SpectrumHeight       = 192,

    BorderLeft           = 32,
    BorderTop            = 24,

    BeeperAmplitude      = 6000,

    Sna48kSize           = 49179
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

    clearKeyboard();
    reset();
}

ZX48kBoard::~ZX48kBoard()
{
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

    clearKeyboard();
}

bool ZX48kBoard::loadSna48k( const unsigned char * buf, unsigned len )
{
    if( buf == 0 || len != Sna48kSize ) {
        return false;
    }

    // Reset CPU core first, then overwrite full machine state.
    cpu_->reset();

    // Header layout for 48K .SNA:
    // 00 I
    // 01-02 HL'
    // 03-04 DE'
    // 05-06 BC'
    // 07-08 AF'
    // 09-10 HL
    // 11-12 DE
    // 13-14 BC
    // 15-16 IY
    // 17-18 IX
    // 19 IFF2 (bit 2 meaningful)
    // 20 R
    // 21-22 AF
    // 23-24 SP
    // 25 IM
    // 26 Border
    // 27.. RAM 48K
    //
    // 48K .SNA stores the PC on the stack; the emulator must recover it from
    // RAM at SP and then advance SP by two bytes.

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

    // Recover PC from stack (48K SNA semantics)
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

    // Approximate RETN / IFF recovery:
    // The file stores IFF2 semantics. This Z80 core does not expose a direct
    // public setter for IFF flags, but we can still reproduce the intended end
    // state by executing DI or EI(+NOP) from a scratch RAM location, then
    // restoring PC to the snapshot entry point.
    {
        unsigned savePC = cpu_->PC;
        unsigned scratch = 0x5B00; // safe RAM area on 48K Spectrum
        unsigned off = scratch - 0x4000;

        unsigned char b0 = ram_[off + 0];
        unsigned char b1 = ram_[off + 1];

        if( iff2 & 0x04 ) {
            // EI becomes effective after the next instruction, so execute EI;NOP
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

    // Reset transient runtime state, but preserve loaded machine snapshot.
    beeper_event_count_ = 0;
    frame_start_beeper_level_ = beeper_level_;
    clearKeyboard();

    return true;
}

void ZX48kBoard::run()
{
    beginAudioFrame();

    cpu_->run( CpuCyclesPerFrame );

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
        // ROM
        return;
    }

    ram_[addr - 0x4000] = value;
}

unsigned char ZX48kBoard::readPort( unsigned port )
{
    // ULA responds to any even port.
    // Tickle's port callback exposes only the low 8 bits, but the Z80 registers
    // are public, so for keyboard scanning through IN A,(C) we can use B as the
    // row selector (high byte of BC), which is what the Spectrum ROM normally uses.
    if( (port & 0x01) == 0 ) {
        unsigned char result = 0xFF;
        unsigned char hi = 0xFF;

        if( cpu_ != 0 ) {
            hi = cpu_->B;
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

    return 0 == resourceHandler()->handle( id, buf, len );
}

bool ZX48k::handleInputEvent( unsigned device, unsigned param, void * data )
{
    (void) data;

    // Direct Spectrum key mapping
    if( device == idSpectrumKey ) {
        bool pressed = (param & 0x01) != 0;
        int row = (param >> 8) & 0x0F;
        int bit = (param >> 4) & 0x0F;
        main_board_->setKeyState( row, bit, pressed );
        return true;
    }

    // Combo mapping (cursors, delete, etc.)
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