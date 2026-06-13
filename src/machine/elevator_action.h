/*
    Elevator Action arcade machine emulator

    Taito SJ system hardware (elevatob romset, bootleg without MCU)
    Based on MAME 0.37b7

    Copyright (c) 2024
*/
#ifndef ELEVATOR_ACTION_H_
#define ELEVATOR_ACTION_H_

#include <emu/emu_standard_machine.h>
#include <cpu/z80.h>
#include <sound/ay-3-8910.h>

struct ElevatorActionSoundBoard : public Z80Environment
{
    ElevatorActionSoundBoard();

    ~ElevatorActionSoundBoard();

    void run( unsigned cycles );
    void reset();

    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );

    void triggerNmi();

    void playSound( TMixer * mixer, unsigned len, unsigned samplingRate );

    unsigned char rom_[0x4000];
    unsigned char ram_[0x400];
    unsigned char sound_command_;
    unsigned char sndnmi_disable_;
    AY_3_8910 sound_chip_[4];
    Z80 * cpu_;
};

struct ElevatorActionMainBoard : public Z80Environment
{
    ElevatorActionMainBoard( ElevatorActionSoundBoard * sound_board );

    ~ElevatorActionMainBoard() {
        delete cpu_;
    }

    void reset();
    void run();

    unsigned char readByte( unsigned addr );
    void writeByte( unsigned addr, unsigned char value );
    unsigned char readPort( unsigned port );
    void writePort( unsigned port, unsigned char value );

    unsigned char   rom_[0x12000];
    unsigned char   ram_[0x800];
    unsigned char   characterram_[0x3000];
    unsigned char   ram_c000_[0x400];
    unsigned char   videoram_[3][0x400];
    unsigned char   colscrolly_[0x60];
    unsigned char   spriteram_[0x100];
    unsigned char   paletteram_[0x80];
    unsigned char   video_priority_;
    unsigned char   collision_reg_[4];
    unsigned char   scroll_[6];
    unsigned char   colorbank_[2];
    unsigned char   gfxpointer_[2];
    unsigned char   video_enable_;
    unsigned char   port0_;
    unsigned char   port1_;
    unsigned char   port2_;
    unsigned char   port3_;
    unsigned char   port4_;
    unsigned char   dsw1_;
    unsigned char   dsw2_;
    unsigned char   dsw3_;
    unsigned char * curr_bank_;
    unsigned char * gfx_rom_;
    Z80 * cpu_;
    ElevatorActionSoundBoard * sound_board_;
};

class ElevatorAction : public TStandardMachine
{
public:
    virtual ~ElevatorAction() {
    }

    virtual bool initialize( TMachineDriverInfo * info );

    virtual bool setResourceFile( int id, const unsigned char * buf, unsigned len );

    virtual void run( TFrame * frame, unsigned samplesPerFrame, unsigned samplingRate );

    virtual void reset();

    static TMachine * createInstance() {
        return new ElevatorAction;
    }

protected:
    ElevatorAction();

    void onVideoROMsChanged();
    TBitmapIndexed * renderVideo();

private:
    bool refresh_roms_;
    ElevatorActionSoundBoard sound_board_;
    ElevatorActionMainBoard main_board_;
    unsigned char   gfx_rom_[0x8000];
    unsigned char   layer_prom_[0x100];
    TBitBlock       char_data_1_;
    TBitBlock       sprite_data_1_;
    TBitBlock       char_data_2_;
    TBitBlock       sprite_data_2_;
    int             draworder_[32][4];
};

#endif // ELEVATOR_ACTION_H_
