/*
    Gauntlet arcade machine emulator

    Atari Gauntlet hardware (1985)
    Based on MAME 0.37b7

    Main CPU: M68010 @ 7MHz (14MHz/2)
    Sound CPU: 6502 @ 1.75MHz (14MHz/8)
    Sound: YM2151 @ 3.5MHz, POKEY @ 1.75MHz, TMS5220 @ 636kHz
    Video: 336x240, 1024 colors
*/
#ifndef GAUNTLET_H_
#define GAUNTLET_H_

#include <emu/emu_standard_machine.h>
#include <cpu/m68000.h>
#include <cpu/n6502.h>

struct GauntletSoundBoard : public N6502Environment
{
    GauntletSoundBoard();
    ~GauntletSoundBoard();

    void run( unsigned cycles );
    void reset();

    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );

    void playSound( TMixer * mixer, unsigned len, unsigned samplingRate );

    // Sound CPU ROM (48KB: 0x4000-0xFFFF)
    unsigned char rom_[0xC000];
    // Sound CPU RAM (4KB: 0x0000-0x0FFF)
    unsigned char ram_[0x1000];

    // Sound communication
    unsigned char sound_cmd_;       // Command from main CPU
    unsigned char sound_resp_;      // Response to main CPU
    bool cmd_pending_;              // Command buffer full
    bool resp_pending_;             // Response buffer full
    bool sound_cpu_reset_;          // Sound CPU held in reset

    // Coin inputs (directly readable by sound CPU)
    unsigned char coin_inputs_;

    // IRQ state
    bool irq_state_;

    N6502 * cpu_;
};

struct GauntletMainBoard : public M68000Environment
{
    GauntletMainBoard( GauntletSoundBoard * sb );
    ~GauntletMainBoard();

    void reset();
    void run();

    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );
    unsigned readWord( unsigned addr );
    void writeWord( unsigned addr, unsigned value );

    // Main CPU ROM (512KB, includes slapstic region)
    unsigned char rom_[0x80000];
    // Program RAM (8KB: 0x800000-0x801FFF)
    unsigned char ram_[0x2000];
    // EEPROM (4KB: 0x802000-0x802FFF)
    unsigned char eeprom_[0x1000];
    bool eeprom_enabled_;

    // Video RAM
    unsigned char playfield_[0x2000];   // 64x64 tiles (0x900000-0x901FFF)
    unsigned char motionobj_[0x2000];   // Motion objects (0x902000-0x903FFF)
    unsigned char sparevram_[0x1000];   // Spare VRAM (0x904000-0x904FFF)
    unsigned char alpha_[0x1000];       // Alphanumerics (0x905000-0x905FFF)
    unsigned char palette_[0x800];      // Palette RAM (0x910000-0x9107FF)

    // Scroll registers
    unsigned xscroll_;
    unsigned yscroll_;      // Also contains tile bank select in bits 0-1

    // Slapstic
    int slapstic_bank_;
    int slapstic_state_;
    void slapstic_tweak( unsigned offset );

    // Input ports
    unsigned char port_in_[4];  // 4 player inputs
    unsigned char port_status_; // Status port (VBLANK, sound, self-test)

    // Interrupt state
    bool vblank_irq_;
    bool sound_irq_;

    M68000 * cpu_;
    GauntletSoundBoard * sound_board_;
};

class Gauntlet : public TStandardMachine
{
public:
    virtual ~Gauntlet() {}

    virtual bool initialize( TMachineDriverInfo * info );
    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );
    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );
    virtual void reset();

    static TMachine * createInstance() {
        return new Gauntlet;
    }

protected:
    Gauntlet();

    void decodeGraphics();
    TBitmapIndexed * renderVideo();

private:
    bool refresh_roms_;
    GauntletSoundBoard sound_board_;
    GauntletMainBoard main_board_;

    // Raw ROM data for graphics
    unsigned char alpha_rom_[0x4000];       // GFX1: alphanumerics
    unsigned char pfmo_rom_[0x40000];       // GFX2: playfield/motion objects

    // Decoded graphics
    TBitBlock alpha_data_;    // 1024 tiles, 8x8, 2bpp
    TBitBlock pfmo_data_;     // 8192 tiles, 8x8, 4bpp

    // VBLANK state
    int frame_counter_;
};

#endif // GAUNTLET_H_
