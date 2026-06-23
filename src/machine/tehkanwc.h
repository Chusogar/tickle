#ifndef TEHKANWC_H_
#define TEHKANWC_H_

#include <emu/emu_standard_machine.h>
#include <cpu/z80.h>

class TehkanWCDriver;

// -----------------------------------------------------------------------------
// CPU environments
// -----------------------------------------------------------------------------
struct TehkanWCMainBoard : public Z80Environment
{
    explicit TehkanWCMainBoard( TehkanWCDriver * drv );
    ~TehkanWCMainBoard();

    void reset();
    void run( unsigned cycles );

    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );
    unsigned char readPort( unsigned port );
    void writePort( unsigned port, unsigned char value );

    Z80 * cpu_;
    TehkanWCDriver * drv_;
};

struct TehkanWCSubBoard : public Z80Environment
{
    explicit TehkanWCSubBoard( TehkanWCDriver * drv );
    ~TehkanWCSubBoard();

    void reset();
    void run( unsigned cycles );

    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );
    unsigned char readPort( unsigned port );
    void writePort( unsigned port, unsigned char value );

    Z80 * cpu_;
    TehkanWCDriver * drv_;
};

struct TehkanWCSoundBoard : public Z80Environment
{
    explicit TehkanWCSoundBoard( TehkanWCDriver * drv );
    ~TehkanWCSoundBoard();

    void reset();
    void run( unsigned cycles );

    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );
    unsigned char readPort( unsigned port );
    void writePort( unsigned port, unsigned char value );

    Z80 * cpu_;
    TehkanWCDriver * drv_;
};

// -----------------------------------------------------------------------------
// Driver
// -----------------------------------------------------------------------------
class TehkanWCDriver : public TStandardMachine
{
public:
    TehkanWCDriver();
    virtual ~TehkanWCDriver();

    virtual bool initialize( TMachineDriverInfo * info );
    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );
    virtual bool handleInputEvent( unsigned device, unsigned param, void * data = 0 );
    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );
    virtual void reset();

    static TMachine * createInstance() {
        return new TehkanWCDriver();
    }

    // Memory helpers used by the CPU environments
    unsigned char mainRead( unsigned addr );
    void mainWrite( unsigned addr, unsigned char value );

    unsigned char subRead( unsigned addr );
    void subWrite( unsigned addr, unsigned char value );

    unsigned char sndRead( unsigned addr );
    void sndWrite( unsigned addr, unsigned char value );

protected:
    void rebuildPalette();
    void onVideoROMsChanged();
    void renderBgBitmap();
    TBitmapIndexed * renderVideo();

    void decodeChars();
    void decodeSprites();
    void decodeTiles();

    void drawFgLayer( bool high_priority );
    void drawSprites();
    void updateTrackballDeltas();

private:
    TehkanWCDriver( const TehkanWCDriver & );
    TehkanWCDriver & operator=( const TehkanWCDriver & );

public:
    // -------------------------------------------------------------------------
    // Memory / ROM / VRAM
    // -------------------------------------------------------------------------
    unsigned char main_rom_[0xC000];
    unsigned char sub_rom_[0x8000];
    unsigned char snd_rom_[0x4000];

    unsigned char main_ram_[0x0800];
    unsigned char sub_ram_[0x4800];
    unsigned char snd_ram_[0x0800];
    unsigned char shared_ram_[0x0800];

    unsigned char fg_vram_[0x0400];
    unsigned char fg_cram_[0x0400];
    unsigned char pal_ram_[0x0600];
    unsigned char bg_vram_[0x0800];
    unsigned char sprite_ram_[0x0400];
    unsigned char scroll_x_lo_;
    unsigned char scroll_x_hi_;
    unsigned char scroll_y_;

    // GFX ROMs
    unsigned char gfx1_[0x04000];
    unsigned char gfx2_[0x10000];
    unsigned char gfx3_[0x10000];
    unsigned char adpcm_[0x4000];

    // Decoded graphics
    TBitBlock char_data_;   // 512 chars, 8x8
    TBitBlock sprite_data_; // 512 sprites, 16x16
    TBitBlock tile_data_;   // 1024 bg tiles, 16x8

    // Palette-indexed background bitmap (512x256)
    TBitBlock bg_bitmap_;

    bool bg_dirty_[0x0800 / 2];

    // CPU boards
    TehkanWCMainBoard * main_board_;
    TehkanWCSubBoard  * sub_board_;
    TehkanWCSoundBoard * snd_board_;

    // Input state
    unsigned char coins_;
    unsigned char btn0_;
    unsigned char btn1_;
    unsigned char dsw_[3];
    signed char track0_[2];
    signed char track1_[2];

    bool p1_left_;
    bool p1_right_;
    bool p1_up_;
    bool p1_down_;

    bool p2_left_;
    bool p2_right_;
    bool p2_up_;
    bool p2_down_;

    bool p1_kick_;
    bool p2_kick_;

    // Sound latches / status
    unsigned char soundlatch_;
    unsigned char sound_answer_;
    bool snd_nmi_pending_;

    bool sub_halted_;
    bool sub_was_halted_;

    bool refresh_roms_;
};

#endif // TEHKANWC_H_