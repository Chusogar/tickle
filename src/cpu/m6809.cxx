/*
    M6809 emulator

    Based on the Motorola 6809 emulator by John Butler (1997)
    as found in MAME 0.37b7.

    Ported to C++ and adapted for tickle by Devin (2026).

    History (from original):
    991026 HJB: Fixed missing CHANGE_PC for TFR and EXG opcodes.
    991024 HJB: Speed improvements, indexed addressing reworked.
    990312 HJB: Bugfixes per Aaron's findings.
    990225 HJB: New interrupt handling.
*/

#include <string.h>
#include "m6809.h"

#define SIGNED8(b) ((unsigned short)((b)&0x80 ? (b)|0xff00 : (b)))

// ---------------------------------------------------------------------------
// Flag tables
// ---------------------------------------------------------------------------
const unsigned char M6809::flags8i_[256] = {
    0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0a,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08
};
const unsigned char M6809::flags8d_[256] = {
    0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08
};

// ---------------------------------------------------------------------------
// Constructor / Reset
// ---------------------------------------------------------------------------
M6809::M6809( M6809Environment & env ) : env_(env)
{
    cycles_ = 0;
    icount_ = 0;
    extra_cycles_ = 0;
    int_state_ = 0;
    nmi_state_ = 0;
    irq_state_ = 0;
    firq_state_ = 0;
    ea_ = 0;
    A = B = CC = DP = 0;
    X = Y = S = U = PC = 0;
}

void M6809::reset()
{
    int_state_ = 0;
    nmi_state_ = 0;
    irq_state_ = 0;
    firq_state_ = 0;
    extra_cycles_ = 0;
    DP = 0;
    CC |= FlagI | FlagF;
    PC = rm16(0xFFFE);
    cycles_ = 0;
}

// ---------------------------------------------------------------------------
// Memory access
// ---------------------------------------------------------------------------
unsigned char M6809::rm( unsigned addr )
{
    return env_.readByte( addr & 0xFFFF );
}

void M6809::wm( unsigned addr, unsigned char val )
{
    env_.writeByte( addr & 0xFFFF, val );
}

unsigned M6809::rm16( unsigned addr )
{
    unsigned hi = rm(addr);
    unsigned lo = rm((addr+1)&0xFFFF);
    return (hi << 8) | lo;
}

void M6809::wm16( unsigned addr, unsigned val )
{
    wm( addr, (val >> 8) & 0xFF );
    wm( (addr+1)&0xFFFF, val & 0xFF );
}

// ---------------------------------------------------------------------------
// Addressing mode helpers
// ---------------------------------------------------------------------------
unsigned char M6809::imm8()
{
    unsigned char b = rm(PC);
    PC = (PC + 1) & 0xFFFF;
    return b;
}

unsigned M6809::imm16()
{
    unsigned w = rm16(PC);
    PC = (PC + 2) & 0xFFFF;
    return w;
}

void M6809::direct()
{
    ea_ = ((unsigned)DP << 8) | imm8();
}

void M6809::extended()
{
    ea_ = imm16();
}

void M6809::indexed()
{
    unsigned char postbyte = imm8();

    // Storage for register value
    unsigned regval;

    switch( postbyte & 0x60 ) {
        case 0x00: regval = X; break;
        case 0x20: regval = Y; break;
        case 0x40: regval = U; break;
        case 0x60: regval = S; break;
        default:   regval = 0; break;
    }

    if( postbyte & 0x80 ) {
        // Non-constant offset
        switch( postbyte & 0x1f ) {
            case 0x00: // ,R+
                ea_ = regval;
                regval = (regval + 1) & 0xFFFF;
                icount_ -= 2;
                break;
            case 0x01: // ,R++
                ea_ = regval;
                regval = (regval + 2) & 0xFFFF;
                icount_ -= 3;
                break;
            case 0x02: // ,-R
                regval = (regval - 1) & 0xFFFF;
                ea_ = regval;
                icount_ -= 2;
                break;
            case 0x03: // ,--R
                regval = (regval - 2) & 0xFFFF;
                ea_ = regval;
                icount_ -= 3;
                break;
            case 0x04: // ,R (no offset)
                ea_ = regval;
                break;
            case 0x05: // B,R
                ea_ = (regval + SIGNED8(B)) & 0xFFFF;
                icount_ -= 1;
                break;
            case 0x06: // A,R
                ea_ = (regval + SIGNED8(A)) & 0xFFFF;
                icount_ -= 1;
                break;
            case 0x08: // n8,R
                ea_ = (regval + SIGNED8(imm8())) & 0xFFFF;
                icount_ -= 1;
                break;
            case 0x09: // n16,R
                ea_ = (regval + (short)(imm16())) & 0xFFFF;
                icount_ -= 4;
                break;
            case 0x0b: // D,R
                ea_ = (regval + (short)D()) & 0xFFFF;
                icount_ -= 4;
                break;
            case 0x0c: // n8,PC
                { unsigned char t = imm8();
                  ea_ = (PC + SIGNED8(t)) & 0xFFFF; }
                icount_ -= 1;
                break;
            case 0x0d: // n16,PC
                { unsigned t = imm16();
                  ea_ = (PC + (short)t) & 0xFFFF; }
                icount_ -= 5;
                break;
            case 0x11: // [,R++]
                ea_ = regval;
                regval = (regval + 2) & 0xFFFF;
                ea_ = rm16(ea_);
                icount_ -= 6;
                break;
            case 0x13: // [,--R]
                regval = (regval - 2) & 0xFFFF;
                ea_ = rm16(regval);
                icount_ -= 6;
                break;
            case 0x14: // [,R]
                ea_ = rm16(regval);
                icount_ -= 3;
                break;
            case 0x15: // [B,R]
                ea_ = rm16((regval + SIGNED8(B)) & 0xFFFF);
                icount_ -= 4;
                break;
            case 0x16: // [A,R]
                ea_ = rm16((regval + SIGNED8(A)) & 0xFFFF);
                icount_ -= 4;
                break;
            case 0x18: // [n8,R]
                ea_ = rm16((regval + SIGNED8(imm8())) & 0xFFFF);
                icount_ -= 4;
                break;
            case 0x19: // [n16,R]
                ea_ = rm16((regval + (short)(imm16())) & 0xFFFF);
                icount_ -= 7;
                break;
            case 0x1b: // [D,R]
                ea_ = rm16((regval + (short)D()) & 0xFFFF);
                icount_ -= 7;
                break;
            case 0x1c: // [n8,PC]
                { unsigned char t = imm8();
                  ea_ = rm16((PC + SIGNED8(t)) & 0xFFFF); }
                icount_ -= 4;
                break;
            case 0x1d: // [n16,PC]
                { unsigned t = imm16();
                  ea_ = rm16((PC + (short)t) & 0xFFFF); }
                icount_ -= 8;
                break;
            case 0x1f: // [n16] (extended indirect)
                ea_ = rm16(imm16());
                icount_ -= 5;
                break;
            default:
                ea_ = 0;
                break;
        }
    } else {
        // 5-bit constant offset
        int offset = postbyte & 0x1F;
        if( offset & 0x10 ) offset |= (int)0xFFFFFFF0u; // sign extend
        ea_ = (regval + offset) & 0xFFFF;
        icount_ -= 1;
    }

    // Write back register
    switch( postbyte & 0x60 ) {
        case 0x00: X = regval; break;
        case 0x20: Y = regval; break;
        case 0x40: U = regval; break;
        case 0x60: S = regval; break;
    }
}

// ---------------------------------------------------------------------------
// Stack operations
// ---------------------------------------------------------------------------
void M6809::pushByteS( unsigned char b ) { S = (S - 1) & 0xFFFF; wm(S, b); }
void M6809::pushWordS( unsigned val )    { S = (S - 1) & 0xFFFF; wm(S, val & 0xFF); S = (S - 1) & 0xFFFF; wm(S, (val >> 8) & 0xFF); }
unsigned char M6809::pullByteS()         { unsigned char b = rm(S); S = (S + 1) & 0xFFFF; return b; }
unsigned M6809::pullWordS()              { unsigned hi = rm(S); S = (S + 1) & 0xFFFF; unsigned lo = rm(S); S = (S + 1) & 0xFFFF; return (hi << 8) | lo; }
void M6809::pushByteU( unsigned char b ) { U = (U - 1) & 0xFFFF; wm(U, b); }
void M6809::pushWordU( unsigned val )    { U = (U - 1) & 0xFFFF; wm(U, val & 0xFF); U = (U - 1) & 0xFFFF; wm(U, (val >> 8) & 0xFF); }
unsigned char M6809::pullByteU()         { unsigned char b = rm(U); U = (U + 1) & 0xFFFF; return b; }
unsigned M6809::pullWordU()              { unsigned hi = rm(U); U = (U + 1) & 0xFFFF; unsigned lo = rm(U); U = (U + 1) & 0xFFFF; return (hi << 8) | lo; }

// ---------------------------------------------------------------------------
// Interrupt handling
// ---------------------------------------------------------------------------
void M6809::check_irq_lines()
{
    if( irq_state_ || firq_state_ )
        int_state_ &= ~StateSYNC;

    if( firq_state_ && !(CC & FlagF) ) {
        do_firq();
    } else if( irq_state_ && !(CC & FlagI) ) {
        do_irq();
    }
}

void M6809::do_irq()
{
    if( int_state_ & StateCWAI ) {
        int_state_ &= ~StateCWAI;
        extra_cycles_ += 7;
    } else {
        CC |= FlagE;
        pushWordS(PC);
        pushWordS(U);
        pushWordS(Y);
        pushWordS(X);
        pushByteS(DP);
        pushByteS(B);
        pushByteS(A);
        pushByteS(CC);
        extra_cycles_ += 19;
    }
    CC |= FlagI;
    PC = rm16(0xFFF8);
}

void M6809::do_firq()
{
    if( int_state_ & StateCWAI ) {
        int_state_ &= ~StateCWAI;
        extra_cycles_ += 7;
    } else {
        CC &= ~FlagE;
        pushWordS(PC);
        pushByteS(CC);
        extra_cycles_ += 10;
    }
    CC |= FlagI | FlagF;
    PC = rm16(0xFFF6);
}

void M6809::irq()
{
    irq_state_ = 1;
    if( !(CC & FlagI) ) {
        int_state_ &= ~StateSYNC;
        do_irq();
    }
}

void M6809::firq()
{
    firq_state_ = 1;
    if( !(CC & FlagF) ) {
        int_state_ &= ~StateSYNC;
        do_firq();
    }
}

void M6809::nmi()
{
    if( !(int_state_ & StateLDS) ) return;
    int_state_ &= ~StateSYNC;

    if( int_state_ & StateCWAI ) {
        int_state_ &= ~StateCWAI;
        extra_cycles_ += 7;
    } else {
        CC |= FlagE;
        pushWordS(PC);
        pushWordS(U);
        pushWordS(Y);
        pushWordS(X);
        pushByteS(DP);
        pushByteS(B);
        pushByteS(A);
        pushByteS(CC);
        extra_cycles_ += 19;
    }
    CC |= FlagI | FlagF;
    PC = rm16(0xFFFC);
}

// ---------------------------------------------------------------------------
// Main execution loop
// ---------------------------------------------------------------------------
unsigned M6809::run( unsigned cycles )
{
    icount_ = (int)cycles - extra_cycles_;
    extra_cycles_ = 0;

    if( int_state_ & (StateCWAI | StateSYNC) ) {
        icount_ = 0;
    } else {
        while( icount_ > 0 ) {
            execute_one();
        }
        icount_ -= extra_cycles_;
        extra_cycles_ = 0;
    }

    cycles_ += cycles - icount_;
    return (icount_ < 0) ? (unsigned)(-icount_) : 0;
}

// ---------------------------------------------------------------------------
// Main opcode dispatch
// ---------------------------------------------------------------------------
void M6809::execute_one()
{
    unsigned char opcode = rm(PC);
    PC = (PC + 1) & 0xFFFF;

    switch( opcode ) {
    case 0x00: neg_di();   icount_-= 6; break;
    case 0x03: com_di();   icount_-= 6; break;
    case 0x04: lsr_di();   icount_-= 6; break;
    case 0x06: ror_di();   icount_-= 6; break;
    case 0x07: asr_di();   icount_-= 6; break;
    case 0x08: asl_di();   icount_-= 6; break;
    case 0x09: rol_di();   icount_-= 6; break;
    case 0x0a: dec_di();   icount_-= 6; break;
    case 0x0c: inc_di();   icount_-= 6; break;
    case 0x0d: tst_di();   icount_-= 6; break;
    case 0x0e: jmp_di();   icount_-= 3; break;
    case 0x0f: clr_di();   icount_-= 6; break;
    case 0x10: pref10();                 break;
    case 0x11: pref11();                 break;
    case 0x12: nop_();     icount_-= 2; break;
    case 0x13: sync_();    icount_-= 4; break;
    case 0x16: lbra();     icount_-= 5; break;
    case 0x17: lbsr();     icount_-= 9; break;
    case 0x19: daa();      icount_-= 2; break;
    case 0x1a: orcc();     icount_-= 3; break;
    case 0x1c: andcc();    icount_-= 3; break;
    case 0x1d: sex();      icount_-= 2; break;
    case 0x1e: exg();      icount_-= 8; break;
    case 0x1f: tfr();      icount_-= 6; break;
    case 0x20: bra();      icount_-= 3; break;
    case 0x21: brn();      icount_-= 3; break;
    case 0x22: bhi();      icount_-= 3; break;
    case 0x23: bls();      icount_-= 3; break;
    case 0x24: bcc();      icount_-= 3; break;
    case 0x25: bcs();      icount_-= 3; break;
    case 0x26: bne();      icount_-= 3; break;
    case 0x27: beq();      icount_-= 3; break;
    case 0x28: bvc();      icount_-= 3; break;
    case 0x29: bvs();      icount_-= 3; break;
    case 0x2a: bpl();      icount_-= 3; break;
    case 0x2b: bmi();      icount_-= 3; break;
    case 0x2c: bge();      icount_-= 3; break;
    case 0x2d: blt();      icount_-= 3; break;
    case 0x2e: bgt();      icount_-= 3; break;
    case 0x2f: ble();      icount_-= 3; break;
    case 0x30: leax();     icount_-= 4; break;
    case 0x31: leay();     icount_-= 4; break;
    case 0x32: leas();     icount_-= 4; break;
    case 0x33: leau();     icount_-= 4; break;
    case 0x34: pshs();     icount_-= 5; break;
    case 0x35: puls();     icount_-= 5; break;
    case 0x36: pshu();     icount_-= 5; break;
    case 0x37: pulu();     icount_-= 5; break;
    case 0x39: rts();      icount_-= 5; break;
    case 0x3a: abx();      icount_-= 3; break;
    case 0x3b: rti();      icount_-= 6; break;
    case 0x3c: cwai();     icount_-=20; break;
    case 0x3d: mul();      icount_-=11; break;
    case 0x3f: swi();      icount_-=19; break;
    case 0x40: nega();     icount_-= 2; break;
    case 0x43: coma();     icount_-= 2; break;
    case 0x44: lsra();     icount_-= 2; break;
    case 0x46: rora();     icount_-= 2; break;
    case 0x47: asra();     icount_-= 2; break;
    case 0x48: asla();     icount_-= 2; break;
    case 0x49: rola();     icount_-= 2; break;
    case 0x4a: deca();     icount_-= 2; break;
    case 0x4c: inca();     icount_-= 2; break;
    case 0x4d: tsta();     icount_-= 2; break;
    case 0x4f: clra();     icount_-= 2; break;
    case 0x50: negb();     icount_-= 2; break;
    case 0x53: comb();     icount_-= 2; break;
    case 0x54: lsrb();     icount_-= 2; break;
    case 0x56: rorb();     icount_-= 2; break;
    case 0x57: asrb();     icount_-= 2; break;
    case 0x58: aslb();     icount_-= 2; break;
    case 0x59: rolb();     icount_-= 2; break;
    case 0x5a: decb();     icount_-= 2; break;
    case 0x5c: incb();     icount_-= 2; break;
    case 0x5d: tstb();     icount_-= 2; break;
    case 0x5f: clrb();     icount_-= 2; break;
    case 0x60: neg_ix();   icount_-= 6; break;
    case 0x63: com_ix();   icount_-= 6; break;
    case 0x64: lsr_ix();   icount_-= 6; break;
    case 0x66: ror_ix();   icount_-= 6; break;
    case 0x67: asr_ix();   icount_-= 6; break;
    case 0x68: asl_ix();   icount_-= 6; break;
    case 0x69: rol_ix();   icount_-= 6; break;
    case 0x6a: dec_ix();   icount_-= 6; break;
    case 0x6c: inc_ix();   icount_-= 6; break;
    case 0x6d: tst_ix();   icount_-= 6; break;
    case 0x6e: jmp_ix();   icount_-= 3; break;
    case 0x6f: clr_ix();   icount_-= 6; break;
    case 0x70: neg_ex();   icount_-= 7; break;
    case 0x73: com_ex();   icount_-= 7; break;
    case 0x74: lsr_ex();   icount_-= 7; break;
    case 0x76: ror_ex();   icount_-= 7; break;
    case 0x77: asr_ex();   icount_-= 7; break;
    case 0x78: asl_ex();   icount_-= 7; break;
    case 0x79: rol_ex();   icount_-= 7; break;
    case 0x7a: dec_ex();   icount_-= 7; break;
    case 0x7c: inc_ex();   icount_-= 7; break;
    case 0x7d: tst_ex();   icount_-= 7; break;
    case 0x7e: jmp_ex();   icount_-= 4; break;
    case 0x7f: clr_ex();   icount_-= 7; break;
    case 0x80: suba_im();  icount_-= 2; break;
    case 0x81: cmpa_im();  icount_-= 2; break;
    case 0x82: sbca_im();  icount_-= 2; break;
    case 0x83: subd_im();  icount_-= 4; break;
    case 0x84: anda_im();  icount_-= 2; break;
    case 0x85: bita_im();  icount_-= 2; break;
    case 0x86: lda_im();   icount_-= 2; break;
    case 0x87: sta_im();   icount_-= 2; break;
    case 0x88: eora_im();  icount_-= 2; break;
    case 0x89: adca_im();  icount_-= 2; break;
    case 0x8a: ora_im();   icount_-= 2; break;
    case 0x8b: adda_im();  icount_-= 2; break;
    case 0x8c: cmpx_im();  icount_-= 4; break;
    case 0x8d: bsr();      icount_-= 7; break;
    case 0x8e: ldx_im();   icount_-= 3; break;
    case 0x8f: stx_im();   icount_-= 2; break;
    case 0x90: suba_di();  icount_-= 4; break;
    case 0x91: cmpa_di();  icount_-= 4; break;
    case 0x92: sbca_di();  icount_-= 4; break;
    case 0x93: subd_di();  icount_-= 6; break;
    case 0x94: anda_di();  icount_-= 4; break;
    case 0x95: bita_di();  icount_-= 4; break;
    case 0x96: lda_di();   icount_-= 4; break;
    case 0x97: sta_di();   icount_-= 4; break;
    case 0x98: eora_di();  icount_-= 4; break;
    case 0x99: adca_di();  icount_-= 4; break;
    case 0x9a: ora_di();   icount_-= 4; break;
    case 0x9b: adda_di();  icount_-= 4; break;
    case 0x9c: cmpx_di();  icount_-= 6; break;
    case 0x9d: jsr_di();   icount_-= 7; break;
    case 0x9e: ldx_di();   icount_-= 5; break;
    case 0x9f: stx_di();   icount_-= 5; break;
    case 0xa0: suba_ix();  icount_-= 4; break;
    case 0xa1: cmpa_ix();  icount_-= 4; break;
    case 0xa2: sbca_ix();  icount_-= 4; break;
    case 0xa3: subd_ix();  icount_-= 6; break;
    case 0xa4: anda_ix();  icount_-= 4; break;
    case 0xa5: bita_ix();  icount_-= 4; break;
    case 0xa6: lda_ix();   icount_-= 4; break;
    case 0xa7: sta_ix();   icount_-= 4; break;
    case 0xa8: eora_ix();  icount_-= 4; break;
    case 0xa9: adca_ix();  icount_-= 4; break;
    case 0xaa: ora_ix();   icount_-= 4; break;
    case 0xab: adda_ix();  icount_-= 4; break;
    case 0xac: cmpx_ix();  icount_-= 6; break;
    case 0xad: jsr_ix();   icount_-= 7; break;
    case 0xae: ldx_ix();   icount_-= 5; break;
    case 0xaf: stx_ix();   icount_-= 5; break;
    case 0xb0: suba_ex();  icount_-= 5; break;
    case 0xb1: cmpa_ex();  icount_-= 5; break;
    case 0xb2: sbca_ex();  icount_-= 5; break;
    case 0xb3: subd_ex();  icount_-= 7; break;
    case 0xb4: anda_ex();  icount_-= 5; break;
    case 0xb5: bita_ex();  icount_-= 5; break;
    case 0xb6: lda_ex();   icount_-= 5; break;
    case 0xb7: sta_ex();   icount_-= 5; break;
    case 0xb8: eora_ex();  icount_-= 5; break;
    case 0xb9: adca_ex();  icount_-= 5; break;
    case 0xba: ora_ex();   icount_-= 5; break;
    case 0xbb: adda_ex();  icount_-= 5; break;
    case 0xbc: cmpx_ex();  icount_-= 7; break;
    case 0xbd: jsr_ex();   icount_-= 8; break;
    case 0xbe: ldx_ex();   icount_-= 6; break;
    case 0xbf: stx_ex();   icount_-= 6; break;
    case 0xc0: subb_im();  icount_-= 2; break;
    case 0xc1: cmpb_im();  icount_-= 2; break;
    case 0xc2: sbcb_im();  icount_-= 2; break;
    case 0xc3: addd_im();  icount_-= 4; break;
    case 0xc4: andb_im();  icount_-= 2; break;
    case 0xc5: bitb_im();  icount_-= 2; break;
    case 0xc6: ldb_im();   icount_-= 2; break;
    case 0xc7: stb_im();   icount_-= 2; break;
    case 0xc8: eorb_im();  icount_-= 2; break;
    case 0xc9: adcb_im();  icount_-= 2; break;
    case 0xca: orb_im();   icount_-= 2; break;
    case 0xcb: addb_im();  icount_-= 2; break;
    case 0xcc: ldd_im();   icount_-= 3; break;
    case 0xcd: std_im();   icount_-= 2; break;
    case 0xce: ldu_im();   icount_-= 3; break;
    case 0xcf: stu_im();   icount_-= 3; break;
    case 0xd0: subb_di();  icount_-= 4; break;
    case 0xd1: cmpb_di();  icount_-= 4; break;
    case 0xd2: sbcb_di();  icount_-= 4; break;
    case 0xd3: addd_di();  icount_-= 6; break;
    case 0xd4: andb_di();  icount_-= 4; break;
    case 0xd5: bitb_di();  icount_-= 4; break;
    case 0xd6: ldb_di();   icount_-= 4; break;
    case 0xd7: stb_di();   icount_-= 4; break;
    case 0xd8: eorb_di();  icount_-= 4; break;
    case 0xd9: adcb_di();  icount_-= 4; break;
    case 0xda: orb_di();   icount_-= 4; break;
    case 0xdb: addb_di();  icount_-= 4; break;
    case 0xdc: ldd_di();   icount_-= 5; break;
    case 0xdd: std_di();   icount_-= 5; break;
    case 0xde: ldu_di();   icount_-= 5; break;
    case 0xdf: stu_di();   icount_-= 5; break;
    case 0xe0: subb_ix();  icount_-= 4; break;
    case 0xe1: cmpb_ix();  icount_-= 4; break;
    case 0xe2: sbcb_ix();  icount_-= 4; break;
    case 0xe3: addd_ix();  icount_-= 6; break;
    case 0xe4: andb_ix();  icount_-= 4; break;
    case 0xe5: bitb_ix();  icount_-= 4; break;
    case 0xe6: ldb_ix();   icount_-= 4; break;
    case 0xe7: stb_ix();   icount_-= 4; break;
    case 0xe8: eorb_ix();  icount_-= 4; break;
    case 0xe9: adcb_ix();  icount_-= 4; break;
    case 0xea: orb_ix();   icount_-= 4; break;
    case 0xeb: addb_ix();  icount_-= 4; break;
    case 0xec: ldd_ix();   icount_-= 5; break;
    case 0xed: std_ix();   icount_-= 5; break;
    case 0xee: ldu_ix();   icount_-= 5; break;
    case 0xef: stu_ix();   icount_-= 5; break;
    case 0xf0: subb_ex();  icount_-= 5; break;
    case 0xf1: cmpb_ex();  icount_-= 5; break;
    case 0xf2: sbcb_ex();  icount_-= 5; break;
    case 0xf3: addd_ex();  icount_-= 7; break;
    case 0xf4: andb_ex();  icount_-= 5; break;
    case 0xf5: bitb_ex();  icount_-= 5; break;
    case 0xf6: ldb_ex();   icount_-= 5; break;
    case 0xf7: stb_ex();   icount_-= 5; break;
    case 0xf8: eorb_ex();  icount_-= 5; break;
    case 0xf9: adcb_ex();  icount_-= 5; break;
    case 0xfa: orb_ex();   icount_-= 5; break;
    case 0xfb: addb_ex();  icount_-= 5; break;
    case 0xfc: ldd_ex();   icount_-= 6; break;
    case 0xfd: std_ex();   icount_-= 6; break;
    case 0xfe: ldu_ex();   icount_-= 6; break;
    case 0xff: stu_ex();   icount_-= 6; break;
    default:               icount_-= 2; break; // illegal
    }
}

// ---------------------------------------------------------------------------
// Page 2 prefix (0x10)
// ---------------------------------------------------------------------------
void M6809::pref10()
{
    unsigned char opcode = rm(PC);
    PC = (PC + 1) & 0xFFFF;

    switch( opcode ) {
    case 0x21: lbrn();     icount_-= 5; break;
    case 0x22: lbhi();     icount_-= 5; break;
    case 0x23: lbls();     icount_-= 5; break;
    case 0x24: lbcc();     icount_-= 5; break;
    case 0x25: lbcs();     icount_-= 5; break;
    case 0x26: lbne();     icount_-= 5; break;
    case 0x27: lbeq();     icount_-= 5; break;
    case 0x28: lbvc();     icount_-= 5; break;
    case 0x29: lbvs();     icount_-= 5; break;
    case 0x2a: lbpl();     icount_-= 5; break;
    case 0x2b: lbmi();     icount_-= 5; break;
    case 0x2c: lbge();     icount_-= 5; break;
    case 0x2d: lblt();     icount_-= 5; break;
    case 0x2e: lbgt();     icount_-= 5; break;
    case 0x2f: lble();     icount_-= 5; break;
    case 0x3f: swi2();     icount_-=20; break;
    case 0x83: cmpd_im();  icount_-= 5; break;
    case 0x8c: cmpy_im();  icount_-= 5; break;
    case 0x8e: ldy_im();   icount_-= 4; break;
    case 0x8f: sty_im();   icount_-= 4; break;
    case 0x93: cmpd_di();  icount_-= 7; break;
    case 0x9c: cmpy_di();  icount_-= 7; break;
    case 0x9e: ldy_di();   icount_-= 6; break;
    case 0x9f: sty_di();   icount_-= 6; break;
    case 0xa3: cmpd_ix();  icount_-= 7; break;
    case 0xac: cmpy_ix();  icount_-= 7; break;
    case 0xae: ldy_ix();   icount_-= 6; break;
    case 0xaf: sty_ix();   icount_-= 6; break;
    case 0xb3: cmpd_ex();  icount_-= 8; break;
    case 0xbc: cmpy_ex();  icount_-= 8; break;
    case 0xbe: ldy_ex();   icount_-= 7; break;
    case 0xbf: sty_ex();   icount_-= 7; break;
    case 0xce: lds_im();   icount_-= 4; break;
    case 0xcf: sts_im();   icount_-= 4; break;
    case 0xde: lds_di();   icount_-= 6; break;
    case 0xdf: sts_di();   icount_-= 6; break;
    case 0xee: lds_ix();   icount_-= 6; break;
    case 0xef: sts_ix();   icount_-= 6; break;
    case 0xfe: lds_ex();   icount_-= 7; break;
    case 0xff: sts_ex();   icount_-= 7; break;
    default:               icount_-= 2; break;
    }
}

// ---------------------------------------------------------------------------
// Page 3 prefix (0x11)
// ---------------------------------------------------------------------------
void M6809::pref11()
{
    unsigned char opcode = rm(PC);
    PC = (PC + 1) & 0xFFFF;

    switch( opcode ) {
    case 0x3f: swi3();     icount_-=20; break;
    case 0x83: cmpu_im();  icount_-= 5; break;
    case 0x8c: cmps_im();  icount_-= 5; break;
    case 0x93: cmpu_di();  icount_-= 7; break;
    case 0x9c: cmps_di();  icount_-= 7; break;
    case 0xa3: cmpu_ix();  icount_-= 7; break;
    case 0xac: cmps_ix();  icount_-= 7; break;
    case 0xb3: cmpu_ex();  icount_-= 8; break;
    case 0xbc: cmps_ex();  icount_-= 8; break;
    default:               icount_-= 2; break;
    }
}

// ===========================================================================
// OPCODE IMPLEMENTATIONS
// ===========================================================================

// --- NEG ---
void M6809::neg_di()  { direct();   unsigned t=rm(ea_); unsigned r=(~t+1)&0xFF; clr_nzvc(); set_flags8(0,t,r); wm(ea_,r); }
void M6809::neg_ix()  { indexed();  unsigned t=rm(ea_); unsigned r=(~t+1)&0xFF; clr_nzvc(); set_flags8(0,t,r); wm(ea_,r); }
void M6809::neg_ex()  { extended(); unsigned t=rm(ea_); unsigned r=(~t+1)&0xFF; clr_nzvc(); set_flags8(0,t,r); wm(ea_,r); }
void M6809::nega()    { unsigned r=(~A+1)&0xFF; clr_nzvc(); set_flags8(0,A,r); A=r; }
void M6809::negb()    { unsigned r=(~B+1)&0xFF; clr_nzvc(); set_flags8(0,B,r); B=r; }

// --- COM ---
void M6809::com_di()  { direct();   unsigned char t=rm(ea_); t=~t; clr_nzv(); set_nz8(t); CC|=FlagC; wm(ea_,t); }
void M6809::com_ix()  { indexed();  unsigned char t=rm(ea_); t=~t; clr_nzv(); set_nz8(t); CC|=FlagC; wm(ea_,t); }
void M6809::com_ex()  { extended(); unsigned char t=rm(ea_); t=~t; clr_nzv(); set_nz8(t); CC|=FlagC; wm(ea_,t); }
void M6809::coma()    { A=~A; clr_nzv(); set_nz8(A); CC|=FlagC; }
void M6809::comb()    { B=~B; clr_nzv(); set_nz8(B); CC|=FlagC; }

// --- LSR ---
void M6809::lsr_di()  { direct();   unsigned char t=rm(ea_); clr_nzc(); CC|=(t&FlagC); t>>=1; set_z8(t); wm(ea_,t); }
void M6809::lsr_ix()  { indexed();  unsigned char t=rm(ea_); clr_nzc(); CC|=(t&FlagC); t>>=1; set_z8(t); wm(ea_,t); }
void M6809::lsr_ex()  { extended(); unsigned char t=rm(ea_); clr_nzc(); CC|=(t&FlagC); t>>=1; set_z8(t); wm(ea_,t); }
void M6809::lsra()    { clr_nzc(); CC|=(A&FlagC); A>>=1; set_z8(A); }
void M6809::lsrb()    { clr_nzc(); CC|=(B&FlagC); B>>=1; set_z8(B); }

// --- ROR ---
void M6809::ror_di()  { direct();   unsigned char t=rm(ea_); unsigned char r=(CC&FlagC)<<7; clr_nzc(); CC|=(t&FlagC); r|=t>>1; set_nz8(r); wm(ea_,r); }
void M6809::ror_ix()  { indexed();  unsigned char t=rm(ea_); unsigned char r=(CC&FlagC)<<7; clr_nzc(); CC|=(t&FlagC); r|=t>>1; set_nz8(r); wm(ea_,r); }
void M6809::ror_ex()  { extended(); unsigned char t=rm(ea_); unsigned char r=(CC&FlagC)<<7; clr_nzc(); CC|=(t&FlagC); r|=t>>1; set_nz8(r); wm(ea_,r); }
void M6809::rora()    { unsigned char r=(CC&FlagC)<<7; clr_nzc(); CC|=(A&FlagC); r|=A>>1; set_nz8(r); A=r; }
void M6809::rorb()    { unsigned char r=(CC&FlagC)<<7; clr_nzc(); CC|=(B&FlagC); r|=B>>1; set_nz8(r); B=r; }

// --- ASR ---
void M6809::asr_di()  { direct();   unsigned char t=rm(ea_); clr_nzc(); CC|=(t&FlagC); t=(t&0x80)|(t>>1); set_nz8(t); wm(ea_,t); }
void M6809::asr_ix()  { indexed();  unsigned char t=rm(ea_); clr_nzc(); CC|=(t&FlagC); t=(t&0x80)|(t>>1); set_nz8(t); wm(ea_,t); }
void M6809::asr_ex()  { extended(); unsigned char t=rm(ea_); clr_nzc(); CC|=(t&FlagC); t=(t&0x80)|(t>>1); set_nz8(t); wm(ea_,t); }
void M6809::asra()    { clr_nzc(); CC|=(A&FlagC); A=(A&0x80)|(A>>1); set_nz8(A); }
void M6809::asrb()    { clr_nzc(); CC|=(B&FlagC); B=(B&0x80)|(B>>1); set_nz8(B); }

// --- ASL/LSL ---
void M6809::asl_di()  { direct();   unsigned t=rm(ea_); unsigned r=t<<1; clr_nzvc(); set_flags8(t,t,r); wm(ea_,r); }
void M6809::asl_ix()  { indexed();  unsigned t=rm(ea_); unsigned r=t<<1; clr_nzvc(); set_flags8(t,t,r); wm(ea_,r); }
void M6809::asl_ex()  { extended(); unsigned t=rm(ea_); unsigned r=t<<1; clr_nzvc(); set_flags8(t,t,r); wm(ea_,r); }
void M6809::asla()    { unsigned r=A<<1; clr_nzvc(); set_flags8(A,A,r); A=r; }
void M6809::aslb()    { unsigned r=B<<1; clr_nzvc(); set_flags8(B,B,r); B=r; }

// --- ROL ---
void M6809::rol_di()  { direct();   unsigned t=rm(ea_); unsigned r=(CC&FlagC)|(t<<1); clr_nzvc(); set_flags8(t,t,r); wm(ea_,r); }
void M6809::rol_ix()  { indexed();  unsigned t=rm(ea_); unsigned r=(CC&FlagC)|(t<<1); clr_nzvc(); set_flags8(t,t,r); wm(ea_,r); }
void M6809::rol_ex()  { extended(); unsigned t=rm(ea_); unsigned r=(CC&FlagC)|(t<<1); clr_nzvc(); set_flags8(t,t,r); wm(ea_,r); }
void M6809::rola()    { unsigned r=(CC&FlagC)|(A<<1); clr_nzvc(); set_flags8(A,A,r); A=r; }
void M6809::rolb()    { unsigned r=(CC&FlagC)|(B<<1); clr_nzvc(); set_flags8(B,B,r); B=r; }

// --- DEC ---
void M6809::dec_di()  { direct();   unsigned char t=rm(ea_); --t; clr_nzv(); CC|=flags8d_[t]; wm(ea_,t); }
void M6809::dec_ix()  { indexed();  unsigned char t=rm(ea_); --t; clr_nzv(); CC|=flags8d_[t]; wm(ea_,t); }
void M6809::dec_ex()  { extended(); unsigned char t=rm(ea_); --t; clr_nzv(); CC|=flags8d_[t]; wm(ea_,t); }
void M6809::deca()    { --A; clr_nzv(); CC|=flags8d_[A]; }
void M6809::decb()    { --B; clr_nzv(); CC|=flags8d_[B]; }

// --- INC ---
void M6809::inc_di()  { direct();   unsigned char t=rm(ea_); ++t; clr_nzv(); CC|=flags8i_[t]; wm(ea_,t); }
void M6809::inc_ix()  { indexed();  unsigned char t=rm(ea_); ++t; clr_nzv(); CC|=flags8i_[t]; wm(ea_,t); }
void M6809::inc_ex()  { extended(); unsigned char t=rm(ea_); ++t; clr_nzv(); CC|=flags8i_[t]; wm(ea_,t); }
void M6809::inca()    { ++A; clr_nzv(); CC|=flags8i_[A]; }
void M6809::incb()    { ++B; clr_nzv(); CC|=flags8i_[B]; }

// --- TST ---
void M6809::tst_di()  { direct();   unsigned char t=rm(ea_); clr_nzv(); set_nz8(t); }
void M6809::tst_ix()  { indexed();  unsigned char t=rm(ea_); clr_nzv(); set_nz8(t); }
void M6809::tst_ex()  { extended(); unsigned char t=rm(ea_); clr_nzv(); set_nz8(t); }
void M6809::tsta()    { clr_nzv(); set_nz8(A); }
void M6809::tstb()    { clr_nzv(); set_nz8(B); }

// --- JMP ---
void M6809::jmp_di()  { direct();   PC=ea_; }
void M6809::jmp_ix()  { indexed();  PC=ea_; }
void M6809::jmp_ex()  { extended(); PC=ea_; }

// --- CLR ---
void M6809::clr_di()  { direct();   wm(ea_,0); clr_nzvc(); CC|=FlagZ; }
void M6809::clr_ix()  { indexed();  wm(ea_,0); clr_nzvc(); CC|=FlagZ; }
void M6809::clr_ex()  { extended(); wm(ea_,0); clr_nzvc(); CC|=FlagZ; }
void M6809::clra()    { A=0; clr_nzvc(); CC|=FlagZ; }
void M6809::clrb()    { B=0; clr_nzvc(); CC|=FlagZ; }

// --- NOP / SYNC ---
void M6809::nop_()    { }
void M6809::sync_()   { int_state_ |= StateSYNC; if(icount_>0) icount_=0; }

// --- Branches ---
void M6809::bra()  { unsigned char t=imm8(); PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::brn()  { imm8(); }
void M6809::bhi()  { unsigned char t=imm8(); if(!(CC&(FlagZ|FlagC))) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bls()  { unsigned char t=imm8(); if(CC&(FlagZ|FlagC)) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bcc()  { unsigned char t=imm8(); if(!(CC&FlagC)) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bcs()  { unsigned char t=imm8(); if(CC&FlagC) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bne()  { unsigned char t=imm8(); if(!(CC&FlagZ)) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::beq()  { unsigned char t=imm8(); if(CC&FlagZ) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bvc()  { unsigned char t=imm8(); if(!(CC&FlagV)) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bvs()  { unsigned char t=imm8(); if(CC&FlagV) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bpl()  { unsigned char t=imm8(); if(!(CC&FlagN)) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bmi()  { unsigned char t=imm8(); if(CC&FlagN) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bge()  { unsigned char t=imm8(); if(!((CC&FlagN)^((CC&FlagV)<<2))) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::blt()  { unsigned char t=imm8(); if((CC&FlagN)^((CC&FlagV)<<2)) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::bgt()  { unsigned char t=imm8(); if(!(((CC&FlagN)^((CC&FlagV)<<2))|(CC&FlagZ))) PC=(PC+SIGNED8(t))&0xFFFF; }
void M6809::ble()  { unsigned char t=imm8(); if(((CC&FlagN)^((CC&FlagV)<<2))|(CC&FlagZ)) PC=(PC+SIGNED8(t))&0xFFFF; }

void M6809::bsr()  { unsigned char t=imm8(); pushWordS(PC); PC=(PC+SIGNED8(t))&0xFFFF; }

// --- Long branches ---
void M6809::lbra() { unsigned t=imm16(); PC=(PC+(short)t)&0xFFFF; }
void M6809::lbsr() { unsigned t=imm16(); pushWordS(PC); PC=(PC+(short)t)&0xFFFF; }
void M6809::lbrn() { imm16(); }
void M6809::lbhi() { unsigned t=imm16(); if(!(CC&(FlagZ|FlagC))){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbls() { unsigned t=imm16(); if(CC&(FlagZ|FlagC)){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbcc() { unsigned t=imm16(); if(!(CC&FlagC)){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbcs() { unsigned t=imm16(); if(CC&FlagC){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbne() { unsigned t=imm16(); if(!(CC&FlagZ)){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbeq() { unsigned t=imm16(); if(CC&FlagZ){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbvc() { unsigned t=imm16(); if(!(CC&FlagV)){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbvs() { unsigned t=imm16(); if(CC&FlagV){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbpl() { unsigned t=imm16(); if(!(CC&FlagN)){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbmi() { unsigned t=imm16(); if(CC&FlagN){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbge() { unsigned t=imm16(); if(!((CC&FlagN)^((CC&FlagV)<<2))){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lblt() { unsigned t=imm16(); if((CC&FlagN)^((CC&FlagV)<<2)){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lbgt() { unsigned t=imm16(); if(!(((CC&FlagN)^((CC&FlagV)<<2))|(CC&FlagZ))){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }
void M6809::lble() { unsigned t=imm16(); if(((CC&FlagN)^((CC&FlagV)<<2))|(CC&FlagZ)){icount_-=1;PC=(PC+(short)t)&0xFFFF;} }

// --- LEA ---
void M6809::leax() { indexed(); X=ea_; clr_z(); set_z16(X); }
void M6809::leay() { indexed(); Y=ea_; clr_z(); set_z16(Y); }
void M6809::leas() { indexed(); S=ea_; int_state_ |= StateLDS; }
void M6809::leau() { indexed(); U=ea_; }

// --- PSH/PUL ---
void M6809::pshs()
{
    unsigned char t = imm8();
    if(t&0x80) { pushWordS(PC); icount_-=2; }
    if(t&0x40) { pushWordS(U);  icount_-=2; }
    if(t&0x20) { pushWordS(Y);  icount_-=2; }
    if(t&0x10) { pushWordS(X);  icount_-=2; }
    if(t&0x08) { pushByteS(DP); icount_-=1; }
    if(t&0x04) { pushByteS(B);  icount_-=1; }
    if(t&0x02) { pushByteS(A);  icount_-=1; }
    if(t&0x01) { pushByteS(CC); icount_-=1; }
}

void M6809::puls()
{
    unsigned char t = imm8();
    if(t&0x01) { CC=pullByteS(); icount_-=1; }
    if(t&0x02) { A=pullByteS();  icount_-=1; }
    if(t&0x04) { B=pullByteS();  icount_-=1; }
    if(t&0x08) { DP=pullByteS(); icount_-=1; }
    if(t&0x10) { X=pullWordS();  icount_-=2; }
    if(t&0x20) { Y=pullWordS();  icount_-=2; }
    if(t&0x40) { U=pullWordS();  icount_-=2; }
    if(t&0x80) { PC=pullWordS(); icount_-=2; }
    if(t&0x01) check_irq_lines();
}

void M6809::pshu()
{
    unsigned char t = imm8();
    if(t&0x80) { pushWordU(PC); icount_-=2; }
    if(t&0x40) { pushWordU(S);  icount_-=2; }
    if(t&0x20) { pushWordU(Y);  icount_-=2; }
    if(t&0x10) { pushWordU(X);  icount_-=2; }
    if(t&0x08) { pushByteU(DP); icount_-=1; }
    if(t&0x04) { pushByteU(B);  icount_-=1; }
    if(t&0x02) { pushByteU(A);  icount_-=1; }
    if(t&0x01) { pushByteU(CC); icount_-=1; }
}

void M6809::pulu()
{
    unsigned char t = imm8();
    if(t&0x01) { CC=pullByteU(); icount_-=1; }
    if(t&0x02) { A=pullByteU();  icount_-=1; }
    if(t&0x04) { B=pullByteU();  icount_-=1; }
    if(t&0x08) { DP=pullByteU(); icount_-=1; }
    if(t&0x10) { X=pullWordU();  icount_-=2; }
    if(t&0x20) { Y=pullWordU();  icount_-=2; }
    if(t&0x40) { S=pullWordU();  icount_-=2; }
    if(t&0x80) { PC=pullWordU(); icount_-=2; }
    if(t&0x01) check_irq_lines();
}

// --- Misc ---
void M6809::rts() { PC=pullWordS(); }

void M6809::abx() { X = (X + B) & 0xFFFF; }

void M6809::rti()
{
    CC = pullByteS();
    if( CC & FlagE ) {
        A  = pullByteS();
        B  = pullByteS();
        DP = pullByteS();
        X  = pullWordS();
        Y  = pullWordS();
        U  = pullWordS();
        icount_ -= 9;
    }
    PC = pullWordS();
    check_irq_lines();
}

void M6809::cwai()
{
    unsigned char t = imm8();
    CC &= t;
    CC |= FlagE;
    pushWordS(PC);
    pushWordS(U);
    pushWordS(Y);
    pushWordS(X);
    pushByteS(DP);
    pushByteS(B);
    pushByteS(A);
    pushByteS(CC);
    int_state_ |= StateCWAI;
    check_irq_lines();
    if( (int_state_ & StateCWAI) && icount_ > 0 )
        icount_ = 0;
}

void M6809::mul()
{
    unsigned r = (unsigned)A * (unsigned)B;
    clr_z();
    set_z16(r);
    CC = (CC & ~FlagC) | ((r & 0x80) ? FlagC : 0);
    setD(r);
}

void M6809::swi()
{
    CC |= FlagE;
    pushWordS(PC);
    pushWordS(U);
    pushWordS(Y);
    pushWordS(X);
    pushByteS(DP);
    pushByteS(B);
    pushByteS(A);
    pushByteS(CC);
    CC |= FlagI | FlagF;
    PC = rm16(0xFFFA);
}

void M6809::swi2()
{
    CC |= FlagE;
    pushWordS(PC);
    pushWordS(U);
    pushWordS(Y);
    pushWordS(X);
    pushByteS(DP);
    pushByteS(B);
    pushByteS(A);
    pushByteS(CC);
    PC = rm16(0xFFF4);
}

void M6809::swi3()
{
    CC |= FlagE;
    pushWordS(PC);
    pushWordS(U);
    pushWordS(Y);
    pushWordS(X);
    pushByteS(DP);
    pushByteS(B);
    pushByteS(A);
    pushByteS(CC);
    PC = rm16(0xFFF2);
}

void M6809::daa()
{
    unsigned t = A;
    unsigned cf = 0;
    if( (CC & FlagH) || (t & 0x0F) > 9 ) cf |= 0x06;
    if( (CC & FlagC) || t > 0x9F || (t > 0x8F && (t & 0x0F) > 9) ) cf |= 0x60;
    t += cf;
    clr_nzv();
    set_nz8(t & 0xFF);
    if( t & 0x100 ) CC |= FlagC;
    A = t & 0xFF;
}

void M6809::orcc()  { CC |= imm8(); check_irq_lines(); }
void M6809::andcc() { CC &= imm8(); check_irq_lines(); }

void M6809::sex()
{
    unsigned d = SIGNED8(B);
    setD(d);
    clr_nzv();
    set_nz16(d);
}

void M6809::exg()
{
    unsigned char t = imm8();
    unsigned tmp1, tmp2;

    // Get source value
    switch( (t >> 4) & 0x0F ) {
        case 0x00: tmp1 = D(); break;
        case 0x01: tmp1 = X; break;
        case 0x02: tmp1 = Y; break;
        case 0x03: tmp1 = U; break;
        case 0x04: tmp1 = S; break;
        case 0x05: tmp1 = PC; break;
        case 0x08: tmp1 = A; break;
        case 0x09: tmp1 = B; break;
        case 0x0a: tmp1 = CC; break;
        case 0x0b: tmp1 = DP; break;
        default:   tmp1 = 0xFF; break;
    }

    // Get destination value
    switch( t & 0x0F ) {
        case 0x00: tmp2 = D(); break;
        case 0x01: tmp2 = X; break;
        case 0x02: tmp2 = Y; break;
        case 0x03: tmp2 = U; break;
        case 0x04: tmp2 = S; break;
        case 0x05: tmp2 = PC; break;
        case 0x08: tmp2 = A; break;
        case 0x09: tmp2 = B; break;
        case 0x0a: tmp2 = CC; break;
        case 0x0b: tmp2 = DP; break;
        default:   tmp2 = 0xFF; break;
    }

    // Set source = old dest
    switch( (t >> 4) & 0x0F ) {
        case 0x00: setD(tmp2); break;
        case 0x01: X = tmp2; break;
        case 0x02: Y = tmp2; break;
        case 0x03: U = tmp2; break;
        case 0x04: S = tmp2; break;
        case 0x05: PC = tmp2; break;
        case 0x08: A = tmp2; break;
        case 0x09: B = tmp2; break;
        case 0x0a: CC = tmp2; break;
        case 0x0b: DP = tmp2; break;
    }

    // Set dest = old source
    switch( t & 0x0F ) {
        case 0x00: setD(tmp1); break;
        case 0x01: X = tmp1; break;
        case 0x02: Y = tmp1; break;
        case 0x03: U = tmp1; break;
        case 0x04: S = tmp1; break;
        case 0x05: PC = tmp1; break;
        case 0x08: A = tmp1; break;
        case 0x09: B = tmp1; break;
        case 0x0a: CC = tmp1; break;
        case 0x0b: DP = tmp1; break;
    }
}

void M6809::tfr()
{
    unsigned char t = imm8();
    unsigned tmp;

    switch( (t >> 4) & 0x0F ) {
        case 0x00: tmp = D(); break;
        case 0x01: tmp = X; break;
        case 0x02: tmp = Y; break;
        case 0x03: tmp = U; break;
        case 0x04: tmp = S; break;
        case 0x05: tmp = PC; break;
        case 0x08: tmp = A; break;
        case 0x09: tmp = B; break;
        case 0x0a: tmp = CC; break;
        case 0x0b: tmp = DP; break;
        default:   tmp = 0xFF; break;
    }

    switch( t & 0x0F ) {
        case 0x00: setD(tmp); break;
        case 0x01: X = tmp; break;
        case 0x02: Y = tmp; break;
        case 0x03: U = tmp; break;
        case 0x04: S = tmp; break;
        case 0x05: PC = tmp; break;
        case 0x08: A = tmp; break;
        case 0x09: B = tmp; break;
        case 0x0a: CC = tmp; break;
        case 0x0b: DP = tmp; break;
    }
}

// --- 8-bit ALU ops on A ---
void M6809::suba_im() { unsigned t=imm8(); unsigned r=A-t; clr_nzvc(); set_flags8(A,t,r); A=r; }
void M6809::suba_di() { direct(); unsigned t=rm(ea_); unsigned r=A-t; clr_nzvc(); set_flags8(A,t,r); A=r; }
void M6809::suba_ix() { indexed(); unsigned t=rm(ea_); unsigned r=A-t; clr_nzvc(); set_flags8(A,t,r); A=r; }
void M6809::suba_ex() { extended(); unsigned t=rm(ea_); unsigned r=A-t; clr_nzvc(); set_flags8(A,t,r); A=r; }

void M6809::cmpa_im() { unsigned t=imm8(); unsigned r=A-t; clr_nzvc(); set_flags8(A,t,r); }
void M6809::cmpa_di() { direct(); unsigned t=rm(ea_); unsigned r=A-t; clr_nzvc(); set_flags8(A,t,r); }
void M6809::cmpa_ix() { indexed(); unsigned t=rm(ea_); unsigned r=A-t; clr_nzvc(); set_flags8(A,t,r); }
void M6809::cmpa_ex() { extended(); unsigned t=rm(ea_); unsigned r=A-t; clr_nzvc(); set_flags8(A,t,r); }

void M6809::sbca_im() { unsigned t=imm8(); unsigned r=A-t-(CC&FlagC); clr_nzvc(); set_flags8(A,t,r); A=r; }
void M6809::sbca_di() { direct(); unsigned t=rm(ea_); unsigned r=A-t-(CC&FlagC); clr_nzvc(); set_flags8(A,t,r); A=r; }
void M6809::sbca_ix() { indexed(); unsigned t=rm(ea_); unsigned r=A-t-(CC&FlagC); clr_nzvc(); set_flags8(A,t,r); A=r; }
void M6809::sbca_ex() { extended(); unsigned t=rm(ea_); unsigned r=A-t-(CC&FlagC); clr_nzvc(); set_flags8(A,t,r); A=r; }

void M6809::anda_im() { A&=imm8(); clr_nzv(); set_nz8(A); }
void M6809::anda_di() { direct(); A&=rm(ea_); clr_nzv(); set_nz8(A); }
void M6809::anda_ix() { indexed(); A&=rm(ea_); clr_nzv(); set_nz8(A); }
void M6809::anda_ex() { extended(); A&=rm(ea_); clr_nzv(); set_nz8(A); }

void M6809::bita_im() { unsigned r=A&imm8(); clr_nzv(); set_nz8(r); }
void M6809::bita_di() { direct(); unsigned r=A&rm(ea_); clr_nzv(); set_nz8(r); }
void M6809::bita_ix() { indexed(); unsigned r=A&rm(ea_); clr_nzv(); set_nz8(r); }
void M6809::bita_ex() { extended(); unsigned r=A&rm(ea_); clr_nzv(); set_nz8(r); }

void M6809::lda_im()  { A=imm8(); clr_nzv(); set_nz8(A); }
void M6809::lda_di()  { direct(); A=rm(ea_); clr_nzv(); set_nz8(A); }
void M6809::lda_ix()  { indexed(); A=rm(ea_); clr_nzv(); set_nz8(A); }
void M6809::lda_ex()  { extended(); A=rm(ea_); clr_nzv(); set_nz8(A); }

void M6809::sta_im()  { clr_nzv(); set_nz8(A); imm8(); /* consume the byte but store is to EA=PC-1 which is a NOP for immediate */ }
void M6809::sta_di()  { direct(); clr_nzv(); set_nz8(A); wm(ea_,A); }
void M6809::sta_ix()  { indexed(); clr_nzv(); set_nz8(A); wm(ea_,A); }
void M6809::sta_ex()  { extended(); clr_nzv(); set_nz8(A); wm(ea_,A); }

void M6809::eora_im() { A^=imm8(); clr_nzv(); set_nz8(A); }
void M6809::eora_di() { direct(); A^=rm(ea_); clr_nzv(); set_nz8(A); }
void M6809::eora_ix() { indexed(); A^=rm(ea_); clr_nzv(); set_nz8(A); }
void M6809::eora_ex() { extended(); A^=rm(ea_); clr_nzv(); set_nz8(A); }

void M6809::adca_im() { unsigned t=imm8(); unsigned r=A+t+(CC&FlagC); clr_hnzvc(); set_h(A,t,r); set_flags8(A,t,r); A=r; }
void M6809::adca_di() { direct(); unsigned t=rm(ea_); unsigned r=A+t+(CC&FlagC); clr_hnzvc(); set_h(A,t,r); set_flags8(A,t,r); A=r; }
void M6809::adca_ix() { indexed(); unsigned t=rm(ea_); unsigned r=A+t+(CC&FlagC); clr_hnzvc(); set_h(A,t,r); set_flags8(A,t,r); A=r; }
void M6809::adca_ex() { extended(); unsigned t=rm(ea_); unsigned r=A+t+(CC&FlagC); clr_hnzvc(); set_h(A,t,r); set_flags8(A,t,r); A=r; }

void M6809::ora_im()  { A|=imm8(); clr_nzv(); set_nz8(A); }
void M6809::ora_di()  { direct(); A|=rm(ea_); clr_nzv(); set_nz8(A); }
void M6809::ora_ix()  { indexed(); A|=rm(ea_); clr_nzv(); set_nz8(A); }
void M6809::ora_ex()  { extended(); A|=rm(ea_); clr_nzv(); set_nz8(A); }

void M6809::adda_im() { unsigned t=imm8(); unsigned r=A+t; clr_hnzvc(); set_h(A,t,r); set_flags8(A,t,r); A=r; }
void M6809::adda_di() { direct(); unsigned t=rm(ea_); unsigned r=A+t; clr_hnzvc(); set_h(A,t,r); set_flags8(A,t,r); A=r; }
void M6809::adda_ix() { indexed(); unsigned t=rm(ea_); unsigned r=A+t; clr_hnzvc(); set_h(A,t,r); set_flags8(A,t,r); A=r; }
void M6809::adda_ex() { extended(); unsigned t=rm(ea_); unsigned r=A+t; clr_hnzvc(); set_h(A,t,r); set_flags8(A,t,r); A=r; }

// --- 16-bit SUB/CMP on D ---
void M6809::subd_im() { unsigned t=imm16(); unsigned d=D(); unsigned r=d-t; clr_nzvc(); set_flags16(d,t,r); setD(r); }
void M6809::subd_di() { direct(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d-t; clr_nzvc(); set_flags16(d,t,r); setD(r); }
void M6809::subd_ix() { indexed(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d-t; clr_nzvc(); set_flags16(d,t,r); setD(r); }
void M6809::subd_ex() { extended(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d-t; clr_nzvc(); set_flags16(d,t,r); setD(r); }

// --- CMP X ---
void M6809::cmpx_im() { unsigned t=imm16(); unsigned r=X-t; clr_nzvc(); set_flags16(X,t,r); }
void M6809::cmpx_di() { direct(); unsigned t=rm16(ea_); unsigned r=X-t; clr_nzvc(); set_flags16(X,t,r); }
void M6809::cmpx_ix() { indexed(); unsigned t=rm16(ea_); unsigned r=X-t; clr_nzvc(); set_flags16(X,t,r); }
void M6809::cmpx_ex() { extended(); unsigned t=rm16(ea_); unsigned r=X-t; clr_nzvc(); set_flags16(X,t,r); }

// --- JSR ---
void M6809::jsr_di()  { direct();   pushWordS(PC); PC=ea_; }
void M6809::jsr_ix()  { indexed();  pushWordS(PC); PC=ea_; }
void M6809::jsr_ex()  { extended(); pushWordS(PC); PC=ea_; }

// --- LDX / STX ---
void M6809::ldx_im() { X=imm16(); clr_nzv(); set_nz16(X); }
void M6809::ldx_di() { direct(); X=rm16(ea_); clr_nzv(); set_nz16(X); }
void M6809::ldx_ix() { indexed(); X=rm16(ea_); clr_nzv(); set_nz16(X); }
void M6809::ldx_ex() { extended(); X=rm16(ea_); clr_nzv(); set_nz16(X); }
void M6809::stx_im() { clr_nzv(); set_nz16(X); imm16(); }
void M6809::stx_di() { direct(); clr_nzv(); set_nz16(X); wm16(ea_,X); }
void M6809::stx_ix() { indexed(); clr_nzv(); set_nz16(X); wm16(ea_,X); }
void M6809::stx_ex() { extended(); clr_nzv(); set_nz16(X); wm16(ea_,X); }

// --- 8-bit ALU ops on B ---
void M6809::subb_im() { unsigned t=imm8(); unsigned r=B-t; clr_nzvc(); set_flags8(B,t,r); B=r; }
void M6809::subb_di() { direct(); unsigned t=rm(ea_); unsigned r=B-t; clr_nzvc(); set_flags8(B,t,r); B=r; }
void M6809::subb_ix() { indexed(); unsigned t=rm(ea_); unsigned r=B-t; clr_nzvc(); set_flags8(B,t,r); B=r; }
void M6809::subb_ex() { extended(); unsigned t=rm(ea_); unsigned r=B-t; clr_nzvc(); set_flags8(B,t,r); B=r; }

void M6809::cmpb_im() { unsigned t=imm8(); unsigned r=B-t; clr_nzvc(); set_flags8(B,t,r); }
void M6809::cmpb_di() { direct(); unsigned t=rm(ea_); unsigned r=B-t; clr_nzvc(); set_flags8(B,t,r); }
void M6809::cmpb_ix() { indexed(); unsigned t=rm(ea_); unsigned r=B-t; clr_nzvc(); set_flags8(B,t,r); }
void M6809::cmpb_ex() { extended(); unsigned t=rm(ea_); unsigned r=B-t; clr_nzvc(); set_flags8(B,t,r); }

void M6809::sbcb_im() { unsigned t=imm8(); unsigned r=B-t-(CC&FlagC); clr_nzvc(); set_flags8(B,t,r); B=r; }
void M6809::sbcb_di() { direct(); unsigned t=rm(ea_); unsigned r=B-t-(CC&FlagC); clr_nzvc(); set_flags8(B,t,r); B=r; }
void M6809::sbcb_ix() { indexed(); unsigned t=rm(ea_); unsigned r=B-t-(CC&FlagC); clr_nzvc(); set_flags8(B,t,r); B=r; }
void M6809::sbcb_ex() { extended(); unsigned t=rm(ea_); unsigned r=B-t-(CC&FlagC); clr_nzvc(); set_flags8(B,t,r); B=r; }

void M6809::andb_im() { B&=imm8(); clr_nzv(); set_nz8(B); }
void M6809::andb_di() { direct(); B&=rm(ea_); clr_nzv(); set_nz8(B); }
void M6809::andb_ix() { indexed(); B&=rm(ea_); clr_nzv(); set_nz8(B); }
void M6809::andb_ex() { extended(); B&=rm(ea_); clr_nzv(); set_nz8(B); }

void M6809::bitb_im() { unsigned r=B&imm8(); clr_nzv(); set_nz8(r); }
void M6809::bitb_di() { direct(); unsigned r=B&rm(ea_); clr_nzv(); set_nz8(r); }
void M6809::bitb_ix() { indexed(); unsigned r=B&rm(ea_); clr_nzv(); set_nz8(r); }
void M6809::bitb_ex() { extended(); unsigned r=B&rm(ea_); clr_nzv(); set_nz8(r); }

void M6809::ldb_im()  { B=imm8(); clr_nzv(); set_nz8(B); }
void M6809::ldb_di()  { direct(); B=rm(ea_); clr_nzv(); set_nz8(B); }
void M6809::ldb_ix()  { indexed(); B=rm(ea_); clr_nzv(); set_nz8(B); }
void M6809::ldb_ex()  { extended(); B=rm(ea_); clr_nzv(); set_nz8(B); }

void M6809::stb_im()  { clr_nzv(); set_nz8(B); imm8(); }
void M6809::stb_di()  { direct(); clr_nzv(); set_nz8(B); wm(ea_,B); }
void M6809::stb_ix()  { indexed(); clr_nzv(); set_nz8(B); wm(ea_,B); }
void M6809::stb_ex()  { extended(); clr_nzv(); set_nz8(B); wm(ea_,B); }

void M6809::eorb_im() { B^=imm8(); clr_nzv(); set_nz8(B); }
void M6809::eorb_di() { direct(); B^=rm(ea_); clr_nzv(); set_nz8(B); }
void M6809::eorb_ix() { indexed(); B^=rm(ea_); clr_nzv(); set_nz8(B); }
void M6809::eorb_ex() { extended(); B^=rm(ea_); clr_nzv(); set_nz8(B); }

void M6809::adcb_im() { unsigned t=imm8(); unsigned r=B+t+(CC&FlagC); clr_hnzvc(); set_h(B,t,r); set_flags8(B,t,r); B=r; }
void M6809::adcb_di() { direct(); unsigned t=rm(ea_); unsigned r=B+t+(CC&FlagC); clr_hnzvc(); set_h(B,t,r); set_flags8(B,t,r); B=r; }
void M6809::adcb_ix() { indexed(); unsigned t=rm(ea_); unsigned r=B+t+(CC&FlagC); clr_hnzvc(); set_h(B,t,r); set_flags8(B,t,r); B=r; }
void M6809::adcb_ex() { extended(); unsigned t=rm(ea_); unsigned r=B+t+(CC&FlagC); clr_hnzvc(); set_h(B,t,r); set_flags8(B,t,r); B=r; }

void M6809::orb_im()  { B|=imm8(); clr_nzv(); set_nz8(B); }
void M6809::orb_di()  { direct(); B|=rm(ea_); clr_nzv(); set_nz8(B); }
void M6809::orb_ix()  { indexed(); B|=rm(ea_); clr_nzv(); set_nz8(B); }
void M6809::orb_ex()  { extended(); B|=rm(ea_); clr_nzv(); set_nz8(B); }

void M6809::addb_im() { unsigned t=imm8(); unsigned r=B+t; clr_hnzvc(); set_h(B,t,r); set_flags8(B,t,r); B=r; }
void M6809::addb_di() { direct(); unsigned t=rm(ea_); unsigned r=B+t; clr_hnzvc(); set_h(B,t,r); set_flags8(B,t,r); B=r; }
void M6809::addb_ix() { indexed(); unsigned t=rm(ea_); unsigned r=B+t; clr_hnzvc(); set_h(B,t,r); set_flags8(B,t,r); B=r; }
void M6809::addb_ex() { extended(); unsigned t=rm(ea_); unsigned r=B+t; clr_hnzvc(); set_h(B,t,r); set_flags8(B,t,r); B=r; }

// --- 16-bit ADD D ---
void M6809::addd_im() { unsigned t=imm16(); unsigned d=D(); unsigned r=d+t; clr_nzvc(); set_flags16(d,t,r); setD(r); }
void M6809::addd_di() { direct(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d+t; clr_nzvc(); set_flags16(d,t,r); setD(r); }
void M6809::addd_ix() { indexed(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d+t; clr_nzvc(); set_flags16(d,t,r); setD(r); }
void M6809::addd_ex() { extended(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d+t; clr_nzvc(); set_flags16(d,t,r); setD(r); }

// --- LDD / STD ---
void M6809::ldd_im() { unsigned d=imm16(); setD(d); clr_nzv(); set_nz16(d); }
void M6809::ldd_di() { direct(); unsigned d=rm16(ea_); setD(d); clr_nzv(); set_nz16(d); }
void M6809::ldd_ix() { indexed(); unsigned d=rm16(ea_); setD(d); clr_nzv(); set_nz16(d); }
void M6809::ldd_ex() { extended(); unsigned d=rm16(ea_); setD(d); clr_nzv(); set_nz16(d); }
void M6809::std_im() { clr_nzv(); set_nz16(D()); imm16(); }
void M6809::std_di() { direct(); clr_nzv(); set_nz16(D()); wm16(ea_,D()); }
void M6809::std_ix() { indexed(); clr_nzv(); set_nz16(D()); wm16(ea_,D()); }
void M6809::std_ex() { extended(); clr_nzv(); set_nz16(D()); wm16(ea_,D()); }

// --- LDU / STU ---
void M6809::ldu_im() { U=imm16(); clr_nzv(); set_nz16(U); }
void M6809::ldu_di() { direct(); U=rm16(ea_); clr_nzv(); set_nz16(U); }
void M6809::ldu_ix() { indexed(); U=rm16(ea_); clr_nzv(); set_nz16(U); }
void M6809::ldu_ex() { extended(); U=rm16(ea_); clr_nzv(); set_nz16(U); }
void M6809::stu_im() { clr_nzv(); set_nz16(U); imm16(); }
void M6809::stu_di() { direct(); clr_nzv(); set_nz16(U); wm16(ea_,U); }
void M6809::stu_ix() { indexed(); clr_nzv(); set_nz16(U); wm16(ea_,U); }
void M6809::stu_ex() { extended(); clr_nzv(); set_nz16(U); wm16(ea_,U); }

// --- Page 2: CMP D ---
void M6809::cmpd_im() { unsigned t=imm16(); unsigned d=D(); unsigned r=d-t; clr_nzvc(); set_flags16(d,t,r); }
void M6809::cmpd_di() { direct(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d-t; clr_nzvc(); set_flags16(d,t,r); }
void M6809::cmpd_ix() { indexed(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d-t; clr_nzvc(); set_flags16(d,t,r); }
void M6809::cmpd_ex() { extended(); unsigned t=rm16(ea_); unsigned d=D(); unsigned r=d-t; clr_nzvc(); set_flags16(d,t,r); }

// --- Page 2: CMP Y ---
void M6809::cmpy_im() { unsigned t=imm16(); unsigned r=Y-t; clr_nzvc(); set_flags16(Y,t,r); }
void M6809::cmpy_di() { direct(); unsigned t=rm16(ea_); unsigned r=Y-t; clr_nzvc(); set_flags16(Y,t,r); }
void M6809::cmpy_ix() { indexed(); unsigned t=rm16(ea_); unsigned r=Y-t; clr_nzvc(); set_flags16(Y,t,r); }
void M6809::cmpy_ex() { extended(); unsigned t=rm16(ea_); unsigned r=Y-t; clr_nzvc(); set_flags16(Y,t,r); }

// --- Page 2: LDY / STY ---
void M6809::ldy_im() { Y=imm16(); clr_nzv(); set_nz16(Y); }
void M6809::ldy_di() { direct(); Y=rm16(ea_); clr_nzv(); set_nz16(Y); }
void M6809::ldy_ix() { indexed(); Y=rm16(ea_); clr_nzv(); set_nz16(Y); }
void M6809::ldy_ex() { extended(); Y=rm16(ea_); clr_nzv(); set_nz16(Y); }
void M6809::sty_im() { clr_nzv(); set_nz16(Y); imm16(); }
void M6809::sty_di() { direct(); clr_nzv(); set_nz16(Y); wm16(ea_,Y); }
void M6809::sty_ix() { indexed(); clr_nzv(); set_nz16(Y); wm16(ea_,Y); }
void M6809::sty_ex() { extended(); clr_nzv(); set_nz16(Y); wm16(ea_,Y); }

// --- Page 2: LDS / STS ---
void M6809::lds_im() { S=imm16(); clr_nzv(); set_nz16(S); int_state_ |= StateLDS; }
void M6809::lds_di() { direct(); S=rm16(ea_); clr_nzv(); set_nz16(S); int_state_ |= StateLDS; }
void M6809::lds_ix() { indexed(); S=rm16(ea_); clr_nzv(); set_nz16(S); int_state_ |= StateLDS; }
void M6809::lds_ex() { extended(); S=rm16(ea_); clr_nzv(); set_nz16(S); int_state_ |= StateLDS; }
void M6809::sts_im() { clr_nzv(); set_nz16(S); imm16(); }
void M6809::sts_di() { direct(); clr_nzv(); set_nz16(S); wm16(ea_,S); }
void M6809::sts_ix() { indexed(); clr_nzv(); set_nz16(S); wm16(ea_,S); }
void M6809::sts_ex() { extended(); clr_nzv(); set_nz16(S); wm16(ea_,S); }

// --- Page 3: CMP U ---
void M6809::cmpu_im() { unsigned t=imm16(); unsigned r=U-t; clr_nzvc(); set_flags16(U,t,r); }
void M6809::cmpu_di() { direct(); unsigned t=rm16(ea_); unsigned r=U-t; clr_nzvc(); set_flags16(U,t,r); }
void M6809::cmpu_ix() { indexed(); unsigned t=rm16(ea_); unsigned r=U-t; clr_nzvc(); set_flags16(U,t,r); }
void M6809::cmpu_ex() { extended(); unsigned t=rm16(ea_); unsigned r=U-t; clr_nzvc(); set_flags16(U,t,r); }

// --- Page 3: CMP S ---
void M6809::cmps_im() { unsigned t=imm16(); unsigned r=S-t; clr_nzvc(); set_flags16(S,t,r); }
void M6809::cmps_di() { direct(); unsigned t=rm16(ea_); unsigned r=S-t; clr_nzvc(); set_flags16(S,t,r); }
void M6809::cmps_ix() { indexed(); unsigned t=rm16(ea_); unsigned r=S-t; clr_nzvc(); set_flags16(S,t,r); }
void M6809::cmps_ex() { extended(); unsigned t=rm16(ea_); unsigned r=S-t; clr_nzvc(); set_flags16(S,t,r); }
