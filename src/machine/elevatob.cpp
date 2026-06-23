/*
    Elevator Action arcade machine emulator

    Taito SJ system hardware (elevatob romset, bootleg without MCU)
    Based on MAME 0.37b7

    Copyright (c) 2024
*/
#include "elevatob.h"

enum {
    ScreenWidth             = 256,
    ScreenHeight            = 256,
    ScreenVisibleWidth      = 256,
    ScreenVisibleHeight     = 224,
    ScreenVisibleOffsetY    = 16,
    ScreenColors            = 64,
    VideoFrequency          = 60,
    MainCpuClock            = 4000000,
    SoundCpuClock           = 3000000,
    MainCpuCyclesPerFrame   = MainCpuClock / VideoFrequency,
    SoundCpuCyclesPerFrame  = SoundCpuClock / VideoFrequency,
    SoundChipClock          = 6000000 / 4
};

enum {
    Ef69, Ef68, Ef67, Ef66, Ef65, Ef64, Ef55, Ef54, Ef52,
    EfS70, EfS71,
    EfGfx1, EfGfx2, EfGfx3, EfGfx4, EfGfx5, EfGfx6, EfGfx7, EfGfx8,
    EfLayerProm,
    OptBonusLife, OptFreePlay, OptLives, OptFlipScreen, OptCabinet,
    OptCoinA, OptCoinB,
    OptDifficulty, OptCoinageDisplay, OptYearDisplay, OptCoinage
};

static TMachineInfo ElevatorActionInfo = {
    "elevatob", "Elevator Action (bootleg)", S_Taito, 1983,
    ScreenVisibleWidth, ScreenVisibleHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler reg( &ElevatorActionInfo, ElevatorAction::createInstance );

// ---------------------------------------------------------------------------
// Palette conversion (Taito SJ system)
// ---------------------------------------------------------------------------
static void decodePalette( TPalette * palette, const unsigned char * paletteram, int index )
{
    int val, bit0, bit1, bit2;
    int r, g, b;

    val = paletteram[index | 1];
    bit0 = (~val >> 6) & 0x01;
    bit1 = (~val >> 7) & 0x01;
    val = paletteram[index & ~1];
    bit2 = (~val >> 0) & 0x01;
    r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

    val = paletteram[index | 1];
    bit0 = (~val >> 3) & 0x01;
    bit1 = (~val >> 4) & 0x01;
    bit2 = (~val >> 5) & 0x01;
    g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

    val = paletteram[index | 1];
    bit0 = (~val >> 0) & 0x01;
    bit1 = (~val >> 1) & 0x01;
    bit2 = (~val >> 2) & 0x01;
    b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

    palette->setColor( index / 2, TPalette::encodeColor(r,g,b) );
}

// ---------------------------------------------------------------------------
// Sound board
// ---------------------------------------------------------------------------
ElevatorActionSoundBoard::ElevatorActionSoundBoard()
{
    cpu_ = new Z80( *this );
    sound_command_ = 0;
    sndnmi_disable_ = 1;

    for( int i = 0; i < 4; i++ ) {
        sound_chip_[i].setClock( SoundChipClock );
    }

    memset( rom_, 0, sizeof(rom_) );
    memset( ram_, 0, sizeof(ram_) );
}

ElevatorActionSoundBoard::~ElevatorActionSoundBoard()
{
    delete cpu_;
}

void ElevatorActionSoundBoard::reset()
{
    cpu_->reset();
    sndnmi_disable_ = 1;
    for( int i = 0; i < 4; i++ ) {
        sound_chip_[i].reset();
    }
}

unsigned char ElevatorActionSoundBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr < 0x4000 ) {
        return rom_[addr];
    }

    if( addr >= 0x4000 && addr < 0x4400 ) {
        return ram_[addr - 0x4000];
    }

    switch( addr ) {
        case 0x4801:
            return sound_chip_[1].readData();
        case 0x4803:
            return sound_chip_[2].readData();
        case 0x4805:
            return sound_chip_[3].readData();
        case 0x5000:
            return sound_command_;
    }

    if( addr >= 0xE000 && addr < 0xF000 ) {
        return rom_[addr - 0xE000];
    }

    return 0xFF;
}

void ElevatorActionSoundBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr >= 0x4000 && addr < 0x4400 ) {
        ram_[addr - 0x4000] = value;
        return;
    }

    switch( addr ) {
        case 0x4800:
            sound_chip_[1].writeAddress(value);
            break;
        case 0x4801:
            sound_chip_[1].writeData(value);
            break;
        case 0x4802:
            sound_chip_[2].writeAddress(value);
            break;
        case 0x4803:
            sound_chip_[2].writeData(value);
            break;
        case 0x4804:
            sound_chip_[3].writeAddress(value);
            break;
        case 0x4805:
            sound_chip_[3].writeData(value);
            break;
    }
}

void ElevatorActionSoundBoard::triggerNmi()
{
    if( sndnmi_disable_ == 0 ) {
        cpu_->nmi();
    }
}

void ElevatorActionSoundBoard::run( unsigned cycles )
{
    cpu_->run( cycles );
}

void ElevatorActionSoundBoard::playSound( TMixer * mixer, unsigned len, unsigned samplingRate )
{
    unsigned samplingSteps = 12;
    unsigned stepSize = len / samplingSteps;
    unsigned cyclesPerStep = SoundCpuCyclesPerFrame / samplingSteps;

    TMixerBuffer * mixerBuffer = mixer->getBuffer( chMono, len, 12 );
    int * dataBuffer = mixerBuffer->data();

    for( unsigned i = 1; i < samplingSteps; i++ ) {
        cpu_->run( cyclesPerStep );

        for( int j = 0; j < 4; j++ ) {
            sound_chip_[j].playSound( dataBuffer, stepSize, samplingRate );
        }

        dataBuffer += stepSize;
    }

    stepSize = len - stepSize * (samplingSteps - 1);
    cpu_->run( cyclesPerStep );

    for( int j = 0; j < 4; j++ ) {
        sound_chip_[j].playSound( dataBuffer, stepSize, samplingRate );
    }
}

// ---------------------------------------------------------------------------
// Main board
// ---------------------------------------------------------------------------
ElevatorActionMainBoard::ElevatorActionMainBoard( ElevatorActionSoundBoard * sound_board )
{
    cpu_ = new Z80( *this );
    sound_board_ = sound_board;

    memset( rom_, 0xFF, sizeof(rom_) );
    memset( ram_, 0, sizeof(ram_) );
    memset( characterram_, 0, sizeof(characterram_) );
    memset( ram_c000_, 0, sizeof(ram_c000_) );
    memset( videoram_[0], 0, sizeof(videoram_[0]) );
    memset( videoram_[1], 0, sizeof(videoram_[1]) );
    memset( videoram_[2], 0, sizeof(videoram_[2]) );
    memset( colscrolly_, 0, sizeof(colscrolly_) );
    memset( spriteram_, 0, sizeof(spriteram_) );
    memset( paletteram_, 0, sizeof(paletteram_) );
    memset( scroll_, 0, sizeof(scroll_) );
    memset( colorbank_, 0, sizeof(colorbank_) );
    memset( gfxpointer_, 0, sizeof(gfxpointer_) );
    memset( collision_reg_, 0, sizeof(collision_reg_) );

    video_priority_ = 0;
    video_enable_ = 0;

    port0_ = 0xFF;
    port1_ = 0xFF;
    port2_ = 0xFF;
    port3_ = 0xFF;
    port4_ = 0xFF;
    dsw1_ = 0xFF;
    dsw2_ = 0x00;
    dsw3_ = 0xFF;

    curr_bank_ = rom_ + 0x6000;
    gfx_rom_ = 0;

    reset();
}

void ElevatorActionMainBoard::reset()
{
    cpu_->reset();
    curr_bank_ = rom_ + 0x6000;
}

void ElevatorActionMainBoard::run()
{
    cpu_->run( MainCpuCyclesPerFrame );
    cpu_->interrupt( 0xFF );
}

unsigned char ElevatorActionMainBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    if( addr < 0x6000 ) {
        return rom_[addr];
    }

    if( addr >= 0x6000 && addr < 0x8000 ) {
        return curr_bank_[addr - 0x6000];
    }

    if( addr >= 0x8000 && addr < 0x8800 ) {
        return ram_[addr - 0x8000];
    }

    // MCU fake (bootleg: always returns 0)
    if( addr == 0x8800 ) {
        return 0x00;
    }
    // MCU fake status (bootleg: returns 0xFF = ready)
    if( addr == 0x8801 ) {
        return 0xFF;
    }

    if( addr >= 0xC000 && addr < 0xC400 ) {
        return ram_c000_[addr - 0xC000];
    }

    if( addr >= 0xC400 && addr < 0xC800 ) {
        return videoram_[0][addr - 0xC400];
    }
    if( addr >= 0xC800 && addr < 0xCC00 ) {
        return videoram_[1][addr - 0xC800];
    }
    if( addr >= 0xCC00 && addr < 0xD000 ) {
        return videoram_[2][addr - 0xCC00];
    }

    if( addr >= 0xD000 && addr < 0xD060 ) {
        return colscrolly_[addr - 0xD000];
    }

    if( addr >= 0xD100 && addr < 0xD180 ) {
        return spriteram_[addr - 0xD100];
    }

    if( addr >= 0xD400 && addr < 0xD404 ) {
        return collision_reg_[addr - 0xD400];
    }

    // GFX ROM read via gfxpointer
    if( addr == 0xD404 ) {
        unsigned offs = gfxpointer_[0] + gfxpointer_[1] * 256;
        gfxpointer_[0]++;
        if( gfxpointer_[0] == 0 ) gfxpointer_[1]++;
        if( gfx_rom_ && offs < 0x8000 ) return gfx_rom_[offs];
        return 0;
    }

    switch( addr & 0xFF0F ) {
        case 0xD408: return port0_;
        case 0xD409: return port1_;
        case 0xD40A: return dsw1_;
        case 0xD40B: return port2_;
        case 0xD40C: return port3_;
        case 0xD40D: return port4_;
        case 0xD40F: return sound_board_->sound_chip_[0].readData();
    }

    if( addr >= 0xE000 && addr < 0xF000 ) {
        return rom_[addr - 0xE000 + 0x8000];
    }

    return 0xFF;
}

void ElevatorActionMainBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    if( addr < 0x8000 ) return;

    if( addr >= 0x8000 && addr < 0x8800 ) {
        ram_[addr - 0x8000] = value;
        return;
    }

    // MCU fake data write (bootleg: ignored)
    if( addr == 0x8800 ) {
        return;
    }

    if( addr >= 0x9000 && addr < 0xC000 ) {
        characterram_[addr - 0x9000] = value;
        return;
    }

    if( addr >= 0xC000 && addr < 0xC400 ) {
        ram_c000_[addr - 0xC000] = value;
        return;
    }

    if( addr >= 0xC400 && addr < 0xC800 ) {
        videoram_[0][addr - 0xC400] = value;
        return;
    }
    if( addr >= 0xC800 && addr < 0xCC00 ) {
        videoram_[1][addr - 0xC800] = value;
        return;
    }
    if( addr >= 0xCC00 && addr < 0xD000 ) {
        videoram_[2][addr - 0xCC00] = value;
        return;
    }

    if( addr >= 0xD000 && addr < 0xD060 ) {
        colscrolly_[addr - 0xD000] = value;
        return;
    }

    if( addr >= 0xD100 && addr < 0xD180 ) {
        spriteram_[addr - 0xD100] = value;
        return;
    }

    if( addr >= 0xD200 && addr < 0xD280 ) {
        paletteram_[addr - 0xD200] = value;
        return;
    }

    switch( addr & 0xFF0F ) {
        case 0xD300:
            video_priority_ = value;
            return;
        case 0xD40E:
            sound_board_->sound_chip_[0].writeAddress(value);
            return;
        case 0xD40F:
            sound_board_->sound_chip_[0].writeData(value);
            return;
    }

    if( addr >= 0xD500 && addr < 0xD506 ) {
        scroll_[addr - 0xD500] = value;
        return;
    }

    switch( addr ) {
        case 0xD506:
        case 0xD507:
            colorbank_[addr - 0xD506] = value;
            return;
        case 0xD508:
            collision_reg_[0] = 0;
            collision_reg_[1] = 0;
            collision_reg_[2] = 0;
            collision_reg_[3] = 0;
            return;
        case 0xD509:
        case 0xD50A:
            gfxpointer_[addr - 0xD509] = value;
            return;
        case 0xD50B:
            sound_board_->sound_command_ = value;
            sound_board_->triggerNmi();
            return;
        case 0xD50D:
            // watchdog reset (ignored)
            return;
        case 0xD50E:
            curr_bank_ = (value & 0x80) ? rom_ + 0x10000 : rom_ + 0x6000;
            return;
        case 0xD50F:
            // NOP
            return;
        case 0xD600:
            video_enable_ = value;
            return;
    }

    if( addr >= 0xE000 && addr < 0xF000 ) {
        return;
    }
}

unsigned char ElevatorActionMainBoard::readPort( unsigned port )
{
    return 0xFF;
}

void ElevatorActionMainBoard::writePort( unsigned port, unsigned char value )
{
}

// ---------------------------------------------------------------------------
// Machine class
// ---------------------------------------------------------------------------
ElevatorAction::ElevatorAction() :
    main_board_( &sound_board_ ),
    char_data_1_( 8, 8*256 ),
    sprite_data_1_( 16, 16*64 ),
    char_data_2_( 8, 8*256 ),
    sprite_data_2_( 16, 16*64 )
{
    createScreen( ScreenVisibleWidth, ScreenVisibleHeight, ScreenColors );

    main_board_.gfx_rom_ = gfx_rom_;

    refresh_roms_ = true;

    memset( draworder_, 0, sizeof(draworder_) );

    setJoystickHandler( 0, new TJoystickToPortHandler(idJoyP1Joystick1, ptInverted, jm4Way, &main_board_.port0_, 0x04080201) );
    setJoystickHandler( 1, new TJoystickToPortHandler(idJoyP2Joystick1, ptInverted, jm4Way, &main_board_.port1_, 0x04080201) );

    eventHandler()->add( idKeyP1Action1,     ptInverted, &main_board_.port0_, 0x10 );
    eventHandler()->add( idKeyP1Action2,     ptInverted, &main_board_.port0_, 0x20 );
    eventHandler()->add( idKeyP2Action1,     ptInverted, &main_board_.port1_, 0x10 );
    eventHandler()->add( idKeyP2Action2,     ptInverted, &main_board_.port1_, 0x20 );

    eventHandler()->add( idCoinSlot1,        ptInverted, &main_board_.port2_, 0x20 );
    eventHandler()->add( idCoinSlot2,        ptInverted, &main_board_.port2_, 0x10 );
    eventHandler()->add( idKeyStartPlayer1,  ptInverted, &main_board_.port2_, 0x40 );
    eventHandler()->add( idKeyStartPlayer2,  ptInverted, &main_board_.port2_, 0x80 );

    registerDriver( ElevatorActionInfo );
}

bool ElevatorAction::initialize( TMachineDriverInfo * info )
{
    // Main CPU ROMs
    resourceHandler()->add( Ef69,  "ea69.bin",     0x1000, efROM, main_board_.rom_+0x0000 );
    resourceHandler()->add( Ef68,  "ea-ic68.bin",  0x1000, efROM, main_board_.rom_+0x1000 );
    resourceHandler()->add( Ef67,  "ea-ic67.bin",  0x1000, efROM, main_board_.rom_+0x2000 );
    resourceHandler()->add( Ef66,  "ea66.bin",     0x1000, efROM, main_board_.rom_+0x3000 );
    resourceHandler()->add( Ef65,  "ea-ic65.bin",  0x1000, efROM, main_board_.rom_+0x4000 );
    resourceHandler()->add( Ef64,  "ea-ic64.bin",  0x1000, efROM, main_board_.rom_+0x5000 );
    resourceHandler()->add( Ef55,  "ea55.bin",     0x1000, efROM, main_board_.rom_+0x6000 );
    resourceHandler()->add( Ef54,  "ea54.bin",     0x1000, efROM, main_board_.rom_+0x7000 );
    resourceHandler()->add( Ef52,  "ea52.bin",     0x1000, efROM, main_board_.rom_+0x11000 );

    // Sound CPU ROMs
    resourceHandler()->add( EfS70, "ea-ic70.bin",  0x1000, efROM, sound_board_.rom_+0x0000 );
    resourceHandler()->add( EfS71, "ea-ic71.bin",  0x1000, efROM, sound_board_.rom_+0x1000 );

    // GFX ROMs
    resourceHandler()->add( EfGfx1, "ea-ic1.bin",  0x1000, efVideoROM, gfx_rom_+0x0000 );
    resourceHandler()->add( EfGfx2, "ea-ic2.bin",  0x1000, efVideoROM, gfx_rom_+0x1000 );
    resourceHandler()->add( EfGfx3, "ea-ic3.bin",  0x1000, efVideoROM, gfx_rom_+0x2000 );
    resourceHandler()->add( EfGfx4, "ea-ic4.bin",  0x1000, efVideoROM, gfx_rom_+0x3000 );
    resourceHandler()->add( EfGfx5, "ea-ic5.bin",  0x1000, efVideoROM, gfx_rom_+0x4000 );
    resourceHandler()->add( EfGfx6, "ea-ic6.bin",  0x1000, efVideoROM, gfx_rom_+0x5000 );
    resourceHandler()->add( EfGfx7, "ea-ic7.bin",  0x1000, efVideoROM, gfx_rom_+0x6000 );
    resourceHandler()->add( EfGfx8, "ea08.bin",    0x1000, efVideoROM, gfx_rom_+0x7000 );

    // Layer priority PROM
    resourceHandler()->add( EfLayerProm, "eb16.22", 0x100, efPROM, layer_prom_ );

    resourceHandler()->assignToMachineDriverInfo( info );

    // Setup user interface
    TUserInterface * ui = info->userInterface();

    TUiOptionGroup * group = ui->addGroup( S_DSW_1, dtDipSwitch );

    TUiOption * option = group->add( OptBonusLife, "Bonus Life", 3, 0x03 );
    option->add( "10000", 0x03 );
    option->add( "15000", 0x02 );
    option->add( "20000", 0x01 );
    option->add( "24000", 0x00 );

    option = group->add( OptFreePlay, S_FreePlay, 0, 0x04 );
    option->add( S_Off, 0x04 );
    option->add( S_On,  0x00 );

    option = group->add( OptLives, S_Lives, 0, 0x18 );
    option->add( "3", 0x18 );
    option->add( "4", 0x10 );
    option->add( "5", 0x08 );
    option->add( "6", 0x00 );

    option = group->add( OptCabinet, S_Cabinet, 0, 0x80 );
    option->add( S_Upright,  0x00 );
    option->add( S_Cocktail, 0x80 );

    optionHandler()->add( &main_board_.dsw1_, ui, OptBonusLife, OptFreePlay, OptLives, OptCabinet );

    group = ui->addGroup( S_DSW_2, dtDipSwitch );

    option = group->add( OptCoinA, "Coin A", 0, 0x0F );
    option->add( S_1Coin1Play, 0x00 );
    option->add( S_1Coin2Play, 0x01 );
    option->add( S_1Coin3Play, 0x02 );
    option->add( S_1Coin4Play, 0x03 );
    option->add( S_1Coin5Play, 0x04 );
    option->add( S_1Coin6Play, 0x05 );
    option->add( S_2Coin1Play, 0x08 );
    option->add( S_3Coin1Play, 0x09 );
    option->add( S_4Coin1Play, 0x0F );

    option = group->add( OptCoinB, "Coin B", 0, 0xF0 );
    option->add( S_1Coin1Play, 0x00 );
    option->add( S_1Coin2Play, 0x10 );
    option->add( S_1Coin3Play, 0x20 );
    option->add( S_1Coin4Play, 0x30 );
    option->add( S_1Coin5Play, 0x40 );
    option->add( S_1Coin6Play, 0x50 );
    option->add( S_2Coin1Play, 0x80 );
    option->add( S_3Coin1Play, 0x90 );
    option->add( S_4Coin1Play, 0xF0 );

    optionHandler()->add( &main_board_.dsw2_, ui, OptCoinA, OptCoinB );

    group = ui->addGroup( S_DSW_3, dtDipSwitch );

    option = group->add( OptDifficulty, "Difficulty", 0, 0x03 );
    option->add( S_Easiest, 0x03 );
    option->add( S_Easy,    0x02 );
    option->add( S_Normal,  0x01 );
    option->add( S_Hard,    0x00 );

    optionHandler()->add( &main_board_.dsw3_, ui, OptDifficulty );

    return true;
}

bool ElevatorAction::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    refresh_roms_ |= (id >= EfGfx1);

    return 0 == resourceHandler()->handle( id, buf, len );
}

void ElevatorAction::reset()
{
    main_board_.reset();
    sound_board_.reset();
}

void ElevatorAction::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    if( refresh_roms_ ) {
        onVideoROMsChanged();
        refresh_roms_ = false;
    }

    // AY8910 port A reads DSW2, port B reads DSW3
    sound_board_.sound_chip_[0].setRegister( YM2149::PortA, main_board_.dsw2_ );
    sound_board_.sound_chip_[0].setRegister( YM2149::PortB, main_board_.dsw3_ );

    // Sound chip 1 port A is DAC output, port B bit 0 is NMI mask
    main_board_.run();

    // Sound board runs via playSound
    sound_board_.playSound( frame->getMixer(), samplesPerFrame, samplingRate );

    frame->setVideo( renderVideo() );
}

// ---------------------------------------------------------------------------
// Character and sprite decoding (Taito SJ)
// ---------------------------------------------------------------------------
static void decodeTaitoSJChar( unsigned char * dst, const unsigned char * src, int count )
{
    // Taito SJ char layout: 8x8, 3 bits per pixel
    // planeofs = { 512*8*8, 256*8*8, 0 }  (i.e. plane offsets in bits)
    // Each char is 8 bytes (8 rows of 8 bits packed)
    for( int c = 0; c < count; c++ ) {
        int charOffset = c * 8;
        for( int y = 0; y < 8; y++ ) {
            int plane0_byte = src[charOffset + y];
            int plane1_byte = src[charOffset + y + 256*8];
            int plane2_byte = src[charOffset + y + 512*8];
            for( int x = 0; x < 8; x++ ) {
                int bit = 7 - x;
                int pixel = ((plane2_byte >> bit) & 1) << 2;
                pixel |= ((plane1_byte >> bit) & 1) << 1;
                pixel |= ((plane0_byte >> bit) & 1) << 0;
                dst[c * 64 + y * 8 + x] = pixel;
            }
        }
    }
}

static void decodeTaitoSJSprite( unsigned char * dst, const unsigned char * src, int count )
{
    // Taito SJ sprite layout: 16x16, 3 bits per pixel
    // planeofs = { 128*16*16, 64*16*16, 0 }
    // xoffs: {7,6,5,4,3,2,1,0, 8*8+7,...,8*8+0}
    // yoffs: {0*8,1*8,...,7*8, 16*8,...,23*8}
    // Each sprite is 32 bytes (16 rows, but interleaved as two 8-row halves)
    for( int c = 0; c < count; c++ ) {
        for( int y = 0; y < 16; y++ ) {
            for( int x = 0; x < 16; x++ ) {
                int pixel = 0;
                int spriteBytes = 32; // 32 bytes per sprite

                // Determine byte and bit within the sprite
                int rowByte, bitInByte;
                if( x < 8 ) {
                    bitInByte = 7 - x;
                    if( y < 8 ) {
                        rowByte = y;
                    } else {
                        rowByte = y + 8; // 16*8 offset for second half
                    }
                } else {
                    bitInByte = 7 - (x - 8);
                    if( y < 8 ) {
                        rowByte = y + 8; // 8*8 offset for right half
                    } else {
                        rowByte = y + 16; // 24*8 offset
                    }
                }

                int spriteOffset = c * spriteBytes + rowByte;

                int plane0_byte = src[spriteOffset];
                int plane1_byte = src[spriteOffset + 64 * spriteBytes];
                int plane2_byte = src[spriteOffset + 128 * spriteBytes];

                pixel = ((plane2_byte >> bitInByte) & 1) << 2;
                pixel |= ((plane1_byte >> bitInByte) & 1) << 1;
                pixel |= ((plane0_byte >> bitInByte) & 1) << 0;

                dst[c * 256 + y * 16 + x] = pixel;
            }
        }
    }
}

void ElevatorAction::onVideoROMsChanged()
{
    // Decode characters and sprites from character RAM (will be done on-the-fly)
    // The GFX ROM is loaded and used for reading via gfxpointer

    // Build the draw order from the layer priority PROM
    for( int i = 0; i < 32; i++ ) {
        int mask = 0;
        for( int j = 3; j >= 0; j-- ) {
            int data = layer_prom_[0x10 * (i & 0x0F) + mask];
            if( i & 0x10 ) {
                data >>= 2;
            }
            data &= 0x03;
            mask |= (1 << data);
            draworder_[i][j] = data;
        }
    }
}

TBitmapIndexed * ElevatorAction::renderVideo()
{
    // Update palette from palette RAM
    for( int i = 0; i < 0x80; i += 2 ) {
        decodePalette( palette(), main_board_.paletteram_, i );
    }

    // Decode characters and sprites from character RAM on the fly
    decodeTaitoSJChar( char_data_1_.data(), main_board_.characterram_, 256 );
    decodeTaitoSJSprite( sprite_data_1_.data(), main_board_.characterram_, 64 );
    decodeTaitoSJChar( char_data_2_.data(), main_board_.characterram_ + 0x1800, 256 );
    decodeTaitoSJSprite( sprite_data_2_.data(), main_board_.characterram_ + 0x1800, 64 );

    // Background color
    unsigned char bgColor = (main_board_.colorbank_[1] & 0x07) * 8;

    screen()->bits()->fill( bgColor );

    // Playfield enable masks
    int playfield_enable_mask[3] = { 0x10, 0x20, 0x40 };

    // Scrolling fudge factors from MAME
    int fudge1[3] = {3, 1, -1};
    int fudge2[3] = {8, 10, 12};

    int priority = main_board_.video_priority_ & 0x1F;

    for( int layer = 0; layer < 4; layer++ ) {
        int n = draworder_[priority][layer];

        if( n == 0 ) {
            // Draw sprites
            if( main_board_.video_enable_ & 0x80 ) {
                TBltAddSrcZeroTrans sp_blitter(0);

                for( int offs = 0x7C; offs >= 0; offs -= 4 ) {
                    if( offs >= 0x40 && offs <= 0x5F ) continue;

                    int sx = main_board_.spriteram_[offs] - 1;
                    int sy = 240 - main_board_.spriteram_[offs + 1];

                    if( sy >= 240 ) continue;

                    int attr = main_board_.spriteram_[offs + 2];
                    int code = main_board_.spriteram_[offs + 3];

                    int flipx = attr & 1;
                    int flipy = attr & 2;

                    TBitBlock * spriteData = (code & 0x40) ? &sprite_data_2_ : &sprite_data_1_;
                    int spriteCode = code & 0x3F;

                    int color = 2 * ((main_board_.colorbank_[1] >> 4) & 0x03) + ((attr >> 2) & 1);
                    unsigned char colorByte = (unsigned char)(color * 8);

                    unsigned op = 0;
                    if( flipx ) op |= opFlipX;
                    if( flipy ) op |= opFlipY;

                    int drawY = sy - ScreenVisibleOffsetY;
                    screen()->bits()->copy( sx, drawY, *spriteData, 0, 16 * spriteCode, 16, 16, op, sp_blitter.color(colorByte) );

                    // Wrap-around
                    if( sx > 240 ) {
                        screen()->bits()->copy( sx - 256, drawY, *spriteData, 0, 16 * spriteCode, 16, 16, op, sp_blitter.color(colorByte) );
                    }
                }
            }
        } else {
            // Draw playfield n-1
            int pf = n - 1;
            if( (main_board_.video_enable_ & playfield_enable_mask[pf]) == 0 ) continue;

            TBitBlock * charData;
            if( pf == 0 ) {
                charData = (main_board_.colorbank_[0] & 0x08) ? &char_data_2_ : &char_data_1_;
            } else if( pf == 1 ) {
                charData = (main_board_.colorbank_[0] & 0x80) ? &char_data_2_ : &char_data_1_;
            } else {
                charData = (main_board_.colorbank_[1] & 0x08) ? &char_data_2_ : &char_data_1_;
            }

            int colorBase;
            if( pf == 0 ) {
                colorBase = (main_board_.colorbank_[0] & 0x07);
            } else if( pf == 1 ) {
                colorBase = (main_board_.colorbank_[0] >> 4) & 0x07;
            } else {
                colorBase = (main_board_.colorbank_[1] & 0x07);
            }

            int scrollx = main_board_.scroll_[2 * pf];
            scrollx = -(scrollx & 0xF8) + ((scrollx + fudge1[pf]) & 7) + fudge2[pf];

            // Pen 0 is transparent
            TBltAddSrcZeroTrans pf_blitter( 0 );

            // Color: use colorBase+8 to enable transparent pen 0
            unsigned char colorByte = (unsigned char)((colorBase + 8) * 8);

            for( int cx = 0; cx < 32; cx++ ) {
                int colScrollY = -(int)main_board_.colscrolly_[32 * pf + cx]
                                 -(int)main_board_.scroll_[2 * pf + 1];

                for( int cy = 0; cy < 32; cy++ ) {
                    int offs = cy * 32 + cx;
                    unsigned char code = main_board_.videoram_[pf][offs];

                    int sx = cx * 8 + scrollx;
                    int sy = cy * 8 + colScrollY - ScreenVisibleOffsetY;

                    // Wrap
                    sx = ((sx % 256) + 256) % 256;
                    sy = ((sy % 256) + 256) % 256;

                    screen()->bits()->copy( sx, sy, *charData, 0, 8 * code, 8, 8, 0, pf_blitter.color(colorByte) );

                    // Handle wrap-around
                    if( sx > 248 ) {
                        screen()->bits()->copy( sx - 256, sy, *charData, 0, 8 * code, 8, 8, 0, pf_blitter.color(colorByte) );
                    }
                    if( sy > 248 ) {
                        screen()->bits()->copy( sx, sy - 256, *charData, 0, 8 * code, 8, 8, 0, pf_blitter.color(colorByte) );
                    }
                }
            }
        }
    }

    return screen();
}