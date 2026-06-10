/*
    ZX Spectrum 48K emulator driver for Tickle
*/

#ifndef SPECTRUM_H_
#define SPECTRUM_H_

#include <emu/emu_standard_machine.h>
#include <cpu/z80.h>

// -----------------------------------------------------------------------------
// Custom input ids used by main.c / TEmuInputManager to forward full Spectrum
// keyboard matrix information directly to the machine.
// -----------------------------------------------------------------------------
enum {
    idSpectrumKey         = dtKey | 0x80,
    idSpectrumKeyCombo    = dtKey | 0x81,
    idSpectrumTapeControl = dtSwitch | 0x82
};

// -----------------------------------------------------------------------------
// Custom resource ids used to pass external buffers to the driver.
// These are meant for command-line loaded files such as --sna= and --tap=.
// -----------------------------------------------------------------------------
enum {
    EfSpectrumSnapshot = 0x7F00,
    EfSpectrumTape     = 0x7F01
};

// -----------------------------------------------------------------------------
// Encoding helpers used by main.c.
// The low bit is reserved for press/release state because TEmuInputManager
// forwards data|param where param is 0 or 1.
// -----------------------------------------------------------------------------
#define ZX48K_KEY_DATA(row,bit) \
    ((((unsigned)(row) & 0x0F) << 8) | (((unsigned)(bit) & 0x0F) << 4))

#define ZX48K_KEYCOMBO_DATA(row1,bit1,row2,bit2) \
    ((((unsigned)(row1) & 0x0F) << 24) | \
     (((unsigned)(bit1) & 0x0F) << 20) | \
     (((unsigned)(row2) & 0x0F) << 16) | \
     (((unsigned)(bit2) & 0x0F) << 12))

// -----------------------------------------------------------------------------
// Small wrapper exposing interrupt mode setter from the Z80 core.
// -----------------------------------------------------------------------------
class ZX48kCpu : public Z80
{
public:
    ZX48kCpu( Z80Environment & env ) : Z80( env ) {
    }

    void setInterruptModePublic( unsigned mode ) {
        setInterruptMode( mode );
    }
};

// -----------------------------------------------------------------------------
// Main machine board: memory map, ULA I/O, keyboard matrix, tape and beeper.
// -----------------------------------------------------------------------------
struct ZX48kBoard : public Z80Environment
{
    ZX48kBoard();
    ~ZX48kBoard();

    virtual void reset();
    virtual void run();

    // Z80Environment
    virtual unsigned char readByte( unsigned addr );
    virtual void writeByte( unsigned addr, unsigned char value );
    virtual unsigned char readPort( unsigned port );
    virtual void writePort( unsigned port, unsigned char value );

    // Keyboard matrix helper (8 rows x 5 bits, active low)
    void clearKeyboard();
    void setKeyState( int row, int bit, bool pressed );

    // Audio helpers
    void beginAudioFrame();
    void logBeeperEdge( unsigned cycle, unsigned char level );

    // Snapshot / tape loaders
    bool loadSna48k( const unsigned char * buf, unsigned len );
    bool loadTap( const unsigned char * buf, unsigned len );

    // TAP playback through EAR (ROM loader oriented)
    void stopTapPlayback();
    bool startNextTapBlock();
    bool rewindAndPlayTap();
    void advanceTap( unsigned cycles );

    enum TTapeState {
        TapeIdle,
        TapePilot,
        TapeSync1,
        TapeSync2,
        TapeData,
        TapePause
    };

    // Memory
    unsigned char rom_[0x4000];
    unsigned char ram_[0xC000];

    // ULA state
    unsigned char key_row_[8];
    unsigned char border_color_;
    unsigned char port_fe_latch_;
    unsigned char beeper_level_;
    unsigned char mic_level_;
    unsigned char ear_level_;

    unsigned frame_counter_;

    // Beeper event log for frame audio synthesis
    enum {
        MaxBeeperEvents = 4096
    };

    unsigned char frame_start_beeper_level_;
    unsigned      beeper_event_count_;
    unsigned      beeper_event_cycle_[MaxBeeperEvents];
    unsigned char beeper_event_level_[MaxBeeperEvents];

    // Tape image
    unsigned char * tap_data_;
    unsigned tap_size_;
    unsigned tap_pos_;

    // Current TAP block playback state
    const unsigned char * tape_block_ptr_;
    unsigned tape_block_len_;
    unsigned tape_byte_index_;
    unsigned char tape_bit_mask_;
    unsigned char tape_half_pulse_;
    unsigned tape_pilot_pulses_left_;
    unsigned tape_cycles_to_edge_;
    bool tape_playing_;
    TTapeState tape_state_;

    ZX48kCpu * cpu_;
};

// -----------------------------------------------------------------------------
// Tickle machine wrapper
// -----------------------------------------------------------------------------
class ZX48k : public TStandardMachine
{
public:
    virtual ~ZX48k();

    virtual bool initialize( TMachineDriverInfo * info );
    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );
    virtual bool handleInputEvent( unsigned device, unsigned param, void * data = 0 );
    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );
    virtual void reset();

    static TMachine * createInstance() {
        return new ZX48k( new ZX48kBoard );
    }

protected:
    ZX48k( ZX48kBoard * board );
    TBitmapIndexed * renderVideo();
    void renderAudio( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );

    static unsigned short pixelAddress( int x, int y );
    static unsigned short attrAddress( int x, int y );

private:
    ZX48k( const ZX48k & );
    ZX48k & operator = ( const ZX48k & );

private:
    ZX48kBoard * main_board_;
};

#endif // SPECTRUM_H_