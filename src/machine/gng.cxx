/*
    Ghosts'n Goblins arcade machine emulator

    Capcom hardware (gng romset)
    Based on MAME 0.37b7

    Main CPU: M6809 @ 1.5MHz
    Sound CPU: Z80 @ 3MHz
    Sound: 2x YM2203 (SSG portion emulated via YM2149)
*/
#include "gng.h"

enum {
    ScreenWidth             = 256,
    ScreenHeight            = 256,
    ScreenVisibleWidth      = 256,
    ScreenVisibleHeight     = 224,
    ScreenVisibleOffsetY    = 16,
    ScreenColors            = 256,
    VideoFrequency          = 60,
    MainCpuClock            = 1500000,
    SoundCpuClock           = 3000000,
    MainCpuCyclesPerFrame   = MainCpuClock / VideoFrequency,
    SoundCpuCyclesPerFrame  = SoundCpuClock / VideoFrequency,
    SoundChipClock          = 1500000
};

enum {
    // Main CPU ROMs
    RomGG4, RomGG3, RomGG5,
    // Sound CPU ROM
    RomGG2,
    // GFX1 - characters
    RomGG1,
    // GFX2 - tiles (6 ROMs)
    RomGG11, RomGG10, RomGG9, RomGG8, RomGG7, RomGG6,
    // GFX3 - sprites (6 ROMs)
    RomGG17, RomGG16, RomGG15, RomGG14, RomGG13, RomGG12,
    // DIP switches
    OptCoinage, OptCoinageAffects, OptDemoSounds, OptFlipScreen,
    OptLives, OptCabinet, OptBonusLife, OptDifficulty
};

static TMachineInfo GnGInfo = {
    "gng", "Ghosts'n Goblins", "Capcom", 1985,
    ScreenVisibleWidth, ScreenVisibleHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler reg( &GnGInfo, GnG::createInstance );

// ---------------------------------------------------------------------------
// Palette conversion: RRRRGGGGBBBBxxxx split across two RAM banks
// ---------------------------------------------------------------------------
// palram_split1 at 0x3800 = RRRR GGGG (high byte)
// palram_split2 at 0x3900 = BBBB xxxx (low byte)
static void decodePalette( TPalette * palette, const unsigned char * split1,
                           const unsigned char * split2, int index )
{
    unsigned char hi = split1[index]; // RRRR GGGG
    unsigned char lo = split2[index]; // BBBB xxxx

    int r = (hi >> 4) & 0x0F;
    int g = hi & 0x0F;
    int b = (lo >> 4) & 0x0F;

    r = (r << 4) | r;
    g = (g << 4) | g;
    b = (b << 4) | b;

    palette->setColor( index, TPalette::encodeColor(r,g,b) );
}

// ---------------------------------------------------------------------------
// Sound board (Z80 + 2x YM2203 SSG)
// ---------------------------------------------------------------------------
GnGSoundBoard::GnGSoundBoard()
{
    cpu_ = new Z80( *this );
    sound_latch_ = 0;

    ym_[0].setClock( SoundChipClock );
    ym_[1].setClock( SoundChipClock );

    memset( rom_, 0, sizeof(rom_) );
    memset( ram_, 0, sizeof(ram_) );
}

GnGSoundBoard::~GnGSoundBoard()
{
    delete cpu_;
}

void GnGSoundBoard::reset()
{
    cpu_->reset();
    ym_[0].reset();
    ym_[1].reset();
    sound_latch_ = 0;
}

unsigned char GnGSoundBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr < 0x8000 )
        return rom_[addr];

    if( addr >= 0xC000 && addr < 0xC800 )
        return ram_[addr - 0xC000];

    if( addr == 0xC800 )
        return sound_latch_;

    return 0xFF;
}

void GnGSoundBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr >= 0xC000 && addr < 0xC800 ) {
        ram_[addr - 0xC000] = value;
        return;
    }

    switch( addr ) {
        case 0xE000: ym_[0].writeAddress(value); break;
        case 0xE001: ym_[0].writeData(value);    break;
        case 0xE002: ym_[1].writeAddress(value); break;
        case 0xE003: ym_[1].writeData(value);    break;
    }
}

void GnGSoundBoard::run( unsigned cycles )
{
    cpu_->run( cycles );
}

void GnGSoundBoard::playSound( TMixer * mixer, unsigned len, unsigned samplingRate )
{
    unsigned samplingSteps = 12;
    unsigned stepSize = len / samplingSteps;
    unsigned cyclesPerStep = SoundCpuCyclesPerFrame / samplingSteps;

    TMixerBuffer * mixerBuffer = mixer->getBuffer( chMono, len, 8 );
    int * dataBuffer = mixerBuffer->data();

    // MAME uses interrupt,4 -> 4 IRQs per frame, trigger every 3 steps
    for( unsigned i = 1; i < samplingSteps; i++ ) {
        cpu_->run( cyclesPerStep );

        if( (i % 3) == 0 ) {
            cpu_->interrupt( 0xFF );
        }

        ym_[0].playSound( dataBuffer, stepSize, samplingRate );
        ym_[1].playSound( dataBuffer, stepSize, samplingRate );

        dataBuffer += stepSize;
    }

    stepSize = len - stepSize * (samplingSteps - 1);
    cpu_->run( cyclesPerStep );
    cpu_->interrupt( 0xFF );

    ym_[0].playSound( dataBuffer, stepSize, samplingRate );
    ym_[1].playSound( dataBuffer, stepSize, samplingRate );
}

// ---------------------------------------------------------------------------
// Main board (M6809)
// ---------------------------------------------------------------------------
GnGMainBoard::GnGMainBoard( GnGSoundBoard * sound_board )
    : cpu_( new M6809(*this) ), sound_board_( sound_board )
{
    memset( rom_, 0xFF, sizeof(rom_) );
    memset( ram_, 0, sizeof(ram_) );
    memset( spriteram_, 0, sizeof(spriteram_) );
    memset( buffered_spriteram_, 0, sizeof(buffered_spriteram_) );
    memset( fgvideoram_, 0, sizeof(fgvideoram_) );
    memset( bgvideoram_, 0, sizeof(bgvideoram_) );
    memset( paletteram_1_, 0, sizeof(paletteram_1_) );
    memset( paletteram_2_, 0, sizeof(paletteram_2_) );
    memset( bgscrollx_, 0, sizeof(bgscrollx_) );
    memset( bgscrolly_, 0, sizeof(bgscrolly_) );
    flipscreen_ = 0;
    bankselect_ = 0;
    port_in0_ = 0xFF;
    port_in1_ = 0xFF;
    port_in2_ = 0xFF;
    dsw0_ = 0xFF;
    dsw1_ = 0xFF;
}

GnGMainBoard::~GnGMainBoard()
{
    delete cpu_;
}

void GnGMainBoard::reset()
{
    cpu_->reset();
    bankselect_ = 0;
    flipscreen_ = 0;
    memset( bgscrollx_, 0, sizeof(bgscrollx_) );
    memset( bgscrolly_, 0, sizeof(bgscrolly_) );

}

void GnGMainBoard::run()
{
    cpu_->run( MainCpuCyclesPerFrame );
    cpu_->irq();

    // Buffer spriteram at end of frame (MAME: gng_eof_callback)
    memcpy( buffered_spriteram_, spriteram_, sizeof(spriteram_) );
}

/*
    Memory map (M6809 main CPU):
    0x0000-0x1DFF: RAM
    0x1E00-0x1FFF: Sprite RAM
    0x2000-0x27FF: FG video RAM
    0x2800-0x2FFF: BG video RAM
    0x3000: IN0 (start, service, coins)
    0x3001: IN1 (P1 joystick/buttons)
    0x3002: IN2 (P2 joystick/buttons)
    0x3003: DSW0
    0x3004: DSW1
    0x3800-0x38FF: Palette RAM 2 (split)
    0x3900-0x39FF: Palette RAM 1 (split)
    0x3A00: Sound latch write
    0x3B08-0x3B09: BG scroll X
    0x3B0A-0x3B0B: BG scroll Y
    0x3C00: Watchdog (NOP)
    0x3D00: Flip screen
    0x3D02-0x3D03: Coin counter
    0x3E00: Bank switch
    0x4000-0x5FFF: Banked ROM
    0x6000-0xFFFF: ROM
*/
unsigned char GnGMainBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr < 0x1E00 )
        return ram_[addr];

    if( addr >= 0x1E00 && addr < 0x2000 )
        return spriteram_[addr - 0x1E00];

    if( addr >= 0x2000 && addr < 0x2800 )
        return fgvideoram_[addr - 0x2000];

    if( addr >= 0x2800 && addr < 0x3000 )
        return bgvideoram_[addr - 0x2800];

    switch( addr ) {
        case 0x3000: return port_in0_;
        case 0x3001: return port_in1_;
        case 0x3002: return port_in2_;
        case 0x3003: return dsw0_;
        case 0x3004: return dsw1_;
    }

    if( addr == 0x3C00 )
        return 0; // watchdog

    // Banked ROM: 0x4000-0x5FFF
    // Banks 0-3 → gg5.bin pages (rom_[0x10000 + bank*0x2000])
    // Bank 4   → gg4.bin first half (rom_[0x4000])
    if( addr >= 0x4000 && addr < 0x6000 ) {
        unsigned bank = bankselect_ & 7;
        if( bank < 4 )
            return rom_[0x10000 + bank * 0x2000 + (addr - 0x4000)];
        else
            return rom_[0x4000 + (addr - 0x4000)]; // gg4.bin
    }

    // Fixed ROM: 0x6000-0xFFFF (mapped at offset 0x6000 in rom_ since ROM is loaded at 0x8000)
    if( addr >= 0x6000 )
        return rom_[addr];

    return 0xFF;
}

void GnGMainBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr < 0x1E00 ) {
        ram_[addr] = value;
        return;
    }

    if( addr >= 0x1E00 && addr < 0x2000 ) {
        spriteram_[addr - 0x1E00] = value;
        return;
    }

    if( addr >= 0x2000 && addr < 0x2800 ) {
        fgvideoram_[addr - 0x2000] = value;
        return;
    }

    if( addr >= 0x2800 && addr < 0x3000 ) {
        bgvideoram_[addr - 0x2800] = value;
        return;
    }

    if( addr >= 0x3800 && addr < 0x3900 ) {
        paletteram_2_[addr - 0x3800] = value;
        return;
    }

    if( addr >= 0x3900 && addr < 0x3A00 ) {
        paletteram_1_[addr - 0x3900] = value;
        return;
    }

    switch( addr ) {
        case 0x3A00:
            sound_board_->sound_latch_ = value;
            sound_board_->cpu_->interrupt( 0xFF );
            return;
        case 0x3B08:
            bgscrollx_[0] = value;
            return;
        case 0x3B09:
            bgscrollx_[1] = value;
            return;
        case 0x3B0A:
            bgscrolly_[0] = value;
            return;
        case 0x3B0B:
            bgscrolly_[1] = value;
            return;
        case 0x3C00:
            return; // watchdog
        case 0x3D00:
            flipscreen_ = ~value & 1;
            return;
        case 0x3D02:
        case 0x3D03:
            return; // coin counter
        case 0x3E00:
            bankselect_ = value;
            return;
    }

}

// ---------------------------------------------------------------------------
// Machine class
// ---------------------------------------------------------------------------
GnG::GnG() :
    main_board_( &sound_board_ ),
    char_data_( 8, 8 * 1024 ),
    tile_data_( 16, 16 * 1024 ),
    sprite_data_( 16, 16 * 768 )
{
    createScreen( ScreenVisibleWidth, ScreenVisibleHeight, ScreenColors );

    refresh_roms_ = true;

    // Player 1 joystick (8-way)
    setJoystickHandler( 0, new TJoystickToPortHandler(idJoyP1Joystick1, ptInverted, jm8Way, &main_board_.port_in1_, 0x08040201) );
    // Player 2 joystick (8-way, cocktail)
    setJoystickHandler( 1, new TJoystickToPortHandler(idJoyP2Joystick1, ptInverted, jm8Way, &main_board_.port_in2_, 0x08040201) );

    // P1 buttons
    eventHandler()->add( idKeyP1Action1,     ptInverted, &main_board_.port_in1_, 0x10 );
    eventHandler()->add( idKeyP1Action2,     ptInverted, &main_board_.port_in1_, 0x20 );

    // P2 buttons
    eventHandler()->add( idKeyP2Action1,     ptInverted, &main_board_.port_in2_, 0x10 );
    eventHandler()->add( idKeyP2Action2,     ptInverted, &main_board_.port_in2_, 0x20 );

    // System inputs (IN0)
    eventHandler()->add( idKeyStartPlayer1,  ptInverted, &main_board_.port_in0_, 0x01 );
    eventHandler()->add( idKeyStartPlayer2,  ptInverted, &main_board_.port_in0_, 0x02 );
    eventHandler()->add( idCoinSlot1,        ptInverted, &main_board_.port_in0_, 0x40 );
    eventHandler()->add( idCoinSlot2,        ptInverted, &main_board_.port_in0_, 0x80 );

    registerDriver( GnGInfo );
}

bool GnG::initialize( TMachineDriverInfo * info )
{
    // Main CPU ROMs:
    // gg4.bin @ 0x04000, 0x4000 (page 4)
    // gg3.bin @ 0x08000, 0x8000
    // gg5.bin @ 0x10000, 0x8000 (pages 0-3)
    resourceHandler()->add( RomGG4, "gg4.bin", 0x4000, efROM, main_board_.rom_ + 0x4000 );
    resourceHandler()->add( RomGG3, "gg3.bin", 0x8000, efROM, main_board_.rom_ + 0x8000 );
    resourceHandler()->add( RomGG5, "gg5.bin", 0x8000, efROM, main_board_.rom_ + 0x10000 );

    // Sound CPU ROM
    resourceHandler()->add( RomGG2, "gg2.bin", 0x8000, efROM, sound_board_.rom_ );

    // GFX1 - characters (2bpp, 8x8)
    resourceHandler()->add( RomGG1, "gg1.bin", 0x4000, efVideoROM, char_rom_ );

    // GFX2 - tiles (3bpp, 16x16, 6 ROMs)
    resourceHandler()->add( RomGG11, "gg11.bin", 0x4000, efVideoROM, tile_rom_ + 0x00000 );
    resourceHandler()->add( RomGG10, "gg10.bin", 0x4000, efVideoROM, tile_rom_ + 0x04000 );
    resourceHandler()->add( RomGG9,  "gg9.bin",  0x4000, efVideoROM, tile_rom_ + 0x08000 );
    resourceHandler()->add( RomGG8,  "gg8.bin",  0x4000, efVideoROM, tile_rom_ + 0x0C000 );
    resourceHandler()->add( RomGG7,  "gg7.bin",  0x4000, efVideoROM, tile_rom_ + 0x10000 );
    resourceHandler()->add( RomGG6,  "gg6.bin",  0x4000, efVideoROM, tile_rom_ + 0x14000 );

    // GFX3 - sprites (4bpp, 16x16, 6 ROMs)
    resourceHandler()->add( RomGG17, "gg17.bin", 0x4000, efVideoROM, sprite_rom_ + 0x00000 );
    resourceHandler()->add( RomGG16, "gg16.bin", 0x4000, efVideoROM, sprite_rom_ + 0x04000 );
    resourceHandler()->add( RomGG15, "gg15.bin", 0x4000, efVideoROM, sprite_rom_ + 0x08000 );
    resourceHandler()->add( RomGG14, "gg14.bin", 0x4000, efVideoROM, sprite_rom_ + 0x0C000 );
    resourceHandler()->add( RomGG13, "gg13.bin", 0x4000, efVideoROM, sprite_rom_ + 0x10000 );
    resourceHandler()->add( RomGG12, "gg12.bin", 0x4000, efVideoROM, sprite_rom_ + 0x14000 );

    resourceHandler()->assignToMachineDriverInfo( info );

    // DIP switches
    TUserInterface * ui = info->userInterface();

    TUiOptionGroup * group = ui->addGroup( S_DSW_1, dtDipSwitch );

    TUiOption * option = group->add( OptCoinage, "Coinage", 0, 0x0F );
    option->add( S_1Coin1Play, 0x0F );
    option->add( S_1Coin2Play, 0x0E );
    option->add( S_1Coin3Play, 0x0D );
    option->add( S_1Coin4Play, 0x0C );
    option->add( S_1Coin5Play, 0x0B );
    option->add( S_1Coin6Play, 0x0A );
    option->add( S_2Coin1Play, 0x08 );
    option->add( S_3Coin1Play, 0x05 );
    option->add( S_4Coin1Play, 0x02 );
    option->add( S_FreePlay,   0x00 );

    option = group->add( OptDemoSounds, "Demo Sounds", 1, 0x20 );
    option->add( S_Off, 0x20 );
    option->add( S_On,  0x00 );

    option = group->add( OptFlipScreen, "Flip Screen", 0, 0x80 );
    option->add( S_Off, 0x80 );
    option->add( S_On,  0x00 );

    optionHandler()->add( &main_board_.dsw0_, ui, OptCoinage, OptDemoSounds, OptFlipScreen );

    group = ui->addGroup( S_DSW_2, dtDipSwitch );

    option = group->add( OptLives, S_Lives, 0, 0x03 );
    option->add( "3", 0x03 );
    option->add( "4", 0x02 );
    option->add( "5", 0x01 );
    option->add( "7", 0x00 );

    option = group->add( OptCabinet, S_Cabinet, 0, 0x04 );
    option->add( S_Upright,  0x00 );
    option->add( S_Cocktail, 0x04 );

    option = group->add( OptBonusLife, "Bonus Life", 0, 0x18 );
    option->add( "20K 70K 70K+", 0x18 );
    option->add( "30K 80K 80K+", 0x10 );
    option->add( "20K 80K",      0x08 );
    option->add( "30K 80K",      0x00 );

    option = group->add( OptDifficulty, "Difficulty", 1, 0x60 );
    option->add( S_Easy,    0x40 );
    option->add( S_Normal,  0x60 );
    option->add( S_Hard,    0x20 );
    option->add( "Very Hard", 0x00 );

    optionHandler()->add( &main_board_.dsw1_, ui, OptLives, OptCabinet, OptBonusLife, OptDifficulty );

    return true;
}

bool GnG::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    if( id >= RomGG1 )
        refresh_roms_ = true;

    return 0 == resourceHandler()->handle( id, buf, len );
}

void GnG::reset()
{
    main_board_.reset();
    sound_board_.reset();
}

void GnG::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    if( refresh_roms_ ) {
        decodeGraphics();
        refresh_roms_ = false;
        // M6809 reads reset vector from ROM, must reset after ROMs are loaded
        main_board_.reset();
        sound_board_.reset();
    }

    main_board_.run();
    sound_board_.playSound( frame->getMixer(), samplesPerFrame, samplingRate );
    frame->setVideo( renderVideo() );
}

// ---------------------------------------------------------------------------
// Graphics decoding
// ---------------------------------------------------------------------------

/*
    MAME GfxLayout bit extraction:
      For each pixel at (x,y) in element c:
        For each plane p with plane_offset[p]:
          global_bit = element_start + plane_offset[p] + yoffs[y] + xoffs[x]
          pixel_bit_p = (src[global_bit/8] >> (global_bit%8)) & 1
      plane[0] = LSB, plane[N-1] = MSB

    Characters: 8x8, 2bpp, 16 bytes/char
      planes={4,0}, xoffs={0,1,2,3,8,9,10,11}, yoffs={0,16,...,112}
*/
static void decodeGnGChars( unsigned char * dst, const unsigned char * src, int count )
{
    for( int c = 0; c < count; c++ ) {
        const unsigned char * s = src + c * 16;
        unsigned char * d = dst + c * 64;
        for( int y = 0; y < 8; y++ ) {
            unsigned char b0 = s[y * 2];
            unsigned char b1 = s[y * 2 + 1];
            // xoffs 0..3 from b0, xoffs 8..11 from b1
            // plane[0]=offset 4 → MSB (bit 1), plane[1]=offset 0 → LSB (bit 0)
            for( int x = 0; x < 4; x++ ) {
                int p1 = (b0 >> (x + 4)) & 1; // plane[0] → bit 1 (MSB)
                int p0 = (b0 >> x) & 1;        // plane[1] → bit 0 (LSB)
                d[y * 8 + x] = p0 | (p1 << 1);
            }
            for( int x = 0; x < 4; x++ ) {
                int p1 = (b1 >> (x + 4)) & 1; // plane[0] → bit 1 (MSB)
                int p0 = (b1 >> x) & 1;        // plane[1] → bit 0 (LSB)
                d[y * 8 + 4 + x] = p0 | (p1 << 1);
            }
        }
    }
}

/*
    Tiles: 16x16, 3bpp, 32 bytes/tile, 1024 tiles
      planes={RGN_FRAC(2,3), RGN_FRAC(1,3), RGN_FRAC(0,3)} = byte offsets {0x10000, 0x8000, 0}
      plane[0] LSB at 0x10000, plane[2] MSB at 0
      xoffs={0,1,2,3,4,5,6,7, 128,...,135}, yoffs={0,8,...,120}
      x<8: byte=y, bit=x; x>=8: byte=y+16, bit=x-8
*/
static void decodeGnGTiles( unsigned char * dst, const unsigned char * src, int count )
{
    for( int c = 0; c < count; c++ ) {
        int base = c * 32;
        unsigned char * d = dst + c * 256;
        for( int y = 0; y < 16; y++ ) {
            for( int x = 0; x < 16; x++ ) {
                int byte_idx = (x < 8) ? y : (y + 16);
                int bit = (x < 8) ? x : (x - 8);

                int pixel = 0;
                pixel |= ((src[base + byte_idx          ] >> bit) & 1) << 0; // plane[2] LSB
                pixel |= ((src[base + byte_idx + 0x08000] >> bit) & 1) << 1;
                pixel |= ((src[base + byte_idx + 0x10000] >> bit) & 1) << 2; // plane[0] MSB

                d[y * 16 + x] = pixel;
            }
        }
    }
}

/*
    Sprites: 16x16, 4bpp, 64 bytes/sprite, 768 sprites
      planes={RGN_FRAC(1,2)+4, RGN_FRAC(1,2)+0, 4, 0}
        = bit offsets {0xC000*8+4, 0xC000*8, 4, 0}
        plane[0] LSB at bit 0, plane[1] at bit 4,
        plane[2] at 0xC000 bytes, plane[3] MSB at 0xC000 bytes + bit 4
      xoffs={0,1,2,3, 8,9,10,11, 256,...,259, 264,...,267}
      yoffs={0,16,32,...,240}
      charincrement=512 bits=64 bytes

    Each sprite row = 16 bits (2 bytes). Left 8px in first 32 bytes, right 8px in next 32.
    Within each byte pair: bits 0-3 = plane0/1 nibble, bits 4-7 = plane0/1 nibble.
*/
static void decodeGnGSprites( unsigned char * dst, const unsigned char * src, int count )
{
    // MAME GfxLayout: 16x16, 4bpp, charincrement=64 bytes
    // planes = { RGN_FRAC(1,2)+4, RGN_FRAC(1,2)+0, 4, 0 }
    // xoffs  = { 0,1,2,3, 8,9,10,11, 256,257,258,259, 264,265,266,267 }
    // yoffs  = { 0,16,32,...,112, 256,272,288,...,368 }
    static const int xoffs[16] = { 0,1,2,3, 8,9,10,11, 256,257,258,259, 264,265,266,267 };
    static const int yoffs[16] = { 0,16,32,48,64,80,96,112, 256,272,288,304,320,336,352,368 };

    for( int c = 0; c < count; c++ ) {
        int charbase = c * 512; // charincrement=512 bits
        unsigned char * d = dst + c * 256;
        for( int y = 0; y < 16; y++ ) {
            for( int x = 0; x < 16; x++ ) {
                int bitpos = charbase + yoffs[y] + xoffs[x];
                int byte0 = bitpos / 8;
                int bit0  = bitpos % 8;

                int pixel = 0;
                // plane[3]=bit 0 (LSB), plane[2]=bit 4, both in first half
                pixel |= ((src[byte0] >> bit0) & 1) << 0;
                pixel |= ((src[byte0] >> (bit0 + 4)) & 1) << 1;
                // plane[1]=bit 0, plane[0]=bit 4, both in second half (+0xC000)
                pixel |= ((src[byte0 + 0xC000] >> bit0) & 1) << 2;
                pixel |= ((src[byte0 + 0xC000] >> (bit0 + 4)) & 1) << 3;

                d[y * 16 + x] = pixel;
            }
        }
    }
}

void GnG::decodeGraphics()
{
    decodeGnGChars( char_data_.data(), char_rom_, 1024 );     // 0x4000 / 16
    decodeGnGTiles( tile_data_.data(), tile_rom_, 1024 );     // 0x8000 / 32
    decodeGnGSprites( sprite_data_.data(), sprite_rom_, 768 ); // 0xC000 / 64
}

// ---------------------------------------------------------------------------
// Video rendering
// ---------------------------------------------------------------------------
TBitmapIndexed * GnG::renderVideo()
{
    // Update palette
    for( int i = 0; i < 0x100; i++ ) {
        decodePalette( palette(), main_board_.paletteram_2_, main_board_.paletteram_1_, i );
    }

    // Clear screen
    screen()->bits()->fill( 0 );

    // --- Background tilemap ---
    // BG tile map: 32x32, each tile 16x16, 3bpp, colors 0x00-0x3F (8 palettes of 8)
    // tilemap_scan_cols: tile_index = x * 32 + y (column-major order)
    int bgScrollX = main_board_.bgscrollx_[0] + 256 * main_board_.bgscrollx_[1];
    int bgScrollY = main_board_.bgscrolly_[0] + 256 * main_board_.bgscrolly_[1];

    // Draw back portion of BG (split type: transmask[0] = 0xFF means fully transparent in front)
    // We draw BG as a single pass for simplicity, then sprites, then FG
    for( int tx = 0; tx < 32; tx++ ) {
        for( int ty = 0; ty < 32; ty++ ) {
            // tilemap_scan_cols: index = col * num_rows + row
            int tile_index = tx * 32 + ty;
            unsigned char code_lo = main_board_.bgvideoram_[tile_index];
            unsigned char attr = main_board_.bgvideoram_[tile_index + 0x400];
            int code = code_lo + ((attr & 0xC0) << 2);
            int color = attr & 0x07;
            int flipx = (attr >> 4) & 1;
            int flipy = (attr >> 5) & 1;

            if( code >= 1024 ) continue;

            int sx = tx * 16 - bgScrollX;
            int sy = ty * 16 - bgScrollY;

            // Wrap
            sx = ((sx % 512) + 512) % 512;
            sy = ((sy % 512) + 512) % 512;

            // Only draw if visible
            if( sx >= ScreenWidth || sy >= ScreenHeight ) {
                // Could wrap - check
                if( sx >= ScreenWidth ) sx -= 512;
                if( sy >= ScreenHeight ) sy -= 512;
            }

            int drawY = sy - ScreenVisibleOffsetY;
            unsigned colorByte = (unsigned char)(color * 8);

            unsigned op = 0;
            if( flipx ) op |= opFlipX;
            if( flipy ) op |= opFlipY;

            TBltAddSrcZeroTrans blitter(0);
            screen()->bits()->copy( sx, drawY, tile_data_, 0, 16 * code, 16, 16, op, blitter.color(colorByte) );
        }
    }

    // --- Sprites ---
    // sprites use colors 0x40-0x7F (4 palettes of 16), transparent pen = 15
    for( int offs = 0x200 - 4; offs >= 0; offs -= 4 ) {
        unsigned char attributes = main_board_.buffered_spriteram_[offs + 1];
        int sx = main_board_.buffered_spriteram_[offs + 3] - 0x100 * (attributes & 0x01);
        int sy = main_board_.buffered_spriteram_[offs + 2];
        int flipx = attributes & 0x04;
        int flipy = attributes & 0x08;

        int code = main_board_.buffered_spriteram_[offs] + ((attributes << 2) & 0x300);
        int color = (attributes >> 4) & 3;

        if( code >= 768 ) continue;

        if( main_board_.flipscreen_ ) {
            sx = 240 - sx;
            sy = 240 - sy;
            flipx = !flipx;
            flipy = !flipy;
        }

        int drawY = sy - ScreenVisibleOffsetY;
        unsigned colorByte = (unsigned char)(0x40 + color * 16);

        unsigned op = 0;
        if( flipx ) op |= opFlipX;
        if( flipy ) op |= opFlipY;

        TBltAddSrcTrans blitter( 0, 15 );
        screen()->bits()->copy( sx, drawY, sprite_data_, 0, 16 * code, 16, 16, op, blitter.color(colorByte) );
    }

    // --- Foreground tilemap ---
    // FG: 32x32, each tile 8x8, 2bpp, colors 0x80-0xBF (16 palettes of 4)
    // transparent pen = 3
    // tilemap_scan_rows: index = row * 32 + col
    for( int ty = 0; ty < 32; ty++ ) {
        for( int tx = 0; tx < 32; tx++ ) {
            int tile_index = ty * 32 + tx;
            unsigned char code_lo = main_board_.fgvideoram_[tile_index];
            unsigned char attr = main_board_.fgvideoram_[tile_index + 0x400];
            int code = code_lo + ((attr & 0xC0) << 2);
            int color = attr & 0x0F;
            int flipx = (attr >> 4) & 1;
            int flipy = (attr >> 5) & 1;

            if( code >= 1024 ) continue;

            int sx = tx * 8;
            int sy = ty * 8 - ScreenVisibleOffsetY;

            unsigned colorByte = (unsigned char)(0x80 + color * 4);

            unsigned op = 0;
            if( flipx ) op |= opFlipX;
            if( flipy ) op |= opFlipY;

            TBltAddSrcTrans blitter( 0, 3 );
            screen()->bits()->copy( sx, sy, char_data_, 0, 8 * code, 8, 8, op, blitter.color(colorByte) );
        }
    }

    return screen();
}
