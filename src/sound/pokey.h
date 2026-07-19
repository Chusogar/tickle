/*
    Tickle sound component: Atari POKEY

    Thin C++ wrapper around the MAME 0.37b7 POKEY core (The MAME Team, based on
    Ron Fries' emulator), adapted to the tickle sound conventions. Used by the
    Gauntlet driver.
*/
#ifndef TICKLE_SOUND_POKEY_H_
#define TICKLE_SOUND_POKEY_H_

class TPokey
{
public:
    TPokey( unsigned baseClock, unsigned samplingRate );
    ~TPokey();

    void reset();

    void writeReg( unsigned offset, unsigned char value );
    unsigned char readReg( unsigned offset );

    // Generate 'len' mono 16-bit samples (native chip level, 0..32767) into 'dest'
    void update( short * dest, int len );

private:
    unsigned samplerate_;
};

#endif // TICKLE_SOUND_POKEY_H_
