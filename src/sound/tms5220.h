/*
    Tickle sound component: Texas Instruments TMS5220 speech synthesizer

    Thin C++ wrapper around the MAME 0.37b7 TMS5220 core
    (Frank Palazzolo / Neill Corlett / Aaron Giles), adapted to the tickle
    sound conventions. Used by the Gauntlet driver for the speech ("Warrior
    needs food badly", etc.).
*/
#ifndef TICKLE_SOUND_TMS5220_H_
#define TICKLE_SOUND_TMS5220_H_

class TTMS5220
{
public:
    TTMS5220();
    ~TTMS5220();

    void reset();

    void writeData( unsigned char value );
    unsigned char readStatus();
    bool readyToReceive();

    // Generate 'len' mono 16-bit samples at 'samplingRate' into 'dest'.
    // The chip runs natively at 8 kHz and the output is resampled.
    void update( short * dest, int len, unsigned samplingRate );

private:
    short * src_;
    int srcLen_;

    void ensureSrc( int len );
};

#endif // TICKLE_SOUND_TMS5220_H_
