#ifndef MINIVADR_H_
#define MINIVADR_H_

/*
    Mini Vaders (c) 1990 Taito Corporation

    Tickle driver header, based on MAME's taito/minivadr.cpp
    (driver by Takahiro Nogi).

    Mini Vaders is a tiny self-contained test/diagnostic board that Taito
    bundled with cabinets in Japan (legally required at the time). It plays
    a miniature Space Invaders-style game on a single PCB:

      - Z80 CPU @ 4 MHz (24 MHz crystal / 6)
      - 8 KB program ROM (only the low 8 KB of the 64 KB address space
        is actually used by the CPU; the ROM itself is 0x2000 bytes)
      - 8 KB of video RAM, used directly as a 1bpp (monochrome) bitmap
        framebuffer: 256x256 pixels, 32 bytes per scanline
      - A single input port (joystick left/right, 1 button, 1 coin)
      - No sound hardware at all
*/

#include <emu/emu_standard_machine.h>
#include <cpu/z80.h>

struct MiniVadrMainBoard : public Z80Environment
{
    MiniVadrMainBoard();
    ~MiniVadrMainBoard() {
        delete cpu_;
    }

    void reset();
    void run();

    // Z80Environment
    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );
    unsigned char readPort( unsigned port );
    void writePort( unsigned port, unsigned char value );

    // ROM
    unsigned char main_rom_[0x2000];     // 0000-1fff (8 KB program ROM)

    // Video RAM: used directly as a 1bpp framebuffer, 256x256 pixels,
    // 32 bytes/row (256/8), 256 rows -> 0x2000 bytes total.
    // Mapped (mirrored) across the whole 0xA000-0xBFFF range (8 KB window
    // onto a notionally larger RAM array; only the low 0x2000 bytes are
    // ever addressed since 256*32 = 0x2000 exactly matches the window).
    unsigned char videoram_[0x2000];     // a000-bfff

    // Input port (e008, read-only; writes are ignored on real hardware)
    unsigned char inputs_;

    Z80 * cpu_;
};

class MiniVadr : public TStandardMachine
{
public:
    virtual ~MiniVadr() {
        delete main_board_;
    }

    virtual bool initialize( TMachineDriverInfo * info );
    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );
    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );
    virtual void reset();

    static TMachine * createInstance() {
        return new MiniVadr( new MiniVadrMainBoard );
    }

protected:
    MiniVadr( MiniVadrMainBoard * board );

    TBitmapIndexed * renderVideo();

protected:
    MiniVadrMainBoard * main_board_;
};

#endif // MINIVADR_H_
