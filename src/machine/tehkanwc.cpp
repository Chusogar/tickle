#include "tehkanwc.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Hardware constants derived from the uploaded emulator
// -----------------------------------------------------------------------------
enum {
    ScreenWidth         = 256,
    ScreenHeight        = 224,
    ScreenColors        = 768,
    VideoFrequency      = 60,
    CpuClock            = 4608000,
    CpuCyclesPerFrame   = CpuClock / VideoFrequency,
    CpuSlices           = 10
};

enum {
    NumChars   = 512,
    NumSprites = 512,
    NumTiles   = 1024
};

enum {
    EfMain1,
    EfMain2,
    EfMain3,
    EfSub4,
    EfSnd6,
    EfChar12,
    EfSpr8,
    EfSpr7,
    EfBg11,
    EfBg9,
    EfAdpcm5
};

static TMachineInfo TehkanWCInfo = {
    "tehkanwc", "Tehkan World Cup", "Tehkan", 1985,
    ScreenWidth, ScreenHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler regTehkanWC( &TehkanWCInfo, TehkanWCDriver::createInstance );

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static inline unsigned char readNibblePixel( const unsigned char * src, unsigned byteOff, bool highNibble )
{
    unsigned char b = src[byteOff];
    return highNibble ? ((b >> 4) & 0x0F) : (b & 0x0F);
}

// -----------------------------------------------------------------------------
// Main board
// -----------------------------------------------------------------------------
TehkanWCMainBoard::TehkanWCMainBoard( TehkanWCDriver * drv )
{
    drv_ = drv;
    cpu_ = new Z80( *this );
}

TehkanWCMainBoard::~TehkanWCMainBoard()
{
    delete cpu_;
}

void TehkanWCMainBoard::reset()
{
    cpu_->reset();
}

void TehkanWCMainBoard::run( unsigned cycles )
{
    cpu_->run( cycles );
}

unsigned char TehkanWCMainBoard::readByte( unsigned addr ) { return drv_->mainRead(addr); }
void TehkanWCMainBoard::writeByte( unsigned addr, unsigned char value ) { drv_->mainWrite(addr, value); }
unsigned char TehkanWCMainBoard::readPort( unsigned ) { return 0xFF; }
void TehkanWCMainBoard::writePort( unsigned, unsigned char ) {}

// -----------------------------------------------------------------------------
// Sub board
// -----------------------------------------------------------------------------
TehkanWCSubBoard::TehkanWCSubBoard( TehkanWCDriver * drv )
{
    drv_ = drv;
    cpu_ = new Z80( *this );
}

TehkanWCSubBoard::~TehkanWCSubBoard()
{
    delete cpu_;
}

void TehkanWCSubBoard::reset()
{
    cpu_->reset();
}

void TehkanWCSubBoard::run( unsigned cycles )
{
    cpu_->run( cycles );
}

unsigned char TehkanWCSubBoard::readByte( unsigned addr ) { return drv_->subRead(addr); }
void TehkanWCSubBoard::writeByte( unsigned addr, unsigned char value ) { drv_->subWrite(addr, value); }
unsigned char TehkanWCSubBoard::readPort( unsigned ) { return 0xFF; }
void TehkanWCSubBoard::writePort( unsigned, unsigned char ) {}

// -----------------------------------------------------------------------------
// Sound board
// -----------------------------------------------------------------------------
TehkanWCSoundBoard::TehkanWCSoundBoard( TehkanWCDriver * drv )
{
    drv_ = drv;
    cpu_ = new Z80( *this );
}

TehkanWCSoundBoard::~TehkanWCSoundBoard()
{
    delete cpu_;
}

void TehkanWCSoundBoard::reset()
{
    cpu_->reset();
}

void TehkanWCSoundBoard::run( unsigned cycles )
{
    if( drv_->snd_nmi_pending_ ) {
        drv_->snd_nmi_pending_ = false;
        cpu_->nmi();
    }
    cpu_->run( cycles );
}

unsigned char TehkanWCSoundBoard::readByte( unsigned addr ) { return drv_->sndRead(addr); }
void TehkanWCSoundBoard::writeByte( unsigned addr, unsigned char value ) { drv_->sndWrite(addr, value); }
unsigned char TehkanWCSoundBoard::readPort( unsigned ) { return 0xFF; }
void TehkanWCSoundBoard::writePort( unsigned, unsigned char ) {}

// -----------------------------------------------------------------------------
// Driver ctor/dtor/reset
// -----------------------------------------------------------------------------
TehkanWCDriver::TehkanWCDriver()
    : char_data_( 8, 8 * NumChars ),
      sprite_data_( 16, 16 * NumSprites ),
      tile_data_( 16, 8 * NumTiles ),
      bg_bitmap_( 512, 256 )
{
    memset( main_rom_,   0, sizeof(main_rom_) );
    memset( sub_rom_,    0, sizeof(sub_rom_) );
    memset( snd_rom_,    0, sizeof(snd_rom_) );
    memset( main_ram_,   0, sizeof(main_ram_) );
    memset( sub_ram_,    0, sizeof(sub_ram_) );
    memset( snd_ram_,    0, sizeof(snd_ram_) );
    memset( shared_ram_, 0, sizeof(shared_ram_) );
    memset( fg_vram_,    0, sizeof(fg_vram_) );
    memset( fg_cram_,    0, sizeof(fg_cram_) );
    memset( pal_ram_,    0, sizeof(pal_ram_) );
    memset( bg_vram_,    0, sizeof(bg_vram_) );
    memset( sprite_ram_, 0, sizeof(sprite_ram_) );
    memset( gfx1_,       0, sizeof(gfx1_) );
    memset( gfx2_,       0, sizeof(gfx2_) );
    memset( gfx3_,       0, sizeof(gfx3_) );
    memset( adpcm_,      0, sizeof(adpcm_) );
    memset( bg_dirty_,   1, sizeof(bg_dirty_) );

    scroll_x_lo_ = 0;
    scroll_x_hi_ = 0;
    scroll_y_    = 0;

    coins_ = 0xFF;
    btn0_  = 0xFF;
    btn1_  = 0xFF;
    dsw_[0] = 0xFF;
    dsw_[1] = 0xFF;
    dsw_[2] = 0xFF;

    track0_[0] = track0_[1] = 0;
    track1_[0] = track1_[1] = 0;

    p1_left_ = p1_right_ = p1_up_ = p1_down_ = false;
    p2_left_ = p2_right_ = p2_up_ = p2_down_ = false;
    p1_kick_ = p2_kick_ = false;

    soundlatch_ = 0;
    sound_answer_ = 0;
    snd_nmi_pending_ = false;

    sub_halted_ = false;
    sub_was_halted_ = false;

    refresh_roms_ = false;

    main_board_ = new TehkanWCMainBoard(this);
    sub_board_  = new TehkanWCSubBoard(this);
    snd_board_  = new TehkanWCSoundBoard(this);

    createScreen( ScreenWidth, ScreenHeight, ScreenColors );
    registerDriver( TehkanWCInfo );
}

TehkanWCDriver::~TehkanWCDriver()
{
    delete main_board_;
    delete sub_board_;
    delete snd_board_;
}

void TehkanWCDriver::reset()
{
    memset( main_ram_,   0, sizeof(main_ram_) );
    memset( sub_ram_,    0, sizeof(sub_ram_) );
    memset( snd_ram_,    0, sizeof(snd_ram_) );
    memset( shared_ram_, 0, sizeof(shared_ram_) );
    memset( fg_vram_,    0, sizeof(fg_vram_) );
    memset( fg_cram_,    0, sizeof(fg_cram_) );
    memset( pal_ram_,    0, sizeof(pal_ram_) );
    memset( bg_vram_,    0, sizeof(bg_vram_) );
    memset( sprite_ram_, 0, sizeof(sprite_ram_) );
    memset( bg_dirty_,   1, sizeof(bg_dirty_) );

    scroll_x_lo_ = 0;
    scroll_x_hi_ = 0;
    scroll_y_    = 0;

    track0_[0] = track0_[1] = 0;
    track1_[0] = track1_[1] = 0;

    soundlatch_ = 0;
    sound_answer_ = 0;
    snd_nmi_pending_ = false;

    sub_halted_ = false;
    sub_was_halted_ = false;

    // Paleta provisional para evitar negro total antes de que el juego escriba palram
    for( int i=0; i<ScreenColors; i++ ) {
        int r = ( i       & 0x0F ) * 17;
        int g = ((i >> 4) & 0x0F ) * 17;
        int b = ((i >> 8) & 0x03 ) * 85;
        palette()->setColor( i, TPalette::encodeColor(r,g,b) );
    }

    main_board_->reset();
    sub_board_->reset();
    snd_board_->reset();
}

// -----------------------------------------------------------------------------
// ROM loading
// -----------------------------------------------------------------------------
bool TehkanWCDriver::initialize( TMachineDriverInfo * info )
{
    resourceHandler()->add( EfMain1,  "twc-1.bin",  0x4000, efROM,      main_rom_ + 0x0000 );
    resourceHandler()->add( EfMain2,  "twc-2.bin",  0x4000, efROM,      main_rom_ + 0x4000 );
    resourceHandler()->add( EfMain3,  "twc-3.bin",  0x4000, efROM,      main_rom_ + 0x8000 );

    resourceHandler()->add( EfSub4,   "twc-4.bin",  0x8000, efROM,      sub_rom_  + 0x0000 );
    resourceHandler()->add( EfSnd6,   "twc-6.bin",  0x4000, efROM,      snd_rom_  + 0x0000 );

    resourceHandler()->add( EfChar12, "twc-12.bin", 0x4000, efVideoROM, gfx1_ + 0x0000 );

    resourceHandler()->add( EfSpr8,   "twc-8.bin",  0x8000, efVideoROM, gfx2_ + 0x0000 );
    resourceHandler()->add( EfSpr7,   "twc-7.bin",  0x8000, efVideoROM, gfx2_ + 0x8000 );

    resourceHandler()->add( EfBg11,   "twc-11.bin", 0x8000, efVideoROM, gfx3_ + 0x0000 );
    resourceHandler()->add( EfBg9,    "twc-9.bin",  0x8000, efVideoROM, gfx3_ + 0x8000 );

    resourceHandler()->add( EfAdpcm5, "twc-5.bin",  0x4000, efROM,      adpcm_ + 0x0000 );

    resourceHandler()->assignToMachineDriverInfo( info );
    refresh_roms_ = true;
    return true;
}

bool TehkanWCDriver::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    bool ok = (0 == resourceHandler()->handle( id, buf, len ));
    if( ok )
        refresh_roms_ = true;
    return ok;
}

// -----------------------------------------------------------------------------
// Input handling
// -----------------------------------------------------------------------------
bool TehkanWCDriver::handleInputEvent( unsigned device, unsigned param, void * )
{
    bool pressed = (param & 1) != 0;

    switch( device )
    {
        case idCoinSlot1:
            if( pressed ) coins_ &= (unsigned char)~0x01;
            else          coins_ |= 0x01;
            return true;

        case idCoinSlot2:
            if( pressed ) coins_ &= (unsigned char)~0x02;
            else          coins_ |= 0x02;
            return true;

        case idKeyStartPlayer1:
            if( pressed ) btn0_ &= (unsigned char)~0x01;
            else          btn0_ |= 0x01;
            return true;

        case idKeyStartPlayer2:
            if( pressed ) btn1_ &= (unsigned char)~0x01;
            else          btn1_ |= 0x01;
            return true;

        case idKeyP1Action1:
            p1_kick_ = pressed;
            if( pressed ) btn0_ &= (unsigned char)~0x10;
            else          btn0_ |= 0x10;
            return true;

        case idKeyP2Action1:
            p2_kick_ = pressed;
            if( pressed ) btn1_ &= (unsigned char)~0x10;
            else          btn1_ |= 0x10;
            return true;

        case idJoyP1Joystick1:
        {
            int x = TInput::getXPosFromParam(param);
            int y = TInput::getYPosFromParam(param);
            p1_left_  = (x < -100);
            p1_right_ = (x >  100);
            p1_up_    = (y < -100);
            p1_down_  = (y >  100);
            return true;
        }

        case idJoyP2Joystick1:
        {
            int x = TInput::getXPosFromParam(param);
            int y = TInput::getYPosFromParam(param);
            p2_left_  = (x < -100);
            p2_right_ = (x >  100);
            p2_up_    = (y < -100);
            p2_down_  = (y >  100);
            return true;
        }
    }

    return TStandardMachine::handleInputEvent( device, param, 0 );
}

void TehkanWCDriver::updateTrackballDeltas()
{
    const int delta = 4;

    if( p1_left_ )  track0_[0] -= delta;
    if( p1_right_ ) track0_[0] += delta;
    if( p1_up_ )    track0_[1] -= delta;
    if( p1_down_ )  track0_[1] += delta;

    if( p2_left_ )  track1_[0] -= delta;
    if( p2_right_ ) track1_[0] += delta;
    if( p2_up_ )    track1_[1] -= delta;
    if( p2_down_ )  track1_[1] += delta;
}

// -----------------------------------------------------------------------------
// Memory maps
// -----------------------------------------------------------------------------
unsigned char TehkanWCDriver::mainRead( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr <= 0xBFFF ) return main_rom_[addr];
    if( addr >= 0xC000 && addr <= 0xC7FF ) return main_ram_[addr - 0xC000];
    if( addr >= 0xC800 && addr <= 0xCFFF ) return shared_ram_[addr - 0xC800];

    if( addr >= 0xD000 && addr <= 0xD3FF ) return fg_vram_[addr - 0xD000];
    if( addr >= 0xD400 && addr <= 0xD7FF ) return fg_cram_[addr - 0xD400];
    if( addr >= 0xD800 && addr <= 0xDDFF ) return pal_ram_[addr - 0xD800];
    if( addr >= 0xE000 && addr <= 0xE7FF ) return bg_vram_[addr - 0xE000];
    if( addr >= 0xE800 && addr <= 0xEBFF ) return sprite_ram_[addr - 0xE800];

    if( addr == 0xEC00 ) return scroll_x_lo_;
    if( addr == 0xEC01 ) return scroll_x_hi_;
    if( addr == 0xEC02 ) return scroll_y_;

    if( addr == 0xF800 ) return (unsigned char)track0_[0];
    if( addr == 0xF801 ) return (unsigned char)track0_[1];
    if( addr == 0xF802 ) return coins_;
    if( addr == 0xF803 ) return btn0_;

    if( addr == 0xF810 ) return (unsigned char)track1_[0];
    if( addr == 0xF811 ) return (unsigned char)track1_[1];
    if( addr == 0xF813 ) return btn1_;

    if( addr == 0xF820 ) return sound_answer_;

    if( addr == 0xF840 ) return dsw_[0];
    if( addr == 0xF850 ) return dsw_[1];
    if( addr == 0xF870 ) return dsw_[2];

    return 0xFF;
}

void TehkanWCDriver::mainWrite( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr >= 0xC000 && addr <= 0xC7FF ) { main_ram_[addr - 0xC000] = value; return; }
    if( addr >= 0xC800 && addr <= 0xCFFF ) { shared_ram_[addr - 0xC800] = value; return; }

    if( addr >= 0xD000 && addr <= 0xD3FF ) { fg_vram_[addr - 0xD000] = value; return; }
    if( addr >= 0xD400 && addr <= 0xD7FF ) { fg_cram_[addr - 0xD400] = value; return; }

    if( addr >= 0xD800 && addr <= 0xDDFF ) {
        pal_ram_[addr - 0xD800] = value;
        rebuildPalette();
        return;
    }

    if( addr >= 0xE000 && addr <= 0xE7FF ) {
        int off = addr - 0xE000;
        bg_vram_[off] = value;
        bg_dirty_[off >> 1] = true;
        return;
    }

    if( addr >= 0xE800 && addr <= 0xEBFF ) {
        sprite_ram_[addr - 0xE800] = value;
        return;
    }

    if( addr == 0xEC00 ) { scroll_x_lo_ = value; return; }
    if( addr == 0xEC01 ) { scroll_x_hi_ = value; return; }
    if( addr == 0xEC02 ) { scroll_y_    = value; return; }

    // Trackball reset / latches
    if( addr == 0xF800 ) { track0_[0] = (signed char)value; return; }
    if( addr == 0xF801 ) { track0_[1] = (signed char)value; return; }
    if( addr == 0xF810 ) { track1_[0] = (signed char)value; return; }
    if( addr == 0xF811 ) { track1_[1] = (signed char)value; return; }

    if( addr == 0xF820 ) {
        soundlatch_ = value;
        snd_nmi_pending_ = true;
        return;
    }

    // bit0 = 1 clear reset, bit0 = 0 assert reset
    if( addr == 0xF840 ) {
        sub_halted_ = ((value & 0x01) == 0);
        return;
    }
}

unsigned char TehkanWCDriver::subRead( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr <= 0x7FFF ) return sub_rom_[addr];
    if( addr >= 0x8000 && addr <= 0xC7FF ) return sub_ram_[addr - 0x8000];
    if( addr >= 0xC800 && addr <= 0xCFFF ) return shared_ram_[addr - 0xC800];

    if( addr >= 0xD000 && addr <= 0xD3FF ) return fg_vram_[addr - 0xD000];
    if( addr >= 0xD400 && addr <= 0xD7FF ) return fg_cram_[addr - 0xD400];
    if( addr >= 0xD800 && addr <= 0xDDFF ) return pal_ram_[addr - 0xD800];
    if( addr >= 0xE000 && addr <= 0xE7FF ) return bg_vram_[addr - 0xE000];
    if( addr >= 0xE800 && addr <= 0xEBFF ) return sprite_ram_[addr - 0xE800];

    if( addr == 0xEC00 ) return scroll_x_lo_;
    if( addr == 0xEC01 ) return scroll_x_hi_;
    if( addr == 0xEC02 ) return scroll_y_;

    return 0xFF;
}

void TehkanWCDriver::subWrite( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr >= 0x8000 && addr <= 0xC7FF ) { sub_ram_[addr - 0x8000] = value; return; }
    if( addr >= 0xC800 && addr <= 0xCFFF ) { shared_ram_[addr - 0xC800] = value; return; }

    if( addr >= 0xD000 && addr <= 0xD3FF ) { fg_vram_[addr - 0xD000] = value; return; }
    if( addr >= 0xD400 && addr <= 0xD7FF ) { fg_cram_[addr - 0xD400] = value; return; }

    if( addr >= 0xD800 && addr <= 0xDDFF ) {
        pal_ram_[addr - 0xD800] = value;
        rebuildPalette();
        return;
    }

    if( addr >= 0xE000 && addr <= 0xE7FF ) {
        int off = addr - 0xE000;
        bg_vram_[off] = value;
        bg_dirty_[off >> 1] = true;
        return;
    }

    if( addr >= 0xE800 && addr <= 0xEBFF ) {
        sprite_ram_[addr - 0xE800] = value;
        return;
    }

    if( addr == 0xEC00 ) { scroll_x_lo_ = value; return; }
    if( addr == 0xEC01 ) { scroll_x_hi_ = value; return; }
    if( addr == 0xEC02 ) { scroll_y_    = value; return; }
}

unsigned char TehkanWCDriver::sndRead( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr <= 0x3FFF ) return snd_rom_[addr];
    if( addr >= 0x4000 && addr <= 0x47FF ) return snd_ram_[addr - 0x4000];
    if( addr == 0xC000 ) return soundlatch_;

    return 0xFF;
}

void TehkanWCDriver::sndWrite( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr >= 0x4000 && addr <= 0x47FF ) {
        snd_ram_[addr - 0x4000] = value;
        return;
    }

    if( addr == 0xC000 ) {
        sound_answer_ = value;
        return;
    }

    // AY8910 / MSM5205 left stub in this pass.
}

// -----------------------------------------------------------------------------
// GFX decode
// Based on the uploaded standalone converter:
// - nibble-swapped order
// - even pixel -> high nibble
// -----------------------------------------------------------------------------
void TehkanWCDriver::decodeChars()
{
    for( int tile=0; tile<NumChars; tile++ ) {
        for( int row=0; row<8; row++ ) {
            int row_base = tile * 32 + row * 4;

            for( int col=0; col<8; col++ ) {
                int pair = col / 2;
                bool high = ((col & 1) == 0);
                unsigned char pix = readNibblePixel( gfx1_, row_base + pair, high );
                char_data_.setPixel( col, tile * 8 + row, pix );
            }
        }
    }
}

void TehkanWCDriver::decodeSprites()
{
    for( int spr=0; spr<NumSprites; spr++ ) {
        int base = spr * 128;

        for( int row=0; row<16; row++ ) {
            int row_base = base + ((row < 8) ? (row * 4) : ((16 + (row - 8)) * 4));

            for( int col=0; col<16; col++ ) {
                int col_group = col / 8;
                int col_in_g  = col & 7;
                int pair      = col_in_g / 2;
                bool high     = ((col_in_g & 1) == 0);

                unsigned char pix = readNibblePixel(
                    gfx2_,
                    row_base + col_group * 32 + pair,
                    high
                );

                sprite_data_.setPixel( col, spr * 16 + row, pix );
            }
        }
    }
}

void TehkanWCDriver::decodeTiles()
{
    for( int tile=0; tile<NumTiles; tile++ ) {
        int base = tile * 64;

        for( int row=0; row<8; row++ ) {
            int row_base = base + row * 4;

            for( int col=0; col<16; col++ ) {
                int col_group = col / 8;
                int col_in_g  = col & 7;
                int pair      = col_in_g / 2;
                bool high     = ((col_in_g & 1) == 0);

                unsigned char pix = readNibblePixel(
                    gfx3_,
                    row_base + col_group * 32 + pair,
                    high
                );

                tile_data_.setPixel( col, tile * 8 + row, pix );
            }
        }
    }
}

void TehkanWCDriver::rebuildPalette()
{
    const int ncolors = 0x0600 / 2;

    for( int i=0; i<ncolors && i<ScreenColors; i++ ) {
        int off = i * 2;

        // Exact standalone format:
        // word = (palram[off+1] << 8) | palram[off]
        unsigned word = ((unsigned)pal_ram_[off + 1] << 8) | pal_ram_[off + 0];

        // xxxxBBBBGGGGRRRR
        int r = ((word >> 0) & 0x0F) * 17;
        int g = ((word >> 4) & 0x0F) * 17;
        int b = ((word >> 8) & 0x0F) * 17;

        palette()->setColor( i, TPalette::encodeColor(r, g, b) );
    }
}

void TehkanWCDriver::onVideoROMsChanged()
{
    decodeChars();
    decodeSprites();
    decodeTiles();
    rebuildPalette();
    memset( bg_dirty_, 1, sizeof(bg_dirty_) );
}

// -----------------------------------------------------------------------------
// Video
// -----------------------------------------------------------------------------
void TehkanWCDriver::renderBgBitmap()
{
    for( int idx=0; idx<(0x0800/2); idx++ ) {
        if( !bg_dirty_[idx] ) continue;
        bg_dirty_[idx] = false;

        int off = idx * 2;
        unsigned char b0 = bg_vram_[off + 0];
        unsigned char b1 = bg_vram_[off + 1];

        int code  = b0 + ((b1 & 0x30) << 4);
        int color = (b1 & 0x0F);
        int flipx = (b1 >> 6) & 1;
        int flipy = (b1 >> 7) & 1;

        int sx = (off % 64) * 8;
        int sy = (off / 64) * 8;

        for( int row=0; row<8; row++ ) {
            int py = sy + (flipy ? (7 - row) : row);
            if( py < 0 || py >= 256 ) continue;

            unsigned char * src = tile_data_.scanline_data( (code % NumTiles) * 8 + row );
            unsigned char * dst = bg_bitmap_.scanline_data( py );

            for( int col=0; col<16; col++ ) {
                int px = sx + (flipx ? (15 - col) : col);
                px = ((px % 512) + 512) % 512;
                dst[px] = (unsigned char)(512 + color * 16 + src[col]);
            }
        }
    }
}

void TehkanWCDriver::drawFgLayer( bool high_priority )
{
    TBitBlock * bits = screen()->bits();

    for( int offs=0; offs<0x400; offs++ ) {
        int sx = (offs % 32) * 8;
        int sy = (offs / 32) * 8;

        unsigned char code  = fg_vram_[offs];
        unsigned char attr  = fg_cram_[offs];

        int color = attr & 0x0F;
        bool flipx = (attr & 0x40) != 0;
        bool flipy = (attr & 0x80) != 0;
        bool pri_hi = ((attr & 0x20) == 0); // bit5=0 over sprites, bit5=1 under sprites

        if( pri_hi != high_priority )
            continue;

        for( int row=0; row<8; row++ ) {
            int py = sy + (flipy ? (7 - row) : row);
            if( py < 0 || py >= ScreenHeight ) continue;

            unsigned char * src = char_data_.scanline_data( (code % NumChars) * 8 + row );
            unsigned char * dst = bits->scanline_data( py );

            for( int col=0; col<8; col++ ) {
                int px = sx + (flipx ? (7 - col) : col);
                if( px < 0 || px >= ScreenWidth ) continue;

                unsigned char pen = src[col];
                if( pen == 0 ) continue;

                dst[px] = (unsigned char)(color * 16 + pen);
            }
        }
    }
}

void TehkanWCDriver::drawSprites()
{
    TBitBlock * bits = screen()->bits();

    for( int offs=0; offs<0x400; offs += 4 ) {
        unsigned char sy   = sprite_ram_[offs + 0];
        unsigned char code = sprite_ram_[offs + 1];
        unsigned char attr = sprite_ram_[offs + 2];
        unsigned char sx   = sprite_ram_[offs + 3];

        int color = attr & 0x0F;
        bool flipx = (attr & 0x40) != 0;
        bool flipy = (attr & 0x80) != 0;
        int x = sx;
        int y = sy - 16;

        for( int row=0; row<16; row++ ) {
            int py = y + (flipy ? (15 - row) : row);
            if( py < 0 || py >= ScreenHeight ) continue;

            unsigned char * src = sprite_data_.scanline_data( (code % NumSprites) * 16 + row );
            unsigned char * dst = bits->scanline_data( py );

            for( int col=0; col<16; col++ ) {
                int px = x + (flipx ? (15 - col) : col);
                if( px < 0 || px >= ScreenWidth ) continue;

                unsigned char pen = src[col];
                if( pen == 0 ) continue;

                dst[px] = (unsigned char)(256 + color * 16 + pen);
            }
        }
    }
}

TBitmapIndexed * TehkanWCDriver::renderVideo()
{
    renderBgBitmap();

    TBitBlock * bits = screen()->bits();
    int scrollx = (int)scroll_x_lo_ + ((int)scroll_x_hi_ << 8);
    int scrolly = (int)scroll_y_;

    // 1) BG with scroll (sign corrected from standalone)
    for( int y=0; y<ScreenHeight; y++ ) {
        unsigned char * dst = bits->scanline_data( y );
        int srcy = ((y + 16 - scrolly) % 256 + 256) % 256;
        unsigned char * src = bg_bitmap_.scanline_data( srcy );

        for( int x=0; x<ScreenWidth; x++ ) {
            int srcx = ((x - scrollx) % 512 + 512) % 512;
            dst[x] = src[srcx];
        }
    }

    // 2) FG chars with priority bit5=1 (below sprites)
    drawFgLayer( false );

    // 3) Sprites
    drawSprites();

    // 4) FG chars with priority bit5=0 (above sprites)
    drawFgLayer( true );

    return screen();
}

// -----------------------------------------------------------------------------
// Frame
// -----------------------------------------------------------------------------
void TehkanWCDriver::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    (void)samplesPerFrame;
    (void)samplingRate;

    if( refresh_roms_ ) {
        onVideoROMsChanged();
        refresh_roms_ = false;
    }

    updateTrackballDeltas();

    const unsigned slice_cycles = CpuCyclesPerFrame / CpuSlices;

    for( int i=0; i<CpuSlices; i++ ) {
        main_board_->run( slice_cycles );

        if( !sub_halted_ ) {
            if( sub_was_halted_ ) {
                sub_board_->reset();
                sub_was_halted_ = false;
            }
            sub_board_->run( slice_cycles );
        }
        else {
            sub_was_halted_ = true;
        }

        snd_board_->run( slice_cycles );
    }

    // VBLANK IRQs to all CPUs, as indicated by the standalone emulator comments
    main_board_->cpu_->interrupt( 0x00 );

    if( !sub_halted_ ) {
        sub_board_->cpu_->interrupt( 0x00 );
    }

    snd_board_->cpu_->interrupt( 0x00 );

    frame->setVideo( renderVideo() );
}