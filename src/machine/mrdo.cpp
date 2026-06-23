#include "mrdo.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Hardware info
// -----------------------------------------------------------------------------
enum {
    // Hardware native (raw, un-rotated) visible area: 240 (H) x 192 (V)
    RawScreenWidth      = 240,
    RawScreenHeight     = 192,

    // Cabinet monitor is ROT270 -> displayed orientation is portrait 192x240
    ScreenWidth         = 192,
    ScreenHeight        = 240,

    ScreenColors        = 256,
    VideoFrequency      = 60,
    CpuClock            = 4000000,
    CpuCyclesPerFrame   = CpuClock / VideoFrequency
};

enum {
    EfMain0,
    EfMain1,
    EfMain2,
    EfMain3,

    EfGfx10,
    EfGfx11,

    EfGfx20,
    EfGfx21,

    EfGfx30,
    EfGfx31,

    EfProm0,
    EfProm1,
    EfProm2,
    EfProm3
};

// -----------------------------------------------------------------------------
// Machine info
// -----------------------------------------------------------------------------
static TMachineInfo MrDoInfo = {
    "mrdo", "Mr. Do!", "Universal", 1982,
    ScreenWidth, ScreenHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler regMrDo( &MrDoInfo, MrDo::createInstance );

// -----------------------------------------------------------------------------
// Sprite lookup table rebuilt from PROM #3 (f10--1.bin)
// Based on the original MAME vidhrdw conversion.
// -----------------------------------------------------------------------------
static unsigned char sprite_lookup_[64];

// -----------------------------------------------------------------------------
// Bit helper for MAME-like packed gfx layouts
// MAME bit offsets are LSB-based within each byte.
// -----------------------------------------------------------------------------
static inline unsigned char bitAtLSB( const unsigned char * src, unsigned bitIndex )
{
    return (src[bitIndex >> 3] >> (bitIndex & 7)) & 1;
}

// -----------------------------------------------------------------------------
// Main board
// -----------------------------------------------------------------------------
MrDoMainBoard::MrDoMainBoard()
{
    cpu_ = new Z80( *this );

    memset( main_rom_, 0, sizeof(main_rom_) );
    memset( gfx1_, 0, sizeof(gfx1_) );
    memset( gfx2_, 0, sizeof(gfx2_) );
    memset( gfx3_, 0, sizeof(gfx3_) );
    memset( proms_, 0, sizeof(proms_) );

    memset( bg_colorram_, 0, sizeof(bg_colorram_) );
    memset( bg_videoram_, 0, sizeof(bg_videoram_) );
    memset( fg_colorram_, 0, sizeof(fg_colorram_) );
    memset( fg_videoram_, 0, sizeof(fg_videoram_) );
    memset( spriteram_, 0, sizeof(spriteram_) );
    memset( workram_, 0, sizeof(workram_) );

    port0_ = 0xFF;
    port1_ = 0xFF;
    dsw1_  = 0xFF;
    dsw2_  = 0xFF;

    flipscreen_ = 0;
    priority_   = 0;
    scrollx_    = 0;
    scrolly_    = 0;

    sound1_     = 0;
    sound2_     = 0;

    reset();
}

void MrDoMainBoard::reset()
{
    cpu_->reset();

    memset( bg_colorram_, 0, sizeof(bg_colorram_) );
    memset( bg_videoram_, 0, sizeof(bg_videoram_) );
    memset( fg_colorram_, 0, sizeof(fg_colorram_) );
    memset( fg_videoram_, 0, sizeof(fg_videoram_) );
    memset( spriteram_, 0, sizeof(spriteram_) );
    memset( workram_, 0, sizeof(workram_) );

    port0_ = 0xFF;
    port1_ = 0xFF;
    dsw1_  = 0xFF;
    dsw2_  = 0xFF;

    flipscreen_ = 0;
    priority_   = 0;
    scrollx_    = 0;
    scrolly_    = 0;

    sound1_     = 0;
    sound2_     = 0;

    sound_chip1_.reset();
    sound_chip2_.reset();
}

void MrDoMainBoard::run()
{
    cpu_->run( CpuCyclesPerFrame );
    // Mr. Do! usa IRQ enmascarable (irq0_line_hold en MAME), no NMI.
    // cpu_->interrupt() solo dispara si el programa ha hecho EI (IFF1),
    // que es exactamente la semántica de irq0_line_hold. El valor 0xFF
    // funciona tanto en modo 1 (RST 38h, dato ignorado) como en modo 0
    // (interpretado como opcode 0xFF = RST 38h, mismo efecto).
    cpu_->interrupt( 0xFF );
}

unsigned char MrDoMainBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr <= 0x7FFF ) {
        return main_rom_[addr];
    }
    else if( addr >= 0x8000 && addr <= 0x83FF ) {
        return bg_colorram_[addr - 0x8000];
    }
    else if( addr >= 0x8400 && addr <= 0x87FF ) {
        return bg_videoram_[addr - 0x8400];
    }
    else if( addr >= 0x8800 && addr <= 0x8BFF ) {
        return fg_colorram_[addr - 0x8800];
    }
    else if( addr >= 0x8C00 && addr <= 0x8FFF ) {
        return fg_videoram_[addr - 0x8C00];
    }
    else if( addr >= 0x9000 && addr <= 0x90FF ) {
        return spriteram_[addr - 0x9000];
    }
    else if( addr == 0x9803 ) {
        // Protección SECRE: protection_r() en MAME devuelve
        // memregion("maincpu")->base()[HL]. La región "maincpu" mide 0x10000
        // bytes; solo 0x0000-0x7FFF tienen ROM cargada. El resto de la
        // región (0x8000-0xFFFF) se rellena por defecto con 0xFF (relleno
        // estándar de ROM_REGION sin ROM_LOAD), NO con 0x00.
        unsigned hl = cpu_->HL();
        return (hl < 0x8000) ? main_rom_[hl] : 0xFF;
    }
    else if( addr == 0xA000 ) {
        return port0_;
    }
    else if( addr == 0xA001 ) {
        return port1_;
    }
    else if( addr == 0xA002 ) {
        return dsw1_;
    }
    else if( addr == 0xA003 ) {
        return dsw2_;
    }
    else if( addr >= 0xE000 && addr <= 0xEFFF ) {
        return workram_[addr - 0xE000];
    }

    return 0xFF;
}

void MrDoMainBoard::writeByte( unsigned addr, unsigned char b )
{
    addr &= 0xFFFF;

    if( addr >= 0x8000 && addr <= 0x83FF ) {
        bg_colorram_[addr - 0x8000] = b;
    }
    else if( addr >= 0x8400 && addr <= 0x87FF ) {
        bg_videoram_[addr - 0x8400] = b;
    }
    else if( addr >= 0x8800 && addr <= 0x8BFF ) {
        fg_colorram_[addr - 0x8800] = b;
    }
    else if( addr >= 0x8C00 && addr <= 0x8FFF ) {
        fg_videoram_[addr - 0x8C00] = b;
    }
    else if( addr >= 0x9000 && addr <= 0x90FF ) {
        spriteram_[addr - 0x9000] = b;
    }
    else if( addr == 0x9800 ) {
        // bit 0 = flip screen
        // bits 1-3 = priority select (not used by Mr. Do! according to original comments)
        flipscreen_ = b & 0x01;
        priority_   = (b >> 1) & 0x07;
    }
    else if( addr == 0x9801 ) {
        sound1_ = b; // SN76496 #0 / SN76489 #0 write port
        sound_chip1_.write( b );
    }
    else if( addr == 0x9802 ) {
        sound2_ = b; // SN76496 #1 / SN76489 #1 write port
        sound_chip2_.write( b );
    }
    else if( addr >= 0xE000 && addr <= 0xEFFF ) {
        workram_[addr - 0xE000] = b;
    }
    else if( addr >= 0xF000 && addr <= 0xF7FF ) {
        // Original base: playfield 0 X scroll (handler named mrdo_scrollx_w)
        scrollx_ = b;
    }
    else if( addr >= 0xF800 && addr <= 0xFFFF ) {
        // Original base: playfield 0 Y scroll (handler named mrdo_scrolly_w)
        scrolly_ = b;
    }
}

unsigned char MrDoMainBoard::readPort( unsigned )
{
    return 0xFF;
}

void MrDoMainBoard::writePort( unsigned, unsigned char )
{
}

// -----------------------------------------------------------------------------
// Machine
// -----------------------------------------------------------------------------
MrDo::MrDo( MrDoMainBoard * board )
    : bg_char_data_( 8, 8 * 512 ),
      fg_char_data_( 8, 8 * 512 ),
      sprite_data_( 16, 16 * 128 ),
      raw_screen_( RawScreenWidth, RawScreenHeight )
{
    main_board_ = board;
    refresh_roms_ = false;

    createScreen( ScreenWidth, ScreenHeight, ScreenColors );

    // Inputs al estilo Frogger: eventHandler + joystickHandler
    eventHandler()->add( idCoinSlot1,       ptInverted, &main_board_->port1_, 0x40 );
    eventHandler()->add( idCoinSlot2,       ptInverted, &main_board_->port1_, 0x80 );
    eventHandler()->add( idKeyStartPlayer1, ptInverted, &main_board_->port0_, 0x20 );
    eventHandler()->add( idKeyStartPlayer2, ptInverted, &main_board_->port0_, 0x40 );
    eventHandler()->add( idKeyP1Action1,    ptInverted, &main_board_->port0_, 0x10 );

    setJoystickHandler( 0, new TJoystickToPortHandler(idJoyP1Joystick1, ptInverted, jm4Way) );
    joystickHandler(0)->setPort( jpLeft,  &main_board_->port0_, 0x01 );
    joystickHandler(0)->setPort( jpDown,  &main_board_->port0_, 0x02 );
    joystickHandler(0)->setPort( jpRight, &main_board_->port0_, 0x04 );
    joystickHandler(0)->setPort( jpUp,    &main_board_->port0_, 0x08 );

    setJoystickHandler( 1, new TJoystickToPortHandler(idJoyP2Joystick1, ptInverted, jm4Way) );
    joystickHandler(1)->setPort( jpLeft,  &main_board_->port1_, 0x01 );
    joystickHandler(1)->setPort( jpDown,  &main_board_->port1_, 0x02 );
    joystickHandler(1)->setPort( jpRight, &main_board_->port1_, 0x04 );
    joystickHandler(1)->setPort( jpUp,    &main_board_->port1_, 0x08 );

    registerDriver( MrDoInfo );
}

bool MrDo::initialize( TMachineDriverInfo * info )
{
    // Parent set "mrdo"
    resourceHandler()->add( EfMain0, "a4-01.bin", 0x2000, efROM, main_board_->main_rom_ + 0x0000 );
    resourceHandler()->add( EfMain1, "c4-02.bin", 0x2000, efROM, main_board_->main_rom_ + 0x2000 );
    resourceHandler()->add( EfMain2, "e4-03.bin", 0x2000, efROM, main_board_->main_rom_ + 0x4000 );
    resourceHandler()->add( EfMain3, "f4-04.bin", 0x2000, efROM, main_board_->main_rom_ + 0x6000 );

    resourceHandler()->add( EfGfx10, "s8-09.bin", 0x1000, efVideoROM, main_board_->gfx1_ + 0x0000 );
    resourceHandler()->add( EfGfx11, "u8-10.bin", 0x1000, efVideoROM, main_board_->gfx1_ + 0x1000 );

    resourceHandler()->add( EfGfx20, "r8-08.bin", 0x1000, efVideoROM, main_board_->gfx2_ + 0x0000 );
    resourceHandler()->add( EfGfx21, "n8-07.bin", 0x1000, efVideoROM, main_board_->gfx2_ + 0x1000 );

    resourceHandler()->add( EfGfx30, "h5-05.bin", 0x1000, efVideoROM, main_board_->gfx3_ + 0x0000 );
    resourceHandler()->add( EfGfx31, "k5-06.bin", 0x1000, efVideoROM, main_board_->gfx3_ + 0x1000 );

    resourceHandler()->add( EfProm0, "u02--2.bin", 0x20, efPalettePROM, main_board_->proms_ + 0x00 );
    resourceHandler()->add( EfProm1, "t02--3.bin", 0x20, efPalettePROM, main_board_->proms_ + 0x20 );
    resourceHandler()->add( EfProm2, "f10--1.bin", 0x20, efPalettePROM, main_board_->proms_ + 0x40 );
    //resourceHandler()->add( EfProm3, "j10--4.bin", 0x20, efPalettePROM, main_board_->proms_ + 0x60 );

    resourceHandler()->assignToMachineDriverInfo( info );

    refresh_roms_ = true;
    return true;
}

bool MrDo::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    refresh_roms_ = true;
    return 0 == resourceHandler()->handle( id, buf, len );
}

void MrDo::reset()
{
    main_board_->reset();
}

void MrDo::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    if( refresh_roms_ ) {
        onVideoROMsChanged();
        refresh_roms_ = false;
    }

    main_board_->run();
    frame->setVideo( renderVideo() );

    // -------------------------------------------------------------------
    // Sound: Mr. Do! has two SN76489 chips, both mixed to a single mono
    // output. Each chip contributes up to 4 channels (3 tone + 1 noise).
    // -------------------------------------------------------------------
    if( samplesPerFrame > 0 && samplingRate > 0 ) {
        TMixerBuffer * mixer_buffer = frame->getMixer()->getBuffer( chMono, samplesPerFrame, 8 );
        int * data_buffer = mixer_buffer->data();

        main_board_->sound_chip1_.setSamplingRate( samplingRate );
        main_board_->sound_chip2_.setSamplingRate( samplingRate );

        main_board_->sound_chip1_.playSound( data_buffer, samplesPerFrame );
        main_board_->sound_chip2_.playSound( data_buffer, samplesPerFrame );
    }
}

// -----------------------------------------------------------------------------
// Palette and decode
// -----------------------------------------------------------------------------
void MrDo::rebuildPalette()
{
    // Exact palette reconstruction based on the vidhrdw.c you attached
    for( int i=0; i<256; i++ ) {
        // Confirmado contra MAME (src/mame/universal/mrdo.cpp), ROM_REGION "proms":
        //   proms_[0x00-0x1F] = u02--2.bin (U2, "palette high bits")
        //   proms_[0x20-0x3F] = t02--3.bin (T2, "palette low bits")
        //   proms_[0x40-0x5F] = f10--1.bin (sprite color lookup table)
        // Este es el MISMO orden que color_prom[] en MAME, así que la
        // fórmula original de mrdo_vh_convert_color_prom (0.37b7) se aplica
        // sin modificaciones: a1 (+32) indexa T2, a2 (sin offset) indexa U2.
        int a1 = ((i >> 3) & 0x1c) + (i & 0x03) + 32;
        int a2 = ((i >> 0) & 0x1c) + (i & 0x03);

        int bit0, bit1, bit2, bit3;

        bit0 = (main_board_->proms_[a1] >> 1) & 0x01;
        bit1 = (main_board_->proms_[a1] >> 0) & 0x01;
        bit2 = (main_board_->proms_[a2] >> 1) & 0x01;
        bit3 = (main_board_->proms_[a2] >> 0) & 0x01;
        int r = 0x2c * bit0 + 0x37 * bit1 + 0x43 * bit2 + 0x59 * bit3;

        bit0 = (main_board_->proms_[a1] >> 3) & 0x01;
        bit1 = (main_board_->proms_[a1] >> 2) & 0x01;
        bit2 = (main_board_->proms_[a2] >> 3) & 0x01;
        bit3 = (main_board_->proms_[a2] >> 2) & 0x01;
        int g = 0x2c * bit0 + 0x37 * bit1 + 0x43 * bit2 + 0x59 * bit3;

        bit0 = (main_board_->proms_[a1] >> 5) & 0x01;
        bit1 = (main_board_->proms_[a1] >> 4) & 0x01;
        bit2 = (main_board_->proms_[a2] >> 5) & 0x01;
        bit3 = (main_board_->proms_[a2] >> 4) & 0x01;
        int b = 0x2c * bit0 + 0x37 * bit1 + 0x43 * bit2 + 0x59 * bit3;

        palette()->setColor( i, TPalette::encodeColor(r, g, b) );
    }

    // Sprite color lookup PROM (offset 0x40, PROM f10--1.bin)
    // Based on original MAME mrdo_vh_convert_color_prom()
    for( int i=0; i<64; i++ ) {
        int bits;
        if( i < 32 ) {
            bits = main_board_->proms_[0x40 + i] & 0x0f;
        }
        else {
            bits = main_board_->proms_[0x40 + (i & 0x1f)] >> 4;
        }
        sprite_lookup_[i] = (unsigned char)(bits + ((bits & 0x0c) << 3));
    }
}

void MrDo::decodeBgChar( int code, TBitBlock * bb, int oy )
{
    // Exact charlayout from classic driver:
    // 8x8, 512 chars, 2bpp, planes separated
    const unsigned char * src = main_board_->gfx1_;
    unsigned baseBit = (unsigned)(code & 0x1ff) * 8 * 8;

    for( int y=0; y<8; y++ ) {
        for( int x=0; x<8; x++ ) {
            unsigned xoff = 7 - x;
            unsigned yoff = y * 8;

            unsigned char pen =
                (bitAtLSB(src, 0x1000 * 8 + baseBit + yoff + xoff) << 1) |
                (bitAtLSB(src,           baseBit + yoff + xoff) << 0);

            bb->setPixel( x, oy + y, pen );
        }
    }
}

void MrDo::decodeFgChar( int code, TBitBlock * bb, int oy )
{
    const unsigned char * src = main_board_->gfx2_;
    unsigned baseBit = (unsigned)(code & 0x1ff) * 8 * 8;

    for( int y=0; y<8; y++ ) {
        for( int x=0; x<8; x++ ) {
            unsigned xoff = 7 - x;
            unsigned yoff = y * 8;

            unsigned char pen =
                (bitAtLSB(src, 0x1000 * 8 + baseBit + yoff + xoff) << 1) |
                (bitAtLSB(src,           baseBit + yoff + xoff) << 0);

            bb->setPixel( x, oy + y, pen );
        }
    }
}

void MrDo::decodeSprite( int code, TBitBlock * bb, int oy )
{
    // Exact spritelayout from classic driver:
    // 16x16, 128 sprites, 2bpp, plane offsets {4,0}
    static const unsigned xoffs[16] = {
         3,  2,  1,  0,
        11, 10,  9,  8,
        19, 18, 17, 16,
        27, 26, 25, 24
    };
    static const unsigned yoffs[16] = {
         0*16,  2*16,  4*16,  6*16,
         8*16, 10*16, 12*16, 14*16,
        16*16, 18*16, 20*16, 22*16,
        24*16, 26*16, 28*16, 30*16
    };

    const unsigned char * src = main_board_->gfx3_;
    unsigned baseBit = (unsigned)(code & 0x7f) * 64 * 8;

    for( int y=0; y<16; y++ ) {
        for( int x=0; x<16; x++ ) {
            unsigned char pen =
                (bitAtLSB(src, baseBit + 4 + yoffs[y] + xoffs[x]) << 1) |
                (bitAtLSB(src, baseBit + 0 + yoffs[y] + xoffs[x]) << 0);

            bb->setPixel( x, oy + y, pen );
        }
    }
}

void MrDo::onVideoROMsChanged()
{
    rebuildPalette();

    for( int i=0; i<512; i++ ) {
        decodeBgChar( i, &bg_char_data_, i * 8 );
        decodeFgChar( i, &fg_char_data_, i * 8 );
    }

    for( int i=0; i<128; i++ ) {
        decodeSprite( i, &sprite_data_, i * 16 );
    }
}

// -----------------------------------------------------------------------------
// Video
// Based on the vidhrdw.c you attached:
// - fill with pen 0
// - draw bg tilemap FRONT
// - draw fg tilemap FRONT
// - draw sprites from end to start
// -----------------------------------------------------------------------------
TBitmapIndexed * MrDo::renderVideo()
{
    unsigned char * bg_attr = main_board_->bg_colorram_;   // first half
    unsigned char * bg_code = main_board_->bg_videoram_;   // second half
    unsigned char * fg_attr = main_board_->fg_colorram_;
    unsigned char * fg_code = main_board_->fg_videoram_;

    TBitBlock * bits = &raw_screen_;
    bits->fill( 0 );

    // -------------------------------------------------------------------------
    // BG tilemap FRONT pass
    // get_bg_tile_info():
    //   attr = mrdo_bgvideoram[tile_index]
    //   code = mrdo_bgvideoram[tile_index+0x400] + ((attr & 0x80) << 1)
    //   color = attr & 0x3f
    //   split = (attr & 0x40) >> 6
    //
    // In the original tilemap split setup:
    //   transmask[0] = 0x01 for FRONT half
    //   transmask[1] = 0x00 for FRONT half
    //
    // So for split type 0, pen 0 is transparent in FRONT.
    // For split type 1, FRONT is opaque.
    // -------------------------------------------------------------------------
    for( int ty=0; ty<32; ty++ ) {
        for( int tx=0; tx<32; tx++ ) {
            int idx   = ty * 32 + tx;
            int attr  = bg_attr[idx];
            int code  = bg_code[idx] + ((attr & 0x80) << 1);
            int color = attr & 0x3f;
            int split = (attr & 0x40) >> 6;

            int sx = tx * 8 - main_board_->scrollx_;
            int sy = ty * 8 - main_board_->scrolly_;

            for( int y=0; y<8; y++ ) {
                int dy = sy + y;
                if( dy < 0 || dy >= RawScreenHeight ) continue;

                unsigned char * dst = bits->scanline_data( dy );
                unsigned char * src = bg_char_data_.scanline_data( (code & 0x1ff) * 8 + y );

                for( int x=0; x<8; x++ ) {
                    int dx = sx + x;
                    if( dx < 0 || dx >= RawScreenWidth ) continue;

                    int px = x;
                    int py = y;

                    if( main_board_->flipscreen_ & 0x01 ) {
                        dx = RawScreenWidth  - 1 - dx;
                        dy = RawScreenHeight - 1 - dy;
                        dst = bits->scanline_data( dy );
                        px = 7 - x;
                        py = 7 - y;
                        src = bg_char_data_.scanline_data( (code & 0x1ff) * 8 + py );
                    }

                    unsigned char pen = src[px];

                    // FRONT half transparency coming from tile split mask
                    if( split == 0 && pen == 0 )
                        continue;

                    dst[dx] = (unsigned char)((color << 2) | pen);
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // FG tilemap FRONT pass
    // Same logic as BG, using fg RAM.
    // -------------------------------------------------------------------------
    for( int ty=0; ty<32; ty++ ) {
        for( int tx=0; tx<32; tx++ ) {
            int idx   = ty * 32 + tx;
            int attr  = fg_attr[idx];
            int code  = fg_code[idx] + ((attr & 0x80) << 1);
            int color = attr & 0x3f;
            int split = (attr & 0x40) >> 6;

            int sx = tx * 8;
            int sy = ty * 8;

            for( int y=0; y<8; y++ ) {
                int dy = sy + y;
                if( dy < 0 || dy >= RawScreenHeight ) continue;

                unsigned char * dst = bits->scanline_data( dy );
                unsigned char * src = fg_char_data_.scanline_data( (code & 0x1ff) * 8 + y );

                for( int x=0; x<8; x++ ) {
                    int dx = sx + x;
                    if( dx < 0 || dx >= RawScreenWidth ) continue;

                    int px = x;
                    int py = y;

                    if( main_board_->flipscreen_ & 0x01 ) {
                        dx = RawScreenWidth  - 1 - dx;
                        dy = RawScreenHeight - 1 - dy;
                        dst = bits->scanline_data( dy );
                        px = 7 - x;
                        py = 7 - y;
                        src = fg_char_data_.scanline_data( (code & 0x1ff) * 8 + py );
                    }

                    unsigned char pen = src[px];

                    if( split == 0 && pen == 0 )
                        continue;

                    dst[dx] = (unsigned char)((color << 2) | pen);
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Sprites
    // draw_sprites():
    //   for offs = spriteram_size-4 down to 0
    //   if spriteram[offs+1] != 0
    //       code  = spriteram[offs]
    //       color = spriteram[offs+2] & 0x0f
    //       flipx = spriteram[offs+2] & 0x10
    //       flipy = spriteram[offs+2] & 0x20
    //       x = spriteram[offs+3]
    //       y = 256 - spriteram[offs+1]
    // TRANSPARENCY_PEN,0 => pen 0 transparent
    // -------------------------------------------------------------------------
    for( int offs = 0x100 - 4; offs >= 0; offs -= 4 ) {
        if( main_board_->spriteram_[offs + 1] == 0 )
            continue;

        int code   = main_board_->spriteram_[offs + 0] & 0x7f;
        int attr   = main_board_->spriteram_[offs + 2];
        int color  = attr & 0x0f;
        int flipx  = attr & 0x10;
        int flipy  = attr & 0x20;
        int sx     = main_board_->spriteram_[offs + 3];
        int sy     = 256 - main_board_->spriteram_[offs + 1];

        for( int y=0; y<16; y++ ) {
            int py = flipy ? (15 - y) : y;
            int dy = sy + y;
            if( dy < 0 || dy >= RawScreenHeight ) continue;

            for( int x=0; x<16; x++ ) {
                int px = flipx ? (15 - x) : x;
                int dx = sx + x;
                if( dx < 0 || dx >= RawScreenWidth ) continue;

                int rdx = dx;
                int rdy = dy;
                if( main_board_->flipscreen_ & 0x01 ) {
                    rdx = RawScreenWidth  - 1 - dx;
                    rdy = RawScreenHeight - 1 - dy;
                }

                unsigned char * src = sprite_data_.scanline_data( code * 16 + py );
                unsigned char pen = src[px];
                if( pen == 0 )
                    continue;

                unsigned char finalColor = sprite_lookup_[color * 4 + pen];
                bits->scanline_data( rdy )[rdx] = finalColor;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Rotate the raw (240x192) framebuffer into the portrait (192x240) screen
    // buffer. The cabinet monitor is mounted ROT270 (90 degrees CCW relative
    // to the raw framebuffer orientation), so:
    //
    //   screen(px, py) = raw(RawScreenWidth-1-py, px)
    //
    //   for px in [0, ScreenWidth)  = [0, RawScreenHeight)
    //      py in [0, ScreenHeight) = [0, RawScreenWidth)
    // -------------------------------------------------------------------------
    TBitBlock * out = screen()->bits();

    for( int py = 0; py < ScreenHeight; py++ ) {
        unsigned char * dst = out->scanline_data( py );
        int rx = RawScreenWidth - 1 - py;

        for( int px = 0; px < ScreenWidth; px++ ) {
            int ry = px;
            dst[px] = raw_screen_.scanline_data( ry )[rx];
        }
    }

    return screen();
}