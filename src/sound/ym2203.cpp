
/*
    YM2203 (OPN) sound chip emulator
    Original compact implementation for Tickle drivers.
*/
#include <math.h>
#include <string.h>
#include "ym2203.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const unsigned char kOperatorOrder[4] = { 0, 2, 1, 3 };

YM2203::YM2203( unsigned clock )
    : address_latch_(0), status_(0), masterClock_(clock), ssg_(clock),
      timer_a_value_(0), timer_b_value_(0), timer_a_counter_(0.0),
      timer_b_counter_(0.0), mode_(0)
{
    setClock( clock );
    reset();
}

void YM2203::setClock( unsigned clock )
{
    if( clock < MinClock ) clock = MinClock;
    if( clock > MaxClock ) clock = MaxClock;
    masterClock_ = clock;
    ssg_.setClock( clock );
}

void YM2203::reset()
{
    memset( reg_, 0, sizeof(reg_) );
    status_ = 0;
    address_latch_ = 0;
    timer_a_value_ = 0;
    timer_b_value_ = 0;
    timer_a_counter_ = 0.0;
    timer_b_counter_ = 0.0;
    mode_ = 0;
    ssg_.reset();

    for( unsigned ch = 0; ch < NumFmChannels; ch++ ) {
        channel_[ch].feedback_mem1 = 0.0;
        channel_[ch].feedback_mem2 = 0.0;
        for( unsigned op = 0; op < NumOperators; op++ ) {
            Operator & o = channel_[ch].op[op];
            o.phase = 0.0;
            o.env_level = 0.0;
            o.last_output = 0.0;
            o.dt_mul = 0x01;
            o.total_level = 0x7F;
            o.attack_rate = 0x00;
            o.decay_rate = 0x00;
            o.sustain_rate = 0x00;
            o.sustain_level_release_rate = 0x0F;
            o.env_stage = EnvOff;
            o.key_on = false;
        }
    }
}

unsigned char YM2203::readData() const
{
    if( address_latch_ < 16 ) {
        return ssg_.getRegister( address_latch_ );
    }
    return reg_[address_latch_];
}

void YM2203::handleKeyOnWrite( unsigned char value )
{
    unsigned ch = value & 0x03;
    if( ch == 3 ) ch = 2;
    if( ch >= NumFmChannels ) return;

    for( unsigned slot = 0; slot < NumOperators; slot++ ) {
        bool on = (value & (0x10 << slot)) != 0;
        Operator & op = channel_[ch].op[slot];
        if( on ) {
            op.key_on = true;
            op.env_stage = EnvAttack;
            if( op.env_level < 0.0001 ) op.env_level = 0.0001;
        }
        else {
            op.key_on = false;
            if( op.env_stage != EnvOff ) {
                op.env_stage = EnvRelease;
            }
        }
    }
}

void YM2203::writeFmRegister( unsigned index, unsigned char value )
{
    reg_[index] = value;

    if( index < 16 ) {
        ssg_.writeAddress( index );
        ssg_.writeData( value );
        return;
    }

    if( index == 0x24 ) {
        timer_a_value_ = (timer_a_value_ & 0x0003) | (value << 2);
        return;
    }
    if( index == 0x25 ) {
        timer_a_value_ = (timer_a_value_ & 0x03FC) | (value & 0x03);
        return;
    }
    if( index == 0x26 ) {
        timer_b_value_ = value;
        return;
    }
    if( index == 0x27 ) {
        mode_ = value;
        if( value & 0x10 ) status_ &= ~0x01;
        if( value & 0x20 ) status_ &= ~0x02;
        if( !(value & 0x01) ) timer_a_counter_ = 0.0;
        if( !(value & 0x02) ) timer_b_counter_ = 0.0;
        return;
    }
    if( index == 0x28 ) {
        handleKeyOnWrite( value );
        return;
    }

    if( index >= 0x30 && index <= 0x9E ) {
        unsigned base = index & 0xF0;
        unsigned ch = index & 0x03;
        if( ch < NumFmChannels ) {
            unsigned group = (index >> 2) & 0x03;
            unsigned slot = kOperatorOrder[group];
            Operator & op = channel_[ch].op[slot];
            switch( base ) {
                case 0x30: op.dt_mul = value; break;
                case 0x40: op.total_level = value & 0x7F; break;
                case 0x50: op.attack_rate = value & 0x1F; break;
                case 0x60: op.decay_rate = value & 0x1F; break;
                case 0x70: op.sustain_rate = value & 0x1F; break;
                case 0x80: op.sustain_level_release_rate = value; break;
                default: break;
            }
        }
        return;
    }
}

void YM2203::writeData( unsigned char data )
{
    writeFmRegister( address_latch_, data );
}

double YM2203::rateToAttackStep( unsigned char value, unsigned samplingRate ) const
{
    unsigned rate = value & 0x1F;
    if( rate == 0 ) return 0.0;
    double x = (double)rate / 31.0;
    return (0.00008 + x * x * 0.09) * (44100.0 / (double)samplingRate);
}

double YM2203::rateToDecayStep( unsigned char value, unsigned samplingRate ) const
{
    unsigned rate = value & 0x1F;
    if( rate == 0 ) return 0.0;
    double x = (double)rate / 31.0;
    return (0.00001 + x * x * 0.01) * (44100.0 / (double)samplingRate);
}

double YM2203::sustainTarget( const Operator & op ) const
{
    unsigned sl = (op.sustain_level_release_rate >> 4) & 0x0F;
    return 1.0 - ((double)sl / 15.0);
}

double YM2203::totalLevelScale( const Operator & op ) const
{
    return pow( 2.0, -((double)(op.total_level & 0x7F) / 16.0) );
}

double YM2203::operatorMultiplier( const Operator & op ) const
{
    unsigned mul = op.dt_mul & 0x0F;
    return (mul == 0) ? 0.5 : (double)mul;
}

double YM2203::channelFrequency( unsigned channel ) const
{
    if( channel >= NumFmChannels ) return 0.0;
    unsigned fnum = ((unsigned)(reg_[0xA4 + channel] & 0x07) << 8) | reg_[0xA0 + channel];
    unsigned block = (reg_[0xA4 + channel] >> 3) & 0x07;
    if( fnum == 0 ) return 0.0;

    double fmClock = (double)masterClock_ / 144.0;
    double freq = fmClock * ((double)fnum / 1048576.0) * pow( 2.0, (double)block + 1.0 );
    if( freq < 0.0 ) freq = 0.0;
    return freq;
}

double YM2203::clockStep( unsigned samplingRate ) const
{
    return (2.0 * M_PI) / (double)samplingRate;
}

double YM2203::renderOperator( Operator & op, double baseFreq, double mod, unsigned samplingRate )
{
    double freq = baseFreq * operatorMultiplier( op );
    double phaseInc = clockStep( samplingRate ) * freq;

    switch( op.env_stage ) {
        case EnvOff:
            op.env_level = 0.0;
            break;
        case EnvAttack: {
            double step = rateToAttackStep( op.attack_rate, samplingRate );
            if( step <= 0.0 ) {
                op.env_level = 1.0;
                op.env_stage = EnvDecay;
            }
            else {
                op.env_level += (1.0 - op.env_level) * step;
                if( op.env_level >= 0.999 ) {
                    op.env_level = 1.0;
                    op.env_stage = EnvDecay;
                }
            }
            break;
        }
        case EnvDecay: {
            double target = sustainTarget( op );
            double step = rateToDecayStep( op.decay_rate, samplingRate );
            if( step <= 0.0 ) {
                op.env_level = target;
                op.env_stage = EnvSustain;
            }
            else {
                op.env_level -= step;
                if( op.env_level <= target ) {
                    op.env_level = target;
                    op.env_stage = EnvSustain;
                }
            }
            break;
        }
        case EnvSustain: {
            double step = rateToDecayStep( op.sustain_rate, samplingRate );
            if( step > 0.0 ) {
                op.env_level -= step * 0.25;
                if( op.env_level < 0.0 ) op.env_level = 0.0;
            }
            break;
        }
        case EnvRelease: {
            double rr = (double)(op.sustain_level_release_rate & 0x0F) / 15.0;
            double step = (0.00002 + rr * rr * 0.04) * (44100.0 / (double)samplingRate);
            op.env_level -= step;
            if( op.env_level <= 0.0 ) {
                op.env_level = 0.0;
                op.env_stage = EnvOff;
            }
            break;
        }
    }

    op.phase += phaseInc;
    while( op.phase >= 2.0 * M_PI ) op.phase -= 2.0 * M_PI;
    double out = sin( op.phase + mod ) * op.env_level * totalLevelScale( op );
    op.last_output = out;
    return out;
}

double YM2203::renderFmChannelSample( unsigned ch, unsigned samplingRate )
{
    double baseFreq = channelFrequency( ch );
    if( baseFreq <= 0.0 ) return 0.0;

    unsigned algo = reg_[0xB0 + ch] & 0x07;
    unsigned fb = (reg_[0xB0 + ch] >> 3) & 0x07;
    double fbScale = (fb == 0) ? 0.0 : (double)(1 << fb) / 64.0;

    Channel & c = channel_[ch];
    Operator & o1 = c.op[0];
    Operator & o2 = c.op[1];
    Operator & o3 = c.op[2];
    Operator & o4 = c.op[3];

    double feedback = (c.feedback_mem1 + c.feedback_mem2) * fbScale;
    double s1 = renderOperator( o1, baseFreq, feedback, samplingRate );
    c.feedback_mem2 = c.feedback_mem1;
    c.feedback_mem1 = s1;

    double s2 = 0.0, s3 = 0.0, s4 = 0.0;
    double out = 0.0;

    switch( algo ) {
        default:
        case 0:
            s2 = renderOperator( o2, baseFreq, s1 * 4.0, samplingRate );
            s3 = renderOperator( o3, baseFreq, s2 * 4.0, samplingRate );
            s4 = renderOperator( o4, baseFreq, s3 * 4.0, samplingRate );
            out = s4;
            break;
        case 1:
            s2 = renderOperator( o2, baseFreq, s1 * 2.0, samplingRate );
            s3 = renderOperator( o3, baseFreq, (s1 + s2) * 2.0, samplingRate );
            s4 = renderOperator( o4, baseFreq, s3 * 4.0, samplingRate );
            out = s4;
            break;
        case 2:
            s2 = renderOperator( o2, baseFreq, s1 * 4.0, samplingRate );
            s3 = renderOperator( o3, baseFreq, 0.0, samplingRate );
            s4 = renderOperator( o4, baseFreq, (s2 + s3) * 2.0, samplingRate );
            out = s4;
            break;
        case 3:
            s2 = renderOperator( o2, baseFreq, s1 * 4.0, samplingRate );
            s3 = renderOperator( o3, baseFreq, 0.0, samplingRate );
            s4 = renderOperator( o4, baseFreq, (s2 + s3) * 2.0, samplingRate );
            out = s4;
            break;
        case 4:
            s2 = renderOperator( o2, baseFreq, s1 * 4.0, samplingRate );
            s3 = renderOperator( o3, baseFreq, 0.0, samplingRate );
            s4 = renderOperator( o4, baseFreq, s3 * 4.0, samplingRate );
            out = (s2 + s4) * 0.5;
            break;
        case 5:
            s2 = renderOperator( o2, baseFreq, s1 * 2.0, samplingRate );
            s3 = renderOperator( o3, baseFreq, s1 * 2.0, samplingRate );
            s4 = renderOperator( o4, baseFreq, (s2 + s3) * 2.0, samplingRate );
            out = (s1 + s4) * 0.5;
            break;
        case 6:
            s2 = renderOperator( o2, baseFreq, s1 * 2.0, samplingRate );
            s3 = renderOperator( o3, baseFreq, 0.0, samplingRate );
            s4 = renderOperator( o4, baseFreq, 0.0, samplingRate );
            out = (s2 + s3 + s4) / 3.0;
            break;
        case 7:
            s2 = renderOperator( o2, baseFreq, 0.0, samplingRate );
            s3 = renderOperator( o3, baseFreq, 0.0, samplingRate );
            s4 = renderOperator( o4, baseFreq, 0.0, samplingRate );
            out = (s1 + s2 + s3 + s4) * 0.25;
            break;
    }

    return out;
}

void YM2203::updateTimers( unsigned samples, unsigned samplingRate )
{
    double dt = (double)samples / (double)samplingRate;

    if( mode_ & 0x01 ) {
        unsigned periodCount = timer_a_value_ ? timer_a_value_ : 1;
        double period = (1024.0 - (double)(periodCount & 0x3FF)) * 12.0 / (double)masterClock_;
        if( period < 1.0 / 1000000.0 ) period = 1.0 / 1000000.0;
        timer_a_counter_ += dt;
        while( timer_a_counter_ >= period ) {
            timer_a_counter_ -= period;
            status_ |= 0x01;
        }
    }

    if( mode_ & 0x02 ) {
        unsigned periodCount = timer_b_value_ ? timer_b_value_ : 1;
        double period = (256.0 - (double)(periodCount & 0xFF)) * 192.0 / (double)masterClock_;
        if( period < 1.0 / 1000000.0 ) period = 1.0 / 1000000.0;
        timer_b_counter_ += dt;
        while( timer_b_counter_ >= period ) {
            timer_b_counter_ -= period;
            status_ |= 0x02;
        }
    }
}

void YM2203::playSound( int * buffer, int len, unsigned samplingRate )
{
    if( len <= 0 ) return;

    ssg_.playSound( buffer, len, samplingRate );

    for( int i = 0; i < len; i++ ) {
        double fm = 0.0;
        fm += renderFmChannelSample( 0, samplingRate );
        fm += renderFmChannelSample( 1, samplingRate );
        fm += renderFmChannelSample( 2, samplingRate );

        int sample = (int)(fm * 1536.0);
        buffer[i] += sample;
    }

    updateTimers( (unsigned)len, samplingRate );
}
