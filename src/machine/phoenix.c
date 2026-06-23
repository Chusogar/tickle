#include "phoenix.h"
#include <string.h>
#include <math.h>

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
static const unsigned VisibleRamSize = 0x0340; // 32 * 26 = 832

// Emulación simple del bit VBLANK (DSW bit 7)
// 0 = in VBLANK, 1 = not in VBLANK
static unsigned phoenix_vblank_phase = 0;

// -----------------------------------------------------------------------------
// Machine info
// -----------------------------------------------------------------------------
static TMachineInfo PhoenixInfo = {
    "phoenix", "Phoenix", "Amstar", 1980,
    ScreenWidth, ScreenHeight, ScreenColors, VideoFrequency
};

static TGameRegistrationHandler regPhoenix( &PhoenixInfo, Phoenix::createInstance );

// -----------------------------------------------------------------------------
// Audio output hook
//
// En esta rama de Tickle, TFrame no expone setAudioMono/setAudio/setSound.
// El único camino confirmado hasta ahora es frame->getMixer() (véase frogger),
// pero sin la API concreta del mixer no podemos inyectar PCM aquí todavía.
// Dejamos el generador de sonido activo y el hook de salida como stub para
// mantener la compilación limpia.
// -----------------------------------------------------------------------------
static inline void phoenixPushMonoAudio(TFrame * frame,
                                        const int16_t * samples,
                                        unsigned count,
                                        unsigned sampleRate)
{
    (void)frame;
    (void)samples;
    (void)count;
    (void)sampleRate;
}

// -----------------------------------------------------------------------------
// Helper: bit extraction with LSB-based offsets as in MAME GfxLayout
// -----------------------------------------------------------------------------
static inline unsigned char bitAtLSB( const unsigned char * src, unsigned bitIndex )
{
    return (src[bitIndex >> 3] >> (bitIndex & 7)) & 1;
}

static inline double sqwave(double phase)
{
    return (phase < 0.5) ? 1.0 : -1.0;
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
    dsw0_       = 0x80;   // bit 7 = not in VBLANK by default
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
    phoenix_vblank_phase = 0;
}

void PhoenixMainBoard::run()
{
    // Classic attached driver uses ignore_interrupt,1.
    // The important thing for this game is to toggle the VBLANK sense bit.
    phoenix_vblank_phase ^= 1;
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

        switch( protection_question_ )
        {
            case 0x00:
            case 0x20:
                break;

            case 0x0C:
            case 0x30:
                ret |= 0x08;
                break;

            default:
                break;
        }

        return ret;
    }
    else if( addr >= 0x7800 && addr <= 0x7BFF ) {
        unsigned char vblank_bit = phoenix_vblank_phase ? 0x00 : 0x80;
        return (dsw0_ & 0x7F) | vblank_bit;
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
        sound_a_ = value;
    }
    else if( addr >= 0x6800 && addr <= 0x6BFF ) {
        sound_b_ = value;
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

    eventHandler()->add( idCoinSlot1,       ptInverted, &main_board_->in0_, 0x01 );
    eventHandler()->add( idKeyStartPlayer1, ptInverted, &main_board_->in0_, 0x02 );
    eventHandler()->add( idKeyStartPlayer2, ptInverted, &main_board_->in0_, 0x04 );
    eventHandler()->add( idKeyP1Action1,    ptInverted, &main_board_->in0_, 0x10 ); // Fire
    eventHandler()->add( idKeyP1Action2,    ptInverted, &main_board_->in0_, 0x80 ); // Shield

    // jm2Way no está en esta rama; usamos jm8Way y conectamos solo left/right
    setJoystickHandler( 0, new TJoystickToPortHandler(idJoyP1Joystick1, ptInverted, jm8Way) );
    joystickHandler(0)->setPort( jpLeft,  &main_board_->in0_, 0x40 );
    joystickHandler(0)->setPort( jpRight, &main_board_->in0_, 0x20 );

    soundReset();
    registerDriver( PhoenixInfo );
}

bool Phoenix::initialize( TMachineDriverInfo * info )
{
    resourceHandler()->add( EfMain45, "ic45", 0x0800, efROM, main_board_->main_rom_ + 0x0000 );
    resourceHandler()->add( EfMain46, "ic46", 0x0800, efROM, main_board_->main_rom_ + 0x0800 );
    resourceHandler()->add( EfMain47, "ic47", 0x0800, efROM, main_board_->main_rom_ + 0x1000 );
    resourceHandler()->add( EfMain48, "ic48", 0x0800, efROM, main_board_->main_rom_ + 0x1800 );
    resourceHandler()->add( EfMain49, "ic49", 0x0800, efROM, main_board_->main_rom_ + 0x2000 );
    resourceHandler()->add( EfMain50, "ic50", 0x0800, efROM, main_board_->main_rom_ + 0x2800 );
    resourceHandler()->add( EfMain51, "ic51", 0x0800, efROM, main_board_->main_rom_ + 0x3000 );
    resourceHandler()->add( EfMain52, "ic52", 0x0800, efROM, main_board_->main_rom_ + 0x3800 );

    // Use the corrected names from your local set
    resourceHandler()->add( EfBg23,   "ic23",       0x0800, efVideoROM,    main_board_->bgtiles_ + 0x0000 );
    resourceHandler()->add( EfBg24,   "ic24",       0x0800, efVideoROM,    main_board_->bgtiles_ + 0x0800 );
    resourceHandler()->add( EfFg39,   "ic39",       0x0800, efVideoROM,    main_board_->fgtiles_ + 0x0000 );
    resourceHandler()->add( EfFg40,   "ic40",       0x0800, efVideoROM,    main_board_->fgtiles_ + 0x0800 );
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
    soundReset();
}

// -----------------------------------------------------------------------------
// Sound
//
// Practical approximation:
// - control B selects melody chip behavior / tune family
// - control A drives reactive effects from real game writes
//
// This is intentionally not the full MAME discrete netlist.
// -----------------------------------------------------------------------------
void Phoenix::soundReset()
{
    prev_sound_a_ = 0;
    prev_sound_b_ = 0;

    melody_mode_  = 0;
    melody_note_  = 0;
    melody_ticks_ = 0;
    melody_phase_ = 0.0;

    fire_env_ = 0.0;
    hit_env_  = 0.0;
    boom_env_ = 0.0;
    shield_on_ = 0;
    shield_phase_ = 0.0;
    noise_lfsr_ = 0xACE1u;
}

void Phoenix::soundSyncRegisters()
{
    unsigned char a = main_board_->sound_a_;
    unsigned char b = main_board_->sound_b_;

    unsigned char rising_a = (unsigned char)((a ^ prev_sound_a_) & a);

    // Reactive effects from control A transitions / levels
    if( rising_a & 0x01 ) fire_env_ = 1.0;    // short 'fire' chirp
    if( rising_a & 0x02 ) hit_env_  = 1.0;    // short hit chirp
    if( rising_a & 0x10 ) boom_env_ = 1.0;    // boom / explode

    shield_on_ = (a & 0x04) ? 1 : 0;

    // Melody behavior from control B
    // Documented examples:
    // 0x8f = Fuer Elise, 0xcf = Phoenix theme
    if( b == 0x8F ) {
        melody_mode_ = 2;
        melody_note_ = 0;
        melody_ticks_ = 0;
    }
    else if( b == 0xCF ) {
        melody_mode_ = 3;
        melody_note_ = 0;
        melody_ticks_ = 0;
    }
    else if( (b & 0x0F) == 0x0F && (b & 0xF0) != 0x00 ) {
        melody_mode_ = 1; // alarm-ish / cue
    }
    else if( (b & 0x0F) != 0 ) {
        melody_mode_ = 4; // plain note family
    }
    else {
        melody_mode_ = 0;
    }

    prev_sound_a_ = a;
    prev_sound_b_ = b;
}

void Phoenix::soundGenerate( int16_t * dst, unsigned samples, unsigned sampleRate )
{
    if( !dst || samples == 0 || sampleRate == 0 )
        return;

    soundSyncRegisters();

    static const double fur_elise[] = {
        659.25, 622.25, 659.25, 622.25, 659.25, 493.88, 587.33, 523.25,
        440.00, 0.00,
        261.63, 329.63, 440.00, 493.88,
        0.00
    };
    static const int fur_elise_len = sizeof(fur_elise) / sizeof(fur_elise[0]);

    static const double phoenix_theme[] = {
        392.00, 440.00, 493.88, 523.25,
        587.33, 523.25, 493.88, 440.00,
        392.00, 329.63, 392.00, 440.00,
        493.88, 440.00, 392.00, 329.63
    };
    static const int phoenix_theme_len = sizeof(phoenix_theme) / sizeof(phoenix_theme[0]);

    for( unsigned i=0; i<samples; i++ ) {
        double mix = 0.0;

        // ---- Melody chip approximation ----
        double melody_freq = 0.0;

        if( melody_mode_ == 1 ) {
            melody_freq = 440.0 + 220.0 * sqwave( melody_phase_ );
        }
        else if( melody_mode_ == 2 ) {
            if( melody_ticks_ <= 0 ) {
                melody_note_ = (melody_note_ + 1) % fur_elise_len;
                melody_ticks_ = (int)(sampleRate * 0.11);
            }
            melody_ticks_--;
            melody_freq = fur_elise[melody_note_];
        }
        else if( melody_mode_ == 3 ) {
            if( melody_ticks_ <= 0 ) {
                melody_note_ = (melody_note_ + 1) % phoenix_theme_len;
                melody_ticks_ = (int)(sampleRate * 0.10);
            }
            melody_ticks_--;
            melody_freq = phoenix_theme[melody_note_];
        }
        else if( melody_mode_ == 4 ) {
            int note_sel = (main_board_->sound_b_ >> 4) & 0x0F;
            melody_freq = 220.0 * pow(2.0, note_sel / 12.0);
        }

        if( melody_freq > 1.0 ) {
            melody_phase_ += melody_freq / (double)sampleRate;
            if( melody_phase_ >= 1.0 )
                melody_phase_ -= floor(melody_phase_);
            mix += sqwave(melody_phase_) * 0.20;
        }

        // ---- Fire chirp ----
        if( fire_env_ > 0.0001 ) {
            double f = 1600.0 - (1200.0 * (1.0 - fire_env_));
            static double fire_phase = 0.0;
            fire_phase += f / (double)sampleRate;
            if( fire_phase >= 1.0 )
                fire_phase -= floor(fire_phase);
            mix += sqwave(fire_phase) * (0.30 * fire_env_);
            fire_env_ *= 0.9925;
        }

        // ---- Hit chirp ----
        if( hit_env_ > 0.0001 ) {
            double f = 900.0 + 600.0 * hit_env_;
            static double hit_phase = 0.0;
            hit_phase += f / (double)sampleRate;
            if( hit_phase >= 1.0 )
                hit_phase -= floor(hit_phase);
            mix += sqwave(hit_phase) * (0.22 * hit_env_);
            hit_env_ *= 0.9940;
        }

        // ---- Boom noise ----
        if( boom_env_ > 0.0001 ) {
            noise_lfsr_ = (noise_lfsr_ >> 1) ^ (-(int)(noise_lfsr_ & 1u) & 0xB400u);
            double nz = (noise_lfsr_ & 1) ? 1.0 : -1.0;
            mix += nz * (0.28 * boom_env_);
            boom_env_ *= 0.9960;
        }

        // ---- Shield ----
        if( shield_on_ ) {
            shield_phase_ += 140.0 / (double)sampleRate;
            if( shield_phase_ >= 1.0 )
                shield_phase_ -= floor(shield_phase_);

            noise_lfsr_ = (noise_lfsr_ >> 1) ^ (-(int)(noise_lfsr_ & 1u) & 0xB400u);
            double nz = (noise_lfsr_ & 1) ? 1.0 : -1.0;
            mix += (0.10 * sqwave(shield_phase_) + 0.06 * nz);
        }

        int v = (int)(mix * 12000.0);
        if( v < -32768 ) v = -32768;
        if( v >  32767 ) v = 32767;
        dst[i] = (int16_t)v;
    }
}

void Phoenix::run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate )
{
    if( refresh_roms_ ) {
        onVideoROMsChanged();
        refresh_roms_ = false;
    }

    main_board_->run();
    frame->setVideo( renderVideo() );

    if( samplesPerFrame == 0 || samplingRate == 0 )
        return;

    if( audio_mix_.size() != samplesPerFrame )
        audio_mix_.resize( samplesPerFrame );

    memset( &audio_mix_[0], 0, samplesPerFrame * sizeof(int16_t) );
    soundGenerate( &audio_mix_[0], samplesPerFrame, samplingRate );

    // Hook de salida a mixer aún pendiente de la API concreta de tu rama
    phoenixPushMonoAudio( frame, &audio_mix_[0], samplesPerFrame, samplingRate );
}

// -----------------------------------------------------------------------------
// Palette / decode
// -----------------------------------------------------------------------------
void Phoenix::rebuildPalette()
{
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
// Video (unchanged logic from the working corrected version)
// -----------------------------------------------------------------------------
TBitmapIndexed * Phoenix::renderVideo()
{
    TBitBlock * bits = screen()->bits();
    bits->fill( 0 );

    unsigned page = main_board_->video_reg_ & 0x01;
    unsigned palette_bank = (main_board_->video_reg_ >> 1) & 0x01;
    unsigned char * current_ram_page = main_board_->paged_ram_[page];

    // Background
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

                int dx = (RawHeight - 1) - raw_y;
                int dy = raw_x;

                if( dx >= 0 && dx < ScreenWidth && dy >= 0 && dy < ScreenHeight ) {
                    bits->scanline_data(dy)[dx] = finalColor;
                }
            }
        }
    }

    // Foreground
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