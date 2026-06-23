#ifndef PHOENIX_H_
#define PHOENIX_H_

#include <emu/emu_standard_machine.h>
#include <cpu/z80.h>
#include <vector>
#include <stdint.h>

struct PhoenixMainBoard : public Z80Environment
{
    PhoenixMainBoard();
    ~PhoenixMainBoard() {
        delete cpu_;
    }

    void reset();
    void run();

    // Z80Environment
    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );
    unsigned char readPort( unsigned port );
    void writePort( unsigned port, unsigned char value );

    // ROM / GFX / PROM
    unsigned char main_rom_[0x4000];
    unsigned char bgtiles_[0x1000];
    unsigned char fgtiles_[0x1000];
    unsigned char proms_[0x0200];

    // Two 0x1000 paged RAM banks, selected by bit 0 of video register
    unsigned char paged_ram_[2][0x1000];

    // Inputs / dips
    unsigned char in0_;
    unsigned char dsw0_;

    // Registers
    unsigned char video_reg_;
    unsigned char scroll_reg_;
    unsigned char sound_a_;
    unsigned char sound_b_;

    // Protection latch derived from video register high bits
    unsigned char protection_question_;

    Z80 * cpu_;
};

class Phoenix : public TStandardMachine
{
public:
    virtual ~Phoenix() {
        delete main_board_;
    }

    virtual bool initialize( TMachineDriverInfo * info );
    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );
    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );
    virtual void reset();

    static TMachine * createInstance() {
        return new Phoenix( new PhoenixMainBoard );
    }

protected:
    Phoenix( PhoenixMainBoard * board );

    void rebuildPalette();
    void onVideoROMsChanged();
    TBitmapIndexed * renderVideo();

    void decodeCharset( const unsigned char * src, TBitBlock * bb );

    // Sound
    void soundReset();
    void soundSyncRegisters();
    void soundGenerate( int16_t * dst, unsigned samples, unsigned sampleRate );

protected:
    PhoenixMainBoard * main_board_;

    bool refresh_roms_;

    // 256 chars 8x8 for each layer
    TBitBlock bg_char_data_;
    TBitBlock fg_char_data_;

    // Sound state
    std::vector<int16_t> audio_mix_;

    unsigned char prev_sound_a_;
    unsigned char prev_sound_b_;

    int melody_mode_;
    int melody_note_;
    int melody_ticks_;
    double melody_phase_;

    double fire_env_;
    double hit_env_;
    double boom_env_;
    int shield_on_;
    double shield_phase_;
    unsigned noise_lfsr_;
};

#endif // PHOENIX_H_