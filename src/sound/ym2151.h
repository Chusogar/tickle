/*
    Tickle sound component: Yamaha YM2151 (OPM) FM synthesizer

    Thin C++ wrapper around the MAME 0.37b7 YM2151 core (Jarek Burczynski),
    adapted to the tickle sound conventions. Used by the Gauntlet driver.
*/
#ifndef TICKLE_SOUND_YM2151_H_
#define TICKLE_SOUND_YM2151_H_

class TYM2151
{
public:
    TYM2151( unsigned clock, unsigned samplingRate );
    ~TYM2151();

    void reset();

    // Register access as seen by the host CPU (address latch + data)
    void writeAddress( unsigned char reg );
    void writeData( unsigned char value );
    unsigned char readStatus();

    // Generate 'len' mono 16-bit samples (native chip level) into 'dest'
    void update( short * dest, int len );

private:
    unsigned samplingRate_;
    unsigned char latch_;
    short * bufL_;
    short * bufR_;
    int bufLen_;

    void ensureBuffers( int len );
};

#endif // TICKLE_SOUND_YM2151_H_
