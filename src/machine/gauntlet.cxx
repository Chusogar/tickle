/*
    Gauntlet arcade machine emulator

    Atari Gauntlet hardware (1985)
    Based on MAME 0.37b7

    Main CPU: M68010 @ 7MHz (14MHz/2)
    Sound CPU: 6502 @ 1.75MHz (14MHz/8)
    Sound: YM2151 @ 3.5MHz, POKEY @ 1.75MHz, TMS5220
    Video: 336x240, 1024 colors (IIIIRRRRGGGGBBBB)
*/
#include "gauntlet.h"

enum {
    ScreenWidth             = 336,
    ScreenHeight            = 240,
    ScreenColors            = 256,
    VideoFrequency          = 60,
    MainCpuClock            = 7000000,      // 14MHz / 2
    SoundCpuClock           = 1750000,      // 14MHz / 8
    MainCpuCyclesPerFrame   = MainCpuClock / VideoFrequency,
    SoundCpuCyclesPerFrame  = SoundCpuClock / VideoFrequency
};

// ROM file IDs
enum {
    // Main CPU ROMs (interleaved even/odd bytes)
    Rom9A, Rom9B,       // 0x000000, 0x8000 each
    Rom10A, Rom10B,     // 0x038000, 0x4000 each (slapstic)
    Rom7A, Rom7B,       // 0x040000, 0x8000 each
    // Sound CPU ROMs
    Rom16R, Rom16S,
    // GFX1 - alphanumerics
    Rom6P,
    // GFX2 - playfield/motion objects (4 planes of 2 ROMs each)
    Rom1A, Rom1B, Rom1L, Rom1MN,
    Rom2A, Rom2B, Rom2L, Rom2MN,
    // DIP options
    OptSelfTest
};

static TMachineInfo GauntletInfo = {
    "gauntlet", "Gauntlet", "Atari Games", 1985,
    ScreenWidth, ScreenHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler reg( &GauntletInfo, Gauntlet::createInstance );

// ---- Slapstic states ----
enum {
    SLAP_DISABLED = 0,
    SLAP_ENABLED
};

// ===========================================================================
// Sound board (6502 + YM2151 + POKEY + TMS5220)
// ===========================================================================

GauntletSoundBoard::GauntletSoundBoard()
{
    cpu_ = new N6502( *this );
    sound_cmd_ = 0;
    sound_resp_ = 0;
    cmd_pending_ = false;
    resp_pending_ = false;
    sound_cpu_reset_ = true;
    coin_inputs_ = 0xFF;
    irq_state_ = false;

    memset( rom_, 0xFF, sizeof(rom_) );
    memset( ram_, 0, sizeof(ram_) );
}

GauntletSoundBoard::~GauntletSoundBoard()
{
    delete cpu_;
}

void GauntletSoundBoard::reset()
{
    cpu_->reset();
    sound_cmd_ = 0;
    sound_resp_ = 0;
    cmd_pending_ = false;
    resp_pending_ = false;
    irq_state_ = false;
}

/*
    Sound CPU memory map (6502):
    0000-0FFF   R/W   Program RAM (mirrored at 0x2000)
    1000        W     Sound response write
    1010        R     Sound command read
    1020        R/W   Coin inputs (R) / Mixer control (W)
    1030        R/W   Sound status (R) / Sound control (W)
    1800-180F   R/W   POKEY
    1810-1811   R/W   YM2151
    1820        W     TMS5220 data
    1830        R/W   IRQ acknowledge
    4000-FFFF   R     Program ROM
*/
unsigned char GauntletSoundBoard::readByte( unsigned addr )
{
    addr &= 0xFFFF;

    // RAM (0x0000-0x0FFF)
    if( addr < 0x1000 )
        return ram_[addr];

    // Sound command read (0x1010)
    if( addr >= 0x1010 && addr < 0x1020 ) {
        cmd_pending_ = false;
        return sound_cmd_;
    }

    // Coin inputs (0x1020)
    if( addr >= 0x1020 && addr < 0x1030 )
        return coin_inputs_;

    // Sound status read (0x1030)
    if( addr >= 0x1030 && addr < 0x1040 ) {
        unsigned char temp = 0x30;
        if( cmd_pending_ ) temp ^= 0x80;
        if( resp_pending_ ) temp ^= 0x40;
        temp ^= 0x20; // TMS5220 always ready
        return temp;
    }

    // POKEY (0x1800-0x180F) - stub
    if( addr >= 0x1800 && addr < 0x1810 )
        return 0;

    // YM2151 (0x1810-0x1811) - stub: not busy
    if( addr >= 0x1810 && addr < 0x1820 )
        return 0;

    // IRQ acknowledge (0x1830)
    if( addr >= 0x1830 && addr < 0x1840 ) {
        irq_state_ = false;
        return 0;
    }

    // ROM (0x4000-0xFFFF)
    if( addr >= 0x4000 )
        return rom_[addr - 0x4000];

    return 0xFF;
}

void GauntletSoundBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFF;

    // RAM (0x0000-0x0FFF)
    if( addr < 0x1000 ) {
        ram_[addr] = value;
        return;
    }

    // Sound response write (0x1000)
    if( addr >= 0x1000 && addr < 0x1010 ) {
        sound_resp_ = value;
        resp_pending_ = true;
        return;
    }

    // Mixer control (0x1020) - stub
    if( addr >= 0x1020 && addr < 0x1030 )
        return;

    // Sound control (0x1030) - stub
    if( addr >= 0x1030 && addr < 0x1040 )
        return;

    // POKEY (0x1800-0x180F) - stub
    if( addr >= 0x1800 && addr < 0x1810 )
        return;

    // YM2151 (0x1810-0x1811) - stub
    if( addr >= 0x1810 && addr < 0x1820 )
        return;

    // TMS5220 data (0x1820) - stub
    if( addr >= 0x1820 && addr < 0x1830 )
        return;

    // IRQ acknowledge (0x1830)
    if( addr >= 0x1830 && addr < 0x1840 ) {
        irq_state_ = false;
        return;
    }
}

void GauntletSoundBoard::run( unsigned cycles )
{
    if( !sound_cpu_reset_ )
        cpu_->run( cycles );
}

void GauntletSoundBoard::playSound( TMixer * mixer, unsigned len, unsigned samplingRate )
{
    if( sound_cpu_reset_ ) return;

    // Run sound CPU with scanline-based IRQ timing
    // Sound IRQ is on 32V (every 32 scanlines)
    // 262 scanlines per frame, IRQ at scanlines where (scanline & 32) is true
    unsigned cyclesPerScanline = SoundCpuCyclesPerFrame / 262;
    for( int scanline = 0; scanline < 262; scanline++ ) {
        cpu_->run( cyclesPerScanline );
        if( scanline & 32 ) {
            if( !irq_state_ ) {
                irq_state_ = true;
                cpu_->interrupt( N6502::Int_IRQ );
            }
        } else {
            irq_state_ = false;
        }
    }
}

// ===========================================================================
// Main board (M68000/M68010)
// ===========================================================================

GauntletMainBoard::GauntletMainBoard( GauntletSoundBoard * sb )
    : cpu_( new M68000(*this) ), sound_board_( sb )
{
    memset( rom_, 0xFF, sizeof(rom_) );
    memset( ram_, 0, sizeof(ram_) );
    memset( eeprom_, 0xFF, sizeof(eeprom_) );
    eeprom_enabled_ = false;
    memset( playfield_, 0, sizeof(playfield_) );
    memset( motionobj_, 0, sizeof(motionobj_) );
    memset( sparevram_, 0, sizeof(sparevram_) );
    memset( alpha_, 0, sizeof(alpha_) );
    memset( palette_, 0, sizeof(palette_) );
    xscroll_ = 0;
    yscroll_ = 0;
    slapstic_bank_ = 3;
    slapstic_state_ = SLAP_DISABLED;
    memset( port_in_, 0xFF, sizeof(port_in_) );
    port_status_ = 0x48;   // VBLANK inactive (bit6=1), self-test off (bit3=1, active low)
    vblank_irq_ = false;
    sound_irq_ = false;
}

GauntletMainBoard::~GauntletMainBoard()
{
    delete cpu_;
}

void GauntletMainBoard::reset()
{
    cpu_->reset();
    slapstic_bank_ = 3;
    slapstic_state_ = SLAP_DISABLED;
    vblank_irq_ = false;
    sound_irq_ = false;
    eeprom_enabled_ = false;
}

void GauntletMainBoard::run()
{
    // VBLANK period: ~10% of frame (typical for 262 line display, ~16 VBLANK lines)
    unsigned vblank_cycles = MainCpuCyclesPerFrame / 10;

    // Set VBLANK active (bit 6 = 0)
    port_status_ &= ~0x40;

    // Fire VBLANK IRQ (IRQ4)
    vblank_irq_ = true;
    int level = 0;
    if( vblank_irq_ ) level = 4;
    if( sound_irq_ ) level = 6;
    cpu_->setIRQLine( level );

    // Run CPU during VBLANK period
    unsigned overflow = cpu_->run( vblank_cycles );

    // Set VBLANK inactive (bit 6 = 1)
    port_status_ |= 0x40;

    // Run CPU for rest of frame
    unsigned remaining = MainCpuCyclesPerFrame - vblank_cycles + overflow;
    cpu_->run( remaining );
}

/*
    Slapstic (chip 104) simplified implementation.
    The slapstic protects 4 banks at 0x038000-0x03FFFF.
    Each bank is 0x2000 bytes. Accessing specific offsets triggers bank changes.
    Offset 0x0000 enables the slapstic.
    In ENABLED state, accessing bank select offsets changes the bank.
*/
void GauntletMainBoard::slapstic_tweak( unsigned offset )
{
    // Offset is word offset within slapstic region
    if( offset == 0x0000 ) {
        slapstic_state_ = SLAP_ENABLED;
        return;
    }

    if( slapstic_state_ == SLAP_ENABLED ) {
        // Check bank select values for chip 104
        if( offset == 0x0020 ) { slapstic_bank_ = 0; slapstic_state_ = SLAP_DISABLED; }
        else if( offset == 0x0028 ) { slapstic_bank_ = 1; slapstic_state_ = SLAP_DISABLED; }
        else if( offset == 0x0030 ) { slapstic_bank_ = 2; slapstic_state_ = SLAP_DISABLED; }
        else if( offset == 0x0038 ) { slapstic_bank_ = 3; slapstic_state_ = SLAP_DISABLED; }
        else {
            slapstic_state_ = SLAP_DISABLED;
        }
    }
}

/*
    Main CPU memory map (M68010):
    000000-037FFF   R     Program ROM
    038000-03FFFF   R     Slapstic-protected ROM (4 banks of 0x2000)
    040000-07FFFF   R     Program ROM
    800000-801FFF   R/W   Program RAM (8KB)
    802000-802FFF   R/W   EEPROM (4KB)
    803000-803007   R     Input ports (4 players)
    803008          R     Status port
    80300E          R     Sound response read
    803100          W     Watchdog reset
    80312E          W     Sound CPU reset
    803140          W     VBLANK IRQ acknowledge
    803150          W     EEPROM enable
    803170          W     Sound command write
    900000-901FFF   R/W   Playfield RAM
    902000-903FFF   R/W   Motion object RAM
    904000-904FFF   R/W   Spare video RAM
    905000-905FFF   R/W   Alphanumerics RAM + SLIP
    910000-9107FF   R/W   Palette RAM
    930000          W     Playfield X scroll
*/
unsigned char GauntletMainBoard::readByte( unsigned addr )
{
    addr &= 0xFFFFFF;

    // Program ROM (000000-037FFF)
    if( addr < 0x038000 )
        return rom_[addr];

    // Slapstic ROM (038000-03FFFF)
    if( addr >= 0x038000 && addr < 0x040000 ) {
        unsigned offset = (addr - 0x038000) >> 1;
        slapstic_tweak( offset );
        unsigned local = addr - 0x038000;
        if( local >= 0x2000 )
            return rom_[0x038000 + local]; // Non-banked part
        return rom_[0x038000 + slapstic_bank_ * 0x2000 + local];
    }

    // Program ROM (040000-07FFFF)
    if( addr >= 0x040000 && addr < 0x080000 )
        return rom_[addr];

    // Program RAM (800000-801FFF, mirrored)
    if( (addr & 0xF00000) == 0x800000 ) {
        unsigned local = addr & 0x3FFF;
        if( local < 0x2000 )
            return ram_[local];

        // EEPROM (802000-802FFF)
        if( local >= 0x2000 && local < 0x3000 )
            return eeprom_[local - 0x2000];

        // Input ports
        if( local >= 0x3000 && local < 0x3008 ) {
            int port = (local - 0x3000) >> 1;
            return (local & 1) ? port_in_[port] : 0xFF;
        }

        // Status port (803008-803009)
        if( local >= 0x3008 && local <= 0x3009 ) {
            if( local & 1 ) {
                unsigned char status = port_status_;
                if( sound_board_->cmd_pending_ ) status ^= 0x20;
                if( sound_board_->resp_pending_ ) status ^= 0x10;
                return status;
            }
            return 0xFF;
        }

        // Sound response (80300E)
        if( local == 0x300E || local == 0x300F ) {
            if( local & 1 ) {
                sound_board_->resp_pending_ = false;
                sound_irq_ = false;
                int level = 0;
                if( vblank_irq_ ) level = 4;
                cpu_->setIRQLine( level );
                return sound_board_->sound_resp_;
            }
            return 0xFF;
        }
    }

    // Video RAM (900000-93FFFF)
    if( (addr & 0xF00000) == 0x900000 ) {
        unsigned local = addr & 0x3FFFF;

        // Playfield (900000-901FFF)
        if( local < 0x2000 )
            return playfield_[local];

        // Motion objects (902000-903FFF)
        if( local >= 0x2000 && local < 0x4000 )
            return motionobj_[local - 0x2000];

        // Spare VRAM (904000-904FFF)
        if( local >= 0x4000 && local < 0x5000 )
            return sparevram_[local - 0x4000];

        // Alpha (905000-905FFF) - includes SLIP at 905F80-905FFF
        if( local >= 0x5000 && local < 0x6000 )
            return alpha_[local - 0x5000];

        // Palette (910000-9107FF)
        if( local >= 0x10000 && local < 0x10800 )
            return palette_[local - 0x10000];
    }

    return 0xFF;
}

void GauntletMainBoard::writeByte( unsigned addr, unsigned char value )
{
    addr &= 0xFFFFFF;

    // Program RAM
    if( (addr & 0xF00000) == 0x800000 ) {
        unsigned local = addr & 0x3FFF;
        if( local < 0x2000 ) {
            ram_[local] = value;
            return;
        }

        // EEPROM
        if( local >= 0x2000 && local < 0x3000 ) {
            if( eeprom_enabled_ ) {
                eeprom_[local - 0x2000] = value;
            }
            return;
        }

        // Control writes (803100-803170)
        if( local >= 0x3100 && local < 0x3200 ) {
            unsigned reg = local & 0xFF;
            if( reg >= 0x00 && reg <= 0x0F ) {
                // Watchdog
                return;
            }
            if( reg >= 0x20 && reg <= 0x2F ) {
                // Sound CPU reset
                if( local & 1 ) {
                    bool old_reset = sound_board_->sound_cpu_reset_;
                    sound_board_->sound_cpu_reset_ = !(value & 1);
                    if( old_reset && !sound_board_->sound_cpu_reset_ ) {
                        sound_board_->reset();
                    }
                }
                return;
            }
            if( reg >= 0x40 && reg <= 0x4F ) {
                // VBLANK IRQ acknowledge
                vblank_irq_ = false;
                int level = 0;
                if( sound_irq_ ) level = 6;
                cpu_->setIRQLine( level );
                return;
            }
            if( reg >= 0x50 && reg <= 0x5F ) {
                // EEPROM enable
                eeprom_enabled_ = true;
                return;
            }
            if( reg >= 0x70 && reg <= 0x7F ) {
                // Sound command write
                if( local & 1 ) {
                    sound_board_->sound_cmd_ = value;
                    sound_board_->cmd_pending_ = true;
                    // Trigger NMI on sound CPU
                    if( !sound_board_->sound_cpu_reset_ )
                        sound_board_->cpu_->interrupt( N6502::Int_NMI );
                    // Set sound IRQ on main CPU
                    sound_irq_ = true;
                    int level = 6;
                    cpu_->setIRQLine( level );
                }
                return;
            }
        }
    }

    // Slapstic area (writes also trigger bank switch)
    if( addr >= 0x038000 && addr < 0x040000 ) {
        unsigned offset = (addr - 0x038000) >> 1;
        slapstic_tweak( offset );
        return;
    }

    // Video RAM
    if( (addr & 0xF00000) == 0x900000 ) {
        unsigned local = addr & 0x3FFFF;

        // Playfield (900000-901FFF)
        if( local < 0x2000 ) {
            playfield_[local] = value;
            return;
        }
        if( local >= 0x2000 && local < 0x4000 ) {
            motionobj_[local - 0x2000] = value;
            return;
        }
        if( local >= 0x4000 && local < 0x5000 ) {
            sparevram_[local - 0x4000] = value;
            return;
        }
        if( local >= 0x5000 && local < 0x6000 ) {
            alpha_[local - 0x5000] = value;
            return;
        }
        if( local >= 0x10000 && local < 0x10800 ) {
            palette_[local - 0x10000] = value;
            return;
        }

        // X scroll (930000)
        if( local >= 0x30000 && local <= 0x3FFFF ) {
            if( !(addr & 1) )
                xscroll_ = (xscroll_ & 0x00FF) | (value << 8);
            else
                xscroll_ = (xscroll_ & 0xFF00) | value;
            return;
        }
    }
}

// Word-level access for 68000 efficiency
unsigned GauntletMainBoard::readWord( unsigned addr )
{
    addr &= 0xFFFFFF;
    return ((unsigned)readByte(addr) << 8) | readByte(addr + 1);
}

void GauntletMainBoard::writeWord( unsigned addr, unsigned value )
{
    addr &= 0xFFFFFF;

    // Y scroll / tile bank at 905F6E
    if( (addr & 0xF00000) == 0x900000 ) {
        unsigned local = addr & 0x3FFFF;
        if( (local & 0xFFFF) == 0x5F6E ) {
            yscroll_ = value;
            alpha_[0xF6E] = (value >> 8) & 0xFF;
            alpha_[0xF6F] = value & 0xFF;
            return;
        }

        // X scroll (930000)
        if( local >= 0x30000 ) {
            xscroll_ = value;
            return;
        }
    }

    writeByte( addr, (value >> 8) & 0xFF );
    writeByte( addr + 1, value & 0xFF );
}

// ===========================================================================
// Machine class
// ===========================================================================

Gauntlet::Gauntlet() :
    main_board_( &sound_board_ ),
    alpha_data_( 8, 8 * 1024 ),
    pfmo_data_( 8, 8 * 8192 )
{
    createScreen( ScreenWidth, ScreenHeight, ScreenColors );

    refresh_roms_ = true;
    frame_counter_ = 0;

    // Player 1 joystick + buttons (port_in_[0])
    // Bits: UP=0x80, DOWN=0x40, LEFT=0x20, RIGHT=0x10, B2/START=0x01, B1=0x02
    setJoystickHandler( 0, new TJoystickToPortHandler(idJoyP1Joystick1, ptInverted, jm8Way, &main_board_.port_in_[0], 0x80402010) );
    eventHandler()->add( idKeyP1Action1,    ptInverted, &main_board_.port_in_[0], 0x02 );
    eventHandler()->add( idKeyStartPlayer1, ptInverted, &main_board_.port_in_[0], 0x01 );

    // Player 2 joystick + buttons (port_in_[1])
    setJoystickHandler( 1, new TJoystickToPortHandler(idJoyP2Joystick1, ptInverted, jm8Way, &main_board_.port_in_[1], 0x80402010) );
    eventHandler()->add( idKeyP2Action1,    ptInverted, &main_board_.port_in_[1], 0x02 );
    eventHandler()->add( idKeyStartPlayer2, ptInverted, &main_board_.port_in_[1], 0x01 );

    // Coin inputs (directly handled by sound CPU)
    eventHandler()->add( idCoinSlot1, ptInverted, &sound_board_.coin_inputs_, 0x08 );
    eventHandler()->add( idCoinSlot2, ptInverted, &sound_board_.coin_inputs_, 0x04 );

    registerDriver( GauntletInfo );
}

bool Gauntlet::initialize( TMachineDriverInfo * info )
{
    // Main CPU ROMs - loaded as interleaved even/odd bytes
    resourceHandler()->add( Rom9A,  "136037-1307.9a",  0x8000, efROM, 0 );
    resourceHandler()->add( Rom9B,  "136037-1308.9b",  0x8000, efROM, 0 );
    resourceHandler()->add( Rom10A, "136037-205.10a",  0x4000, efROM, 0 );
    resourceHandler()->add( Rom10B, "136037-206.10b",  0x4000, efROM, 0 );
    resourceHandler()->add( Rom7A,  "136037-1409.7a",  0x8000, efROM, 0 );
    resourceHandler()->add( Rom7B,  "136037-1410.7b",  0x8000, efROM, 0 );

    // Sound CPU ROMs
    resourceHandler()->add( Rom16R, "136037-120.16r",  0x4000, efROM, 0 );
    resourceHandler()->add( Rom16S, "136037-119.16s",  0x8000, efROM, 0 );

    // GFX1 - alphanumerics
    resourceHandler()->add( Rom6P,  "136037-104.6p",   0x4000, efVideoROM, alpha_rom_ );

    // GFX2 - playfield/motion objects (8 ROMs, inverted)
    resourceHandler()->add( Rom1A,  "136037-111.1a",   0x8000, efVideoROM, pfmo_rom_ + 0x00000 );
    resourceHandler()->add( Rom1B,  "136037-112.1b",   0x8000, efVideoROM, pfmo_rom_ + 0x08000 );
    resourceHandler()->add( Rom1L,  "136037-113.1l",   0x8000, efVideoROM, pfmo_rom_ + 0x10000 );
    resourceHandler()->add( Rom1MN, "136037-114.1mn",  0x8000, efVideoROM, pfmo_rom_ + 0x18000 );
    resourceHandler()->add( Rom2A,  "136037-115.2a",   0x8000, efVideoROM, pfmo_rom_ + 0x20000 );
    resourceHandler()->add( Rom2B,  "136037-116.2b",   0x8000, efVideoROM, pfmo_rom_ + 0x28000 );
    resourceHandler()->add( Rom2L,  "136037-117.2l",   0x8000, efVideoROM, pfmo_rom_ + 0x30000 );
    resourceHandler()->add( Rom2MN, "136037-118.2mn",  0x8000, efVideoROM, pfmo_rom_ + 0x38000 );

    resourceHandler()->assignToMachineDriverInfo( info );

    return true;
}

bool Gauntlet::setResourceFile( int id, const unsigned char * buf, unsigned len )
{
    // Handle interleaved 68000 ROM loading
    switch( id ) {
        case Rom9A:
            // Even bytes at 0x000000
            for( unsigned i = 0; i < len && i < 0x8000; i++ )
                main_board_.rom_[i * 2] = buf[i];
            refresh_roms_ = true;
            return true;
        case Rom9B:
            // Odd bytes at 0x000001
            for( unsigned i = 0; i < len && i < 0x8000; i++ )
                main_board_.rom_[i * 2 + 1] = buf[i];
            refresh_roms_ = true;
            return true;
        case Rom10A:
            // Even bytes at 0x038000
            for( unsigned i = 0; i < len && i < 0x4000; i++ )
                main_board_.rom_[0x038000 + i * 2] = buf[i];
            refresh_roms_ = true;
            return true;
        case Rom10B:
            // Odd bytes at 0x038001
            for( unsigned i = 0; i < len && i < 0x4000; i++ )
                main_board_.rom_[0x038000 + i * 2 + 1] = buf[i];
            refresh_roms_ = true;
            return true;
        case Rom7A:
            // Even bytes at 0x040000
            for( unsigned i = 0; i < len && i < 0x8000; i++ )
                main_board_.rom_[0x040000 + i * 2] = buf[i];
            refresh_roms_ = true;
            return true;
        case Rom7B:
            // Odd bytes at 0x040001
            for( unsigned i = 0; i < len && i < 0x8000; i++ )
                main_board_.rom_[0x040000 + i * 2 + 1] = buf[i];
            refresh_roms_ = true;
            return true;
        case Rom16R:
            // Sound ROM at 0x4000 in 6502 space = offset 0x0000 in rom_
            memcpy( sound_board_.rom_, buf, len < 0x4000 ? len : 0x4000 );
            return true;
        case Rom16S:
            // Sound ROM at 0x8000 in 6502 space = offset 0x4000 in rom_
            memcpy( sound_board_.rom_ + 0x4000, buf, len < 0x8000 ? len : 0x8000 );
            return true;
        default:
            break;
    }

    // Graphics ROMs handled by resource handler directly
    if( id >= Rom6P )
        refresh_roms_ = true;

    return 0 == resourceHandler()->handle( id, buf, len );
}

void Gauntlet::reset()
{
    main_board_.reset();
    sound_board_.reset();
}

void Gauntlet::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    if( refresh_roms_ ) {
        // Swap the two halves of each 68000 ROM pair so vectors are at address 0
        // 9a/9b: swap 0x00000-0x07FFF with 0x08000-0x0FFFF
        for( unsigned i = 0; i < 0x8000; i++ ) {
            unsigned char tmp = main_board_.rom_[i];
            main_board_.rom_[i] = main_board_.rom_[0x8000 + i];
            main_board_.rom_[0x8000 + i] = tmp;
        }
        // 7a/7b: swap 0x40000-0x47FFF with 0x48000-0x4FFFF
        for( unsigned i = 0; i < 0x8000; i++ ) {
            unsigned char tmp = main_board_.rom_[0x40000 + i];
            main_board_.rom_[0x40000 + i] = main_board_.rom_[0x48000 + i];
            main_board_.rom_[0x48000 + i] = tmp;
        }

        decodeGraphics();
        refresh_roms_ = false;

        // Initialize default EEPROM data
        // Default EEPROM: fill with 0xFF (game will initialize on first boot)
        memset( main_board_.eeprom_, 0xFF, sizeof(main_board_.eeprom_) );

        main_board_.reset();
        sound_board_.reset();
    }

    main_board_.run();
    sound_board_.playSound( frame->getMixer(), samplesPerFrame, samplingRate );
    frame->setVideo( renderVideo() );

    frame_counter_++;
}

// ===========================================================================
// Graphics decoding
// ===========================================================================

/*
    Alpha tiles: 8x8, 2bpp
    Layout: planes={0,4}, xoffs={0,1,2,3,8,9,10,11}, yoffs={0*16,1*16,...,7*16}
    Total size: 16 bytes per tile, 256 tiles in 0x2000 bytes (0x2000/16=256... wait, 0x2000=8192, 8192/16=512)
    Actually: RGN_FRAC(1,1) = 0x2000 bytes = 8192 bytes / 16 bytes per tile = 512 tiles
    But alpha_data_ is 256 tiles... let me check.
    From MAME: ROM_LOAD( "136037-104.6p", 0x000000, 0x002000, ...) = 0x2000 = 8192 bytes
    anlayout: 8x8, planes=2, 8*16 bits per tile = 16 bytes per tile
    0x2000/16 = 512 tiles.
*/
static void decodeAlphaTiles( unsigned char * dst, const unsigned char * src, int count )
{
    // MAME anlayout: 8x8, 2bpp
    // planes = { 0, 4 } (bit offsets within each row-pair)
    // xoffs = { 0, 1, 2, 3, 8, 9, 10, 11 }
    // yoffs = { 0*16, 1*16, 2*16, ..., 7*16 }
    // Element size = 8*16 = 128 bits = 16 bytes

    for( int c = 0; c < count; c++ ) {
        const unsigned char * s = src + c * 16;
        unsigned char * d = dst + c * 64;

        for( int y = 0; y < 8; y++ ) {
            unsigned char b0 = s[y * 2];     // Contains planes for x=0-3 and x=4-7
            unsigned char b1 = s[y * 2 + 1]; // Same but offset by 8 bits

            // xoffs: 0,1,2,3 in b0, 8,9,10,11 in b1
            // plane 0 at bit offset 0 (within xoffs), plane 1 at bit offset 4
            for( int x = 0; x < 4; x++ ) {
                int bit = 7 - x;     // MSB first: x=0→bit7, x=1→bit6, ...
                int p0 = (b0 >> bit) & 1;          // plane at offset 0
                int p1 = (b0 >> (bit - 4)) & 1;    // plane at offset 4
                d[y * 8 + x] = p0 | (p1 << 1);
            }
            for( int x = 0; x < 4; x++ ) {
                int bit = 7 - x;
                int p0 = (b1 >> bit) & 1;
                int p1 = (b1 >> (bit - 4)) & 1;
                d[y * 8 + 4 + x] = p0 | (p1 << 1);
            }
        }
    }
}

/*
    Playfield/MO tiles: 8x8, 4bpp
    Layout: planes = { RGN_FRAC(3,4), RGN_FRAC(2,4), RGN_FRAC(1,4), RGN_FRAC(0,4) }
    xoffs = { 0,1,2,3,4,5,6,7 }, yoffs = { 0*8,1*8,...,7*8 }
    Element size = 8*8 = 64 bits = 8 bytes
    Total ROM = 0x40000 bytes, 4 planes → RGN_FRAC = 0x10000 bytes per plane
    Tiles per plane = 0x10000/8 = 8192 tiles

    ROMREGION_INVERT: all bytes are inverted (XOR 0xFF)
*/
static void decodePFMOTiles( unsigned char * dst, const unsigned char * src, int count,
                             unsigned plane_size )
{
    for( int c = 0; c < count; c++ ) {
        const unsigned char * base = src + c * 8;
        unsigned char * d = dst + c * 64;

        for( int y = 0; y < 8; y++ ) {
            // Inverted ROM data (ROMREGION_INVERT)
            unsigned char p0 = ~base[y + plane_size * 0];   // RGN_FRAC(0,4)
            unsigned char p1 = ~base[y + plane_size * 1];   // RGN_FRAC(1,4)
            unsigned char p2 = ~base[y + plane_size * 2];   // RGN_FRAC(2,4)
            unsigned char p3 = ~base[y + plane_size * 3];   // RGN_FRAC(3,4)

            for( int x = 0; x < 8; x++ ) {
                int bit = 7 - x;
                int pixel = ((p0 >> bit) & 1)
                          | (((p1 >> bit) & 1) << 1)
                          | (((p2 >> bit) & 1) << 2)
                          | (((p3 >> bit) & 1) << 3);
                d[y * 8 + x] = pixel;
            }
        }
    }
}

void Gauntlet::decodeGraphics()
{
    // Alpha: 0x4000 bytes / 16 bytes per tile = 1024 tiles
    int alpha_count = 0x4000 / 16;  // 1024 tiles
    decodeAlphaTiles( alpha_data_.data(), alpha_rom_, alpha_count );

    // PF/MO: 0x40000 bytes total, 4 planes of 0x10000 bytes each
    // 0x10000 / 8 = 8192 tiles per plane
    unsigned plane_size = 0x10000;
    int pfmo_count = plane_size / 8;  // 8192 tiles
    if( pfmo_count > 8192 ) pfmo_count = 8192;
    decodePFMOTiles( pfmo_data_.data(), pfmo_rom_, pfmo_count, plane_size );
}

// ===========================================================================
// Video rendering
// ===========================================================================

static unsigned decodeHWColor( const unsigned char * ram, int hwIndex )
{
    unsigned char hi = ram[hwIndex * 2];     // IIIIRRRR
    unsigned char lo = ram[hwIndex * 2 + 1]; // GGGGBBBB

    int intensity = (hi >> 4) & 0x0F;
    int r = hi & 0x0F;
    int g = (lo >> 4) & 0x0F;
    int b = lo & 0x0F;

    r = (r * (intensity + 1)) >> 4;
    g = (g * (intensity + 1)) >> 4;
    b = (b * (intensity + 1)) >> 4;

    r = (r << 4) | r;
    g = (g << 4) | g;
    b = (b << 4) | b;

    return TPalette::encodeColor(r, g, b);
}

TBitmapIndexed * Gauntlet::renderVideo()
{
    // Build 256-entry palette from hardware's 1024-entry palette RAM:
    //   Indices   0-127: PF  (hw 0x200-0x27F: 8 palette groups × 16 colors)
    //   Indices 128-255: Alpha (hw 0x000-0x07F: 32 palette groups × 4 colors)
    // MO shares PF range (0-127) or renders pixel-by-pixel for extra groups.
    palette()->setColor( 0, TPalette::encodeColor(0, 0, 0) );
    for( int i = 0; i < 128; i++ )
        palette()->setColor( i, decodeHWColor(main_board_.palette_, 0x200 + i) );
    for( int i = 0; i < 128; i++ )
        palette()->setColor( 128 + i, decodeHWColor(main_board_.palette_, i) );

    // Clear screen
    screen()->bits()->fill( 0 );

    // Get scroll values
    int pfScrollX = main_board_.xscroll_ >> 7;
    int pfScrollY = (main_board_.yscroll_ >> 7) & 0x1FF;
    int tileBankSelect = main_board_.yscroll_ & 3;

    // === Playfield layer ===
    // 64x64 tiles, 8x8 each = 512x512 pixels
    // Column-major scan (tilemap_scan_cols): tile_index = col*64 + row
    // Data format: x-------: hflip, -xxx----: palette, ----xxxx xxxxxxxx: tile index

    for( int col = 0; col < 64; col++ ) {
        for( int row = 0; row < 64; row++ ) {
            int tile_index = col * 64 + row;
            int offset = tile_index * 2;

            unsigned char hi = main_board_.playfield_[offset];
            unsigned char lo = main_board_.playfield_[offset + 1];
            unsigned data = (hi << 8) | lo;

            int hflip = (data >> 15) & 1;
            int pal_sel = (data >> 12) & 7;
            int code = data & 0xFFF;

            // Apply tile bank and XOR
            code = ((tileBankSelect * 0x1000) + code) ^ 0x800;
            if( code < 0 || code >= 8192 ) continue;

            int sx = col * 8 - pfScrollX;
            int sy = row * 8 - pfScrollY;

            // Wrap within 512x512
            sx = ((sx % 512) + 512) % 512;
            sy = ((sy % 512) + 512) % 512;

            // Clip to visible area
            if( sx >= ScreenWidth ) continue;
            if( sy >= ScreenHeight ) continue;

            unsigned op = 0;
            if( hflip ) op |= opFlipX;

            // PF palette: indices 0-127, colorBase = pal_sel * 16
            unsigned char colorBase = (unsigned char)(pal_sel * 16);
            TBltAddSrcZeroTrans blitter(0);
            screen()->bits()->copy( sx, sy, pfmo_data_, 0, 8 * code, 8, 8, op, blitter.color(colorBase) );
        }
    }

    // === Motion objects (sprites) ===
    // Linked list format, 1024 entries × 4 words
    // Word 0 (offset 0x0000): xxxxxxx xxxxxxxx = tile index (15 bits)
    // Word 1 (offset 0x0800): xxxxxxxx x------- = X position (9 bits), ----xxxx = palette
    // Word 2 (offset 0x1000): xxxxxxxx x------- = Y position (9 bits), -x------ = hflip,
    //                          --xxx--- = width-1, -----xxx = height-1
    // Word 3 (offset 0x1800): ------xx xxxxxxxx = link (10 bits)
    //
    // SLIP (Scanline Link Pointer): at 0x905F80-0x905FFF
    // Each SLIP entry covers 8 scanlines
    // For simplicity, render all MOs by following the link chain from entry 0

    {
        bool visited[1024];
        memset( visited, 0, sizeof(visited) );

        int entry = 0;
        int count = 0;
        while( !visited[entry] && count < 1024 ) {
            visited[entry] = true;
            count++;

            int w0_off = entry * 2;
            int w1_off = 0x800 + entry * 2;
            int w2_off = 0x1000 + entry * 2;
            int w3_off = 0x1800 + entry * 2;

            unsigned w0 = (main_board_.motionobj_[w0_off] << 8) | main_board_.motionobj_[w0_off + 1];
            unsigned w1 = (main_board_.motionobj_[w1_off] << 8) | main_board_.motionobj_[w1_off + 1];
            unsigned w2 = (main_board_.motionobj_[w2_off] << 8) | main_board_.motionobj_[w2_off + 1];
            unsigned w3 = (main_board_.motionobj_[w3_off] << 8) | main_board_.motionobj_[w3_off + 1];

            int code = w0 & 0x7FFF;
            code ^= 0x800;
            int xpos = (w1 >> 7) & 0x1FF;
            int pal  = w1 & 0x0F;
            int ypos = (w2 >> 7) & 0x1FF;
            int hflip = (w2 >> 6) & 1;
            int width = ((w2 >> 3) & 7) + 1;
            int height = (w2 & 7) + 1;
            int link = w3 & 0x3FF;

            // Adjust positions
            if( xpos >= 0x100 ) xpos -= 0x200;
            if( ypos >= 0x100 ) ypos -= 0x200;

            // Offset Y by 1 (SLIP offset)
            ypos += 1;

            // Apply MO scroll
            xpos -= pfScrollX;
            ypos -= (pfScrollY & 0x1FF);

            xpos = ((xpos % 512) + 512) % 512;
            ypos = ((ypos % 512) + 512) % 512;

            // Render all tiles in the sprite pixel-by-pixel
            // MO palette at hw 0x100 + pal * 16
            for( int ty = 0; ty < height; ty++ ) {
                for( int tx = 0; tx < width; tx++ ) {
                    int tile = code + ty + tx * 8;
                    if( tile < 0 || tile >= 8192 ) continue;

                    int dx = xpos + (hflip ? (width - 1 - tx) : tx) * 8;
                    int dy = ypos + ty * 8;

                    if( dx >= ScreenWidth || dy >= ScreenHeight ) continue;
                    if( dx + 8 <= 0 || dy + 8 <= 0 ) continue;

                    for( int py = 0; py < 8; py++ ) {
                        int sy = dy + py;
                        if( sy < 0 || sy >= ScreenHeight ) continue;
                        for( int px = 0; px < 8; px++ ) {
                            int sx = dx + (hflip ? (7 - px) : px);
                            if( sx < 0 || sx >= ScreenWidth ) continue;
                            unsigned char pix = pfmo_data_.pixel( px, tile * 8 + py );
                            if( pix == 0 ) continue;
                            int hwColor = 0x100 + pal * 16 + pix;
                            unsigned rgb = decodeHWColor(main_board_.palette_, hwColor);
                            int nearest = palette()->getNearestColor( rgb );
                            screen()->bits()->setPixel( sx, sy, nearest );
                        }
                    }
                }
            }

            entry = link;
        }
    }

    // === Alpha layer ===
    // 64x32 tiles, 8x8 each, 2bpp
    // Row-major scan: tile_index = row*64 + col
    // Data format: x-------: opaque flag, -xxxxx--: palette, ------xx xxxxxxxx: tile index

    for( int row = 0; row < 32; row++ ) {
        for( int col = 0; col < 64; col++ ) {
            int tile_index = row * 64 + col;
            int offset = tile_index * 2;

            // Skip if offset is in SLIP area (0xF80-0xFFF)
            if( offset >= 0xF80 ) continue;

            unsigned char hi = main_board_.alpha_[offset];
            unsigned char lo = main_board_.alpha_[offset + 1];
            unsigned data = (hi << 8) | lo;

            int opaque = (data >> 15) & 1;
            int pal = ((data >> 10) & 0x0F) | ((data >> 9) & 0x20);
            int code = data & 0x3FF;

            if( code == 0 && !opaque ) continue;
            if( code >= 1024 ) continue;

            int sx = col * 8;
            int sy = row * 8;

            if( sx >= ScreenWidth || sy >= ScreenHeight ) continue;

            // Alpha palette at indices 128-255 (hw 0x000-0x07F)
            unsigned char colorBase = (unsigned char)(128 + (pal & 0x1F) * 4);

            if( opaque ) {
                TBltAddSrcZeroTrans blitter(0);
                screen()->bits()->copy( sx, sy, alpha_data_, 0, 8 * code, 8, 8, 0, blitter.color(colorBase) );
            } else {
                TBltAddSrcTrans blitter( 0, 0 );
                screen()->bits()->copy( sx, sy, alpha_data_, 0, 8 * code, 8, 8, 0, blitter.color(colorBase) );
            }
        }
    }

    return screen();
}
