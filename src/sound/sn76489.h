/*
    SN76489 / SN76489A / SN76496 programmable sound generator emulator

    Used by many arcade boards of the early-mid 1980s (including Mr. Do!,
    which has two of these chips). Each chip provides three square-wave
    tone channels and one noise channel, each with independent 4-bit
    (16-step) attenuation.

    Copyright (c) 2004-2011 Alessandro Scotti
    (written for the Tickle emulator, following the style of ym2149.h /
     namcowsg3.h)
*/
#ifndef SN76489_H_
#define SN76489_H_

class SN76489
{
public:
    enum {
        NumTones    = 3,    //@- Number of tone (square wave) channels
        NumChannels = 4      //@- Tone channels plus noise
    };

    /**
        Constructor.

        @param clock master clock in Hertz (typically 2 MHz on Mr. Do!)
    */
    explicit SN76489( unsigned clock = 2000000 );

    /** Destructor. */
    ~SN76489() {
    }

    /** Resets the sound chip to its power-on state. */
    void reset();

    /**
        Sets the chip master clock.

        @param clock master clock in Hertz
    */
    void setClock( unsigned clock ) {
        master_clock_ = clock;
    }

    /**
        Writes a byte to the chip's single write port.

        The SN76489 decodes the register and (for tone registers) whether
        this is the first (LATCH/DATA, bit7=1) or second (DATA, bit7=0)
        byte of a two-byte sequence, exactly as the real chip does.

        @param data byte written to the chip
    */
    void write( unsigned char data );

    /**
        Sets the output sampling rate used by playSound().

        @param samplingRate sampling rate in Hertz (samples per second)
    */
    void setSamplingRate( unsigned samplingRate );

    /** Returns the sampling rate currently in use for rendering sound. */
    unsigned getSamplingRate() const {
        return sampling_rate_;
    }

    /**
        Lets the chip play for the specified amount of time, mixing its
        output (additively) into the specified buffer.

        Output of each active channel is a signed value scaled by its
        4-bit (0..15, where 0 = loudest and 15 = silent) attenuation,
        producing a contribution in roughly the -4096..+4095 range per
        channel so four channels fit comfortably in a 16-bit mix buffer
        with headroom for further mixing by the caller.

        Note: this function does NOT clear the buffer before mixing.

        @param buffer buffer where sound output is mixed (added)
        @param len    length of buffer (in samples)
    */
    void playSound( int * buffer, int len );

protected:
    // Volume table: vol_table_[a] = peak amplitude for attenuation a (0..15)
    void initializeVolumeTable();

private:
    unsigned master_clock_;        // Master clock (Hz)
    unsigned sampling_rate_;        // Output sampling rate (Hz)

    // --- Register state -----------------------------------------------
    unsigned tone_period_[NumTones];   // 10-bit tone period for ch 0..2
    unsigned tone_attn_[NumChannels];  // 4-bit attenuation, ch 0..2 + noise
    unsigned noise_control_;            // 3-bit noise control register

    // For the two-byte tone period writes: which channel/byte is latched
    unsigned latched_register_;        // 0..7 (R0..R7), see write()

    // --- Internal generator state --------------------------------------
    unsigned tone_counter_[NumTones];  // Fixed point (10 decimal bits) down-counters
    int      tone_output_[NumTones];   // Current output level (+1 / -1) per tone channel

    unsigned noise_counter_;            // Fixed point down-counter for noise
    unsigned noise_shift_register_;    // 16-bit LFSR
    int      noise_output_;            // Current noise output level (+1 / -1)

    unsigned resample_step_;            // master_clock_ / sampling_rate_ in 10-bit fixed point

    int vol_table_[16];                  // Amplitude table indexed by attenuation
};

#endif // SN76489_H_
