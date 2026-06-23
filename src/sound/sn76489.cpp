/*
    SN76489 / SN76489A / SN76496 programmable sound generator emulator

    Copyright (c) 2004-2011 Alessandro Scotti
    (written for the Tickle emulator)
*/
#include <string.h>

#include "sn76489.h"

SN76489::SN76489( unsigned clock )
{
    master_clock_ = clock;
    sampling_rate_ = 0;
    resample_step_ = 0;

    initializeVolumeTable();

    reset();
}

void SN76489::initializeVolumeTable()
{
    // The SN76489 attenuates in steps of 2 dB, with attenuation 15 = silence.
    // We build a table of peak amplitudes; attenuation 0 (loudest) maps to
    // about 4000, leaving headroom for mixing up to 4 channels (and a second
    // chip) within a 16-bit sample.
    double level = 4000.0;

    for( int i = 0; i < 15; i++ ) {
        vol_table_[i] = (int)(level + 0.5);
        level *= 0.7943282347; // 10 ^ (-2dB/20) = 2dB attenuation per step
    }
    vol_table_[15] = 0; // Attenuation 15 = silence
}

void SN76489::reset()
{
    for( int i = 0; i < NumTones; i++ ) {
        tone_period_[i]  = 0;
        tone_counter_[i] = 0;
        tone_output_[i]  = 1;
    }

    for( int i = 0; i < NumChannels; i++ ) {
        tone_attn_[i] = 0x0F; // All channels start silent
    }

    noise_control_ = 0;
    latched_register_ = 0;

    noise_counter_ = 0;
    noise_shift_register_ = 0x8000; // Must be non-zero
    noise_output_ = 1;
}

void SN76489::setSamplingRate( unsigned samplingRate )
{
    sampling_rate_ = samplingRate;

    // resample_step_ expresses how many master clock "half-cycles" of the
    // internal /16 prescaler elapse per output sample, in fixed point
    // (10 bits for the fractional part). The SN76489 tone/noise generators
    // run from a clock that is master_clock_/16.
    unsigned internal_clock = master_clock_ >> 4;

    resample_step_ = samplingRate ? ((internal_clock << 10) / samplingRate) : 0;
}

/*
    Write protocol (real SN76489):

      bit7 = 1 : LATCH/DATA byte
        bits 6-4 = register select (0..7):
          0 = tone 0 period (low 4 bits in this byte's bits 3-0)
          1 = tone 0 attenuation (bits 3-0)
          2 = tone 1 period (low 4 bits)
          3 = tone 1 attenuation (bits 3-0)
          4 = tone 2 period (low 4 bits)
          5 = tone 2 attenuation (bits 3-0)
          6 = noise control (bits 2-0)
          7 = noise attenuation (bits 3-0)

      bit7 = 0 : DATA byte
        bits 5-0 = high 6 bits of the period for the LAST LATCHED tone
                   register (only meaningful if that register was 0, 2 or 4)
*/
void SN76489::write( unsigned char data )
{
    if( data & 0x80 ) {
        // LATCH/DATA byte
        unsigned reg = (data >> 4) & 0x07;
        latched_register_ = reg;

        switch( reg ) {
            case 0: // Tone 0 period, low 4 bits
                tone_period_[0] = (tone_period_[0] & 0x3F0) | (data & 0x0F);
                break;
            case 2: // Tone 1 period, low 4 bits
                tone_period_[1] = (tone_period_[1] & 0x3F0) | (data & 0x0F);
                break;
            case 4: // Tone 2 period, low 4 bits
                tone_period_[2] = (tone_period_[2] & 0x3F0) | (data & 0x0F);
                break;
            case 1: // Tone 0 attenuation
                tone_attn_[0] = data & 0x0F;
                break;
            case 3: // Tone 1 attenuation
                tone_attn_[1] = data & 0x0F;
                break;
            case 5: // Tone 2 attenuation
                tone_attn_[2] = data & 0x0F;
                break;
            case 6: // Noise control
                noise_control_ = data & 0x07;
                // Re-seed the LFSR whenever the noise mode changes, as the
                // real chip does
                noise_shift_register_ = 0x8000;
                break;
            case 7: // Noise attenuation
                tone_attn_[3] = data & 0x0F;
                break;
        }
    }
    else {
        // DATA byte: high 6 bits of period for the last latched tone register
        unsigned hi = (data & 0x3F) << 4;

        switch( latched_register_ ) {
            case 0: tone_period_[0] = (tone_period_[0] & 0x0F) | hi; break;
            case 2: tone_period_[1] = (tone_period_[1] & 0x0F) | hi; break;
            case 4: tone_period_[2] = (tone_period_[2] & 0x0F) | hi; break;
            default: break; // DATA bytes for non-tone registers are ignored
        }
    }
}

/*
    Generates len samples and mixes them additively into buffer.

    Tone channels: each is a square wave generator. The down-counter
    decrements once per "internal clock" tick (master_clock_/16) and, on
    reaching zero, reloads from the 10-bit period (treating 0 as 1024) and
    flips the output polarity. The resulting square wave alternates between
    +vol and -vol.

    Noise channel: a 16-bit LFSR clocked at a rate selected by bits 0-1 of
    the noise control register (master/16/16, master/16/32, master/16/64, or
    the same rate as tone channel 2). Bit 2 selects "white" noise (feedback
    taps at bits 0 and 3, XORed) vs "periodic" noise (single tap at bit 0).
*/
void SN76489::playSound( int * buffer, int len )
{
    if( resample_step_ == 0 ) {
        return;
    }

    for( int i = 0; i < len; i++ ) {
        int sample = 0;

        // ---- Tone channels 0..2 -----------------------------------------
        for( int ch = 0; ch < NumTones; ch++ ) {
            unsigned period = tone_period_[ch] ? tone_period_[ch] : 1024;

            // Advance the down-counter by resample_step_ (fixed point, 10
            // fractional bits). Each full "period" worth of internal clock
            // ticks flips the output polarity.
            unsigned ticks = resample_step_;
            unsigned counter = tone_counter_[ch];

            // counter and ticks are both in 10-bit fixed point "internal
            // clock tick" units; period is in whole ticks (<<10 for the
            // same fixed point scale)
            counter += ticks;

            unsigned period_fp = period << 10;

            while( counter >= period_fp ) {
                counter -= period_fp;
                tone_output_[ch] = -tone_output_[ch];
            }

            tone_counter_[ch] = counter;

            if( tone_attn_[ch] < 15 ) {
                sample += tone_output_[ch] * vol_table_[ tone_attn_[ch] ];
            }
        }

        // ---- Noise channel -------------------------------------------------
        {
            unsigned rate_select = noise_control_ & 0x03;
            unsigned period;

            if( rate_select == 3 ) {
                // Noise clock follows tone channel 2's period
                period = tone_period_[2] ? tone_period_[2] : 1024;
            }
            else {
                // 0->/16, 1->/32, 2->/64 (relative to the internal clock,
                // which is already master/16)
                period = 16u << rate_select;
            }

            unsigned period_fp = period << 10;
            unsigned counter = noise_counter_;
            counter += resample_step_;

            while( counter >= period_fp ) {
                counter -= period_fp;

                unsigned sr = noise_shift_register_;
                unsigned feedback;

                if( noise_control_ & 0x04 ) {
                    // "White" noise: feedback = bit0 XOR bit3
                    feedback = (sr & 0x0001) ^ ((sr >> 3) & 0x0001);
                }
                else {
                    // "Periodic" noise: feedback = bit0
                    feedback = sr & 0x0001;
                }

                sr = (sr >> 1) | (feedback << 15);
                noise_shift_register_ = sr;

                noise_output_ = (sr & 0x0001) ? 1 : -1;
            }

            noise_counter_ = counter;

            if( tone_attn_[3] < 15 ) {
                sample += noise_output_ * vol_table_[ tone_attn_[3] ];
            }
        }

        buffer[i] += sample;
    }
}
