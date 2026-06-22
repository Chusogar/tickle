/*
    M68000 emulator

    Portable Motorola 68000 CPU emulator for tickle.
    Implements the base M68000 instruction set (also covers M68010 basics).

    Based on the Motorola 68000 Programmer's Reference Manual.
*/
#ifndef M68000_H_
#define M68000_H_

class M68000Environment
{
public:
    M68000Environment() {}
    virtual ~M68000Environment() {}

    virtual unsigned char readByte( unsigned addr ) { return 0xFF; }
    virtual void writeByte( unsigned addr, unsigned char value ) {}

    virtual unsigned readWord( unsigned addr ) {
        return ((unsigned)readByte(addr) << 8) | readByte(addr + 1);
    }
    virtual void writeWord( unsigned addr, unsigned value ) {
        writeByte( addr, (value >> 8) & 0xFF );
        writeByte( addr + 1, value & 0xFF );
    }

    virtual unsigned readLong( unsigned addr ) {
        return ((unsigned)readWord(addr) << 16) | readWord(addr + 2);
    }
    virtual void writeLong( unsigned addr, unsigned value ) {
        writeWord( addr, (value >> 16) & 0xFFFF );
        writeWord( addr + 2, value & 0xFFFF );
    }

    // Called when the CPU acknowledges an interrupt at the given level.
    // Should return the vector number (0-255) or -1 for autovector.
    virtual int interruptAcknowledge( int level ) { return 24 + level; }
};

class M68000
{
public:
    // Status register flags
    enum {
        FlagC = 0x0001,
        FlagV = 0x0002,
        FlagZ = 0x0004,
        FlagN = 0x0008,
        FlagX = 0x0010,
        // System byte
        MaskIPM = 0x0700,   // Interrupt priority mask
        FlagS   = 0x2000,   // Supervisor mode
        FlagT   = 0x8000    // Trace mode
    };

    // Registers (public for driver access)
    unsigned D[8];          // Data registers
    unsigned A[8];          // Address registers (A7 = active stack pointer)
    unsigned PC;            // Program counter
    unsigned SR;            // Status register (CCR + system byte)
    unsigned USP;           // User stack pointer
    unsigned SSP;           // Supervisor stack pointer

    M68000( M68000Environment & env );
    virtual ~M68000() {}

    virtual void reset();
    virtual unsigned run( unsigned cycles );
    void interrupt( int level );
    void setIRQLine( int level );

    unsigned getCycles() const { return cycles_; }
    void setCycles( unsigned c ) { cycles_ = c; }

    // Helpers
    bool isSupervisor() const { return (SR & FlagS) != 0; }
    int getIPM() const { return (SR >> 8) & 7; }
    void setSR( unsigned val );
    unsigned getCCR() const { return SR & 0x1F; }
    void setCCR( unsigned val ) { SR = (SR & 0xFFE0) | (val & 0x1F); }

    bool stopped() const { return stopped_; }

private:
    M68000Environment & env_;

    unsigned cycles_;
    int icount_;
    int irq_level_;         // Pending interrupt level (0 = none)
    bool stopped_;          // STOP instruction active
    bool halted_;

    // Memory access helpers (big-endian)
    unsigned char rm8( unsigned addr );
    void wm8( unsigned addr, unsigned char val );
    unsigned rm16( unsigned addr );
    void wm16( unsigned addr, unsigned val );
    unsigned rm32( unsigned addr );
    void wm32( unsigned addr, unsigned val );

    // Fetch from PC
    unsigned fetch16();
    unsigned fetch32();

    // Stack operations
    void pushWord( unsigned val );
    void pushLong( unsigned val );
    unsigned popWord();
    unsigned popLong();

    // Effective address computation
    // Returns the EA for the given mode/reg; size is 1/2/4
    unsigned computeEA( int mode, int reg, int size );
    // Read from EA
    unsigned readEA( int mode, int reg, int size );
    // Write to EA
    void writeEA( int mode, int reg, int size, unsigned val );

    // Condition code helpers
    void setFlagNZ_B( unsigned val );
    void setFlagNZ_W( unsigned val );
    void setFlagNZ_L( unsigned val );

    // Condition test (for Bcc, Scc, DBcc)
    bool testCondition( int cond );

    // Exception processing
    void exception( int vector );
    void checkInterrupts();
    void groupZeroException( int vector, unsigned addr, unsigned status );

    // Main decode and execute
    void executeOne();

    // Instruction groups
    void group0000( unsigned op );  // Bit manipulation/MOVEP/Immediate
    void group0001( unsigned op );  // MOVE.B
    void group0010( unsigned op );  // MOVE.L
    void group0011( unsigned op );  // MOVE.W
    void group0100( unsigned op );  // Miscellaneous
    void group0101( unsigned op );  // ADDQ/SUBQ/Scc/DBcc
    void group0110( unsigned op );  // Bcc/BSR/BRA
    void group0111( unsigned op );  // MOVEQ
    void group1000( unsigned op );  // OR/DIV/SBCD
    void group1001( unsigned op );  // SUB/SUBX
    void group1011( unsigned op );  // CMP/EOR
    void group1100( unsigned op );  // AND/MUL/ABCD/EXG
    void group1101( unsigned op );  // ADD/ADDX
    void group1110( unsigned op );  // Shift/Rotate

    // ALU operations with flag setting
    unsigned add_b( unsigned s, unsigned d );
    unsigned add_w( unsigned s, unsigned d );
    unsigned add_l( unsigned s, unsigned d );
    unsigned sub_b( unsigned s, unsigned d );
    unsigned sub_w( unsigned s, unsigned d );
    unsigned sub_l( unsigned s, unsigned d );
    unsigned addx_b( unsigned s, unsigned d );
    unsigned addx_w( unsigned s, unsigned d );
    unsigned addx_l( unsigned s, unsigned d );
    unsigned subx_b( unsigned s, unsigned d );
    unsigned subx_w( unsigned s, unsigned d );
    unsigned subx_l( unsigned s, unsigned d );
    void cmp_b( unsigned s, unsigned d );
    void cmp_w( unsigned s, unsigned d );
    void cmp_l( unsigned s, unsigned d );
    unsigned neg_b( unsigned d );
    unsigned neg_w( unsigned d );
    unsigned neg_l( unsigned d );
    unsigned negx_b( unsigned d );
    unsigned negx_w( unsigned d );
    unsigned negx_l( unsigned d );
};

#endif // M68000_H_
