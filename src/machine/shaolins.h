#ifndef SHAOLINS_H_
#define SHAOLINS_H_

#include <emu/emu_standard_machine.h>
#include <cpu/m6809.h>

struct ShaolinsMainBoard : public M6809Environment
{
    explicit ShaolinsMainBoard( class Shaolins * drv );
    ~ShaolinsMainBoard() {
        delete cpu_;
    }

    void reset();
    void run( unsigned cycles );

    // M6809Environment
    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );

    class M6809 * cpu_;
    class Shaolins * drv_;
};

class Shaolins : public TStandardMachine
{
public:
    Shaolins();
    virtual ~Shaolins();

    virtual bool initialize( TMachineDriverInfo * info );
    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );
    virtual bool handleInputEvent( unsigned device, unsigned param, void * data = 0 );
    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );
    virtual void reset();

    static TMachine * createInstance() {
        return new Shaolins();
    }

    // access from board
    unsigned char mainRead( unsigned addr );
    void mainWrite( unsigned addr, unsigned char value );

protected:
    void rebuildPalette();
    void onVideoROMsChanged();

    void decodeChars();
    void decodeSprites();

    TBitmapIndexed * renderVideo();
    void drawBackground();
    void drawSprites();

private:
    Shaolins( const Shaolins & );
    Shaolins & operator=( const Shaolins & );

public:
    ShaolinsMainBoard * board_;

    // ROMs
    unsigned char main_rom_[0x10000];
    unsigned char gfx1_[0x4000];  // tiles / chars
    unsigned char gfx2_[0x8000];  // sprites
    unsigned char proms_[0x0500]; // 3 palette + 2 lookup

    // RAM / video
    unsigned char ram2_[0x0400];      // 0x2800-0x2bff
    unsigned char colorram_[0x0400];  // 0x3000-0x33ff
    unsigned char workram_[0x0400];   // 0x3400-0x37ff
    unsigned char videoram_[0x0400];  // 0x3800-0x3bff
    unsigned char videoram2_[0x0400]; // 0x3c00-0x3fff

    // control
    unsigned char nmi_enable_;
    unsigned char palette_bank_;
    unsigned char scroll_;
    unsigned char sound0_;
    unsigned char sound1_;

    // inputs
    unsigned char in_system_;
    unsigned char in_p1_;
    unsigned char in_p2_;
    unsigned char dsw1_;
    unsigned char dsw2_;
    unsigned char test_;

    // decoded gfx
    TBitBlock char_data_;    // 512 chars 8x8
    TBitBlock sprite_data_;  // 256 sprites 16x16

    bool refresh_roms_;
};

#endif // SHAOLINS_H_