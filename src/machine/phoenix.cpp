#include "phoenix.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Hardware info
// Original classic driver:
// - raw raster 256x208
// - ROT90
// In Tickle we expose the final rotated geometry directly as 208x256.
// -----------------------------------------------------------------------------
enum {
    RawWidth            = 256,
    RawHeight           = 208,
    ScreenWidth         = 208,
    ScreenHeight        = 256,
    ScreenColors        = 256,
    VideoFrequency      = 60,
    CpuClock            = 3072000,
    CpuCyclesPerFrame   = CpuClock / VideoFrequency
};

enum {
    EfMain45,
    EfMain46,
    EfMain47,
    EfMain48,
    EfMain49,
    EfMain50,
    EfMain51,
    EfMain52,

    EfBg23,
    EfBg24,

    EfFg39,
    EfFg40,

    EfProm40,
    EfProm41
};

// -----------------------------------------------------------------------------
// Constants from original vidhrdw.c
// -----------------------------------------------------------------------------
static const unsigned BackgroundVideoRamOffset = 0x0800;
static const unsigned VisibleCharsX = 32;
static const unsigned VisibleCharsY = 26;
static const unsigned VisibleRamSize = 0x0340; // 32 * 26 = 832

// -----------------------------------------------------------------------------
// Machine info
// -----------------------------------------------------------------------------
static TMachineInfo PhoenixInfo = {
    "phoenix", "Phoenix", "Amstar", 1980,
    ScreenWidth, ScreenHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler regPhoenix( &PhoenixInfo, Phoenix::createInstance );

// -----------------------------------------------------------------------------
// Helper: bit extraction with LSB-based offsets as in MAME GfxLayout
// -----------------------------------------------------------------------------
static inline unsigned char bitAtLSB( const unsigned char * src, unsigned bitIndex )
{
    return (src[bitIndex >> 3] >> (bitIndex & 7)) & 1;
}

// -----------------------------------------------------------------------------
// Main board
// -----------------------------------------------------------------------------
PhoenixMainBoard::PhoenixMainBoard()
{
    cpu_ = new Z80( *this );

    memset( main_rom_,  0, sizeof(main_rom_) );
    memset( bgtiles_,   0, sizeof(bgtiles_) );
    memset( fgtiles_,   0, sizeof(fgtiles_) );
    memset( proms_,     0, sizeof(proms_) );
    memset( paged_ram_, 0, sizeof(paged_ram_) );

    in0_        = 0xFF;
    dsw0_       = 0x80;   // VBlank bit high by default
    video_reg_  = 0x00;
    scroll_reg_ = 0x00;
    sound_a_    = 0x00;
    sound_b_    = 0x00;

    protection_question_ = 0x00;

    reset();
}

void PhoenixMainBoard::reset()
{
    cpu_->reset();

    memset( paged_ram_, 0, sizeof(paged_ram_) );

    in0_        = 0xFF;
    dsw0_       = 0x80;
    video_reg_  = 0x00;
    scroll_reg_ = 0x00;
    sound_a_    = 0x00;
    sound_b_    = 0x00;

    protection_question_ = 0x00;
}

void PhoenixMainBoard::run()
{
    // Classic attached driver uses ignore_interrupt,1, so no IRQ/NMI injected here.
    cpu_->run( CpuCyclesPerFrame );
}

unsigned char PhoenixMainBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr <= 0x3FFF ) {
        return main_rom_[addr];
    }
    else if( addr >= 0x4000 && addr <= 0x4FFF ) {
        unsigned page = video_reg_ & 0x01;
        return paged_ram_[page][addr - 0x4000];
    }
    else if( addr >= 0x7000 && addr <= 0x73FF ) {
        unsigned char ret = in0_ & 0xF7;

        // Protection handling as in the attached video layer source.
        switch( protection_question_ )
        {
            case 0x00:
            case 0x20:
                // Bit 3 is 0
                break;

            case 0x0C:
            case 0x30:
                // Bit 3 is 1
                ret |= 0x08;
                break;

            default:
                // Unknown protection question: leave as-is
                break;
        }

        return ret;
    }
    else if( addr >= 0x7800 && addr <= 0x7BFF ) {
        return dsw0_;
    }

    return 0xFF;
}

void PhoenixMainBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr >= 0x4000 && addr <= 0x4FFF ) {
        unsigned page = video_reg_ & 0x01;
        paged_ram_[page][addr - 0x4000] = value;
    }
    else if( addr >= 0x5000 && addr <= 0x53FF ) {
        video_reg_ = value;
        protection_question_ = value & 0xFC;
    }
    else if( addr >= 0x5800 && addr <= 0x5BFF ) {
        scroll_reg_ = value;
    }
    else if( addr >= 0x6000 && addr <= 0x63FF ) {
        sound_a_ = value; // stub
    }
    else if( addr >= 0x6800 && addr <= 0x6BFF ) {
        sound_b_ = value; // stub
    }
}

unsigned char PhoenixMainBoard::readPort( unsigned )
{
    return 0xFF;
}

void PhoenixMainBoard::writePort( unsigned, unsigned char )
{
}

// -----------------------------------------------------------------------------
// Machine
// -----------------------------------------------------------------------------
Phoenix::Phoenix( PhoenixMainBoard * board )
    : bg_char_data_( 8, 8 * 256 ),
      fg_char_data_( 8, 8 * 256 )
{
    main_board_ = board;
    refresh_roms_ = false;

    createScreen( ScreenWidth, ScreenHeight, ScreenColors );

    // Inputs (all inverted, matching attached driver)
    eventHandler()->add( idCoinSlot1,       ptInverted, &main_board_->in0_, 0x01 );
    eventHandler()->add( idKeyStartPlayer1, ptInverted, &main_board_->in0_, 0x02 );
    eventHandler()->add( idKeyStartPlayer2, ptInverted, &main_board_->in0_, 0x04 );
    eventHandler()->add( idKeyP1Action1,    ptInverted, &main_board_->in0_, 0x10 ); // Fire
    eventHandler()->add( idKeyP1Action2,    ptInverted, &main_board_->in0_, 0x80 ); // Shield

    // jm2Way is not present in this Tickle tree; use jm8Way but wire only left/right.
    setJoystickHandler( 0, new TJoystickToPortHandler(idJoyP1Joystick1, ptInverted, jm8Way) );
    joystickHandler(0)->setPort( jpLeft,  &main_board_->in0_, 0x40 );
    joystickHandler(0)->setPort( jpRight, &main_board_->in0_, 0x20 );

    registerDriver( PhoenixInfo );
}

bool Phoenix::initialize( TMachineDriverInfo * info )
{
    // CPU ROMs from attached classic driver
    resourceHandler()->add( EfMain45, "ic45", 0x0800, efROM, main_board_->main_rom_ + 0x0000 );
    resourceHandler()->add( EfMain46, "ic46", 0x0800, efROM, main_board_->main_rom_ + 0x0800 );
    resourceHandler()->add( EfMain47, "ic47", 0x0800, efROM, main_board_->main_rom_ + 0x1000 );
    resourceHandler()->add( EfMain48, "ic48", 0x0800, efROM, main_board_->main_rom_ + 0x1800 );
    resourceHandler()->add( EfMain49, "ic49", 0x0800, efROM, main_board_->main_rom_ + 0x2000 );
    resourceHandler()->add( EfMain50, "ic50", 0x0800, efROM, main_board_->main_rom_ + 0x2800 );
    resourceHandler()->add( EfMain51, "ic51", 0x0800, efROM, main_board_->main_rom_ + 0x3000 );
    resourceHandler()->add( EfMain52, "ic52", 0x0800, efROM, main_board_->main_rom_ + 0x3800 );

    // Video ROMs / PROMs using the standard MAME-compatible Phoenix names
    resourceHandler()->add( EfBg23,   "ic23",      0x0800, efVideoROM,    main_board_->bgtiles_ + 0x0000 );
    resourceHandler()->add( EfBg24,   "ic24",      0x0800, efVideoROM,    main_board_->bgtiles_ + 0x0800 );
    resourceHandler()->add( EfFg39,   "ic39",   0x0800, efVideoROM,    main_board_->fgtiles_ + 0x0000 );
    resourceHandler()->add( EfFg40,   "ic40",   0x0800, efVideoROM,    main_board_->fgtiles_ + 0x0800 );
    resourceHandler()->add( EfProm40, "ic40_b.bin", 0x0100, efPalettePROM, main_board_->proms_   + 0x0000 );
    resourceHandler()->add( EfProm41, "ic41_a.bin", 0x0100, efPalettePROM, main_board_->proms_   + 0x0100 );

    resourceHandler()->assignToMachineDriverInfo( info );

    refresh_roms_ = true;
    return true;
}

bool Phoenix::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    refresh_roms_ = true;
    return 0 == resourceHandler()->handle( id, buf, len );
}

void Phoenix::reset()
{
    main_board_->reset();
}

void Phoenix::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    (void)samplesPerFrame;
    (void)samplingRate;

    if( refresh_roms_ ) {
        onVideoROMsChanged();
        refresh_roms_ = false;
    }

    main_board_->run();
    frame->setVideo( renderVideo() );
}

// -----------------------------------------------------------------------------
// Palette / decode
// -----------------------------------------------------------------------------
void Phoenix::rebuildPalette()
{
    // Exact reconstruction from attached video layer:
    // R from bit0 of both PROMs, B from bit1, G from bit2 with weights 0x55/0xaa
    for( int i=0; i<256; i++ ) {
        int bit0, bit1;

        bit0 = (main_board_->proms_[0x000 + i] >> 0) & 0x01;
        bit1 = (main_board_->proms_[0x100 + i] >> 0) & 0x01;
        int r = 0x55 * bit0 + 0xAA * bit1;

        bit0 = (main_board_->proms_[0x000 + i] >> 2) & 0x01;
        bit1 = (main_board_->proms_[0x100 + i] >> 2) & 0x01;
        int g = 0x55 * bit0 + 0xAA * bit1;

        bit0 = (main_board_->proms_[0x000 + i] >> 1) & 0x01;
        bit1 = (main_board_->proms_[0x100 + i] >> 1) & 0x01;
        int b = 0x55 * bit0 + 0xAA * bit1;

        palette()->setColor( i, TPalette::encodeColor(r, g, b) );
    }
}

void Phoenix::decodeCharset( const unsigned char * src, TBitBlock * bb )
{
    // charlayout from attached source:
    // 8x8, 256 chars, 2bpp, planes at 0x0800 and 0x0000
    for( int code=0; code<256; code++ ) {
        unsigned baseBit = (unsigned)code * 8 * 8;

        for( int y=0; y<8; y++ ) {
            for( int x=0; x<8; x++ ) {
                unsigned xoff = 7 - x;
                unsigned yoff = y * 8;

                unsigned char pen =
                    (bitAtLSB(src, 0x0800 * 8 + baseBit + yoff + xoff) << 1) |
                    (bitAtLSB(src,            baseBit + yoff + xoff) << 0);

                bb->setPixel( x, code * 8 + y, pen );
            }
        }
    }
}

void Phoenix::onVideoROMsChanged()
{
    rebuildPalette();
    decodeCharset( main_board_->bgtiles_, &bg_char_data_ );
    decodeCharset( main_board_->fgtiles_, &fg_char_data_ );
}

// -----------------------------------------------------------------------------
// Video
//
// Based directly on attached phoenix_vh_screenrefresh():
// - background tiles come from current_ram_page[0x800 + offs]
// - foreground tiles come from current_ram_page[offs]
// - color = (code >> 5) + 8 * palette_bank
// - background is copied with horizontal scroll
// - foreground uses pen 0 transparency except first column (sx == 0), which is solid
//
// Final framebuffer is rotated 90° clockwise so Phoenix appears vertical.
// -----------------------------------------------------------------------------
TBitmapIndexed * Phoenix::renderVideo()
{
    TBitBlock * bits = screen()->bits();
    bits->fill( 0 );

    unsigned page = main_board_->video_reg_ & 0x01;
    unsigned palette_bank = (main_board_->video_reg_ >> 1) & 0x01;
    unsigned char * current_ram_page = main_board_->paged_ram_[page];

    // -------------------------------------------------------------------------
    // Background layer from current_ram_page[0x800 + offs], scrolled
    // -------------------------------------------------------------------------
    for( int offs = (int)VisibleRamSize - 1; offs >= 0; offs-- ) {
        int code = current_ram_page[BackgroundVideoRamOffset + offs];
        int sx = offs % 32;
        int sy = offs / 32;

        int color_group = (code >> 5) + 8 * (int)palette_bank;

        for( int py=0; py<8; py++ ) {
            unsigned char * src = bg_char_data_.scanline_data( code * 8 + py );

            int raw_y = sy * 8 + py;
            int scrolled_raw_x_base = sx * 8 - (int)main_board_->scroll_reg_;

            for( int px=0; px<8; px++ ) {
                int raw_x = scrolled_raw_x_base + px;

                while( raw_x < 0 ) raw_x += RawWidth;
                while( raw_x >= RawWidth ) raw_x -= RawWidth;

                if( raw_y < 0 || raw_y >= RawHeight )
                    continue;

                unsigned char pen = src[px];
                unsigned char finalColor = (unsigned char)((pen * 8) + color_group);

                // ROT90 clockwise: raw(x,y) -> final( RawHeight-1-y, x )
                int dx = (RawHeight - 1) - raw_y;
                int dy = raw_x;

                if( dx >= 0 && dx < ScreenWidth && dy >= 0 && dy < ScreenHeight ) {
                    bits->scanline_data(dy)[dx] = finalColor;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Foreground layer from current_ram_page[offs]
    // First column opaque, others transparent on pen 0
    // -------------------------------------------------------------------------
    for( int offs = (int)VisibleRamSize - 1; offs >= 0; offs-- ) {
        int code = current_ram_page[offs];
        int sx = offs % 32;
        int sy = offs / 32;

        int color_group = (code >> 5) + 8 * (int)palette_bank;

        for( int py=0; py<8; py++ ) {
            unsigned char * src = fg_char_data_.scanline_data( code * 8 + py );
            int raw_y = sy * 8 + py;

            if( raw_y < 0 || raw_y >= RawHeight )
                continue;

            for( int px=0; px<8; px++ ) {
                int raw_x = sx * 8 + px;
                if( raw_x < 0 || raw_x >= RawWidth )
                    continue;

                unsigned char pen = src[px];

                if( sx >= 1 ) {
                    if( pen == 0 )
                        continue;
                }

                unsigned char finalColor = (unsigned char)(32 + (pen * 8) + color_group);

                int dx = (RawHeight - 1) - raw_y;
                int dy = raw_x;

                if( dx >= 0 && dx < ScreenWidth && dy >= 0 && dy < ScreenHeight ) {
                    bits->scanline_data(dy)[dx] = finalColor;
                }
            }
        }
    }

    return screen();
}