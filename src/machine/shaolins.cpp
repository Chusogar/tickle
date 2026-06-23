#include "shaolins.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Constantes hardware
// -----------------------------------------------------------------------------
enum {
    RawWidth            = 256,
    RawHeight           = 224,
    ScreenWidth         = 224,   // rotado 90° para Tickle
    ScreenHeight        = 256,
    ScreenColors        = 256,
    VideoFrequency      = 60,
    CpuClock            = 1536000,
    CpuCyclesPerFrame   = CpuClock / VideoFrequency,
    CpuSlices           = 8
};

enum {
    NumChars   = 512,
    NumSprites = 256
};

enum {
    EfRom03,
    EfRom04,
    EfRom05,

    EfGfx06,
    EfGfx07,

    EfSpr02,
    EfSpr01,

    EfProm10,
    EfProm11,
    EfProm12,
    EfProm09,
    EfProm08
};

// -----------------------------------------------------------------------------
// Machine info
// -----------------------------------------------------------------------------
static TMachineInfo ShaolinsInfo = {
    "shaolins", "Shao-lin's Road", "Konami", 1985,
    ScreenWidth, ScreenHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler regShaolins( &ShaolinsInfo, Shaolins::createInstance );

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static inline unsigned char bitAtLSB( const unsigned char * src, unsigned bitIndex )
{
    return (src[bitIndex >> 3] >> (bitIndex & 7)) & 1;
}

// -----------------------------------------------------------------------------
// Main board
// -----------------------------------------------------------------------------
ShaolinsMainBoard::ShaolinsMainBoard( Shaolins * drv )
{
    drv_ = drv;
    cpu_ = new M6809( *this );
}

void ShaolinsMainBoard::reset()
{
    cpu_->reset();
}

void ShaolinsMainBoard::run( unsigned cycles )
{
    cpu_->run( cycles );
}

unsigned char ShaolinsMainBoard::readByte( unsigned addr )
{
    return drv_->mainRead( addr & 0xFFFF );
}

void ShaolinsMainBoard::writeByte( unsigned addr, unsigned char value )
{
    drv_->mainWrite( addr & 0xFFFF, value );
}

// -----------------------------------------------------------------------------
// Driver
// -----------------------------------------------------------------------------
Shaolins::Shaolins()
    : char_data_( 8, 8 * NumChars ),
      sprite_data_( 16, 16 * NumSprites )
{
    memset( main_rom_, 0, sizeof(main_rom_) );
    memset( gfx1_,     0, sizeof(gfx1_) );
    memset( gfx2_,     0, sizeof(gfx2_) );
    memset( proms_,    0, sizeof(proms_) );

    memset( ram2_,      0, sizeof(ram2_) );      // 0x2800-0x2bff
    memset( colorram_,  0, sizeof(colorram_) );  // 0x3800-0x3bff
    memset( workram_,   0, sizeof(workram_) );   // 0x3000-0x33ff (0x3100-0x33ff=spriteram)
    memset( videoram_,  0, sizeof(videoram_) );  // 0x3c00-0x3fff
    memset( videoram2_, 0, sizeof(videoram2_) ); // 0x3400-0x37ff

    nmi_enable_   = 0;
    palette_bank_ = 0;
    scroll_       = 0;
    sound0_       = 0;
    sound1_       = 0;

    // inputs active-low
    in_system_ = 0xFF;
    in_p1_     = 0xFF;
    in_p2_     = 0xFF;
    dsw1_      = 0xFF;
    dsw2_      = 0xFF;
    test_      = 0xFF;

    refresh_roms_ = false;

    board_ = new ShaolinsMainBoard( this );

    createScreen( ScreenWidth, ScreenHeight, ScreenColors );

    // Controles 4-way + 2 botones
    eventHandler()->add( idCoinSlot1,       ptInverted, &in_system_, 0x01 );
    eventHandler()->add( idCoinSlot2,       ptInverted, &in_system_, 0x02 );
    eventHandler()->add( idKeyStartPlayer1, ptInverted, &in_system_, 0x08 );
    eventHandler()->add( idKeyStartPlayer2, ptInverted, &in_system_, 0x10 );

    eventHandler()->add( idKeyP1Action1,    ptInverted, &in_p1_, 0x10 );
    eventHandler()->add( idKeyP1Action2,    ptInverted, &in_p1_, 0x20 );
    eventHandler()->add( idKeyP2Action1,    ptInverted, &in_p2_, 0x10 );
    eventHandler()->add( idKeyP2Action2,    ptInverted, &in_p2_, 0x20 );

    setJoystickHandler( 0, new TJoystickToPortHandler(idJoyP1Joystick1, ptInverted, jm4Way) );
    joystickHandler(0)->setPort( jpLeft,  &in_p1_, 0x01 );
    joystickHandler(0)->setPort( jpRight, &in_p1_, 0x02 );
    joystickHandler(0)->setPort( jpUp,    &in_p1_, 0x04 );
    joystickHandler(0)->setPort( jpDown,  &in_p1_, 0x08 );

    setJoystickHandler( 1, new TJoystickToPortHandler(idJoyP2Joystick1, ptInverted, jm4Way) );
    joystickHandler(1)->setPort( jpLeft,  &in_p2_, 0x01 );
    joystickHandler(1)->setPort( jpRight, &in_p2_, 0x02 );
    joystickHandler(1)->setPort( jpUp,    &in_p2_, 0x04 );
    joystickHandler(1)->setPort( jpDown,  &in_p2_, 0x08 );

    registerDriver( ShaolinsInfo );
}

Shaolins::~Shaolins()
{
    delete board_;
}

bool Shaolins::initialize( TMachineDriverInfo * info )
{
    // Set parent shaolins
    resourceHandler()->add( EfRom03, "477l03.d9",  0x2000, efROM,      main_rom_ + 0x6000 );
    resourceHandler()->add( EfRom04, "477l04.d10", 0x4000, efROM,      main_rom_ + 0x8000 );
    resourceHandler()->add( EfRom05, "477l05.d11", 0x4000, efROM,      main_rom_ + 0xC000 );

    resourceHandler()->add( EfGfx06, "477j06.a10", 0x2000, efVideoROM, gfx1_ + 0x0000 );
    resourceHandler()->add( EfGfx07, "477j07.a11", 0x2000, efVideoROM, gfx1_ + 0x2000 );

    resourceHandler()->add( EfSpr02, "477j02.h15", 0x4000, efVideoROM, gfx2_ + 0x0000 );
    resourceHandler()->add( EfSpr01, "477j01.h14", 0x4000, efVideoROM, gfx2_ + 0x4000 );

    resourceHandler()->add( EfProm10, "477j10.a12", 0x0100, efPalettePROM, proms_ + 0x000 );
    resourceHandler()->add( EfProm11, "477j11.a13", 0x0100, efPalettePROM, proms_ + 0x100 );
    resourceHandler()->add( EfProm12, "477j12.a14", 0x0100, efPalettePROM, proms_ + 0x200 );
    resourceHandler()->add( EfProm09, "477j09.b8",  0x0100, efPalettePROM, proms_ + 0x300 ); // chars lookup
    resourceHandler()->add( EfProm08, "477j08.f16", 0x0100, efPalettePROM, proms_ + 0x400 ); // sprites lookup

    resourceHandler()->assignToMachineDriverInfo( info );

    refresh_roms_ = true;
    return true;
}

bool Shaolins::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    bool ok = (0 == resourceHandler()->handle( id, buf, len ));
    if( ok ) refresh_roms_ = true;
    return ok;
}

bool Shaolins::handleInputEvent( unsigned device, unsigned param, void * data )
{
    return TStandardMachine::handleInputEvent( device, param, data );
}

void Shaolins::reset()
{
    memset( ram2_,      0, sizeof(ram2_) );
    memset( colorram_,  0, sizeof(colorram_) );
    memset( workram_,   0, sizeof(workram_) );
    memset( videoram_,  0, sizeof(videoram_) );
    memset( videoram2_, 0, sizeof(videoram2_) );

    nmi_enable_   = 0;
    palette_bank_ = 0;
    scroll_       = 0;
    sound0_       = 0;
    sound1_       = 0;

    in_system_ = 0xFF;
    in_p1_     = 0xFF;
    in_p2_     = 0xFF;
    dsw1_      = 0xFF;
    dsw2_      = 0xFF;
    test_      = 0xFF;

    board_->reset();
}

// -----------------------------------------------------------------------------
// Memoria principal
// Mapeo corregido según el driver MAME:
// 0x3000-0x30ff = RAM
// 0x3100-0x33ff = sprite RAM
// 0x3400-0x37ff = RAM
// 0x3800-0x3bff = color RAM
// 0x3c00-0x3fff = video RAM
// -----------------------------------------------------------------------------
unsigned char Shaolins::mainRead( unsigned addr )
{
    if( addr == 0x0500 ) return dsw1_;
    if( addr == 0x0600 ) return dsw2_;
    if( addr == 0x0700 ) return in_system_;
    if( addr == 0x0701 ) return in_p1_;
    if( addr == 0x0702 ) return in_p2_;
    if( addr == 0x0703 ) return test_;

    if( addr >= 0x2800 && addr <= 0x2BFF ) return ram2_[addr - 0x2800];

    if( addr >= 0x3000 && addr <= 0x33FF ) return workram_[addr - 0x3000];
    if( addr >= 0x3400 && addr <= 0x37FF ) return videoram2_[addr - 0x3400];
    if( addr >= 0x3800 && addr <= 0x3BFF ) return colorram_[addr - 0x3800];
    if( addr >= 0x3C00 && addr <= 0x3FFF ) return videoram_[addr - 0x3C00];

    if( addr >= 0x4000 && addr <= 0x5FFF ) return 0xFF; // machine checks for extra rom
    if( addr >= 0x6000 ) return main_rom_[addr];

    return 0xFF;
}

void Shaolins::mainWrite( unsigned addr, unsigned char value )
{
    switch( addr )
    {
        case 0x0000:
            // bit0 = flip screen, bit1 = nmi enable, bits 3/4 = coin counters
            nmi_enable_ = value;
            return;

        case 0x0100:
            // watchdog
            return;

        case 0x0300:
            // SN76489 #0 (stub en esta pasada)
            sound0_ = value;
            return;

        case 0x0400:
            // SN76489 #1 (stub en esta pasada)
            sound1_ = value;
            return;

        case 0x0800:
            // latch de SN #0, ignorado como en MAME
            return;

        case 0x1000:
            // latch de SN #1, ignorado como en MAME
            return;

        case 0x1800:
            palette_bank_ = value & 0x07;
            return;

        case 0x2000:
            scroll_ = value;
            return;
    }

    if( addr >= 0x2800 && addr <= 0x2BFF ) {
        ram2_[addr - 0x2800] = value;
        return;
    }

    if( addr >= 0x3000 && addr <= 0x33FF ) {
        workram_[addr - 0x3000] = value;
        return;
    }

    if( addr >= 0x3400 && addr <= 0x37FF ) {
        videoram2_[addr - 0x3400] = value;
        return;
    }

    if( addr >= 0x3800 && addr <= 0x3BFF ) {
        colorram_[addr - 0x3800] = value;
        return;
    }

    if( addr >= 0x3C00 && addr <= 0x3FFF ) {
        videoram_[addr - 0x3C00] = value;
        return;
    }
}

// -----------------------------------------------------------------------------
// Paleta / decode
// -----------------------------------------------------------------------------
void Shaolins::rebuildPalette()
{
    // Pesos clásicos del driver antiguo / MAME2003
    for( int i=0; i<0x100; i++ ) {
        int bit0, bit1, bit2, bit3;

        bit0 = (proms_[0x000 + i] >> 0) & 0x01;
        bit1 = (proms_[0x000 + i] >> 1) & 0x01;
        bit2 = (proms_[0x000 + i] >> 2) & 0x01;
        bit3 = (proms_[0x000 + i] >> 3) & 0x01;
        int r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

        bit0 = (proms_[0x100 + i] >> 0) & 0x01;
        bit1 = (proms_[0x100 + i] >> 1) & 0x01;
        bit2 = (proms_[0x100 + i] >> 2) & 0x01;
        bit3 = (proms_[0x100 + i] >> 3) & 0x01;
        int g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

        bit0 = (proms_[0x200 + i] >> 0) & 0x01;
        bit1 = (proms_[0x200 + i] >> 1) & 0x01;
        bit2 = (proms_[0x200 + i] >> 2) & 0x01;
        bit3 = (proms_[0x200 + i] >> 3) & 0x01;
        int b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

        palette()->setColor( i, TPalette::encodeColor(r,g,b) );
    }
}

void Shaolins::decodeChars()
{
    // Decodificación práctica para 8x8 4bpp packed en dos ROMs de 0x2000
    for( int code=0; code<NumChars; code++ ) {
        unsigned base = code * 32;

        for( int y=0; y<8; y++ ) {
            unsigned char p0 = gfx1_[base + y];
            unsigned char p1 = gfx1_[base + y + 8];
            unsigned char p2 = gfx1_[base + y + 16];
            unsigned char p3 = gfx1_[base + y + 24];

            for( int x=0; x<8; x++ ) {
                int bit = 7 - x;
                unsigned char pen =
                    (((p0 >> bit) & 1) << 0) |
                    (((p1 >> bit) & 1) << 1) |
                    (((p2 >> bit) & 1) << 2) |
                    (((p3 >> bit) & 1) << 3);

                char_data_.setPixel( x, code * 8 + y, pen );
            }
        }
    }
}

void Shaolins::decodeSprites()
{
    // Decodificación práctica 16x16 4bpp
    for( int code=0; code<NumSprites; code++ ) {
        unsigned base = code * 64;

        for( int y=0; y<16; y++ ) {
            unsigned char p0 = gfx2_[base + y];
            unsigned char p1 = gfx2_[base + y + 16];
            unsigned char p2 = gfx2_[base + y + 32];
            unsigned char p3 = gfx2_[base + y + 48];

            for( int x=0; x<8; x++ ) {
                int bit = 7 - x;
                unsigned char penL =
                    (((p0 >> bit) & 1) << 0) |
                    (((p1 >> bit) & 1) << 1) |
                    (((p2 >> bit) & 1) << 2) |
                    (((p3 >> bit) & 1) << 3);

                sprite_data_.setPixel( x, code * 16 + y, penL );
            }

            unsigned char q0 = gfx2_[base + y + 8];
            unsigned char q1 = gfx2_[base + y + 24];
            unsigned char q2 = gfx2_[base + y + 40];
            unsigned char q3 = gfx2_[base + y + 56];

            for( int x=0; x<8; x++ ) {
                int bit = 7 - x;
                unsigned char penR =
                    (((q0 >> bit) & 1) << 0) |
                    (((q1 >> bit) & 1) << 1) |
                    (((q2 >> bit) & 1) << 2) |
                    (((q3 >> bit) & 1) << 3);

                sprite_data_.setPixel( 8 + x, code * 16 + y, penR );
            }
        }
    }
}

void Shaolins::onVideoROMsChanged()
{
    rebuildPalette();
    decodeChars();
    decodeSprites();
}

// -----------------------------------------------------------------------------
// Vídeo
// -----------------------------------------------------------------------------
void Shaolins::drawBackground()
{
    TBitBlock * bits = screen()->bits();
    bits->fill( 0 );

    // Tilemap 32x32; MAME real usa colorram en 0x3800 y videoram en 0x3c00
    for( int offs=0; offs<0x0400; offs++ ) {
        int tx = offs & 0x1F;
        int ty = offs >> 5;

        unsigned char code = videoram_[offs];
        unsigned char attr = colorram_[offs];

        int tile_code = code | ((attr & 0x40) << 2);
        int colorattr = attr & 0x0F;
        bool flipx = (attr & 0x80) != 0;

        int raw_x = tx * 8;
        int raw_y = ((ty * 8) - scroll_) & 0xFF;

        for( int y=0; y<8; y++ ) {
            unsigned char * src = char_data_.scanline_data( (tile_code % NumChars) * 8 + y );

            for( int x=0; x<8; x++ ) {
                unsigned char pen = src[ flipx ? (7 - x) : x ];

                // lookup PROM chars en proms_[0x300]
                // chars usan 0x10-0x1f dentro del banco seleccionado
                unsigned lookup = proms_[0x300 + ((colorattr << 4) | (pen & 0x0F))] & 0x0F;
                int pal_index = ((palette_bank_ & 0x07) << 5) | 0x10 | lookup;

                int sx = raw_x + x;
                int sy = (raw_y + y) & 0xFF;

                if( sy >= RawHeight )
                    continue;

                // rotación CW
                int dx = (RawHeight - 1) - sy;
                int dy = sx;

                if( dx >= 0 && dx < ScreenWidth && dy >= 0 && dy < ScreenHeight ) {
                    bits->scanline_data(dy)[dx] = (unsigned char)(pal_index & 0xFF);
                }
            }
        }
    }
}

void Shaolins::drawSprites()
{
    TBitBlock * bits = screen()->bits();

    // Sprite RAM real: 0x3100-0x33ff => workram_[0x100..0x3ff]
    unsigned char * spr = &workram_[0x100];

    for( int offs=0; offs<0x300; offs += 4 ) {
        unsigned char sy   = spr[offs + 0];
        unsigned char attr = spr[offs + 1];
        unsigned char code = spr[offs + 2];
        unsigned char sx   = spr[offs + 3];

        int sprite_code = code | ((attr & 0x40) << 2);
        int colorattr   = attr & 0x0F;
        bool flipx      = (attr & 0x10) != 0;
        bool flipy      = (attr & 0x20) != 0;

        int raw_x = sx;
        int raw_y = sy - 16;

        for( int y=0; y<16; y++ ) {
            int srcy = flipy ? (15 - y) : y;
            unsigned char * src = sprite_data_.scanline_data( (sprite_code % NumSprites) * 16 + srcy );

            for( int x=0; x<16; x++ ) {
                int srcx = flipx ? (15 - x) : x;
                unsigned char pen = src[srcx];
                if( pen == 0 ) continue;

                // lookup PROM sprites en proms_[0x400]
                unsigned lookup = proms_[0x400 + ((colorattr << 4) | (pen & 0x0F))] & 0x0F;
                int pal_index = ((palette_bank_ & 0x07) << 5) | lookup;

                int sx2 = raw_x + x;
                int sy2 = raw_y + y;

                if( sx2 < 0 || sx2 >= RawWidth || sy2 < 0 || sy2 >= RawHeight )
                    continue;

                int dx = (RawHeight - 1) - sy2;
                int dy = sx2;

                if( dx >= 0 && dx < ScreenWidth && dy >= 0 && dy < ScreenHeight ) {
                    bits->scanline_data(dy)[dx] = (unsigned char)(pal_index & 0xFF);
                }
            }
        }
    }
}

TBitmapIndexed * Shaolins::renderVideo()
{
    drawBackground();
    drawSprites();
    return screen();
}

// -----------------------------------------------------------------------------
// Frame
// -----------------------------------------------------------------------------
void Shaolins::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    (void)samplesPerFrame;
    (void)samplingRate;

    if( refresh_roms_ ) {
        onVideoROMsChanged();
        refresh_roms_ = false;
    }

    const unsigned slice_cycles = CpuCyclesPerFrame / CpuSlices;

    for( int i=0; i<CpuSlices; i++ ) {
        board_->run( slice_cycles );

        // Driver histórico: NMIs en slices impares si están habilitadas
        if( (i & 1) && (nmi_enable_ & 0x02) ) {
            board_->cpu_->nmi();
        }
    }

    // IRQ principal de VBLANK
    board_->cpu_->irq();

    frame->setVideo( renderVideo() );
}
