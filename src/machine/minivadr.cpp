/*
    Mini Vaders (c) 1990 Taito Corporation

    Tickle driver, based on MAME's taito/minivadr.cpp (driver by
    Takahiro Nogi, license BSD-3-Clause).

    This is about as simple as an arcade board gets: a Z80, 8 KB of
    program ROM, 8 KB of RAM used directly as a 1bpp video framebuffer,
    and a single input port. No sound, no DIP switches, no colour PROMs.

    Memory map (from MAME):
      0000-1FFF  ROM (8 KB, only the low half of the chip's 0x2000 bytes
                  used by the address decoding - actually the whole ROM)
      A000-BFFF  Video RAM / framebuffer, 1bpp, 256x256, 32 bytes/row
      E008       Input port (read only; writes are ignored - "nopw")

    Video: 256x256 raw screen, visible area is (0,255) x (16,239), i.e.
    256x224 pixels actually shown. Each video RAM byte packs 8 horizontal
    monochrome pixels, most-significant bit = leftmost pixel:

        pixel(x,y) = (videoram[y*32 + x/8] >> (7 - (x&7))) & 1

    1 = white (lit), 0 = black.
*/

#include "minivadr.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Hardware constants
// ---------------------------------------------------------------------------
enum {
    ScreenWidth         = 256,
    ScreenHeight        = 224,    // visible area: rows 16..239 of the raw 256-row framebuffer
    ScreenColors        = 2,      // pure monochrome: black / white
    VideoFrequency      = 60,

    CpuClock            = 4000000,    // 24 MHz crystal / 6
    CpuCyclesPerFrame   = CpuClock / VideoFrequency,

    VisibleY0           = 16,     // first visible framebuffer row (matches MAME's set_visarea)

    RomSize             = 0x2000, // 8 KB program ROM
    VideoRamSize        = 0x2000, // 256 rows * 32 bytes/row
    BytesPerRow         = 32      // 256 pixels / 8 bits per byte
};

// ---------------------------------------------------------------------------
// Resource IDs
// ---------------------------------------------------------------------------
enum {
    EfMainRom
};

// ---------------------------------------------------------------------------
// Machine info
// ---------------------------------------------------------------------------
static TMachineInfo MiniVadrInfo = {
    "minivadr", "Mini Vaders", "Taito Corporation", 1990,
    ScreenWidth, ScreenHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler regMiniVadr( &MiniVadrInfo, MiniVadr::createInstance );

// ===========================================================================
// MiniVadrMainBoard
// ===========================================================================

MiniVadrMainBoard::MiniVadrMainBoard()
{
    cpu_ = new Z80( *this );

    memset( main_rom_, 0, sizeof(main_rom_) );
    memset( videoram_,  0, sizeof(videoram_)  );

    inputs_ = 0xFF;

    reset();
}

void MiniVadrMainBoard::reset()
{
    cpu_->reset();

    memset( videoram_, 0, sizeof(videoram_) );

    inputs_ = 0xFF;
}

void MiniVadrMainBoard::run()
{
    cpu_->run( CpuCyclesPerFrame );

    // MAME: m_maincpu->set_vblank_int("screen", FUNC(minivadr_state::irq0_line_hold));
    // Standard maskable IRQ once per frame, at vblank. cpu_->interrupt() only
    // actually fires if the Z80 program has enabled interrupts (EI), which is
    // exactly the semantics of irq0_line_hold. The data byte is irrelevant in
    // IM1 (always RST 38h); 0xFF also happens to decode as RST 38h if the
    // program is running in IM0, so it is a safe universal choice.
    cpu_->interrupt( 0xFF );
}

// ---------------------------------------------------------------------------
// Memory read
// ---------------------------------------------------------------------------
unsigned char MiniVadrMainBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    // 0000-1FFF: program ROM
    if( addr < RomSize ) {
        return main_rom_[addr];
    }

    // A000-BFFF: video RAM (8 KB, exactly matches 256 rows * 32 bytes/row)
    if( addr >= 0xA000 && addr <= 0xBFFF ) {
        return videoram_[ addr - 0xA000 ];
    }

    // E008: input port
    if( addr == 0xE008 ) {
        return inputs_;
    }

    return 0xFF;
}

// ---------------------------------------------------------------------------
// Memory write
// ---------------------------------------------------------------------------
void MiniVadrMainBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    // A000-BFFF: video RAM
    if( addr >= 0xA000 && addr <= 0xBFFF ) {
        videoram_[ addr - 0xA000 ] = value;
        return;
    }

    // E008: write side is a no-op on real hardware (MAME: .nopw())
    if( addr == 0xE008 ) {
        return;
    }

    // ROM region and anything else: not writable
}

// ---------------------------------------------------------------------------
// I/O - Mini Vaders does not use the Z80's I/O port space at all; every
// access is memory-mapped.
// ---------------------------------------------------------------------------
unsigned char MiniVadrMainBoard::readPort( unsigned /*port*/ )
{
    return 0xFF;
}

void MiniVadrMainBoard::writePort( unsigned /*port*/, unsigned char /*value*/ )
{
    // Not used
}

// ===========================================================================
// MiniVadr (machine)
// ===========================================================================

MiniVadr::MiniVadr( MiniVadrMainBoard * board )
{
    main_board_ = board;

    createScreen( ScreenWidth, ScreenHeight, ScreenColors );

    // Pure monochrome palette: index 0 = black, index 1 = white.
    // (Set once here; there is no colour PROM to decode, so this never
    // needs to be rebuilt.)
    palette()->setColor( 0, TPalette::encodeColor( 0, 0, 0 ) );
    palette()->setColor( 1, TPalette::encodeColor( 255, 255, 255 ) );

    // -----------------------------------------------------------------------
    // Input port (E008), from MAME's INPUT_PORTS_START(minivadr):
    //
    //   bit0 = JOYSTICK LEFT    (active low)
    //   bit1 = JOYSTICK RIGHT   (active low)
    //   bit2 = BUTTON 1         (active low)
    //   bit3 = COIN 1           (active low)
    //   bit4-7 = unused
    //
    // The cabinet only has a 2-way joystick (left/right), so we connect
    // just those two directions; up/down are left unmapped.
    // -----------------------------------------------------------------------
    eventHandler()->add( idCoinSlot1,    ptInverted, &main_board_->inputs_, 0x08 );
    eventHandler()->add( idKeyP1Action1, ptInverted, &main_board_->inputs_, 0x04 );

    setJoystickHandler( 0, new TJoystickToPortHandler(idJoyP1Joystick1, ptInverted, jm4Way) );
    joystickHandler(0)->setPort( jpLeft,  &main_board_->inputs_, 0x01 );
    joystickHandler(0)->setPort( jpRight, &main_board_->inputs_, 0x02 );

    registerDriver( MiniVadrInfo );
}

// ---------------------------------------------------------------------------
bool MiniVadr::initialize( TMachineDriverInfo * info )
{
    // Single 8 KB program ROM (MAME: d26-01.ic7, 0x2000 bytes at 0x0000)
    resourceHandler()->add( EfMainRom, "d26-01.ic7", RomSize, efROM, main_board_->main_rom_ );

    resourceHandler()->assignToMachineDriverInfo( info );

    // No DIP switches, no colour PROMs, no sound - nothing else to wire up.
    return true;
}

// ---------------------------------------------------------------------------
bool MiniVadr::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    return 0 == resourceHandler()->handle( id, buf, len );
}

// ---------------------------------------------------------------------------
void MiniVadr::reset()
{
    main_board_->reset();
}

// ---------------------------------------------------------------------------
void MiniVadr::run( TFrame * frame, unsigned /*samplesPerFrame*/, unsigned /*samplingRate*/ )
{
    main_board_->run();

    frame->setVideo( renderVideo() );

    // No sound hardware on this board (MAME: MACHINE_NO_SOUND_HW) - nothing
    // to mix into the audio buffer.
}

// ===========================================================================
// Video rendering
//
// Direct 1bpp framebuffer readout, matching MAME's screen_update exactly:
//
//   for each visible row y (16..239 of the raw 256-row buffer):
//     src = &videoram[y * 32]
//     for each column x (0..255):
//       pixel = (src[x >> 3] >> (7 - (x & 7))) & 1
//
// The visible area is rows 16..239 (224 rows); row 0 of our output
// corresponds to raw framebuffer row VisibleY0 (16).
// ===========================================================================
TBitmapIndexed * MiniVadr::renderVideo()
{
    TBitBlock * bits = screen()->bits();

    for( int y = 0; y < ScreenHeight; y++ ) {
        const unsigned char * src = main_board_->videoram_ + (y + VisibleY0) * BytesPerRow;
        unsigned char * dst = bits->scanline_data( y );

        for( int x = 0; x < ScreenWidth; x++ ) {
            unsigned char byte = src[ x >> 3 ];
            unsigned char pen  = (byte >> (7 - (x & 7))) & 1;
            dst[x] = pen;
        }
    }

    return screen();
}
