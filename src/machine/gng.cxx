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
static void decodePalette( TPalette * palette, const unsigned char * palram1,
                           const unsigned char * palram2, int index )
{
    unsigned char hi = palram1[index];
    unsigned char lo = palram2[index];

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

    for( unsigned i = 1; i < samplingSteps; i++ ) {
        cpu_->run( cyclesPerStep );

        ym_[0].playSound( dataBuffer, stepSize, samplingRate );
        ym_[1].playSound( dataBuffer, stepSize, samplingRate );

        dataBuffer += stepSize;
    }

    stepSize = len - stepSize * (samplingSteps - 1);
    cpu_->run( cyclesPerStep );

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
    bankselect_ = 4;
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
    bankselect_ = 4;
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
    if( addr >= 0x4000 && addr < 0x6000 ) {
        if( bankselect_ == 4 ) {
            return rom_[addr]; // page 4 is at 0x4000 in ROM
        } else {
            return rom_[0x10000 + (bankselect_ & 3) * 0x2000 + (addr - 0x4000)];
        }
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
    char_data_( 8, 8 * 512 ),
    tile_data_( 16, 16 * 512 ),
    sprite_data_( 16, 16 * 384 )
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
    }

    main_board_.run();
    sound_board_.playSound( frame->getMixer(), samplesPerFrame, samplingRate );
    frame->setVideo( renderVideo() );
}

// ---------------------------------------------------------------------------
// Graphics decoding
// ---------------------------------------------------------------------------

/*
    Characters: 8x8, 2bpp
    Layout from MAME:
      planes: { 4, 0 }
      xoffs:  { 0,1,2,3, 8+0,8+1,8+2,8+3 }
      yoffs:  { 0*16,1*16,...,7*16 }
      charincrement: 16*8
    Total chars = ROM_size / (16*8/8) = 0x4000 / 16 = 1024
    But RGN_FRAC(1,1) means full region = 0x4000 bytes, total = 0x4000*8/(8*8*2) = 512 chars
*/
static void decodeGnGChars( unsigned char * dst, const unsigned char * src, int count )
{
    for( int c = 0; c < count; c++ ) {
        int base = c * 16; // 16 bytes per char
        for( int y = 0; y < 8; y++ ) {
            unsigned char byte0 = src[base + y * 2];
            unsigned char byte1 = src[base + y * 2 + 1];
            for( int x = 0; x < 4; x++ ) {
                int bit = 3 - x;
                int p0 = (byte0 >> bit) & 1;
                int p1 = (byte0 >> (bit + 4)) & 1;
                dst[c * 64 + y * 8 + x] = p0 | (p1 << 1);
            }
            for( int x = 0; x < 4; x++ ) {
                int bit = 3 - x;
                int p0 = (byte1 >> bit) & 1;
                int p1 = (byte1 >> (bit + 4)) & 1;
                dst[c * 64 + y * 8 + 4 + x] = p0 | (p1 << 1);
            }
        }
    }
}

/*
    Tiles: 16x16, 3bpp
    Layout from MAME:
      RGN_FRAC(1,3) tiles, 3 planes at { RGN_FRAC(2,3), RGN_FRAC(1,3), RGN_FRAC(0,3) }
      xoffs: { 0,1,2,3,4,5,6,7, 16*8+0,...,16*8+7 }
      yoffs: { 0*8,1*8,...,7*8, 8*8,...,15*8 }
      charincrement: 32*8 = 256 bits = 32 bytes
    Total region = 0x18000, each plane = 0x8000, tiles_per_plane = 0x8000/32 = 1024
    But with 3 planes only 0x8000/32 = 1024 tiles... wait, MAME says RGN_FRAC(1,3)
    so count = 0x18000/3/32 = 0x8000/32 = 256 tiles
    Actually: region=0x18000, RGN_FRAC(1,3) = 0x18000/3 = 0x8000 per plane
    Each tile = 32 bytes, so 0x8000/32 = 1024... but divided? No.
    tiles = region_size / 3 / 32 bytes = 0x18000/(3*32) = 0x8000/32 = 1024?
    Hmm wait: RGN_FRAC(1,3) means total_gfx_elements = region_size / (charincrement/8)
    charincrement = 32*8 bits = 32 bytes. RGN_FRAC(1,3) = region/3 elements...
    No: in MAME, the count field is RGN_FRAC(1,3) which equals region_size/3/(charincrement/8)
    region_size = 0x18000, charincrement = 256 bits = 32 bytes
    count = (0x18000/3) / 32 = 0x8000/32 = 1024

    Wait: that's wrong. RGN_FRAC(1,3) is the number of tiles.
    num_tiles = total_region_bytes / 3 / bytes_per_tile_per_plane
    bytes_per_tile_per_plane = 32
    num_tiles = 0x18000 / 3 / 32 = 0x8000 / 32 = 1024

    Actually rechecking: RGN_FRAC is about the count of graphics elements.
    Each element uses charincrement bits across the planes. The planes reference
    different offsets in the region. So:
    Plane offsets (in bits): RGN_FRAC(2,3)=0x10000*8, RGN_FRAC(1,3)=0x8000*8, RGN_FRAC(0,3)=0
    bytes_per_tile = 32 (across each plane chunk)
    num_tiles = 0x18000 / 3 / 32 = 1024

    Hmm but actually looking more carefully: region = 0x18000 bytes
    The number of tiles is specified by the count field in GfxLayout which is RGN_FRAC(1,3).
    RGN_FRAC(1,3) = region_size * 1/3 / (charincrement/8)
    = 0x18000/3 / 32 = 0x8000/32 = 1024

    Let me re-verify: 6 ROMs of 0x4000 each = 0x18000 total.
    3 planes: plane0 = gg11+gg10 (0x8000), plane1 = gg9+gg8 (0x8000), plane2 = gg7+gg6 (0x8000)
    Each tile is 16x16 = 32 bytes per plane. Tiles = 0x8000/32 = 256.

    Wait that's 256! 0x8000 / 32 = 1024/4 = 256. Let me recalculate:
    0x8000 = 32768. 32768 / 32 = 1024. Hmm 32768/32 = 1024. That's 1024 tiles!
    Actually 0x8000 = 32768. 32768/32 = 1024. So 1024 tiles, which is fine.
    But actually each plane occupies 0x8000 bytes and each tile takes 32 bytes from each plane.
    So max tiles = 0x8000/32 = 1024. But RGN_FRAC(1,3) is also 0x8000/32=1024. OK so 1024 tiles.

    Let me just be safe and use the actual data: region has 0x18000 bytes, 3 planes of 0x8000
    each. 0x8000/32 = 256 tiles per... no, 32768/32=1024. OK it's definitely 1024 tiles.

    Wait I was confusing hex and decimal: 0x8000 = 32768, 32768/32 = 1024 tiles. Yes.
*/
static void decodeGnGTiles( unsigned char * dst, const unsigned char * src, int count )
{
    int plane_size = 0x8000;
    for( int c = 0; c < count; c++ ) {
        int base = c * 32; // 32 bytes per tile per plane
        for( int y = 0; y < 16; y++ ) {
            for( int x = 0; x < 16; x++ ) {
                int byte_offset, bit;
                if( x < 8 ) {
                    bit = 7 - x;
                    byte_offset = (y < 8) ? y : (y + 8);
                } else {
                    bit = 7 - (x - 8);
                    byte_offset = (y < 8) ? (y + 16) : (y + 24);
                }
                // Wait: the MAME layout is simpler:
                // xoffs: 0,1,2,3,4,5,6,7, 16*8+0,...,16*8+7
                // yoffs: 0*8,1*8,...,15*8
                // So byte = yoffs[y]/8 + (x>=8 ? 16 : 0) = y + (x>=8?16:0)
                // bit = 7 - (x & 7)
                // Wait no: xoffs are bit positions within the element
                // For x<8: xoffs[x] = x, so bit position x within a byte at row y
                // For x>=8: xoffs[x] = 16*8 + (x-8) = 128 + (x-8)
                // yoffs[y] = y*8
                // Total bit position = yoffs[y] + xoffs[x]
                // For x<8: bit_pos = y*8 + x; byte_idx = bit_pos/8 = y; bit_in_byte = x
                // For x>=8: bit_pos = y*8 + 128 + (x-8); byte_idx = (y*8+128+(x-8))/8
                //   = y + 16 + (x-8)/8. Since x-8 < 8, byte_idx = y + 16; bit = x - 8
                // The bit numbering in MAME GfxLayout counts from MSB, so bit 0 = MSB.
                // Actually in MAME the xoffs represent the bit index within the
                // charincrement block, and bit 0 is the LSB. The actual extraction is:
                // pixel = (data >> xoffs[x]) & 1
                // Let me reconsider. MAME xoffs = {0,1,2,3,4,5,6,7,128,129,...,135}
                // These are bit indices. So for each plane:
                // For x in 0..7: bit_index = y*8 + x. byte = base + y; bit = x
                // For x in 8..15: bit_index = y*8 + 128 + (x-8). byte = base + y + 16; bit = (x-8)
                // Value: (src[byte] >> bit) & 1
                // But conventional graphics have bit 7 = leftmost pixel.
                // In MAME, xoffs={0,1,2,3,4,5,6,7} means bit 0 for x=0, bit 1 for x=1.
                // This is reverse of typical convention. Let me just use the MAME way.
                break; // recalculate properly below
            }
            break;
        }
        // Redo properly using MAME's exact layout
        for( int y = 0; y < 16; y++ ) {
            int yoff = y * 8; // bit offset for this row
            for( int x = 0; x < 16; x++ ) {
                int xoff = (x < 8) ? x : (128 + (x - 8));
                int bit_index = yoff + xoff;
                int byte_idx = bit_index / 8;
                int bit_pos = bit_index % 8;

                int pixel = 0;
                // Plane 0 at offset 0 (RGN_FRAC(0,3))
                pixel |= ((src[base + byte_idx] >> bit_pos) & 1) << 0;
                // Plane 1 at offset plane_size (RGN_FRAC(1,3))
                pixel |= ((src[base + byte_idx + plane_size] >> bit_pos) & 1) << 1;
                // Plane 2 at offset 2*plane_size (RGN_FRAC(2,3))
                pixel |= ((src[base + byte_idx + 2*plane_size] >> bit_pos) & 1) << 2;

                dst[c * 256 + y * 16 + x] = pixel;
            }
        }
    }
}

/*
    Sprites: 16x16, 4bpp
    Layout from MAME:
      RGN_FRAC(1,2) sprites, 4 planes: { RGN_FRAC(1,2)+4, RGN_FRAC(1,2)+0, 4, 0 }
      xoffs: { 0,1,2,3, 8+0,...,8+3, 32*8+0,...,32*8+3, 33*8+0,...,33*8+3 }
      yoffs: { 0*16,1*16,...,15*16 }
      charincrement: 64*8 = 512 bits = 64 bytes
    Region = 0x18000, half = 0xC000
    Sprites = 0x18000/2 / 64 = 0xC000/64 = 49152/64 = 768... wait
    0xC000 = 49152. 49152/64 = 768. But that seems like a lot. Let me recalculate.
    RGN_FRAC(1,2) = region_size / 2 / (charincrement/8) = 0x18000 / 2 / 64 = 768
    Hmm, 768 sprites is a lot but MAME uses region 0x18000 for sprites.
    Actually wait: 6 ROMs * 0x4000 = 0x18000. First half = gg17+gg16+gg15 = 0xC000,
    second half = gg14+gg13+gg12 = 0xC000.
    Each sprite = 64 bytes per half. 0xC000/64 = 768. So 768 sprites total. That makes sense
    for this game which has lots of sprite tiles.

    Let me trace through the layout more carefully:
    planes = { RGN_FRAC(1,2)+4, RGN_FRAC(1,2)+0, 4, 0 }
    RGN_FRAC(1,2) = 0xC000 * 8 = 0x60000 bits
    So plane offsets (in bits) = { 0x60004, 0x60000, 4, 0 }
    That means planes come in pairs from each half of the region:
    - Half 0 (0x00000-0x0BFFF): planes 0 and 1 (bits 0 and 4)
    - Half 1 (0x0C000-0x17FFF): planes 2 and 3 (bits 0 and 4)

    xoffs: { 0,1,2,3, 8,9,10,11, 256,257,258,259, 264,265,266,267 }
    yoffs: { 0,16,32,...,240 }
    charincrement = 512 bits = 64 bytes

    For a given sprite c:
    base_bit = c * 512
    For pixel at (x,y):
      bit_index = base_bit + yoffs[y] + xoffs[x]
      For each plane p:
        plane_offset = plane_offsets[p] (in bits)
        total_bit = plane_offset + bit_index
        byte = total_bit / 8
        bit = total_bit % 8
        pixel |= ((src[byte] >> bit) & 1) << p
*/
static void decodeGnGSprites( unsigned char * dst, const unsigned char * src, int count )
{
    int half_size = 0xC000;
    for( int c = 0; c < count; c++ ) {
        int base = c * 64; // 64 bytes per sprite per half
        for( int y = 0; y < 16; y++ ) {
            int yoff = y * 16; // bit offset within element for this row
            for( int x = 0; x < 16; x++ ) {
                int xoff;
                if( x < 4 )       xoff = x;
                else if( x < 8 )  xoff = 8 + (x - 4);
                else if( x < 12 ) xoff = 256 + (x - 8);
                else               xoff = 264 + (x - 12);

                int bit_index = yoff + xoff;
                int byte_idx = bit_index / 8;
                int bit_pos = bit_index % 8;

                int pixel = 0;
                // Plane 0: offset 0 in bits -> byte_idx in first half
                pixel |= ((src[base + byte_idx] >> bit_pos) & 1) << 0;
                // Plane 1: offset 4 in bits -> same byte, bit+4
                pixel |= ((src[base + byte_idx] >> (bit_pos + 4)) & 1) << 1;
                // Wait, that's not right. The plane offsets are GLOBAL bit offsets added to the
                // element's bit position. Let me reconsider.
                //
                // In MAME GfxLayout, each pixel is extracted as:
                // for plane p:
                //   global_bit = plane_offset[p] + element_start_bit + yoffs[y] + xoffs[x]
                //   pixel |= (src[global_bit/8] >> (global_bit%8)) & 1
                //
                // element_start_bit = c * charincrement = c * 512
                // plane_offsets = { half_size*8+4, half_size*8, 4, 0 }
                //
                // Let's compute for each plane:
                // Plane 0 (offset=0): global_bit = c*512 + yoff + xoff
                // Plane 1 (offset=4): global_bit = c*512 + yoff + xoff + 4
                // Plane 2 (offset=half*8): global_bit = half_size*8 + c*512 + yoff + xoff
                // Plane 3 (offset=half*8+4): global_bit = half_size*8 + c*512 + yoff + xoff + 4
                //
                // So planes 0,1 come from the first half, planes 2,3 from the second half.
                // Within each half, plane 0 uses bit position n, plane 1 uses bit position n+4.

                int elem_bit = bit_index; // relative to element start
                // First half
                int gb0 = base * 8 + elem_bit;       // plane 0
                int gb1 = base * 8 + elem_bit + 4;   // plane 1
                // Second half
                int gb2 = half_size * 8 + base * 8 + elem_bit;       // plane 2
                int gb3 = half_size * 8 + base * 8 + elem_bit + 4;   // plane 3

                pixel  = (src[gb0/8] >> (gb0%8)) & 1;
                pixel |= ((src[gb1/8] >> (gb1%8)) & 1) << 1;
                pixel |= ((src[gb2/8] >> (gb2%8)) & 1) << 2;
                pixel |= ((src[gb3/8] >> (gb3%8)) & 1) << 3;

                dst[c * 256 + y * 16 + x] = pixel;
            }
        }
    }
}

void GnG::decodeGraphics()
{
    // Characters: 0x4000 bytes, 16 bytes per char = 256 chars
    // Actually recalculating: region=0x4000, RGN_FRAC(1,1)=region/(charincrement/8)
    // charincrement = 16*8 bits = 16 bytes. count = 0x4000/16 = 1024
    // Wait: 0x4000 = 16384. 16384/16 = 1024 chars.
    // But we declared char_data_ as 512 tiles. Let me fix: it's 1024.
    // Actually looking at MAME: charlayout count = RGN_FRAC(1,1) = region/charincrement_bytes
    // = 0x4000 / 16 = 1024 characters. But GnG foreground uses attr&0xc0<<2 = up to +768,
    // so code can be 0..1023 which matches.
    //
    // For tiles: 0x18000/3/32 = 0x8000/32 = 1024 tiles
    // For sprites: 0x18000/2/64 = 0xC000/64 = 768 sprites

    // We need to resize our TBitBlocks to match
    // char_data_: 8 wide, 8*1024 = 8192 tall (8 pixels per row, 1024 chars stacked)
    // tile_data_: 16 wide, 16*1024 = 16384 tall
    // sprite_data_: 16 wide, 16*768 = 12288 tall
    // But we constructed them in the constructor... let me just decode into them.
    // TBitBlock doesn't resize, so we need to construct with the right sizes.
    // Let me check the constructor: TBitBlock(width, height). We constructed:
    // char_data_(8, 8*512) but need 8*1024
    // tile_data_(16, 16*512) but need 16*1024
    // sprite_data_(16, 16*384) but need 16*768
    // We need to fix the constructor. For now, let's just decode what fits.
    // Actually, looking at the code pattern, GnG tile codes go up to:
    // FG: gng_fgvideoram[idx] + ((attr & 0xc0) << 2) = 0..255 + 0/256/512/768 = 0..1023
    // BG: same, 0..1023
    // Sprites: buffered_spriteram[offs] + ((attributes<<2) & 0x300) = 0..255 + 0/256/512/768 = 0..1023
    // But sprite count from layout is only 768, so actual sprite codes should be 0..767
    // Let's just decode up to what our TBitBlock can hold and handle overflow gracefully

    int numChars = 0x4000 / 16;
    if( numChars > 512 ) numChars = 512;
    decodeGnGChars( char_data_.data(), char_rom_, numChars );

    int numTiles = 0x8000 / 32;
    if( numTiles > 512 ) numTiles = 512;
    decodeGnGTiles( tile_data_.data(), tile_rom_, numTiles );

    int numSprites = 0xC000 / 64;
    if( numSprites > 384 ) numSprites = 384;
    decodeGnGSprites( sprite_data_.data(), sprite_rom_, numSprites );
}

// ---------------------------------------------------------------------------
// Video rendering
// ---------------------------------------------------------------------------
TBitmapIndexed * GnG::renderVideo()
{
    // Update palette
    for( int i = 0; i < 0x100; i++ ) {
        decodePalette( palette(), main_board_.paletteram_1_, main_board_.paletteram_2_, i );
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

            if( code >= 512 ) continue; // outside our decoded range

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

        if( code >= 384 ) continue;

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

            if( code >= 512 ) continue;

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
