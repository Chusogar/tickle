
/*
    Ghosts'n Goblins arcade machine emulator
    Capcom hardware (gng romset)
    Based on MAME 0.37b7
    Main CPU: M6809 @ 1.5MHz
    Sound CPU: Z80 @ 3MHz
    Sound: 2x YM2203
*/
#ifndef GNG_H_
#define GNG_H_
#include <emu/emu_standard_machine.h>
#include <cpu/z80.h>
#include <cpu/m6809.h>
#include <sound/ym2203.h>

struct GnGSoundBoard : public Z80Environment
{
    GnGSoundBoard();
    ~GnGSoundBoard();
    void run( unsigned cycles );
    void reset();
    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );
    void playSound( TMixer * mixer, unsigned len, unsigned samplingRate );

    unsigned char rom_[0x8000];
    unsigned char ram_[0x800];
    unsigned char sound_latch_;
    YM2203 ym_[2];
    Z80 * cpu_;
};

struct GnGMainBoard : public M6809Environment
{
    GnGMainBoard( GnGSoundBoard * sound_board );
    ~GnGMainBoard();
    void reset();
    void run();
    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );

    unsigned char   rom_[0x18000];
    unsigned char   ram_[0x1E00];
    unsigned char   spriteram_[0x200];
    unsigned char   buffered_spriteram_[0x200];
    unsigned char   fgvideoram_[0x800];
    unsigned char   bgvideoram_[0x800];
    unsigned char   paletteram_1_[0x100];
    unsigned char   paletteram_2_[0x100];
    unsigned char   bgscrollx_[2];
    unsigned char   bgscrolly_[2];
    unsigned char   flipscreen_;
    unsigned char   bankselect_;
    unsigned char   port_in0_;
    unsigned char   port_in1_;
    unsigned char   port_in2_;
    unsigned char   dsw0_;
    unsigned char   dsw1_;
    M6809 * cpu_;
    GnGSoundBoard * sound_board_;
};

class GnG : public TStandardMachine
{
public:
    virtual ~GnG() {}
    virtual bool initialize( TMachineDriverInfo * info );
    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );
    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );
    virtual void reset();
    static TMachine * createInstance() {
        return new GnG;
    }
protected:
    GnG();
    void decodeGraphics();
    TBitmapIndexed * renderVideo();
private:
    bool refresh_roms_;
    GnGSoundBoard sound_board_;
    GnGMainBoard main_board_;
    unsigned char   char_rom_[0x4000];
    unsigned char   tile_rom_[0x18000];
    unsigned char   sprite_rom_[0x18000];
    TBitBlock       char_data_;
    TBitBlock       tile_data_;
    TBitBlock       sprite_data_;
};

#endif // GNG_H_
