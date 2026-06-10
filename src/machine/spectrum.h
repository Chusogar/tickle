/*
    ZX Spectrum 48K emulator driver for Tickle
*/

#ifndef SPECTRUM_H_
#define SPECTRUM_H_

#include <emu/emu_standard_machine.h>
#include <cpu/z80.h>

enum {
    idSpectrumKey      = dtKey | 0x80,
    idSpectrumKeyCombo = dtKey | 0x81
};

// Custom resource/event id used to pass an external .sna snapshot to the driver
enum {
    EfSpectrumSnapshot = 0x7F00
};

// The low bit is reserved for press/release state because TEmuInputManager
// forwards data|param where param is 0 or 1.
#define ZX48K_KEY_DATA(row,bit) \
    ((((unsigned)(row) & 0x0F) << 8) | (((unsigned)(bit) & 0x0F) << 4))

#define ZX48K_KEYCOMBO_DATA(row1,bit1,row2,bit2) \
    ((((unsigned)(row1) & 0x0F) << 24) | \
     (((unsigned)(bit1) & 0x0F) << 20) | \
     (((unsigned)(row2) & 0x0F) << 16) | \
     (((unsigned)(bit2) & 0x0F) << 12))

class ZX48kCpu : public Z80
{
public:
    ZX48kCpu( Z80Environment & env ) : Z80( env ) {
    }

    void setInterruptModePublic( unsigned mode ) {
        setInterruptMode( mode );
    }
};

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

    // Snapshot loader (.sna 48K)
    bool loadSna48k( const unsigned char * buf, unsigned len );

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

    enum {
        MaxBeeperEvents = 4096
    };

    unsigned char frame_start_beeper_level_;
    unsigned      beeper_event_count_;
    unsigned      beeper_event_cycle_[MaxBeeperEvents];
    unsigned char beeper_event_level_[MaxBeeperEvents];

    ZX48kCpu * cpu_;
};

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
