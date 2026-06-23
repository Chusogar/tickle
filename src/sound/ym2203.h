
/*
    YM2203 (OPN) sound chip emulator
    Original compact implementation for Tickle drivers.
    Provides:
      - 3 FM channels (4 operators each)
      - 3 SSG channels via embedded YM2149-compatible block
      - status register and basic Timer A / Timer B behaviour

    Notes:
      This implementation is intentionally lightweight and focuses on
      compatibility with Capcom arcade drivers such as Ghosts'n Goblins.
      It is not a cycle-exact emulator, but it supports the register set
      that music drivers typically use, including FM key on/off,
      frequency/block, algorithm/feedback, operator rates and levels.
*/
#ifndef YM2203_H_
#define YM2203_H_

#include "ym2149.h"

class YM2203
{
public:
    enum {
        MinClock        = 1000000,
        MaxClock        = 4000000,
        NumRegisters    = 256,
        NumFmChannels   = 3,
        NumOperators    = 4
    };

    YM2203( unsigned clock = MinClock );
    ~YM2203() {}

    void setClock( unsigned clock );
    void reset();

    void writeAddress( unsigned address ) { address_latch_ = address & 0xFF; }
    void writeData( unsigned char data );
    unsigned char readData() const;
    unsigned char readStatus() const { return status_; }

    void playSound( int * buffer, int len, unsigned samplingRate );

    unsigned char getRegister( unsigned index ) const {
        return (index < NumRegisters) ? reg_[index] : 0;
    }

private:
    enum EnvelopeStage {
        EnvOff = 0,
        EnvAttack,
        EnvDecay,
        EnvSustain,
        EnvRelease
    };

    struct Operator {
        double phase;
        double env_level;
        double last_output;
        unsigned char dt_mul;
        unsigned char total_level;
        unsigned char attack_rate;
        unsigned char decay_rate;
        unsigned char sustain_rate;
        unsigned char sustain_level_release_rate;
        EnvelopeStage env_stage;
        bool key_on;
    };

    struct Channel {
        Operator op[NumOperators];
        double feedback_mem1;
        double feedback_mem2;
    };

    double rateToAttackStep( unsigned char value, unsigned samplingRate ) const;
    double rateToDecayStep( unsigned char value, unsigned samplingRate ) const;
    double sustainTarget( const Operator & op ) const;
    double totalLevelScale( const Operator & op ) const;
    double operatorMultiplier( const Operator & op ) const;
    double channelFrequency( unsigned channel ) const;
    double clockStep( unsigned samplingRate ) const;
    void handleKeyOnWrite( unsigned char value );
    void writeFmRegister( unsigned index, unsigned char value );
    void updateTimers( unsigned samples, unsigned samplingRate );
    double renderFmChannelSample( unsigned ch, unsigned samplingRate );
    double renderOperator( Operator & op, double baseFreq, double mod, unsigned samplingRate );

private:
    unsigned char reg_[NumRegisters];
    unsigned char address_latch_;
    unsigned char status_;
    unsigned masterClock_;

    YM2149 ssg_;
    Channel channel_[NumFmChannels];

    unsigned timer_a_value_;
    unsigned timer_b_value_;
    double timer_a_counter_;
    double timer_b_counter_;
    unsigned char mode_;
};

#endif // YM2203_H_
