#ifndef MRDO_H_
#define MRDO_H_

#include <emu/emu_standard_machine.h>
#include <cpu/z80.h>
#include "sound/sn76489.h"

struct MrDoMainBoard : public Z80Environment
{
    MrDoMainBoard();
    ~MrDoMainBoard() {
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
    unsigned char main_rom_[0x8000];
    unsigned char gfx1_[0x2000];
    unsigned char gfx2_[0x2000];
    unsigned char gfx3_[0x2000];
    unsigned char proms_[0x0080];

    // Video/color RAM
    unsigned char bg_colorram_[0x400];   // 8000-83ff
    unsigned char bg_videoram_[0x400];   // 8400-87ff
    unsigned char fg_colorram_[0x400];   // 8800-8bff
    unsigned char fg_videoram_[0x400];   // 8c00-8fff

    unsigned char spriteram_[0x100];     // 9000-90ff
    unsigned char workram_[0x1000];      // e000-efff

    // Inputs / dips
    unsigned char port0_;                // a000
    unsigned char port1_;                // a001
    unsigned char dsw1_;                 // a002
    unsigned char dsw2_;                 // a003

    // Control latches
    unsigned char flipscreen_;
    unsigned char priority_;
    unsigned char scrollx_;
    unsigned char scrolly_;

    // Sound writes (SN76496/SN76489 ports)
    unsigned char sound1_;
    unsigned char sound2_;

    // Mr. Do! has two SN76489 sound chips, each fed from one of the
    // write-only ports above (0x9801 and 0x9802). Clock is 2 MHz (the
    // Z80's 4 MHz clock divided by 2 on this board).
    SN76489 sound_chip1_;
    SN76489 sound_chip2_;

    Z80 * cpu_;
};

class MrDo : public TStandardMachine
{
public:
    virtual ~MrDo() {
        delete main_board_;
    }

    virtual bool initialize( TMachineDriverInfo * info );
    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );
    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );
    virtual void reset();

    static TMachine * createInstance() {
        return new MrDo( new MrDoMainBoard );
    }

protected:
    MrDo( MrDoMainBoard * board );

    void rebuildPalette();
    void onVideoROMsChanged();
    TBitmapIndexed * renderVideo();

    // Decode helpers using exact MAME layout logic as base
    void decodeBgChar( int code, TBitBlock * bb, int oy );
    void decodeFgChar( int code, TBitBlock * bb, int oy );
    void decodeSprite( int code, TBitBlock * bb, int oy );

protected:
    MrDoMainBoard * main_board_;

    bool refresh_roms_;

    // Decoded graphics
    TBitBlock bg_char_data_;     // 512 chars 8x8
    TBitBlock fg_char_data_;     // 512 chars 8x8
    TBitBlock sprite_data_;      // 128 sprites 16x16

    // Raw (unrotated) framebuffer: hardware native orientation is
    // 240 (H) x 192 (V). The cabinet monitor is mounted ROT270, so the
    // final image presented to the player must be 192 x 240 (portrait).
    // We render into this buffer using the original (un-rotated)
    // coordinate system, then rotate it into screen() in renderVideo().
    TBitBlock raw_screen_;
};

#endif // MRDO_H_