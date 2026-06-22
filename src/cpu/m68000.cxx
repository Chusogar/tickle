/*
    M68000 emulator

    Portable Motorola 68000 CPU emulator for tickle.
    Implements the base M68000 instruction set.
*/
#include "m68000.h"

#define SIGN8(x)  ((int)(signed char)(x))
#define SIGN16(x) ((int)(short)(x))

M68000::M68000( M68000Environment & env ) : env_(env)
{
    cycles_ = 0;
    icount_ = 0;
    irq_level_ = 0;
    stopped_ = false;
    halted_ = false;
    for( int i = 0; i < 8; i++ ) { D[i] = 0; A[i] = 0; }
    PC = 0; SR = 0x2700; USP = 0; SSP = 0;
}

void M68000::setSR( unsigned val )
{
    unsigned old = SR;
    SR = val & 0xFFFF;

    // Handle S bit change: swap stack pointers
    if( (old ^ SR) & FlagS ) {
        if( SR & FlagS ) {
            // Entering supervisor: save USP, load SSP
            USP = A[7];
            A[7] = SSP;
        } else {
            // Entering user: save SSP, load USP
            SSP = A[7];
            A[7] = USP;
        }
    }
}

void M68000::reset()
{
    SR = 0x2700;  // Supervisor mode, all interrupts masked
    SSP = env_.readLong( 0 );
    A[7] = SSP;
    PC  = env_.readLong( 4 );
    stopped_ = false;
    halted_ = false;
    irq_level_ = 0;
    cycles_ = 0;
}

void M68000::interrupt( int level )
{
    irq_level_ = level;
}

void M68000::setIRQLine( int level )
{
    irq_level_ = level;
}

// ---- Memory access ----

unsigned char M68000::rm8( unsigned addr )  { return env_.readByte( addr & 0xFFFFFF ); }
void M68000::wm8( unsigned addr, unsigned char val ) { env_.writeByte( addr & 0xFFFFFF, val ); }
unsigned M68000::rm16( unsigned addr )      { return env_.readWord( addr & 0xFFFFFF ); }
void M68000::wm16( unsigned addr, unsigned val )     { env_.writeWord( addr & 0xFFFFFF, val ); }
unsigned M68000::rm32( unsigned addr )      { return env_.readLong( addr & 0xFFFFFF ); }
void M68000::wm32( unsigned addr, unsigned val )     { env_.writeLong( addr & 0xFFFFFF, val ); }

unsigned M68000::fetch16()
{
    unsigned val = rm16( PC );
    PC += 2;
    return val;
}

unsigned M68000::fetch32()
{
    unsigned val = rm32( PC );
    PC += 4;
    return val;
}

// ---- Stack ----

void M68000::pushWord( unsigned val ) { A[7] -= 2; wm16( A[7], val ); }
void M68000::pushLong( unsigned val ) { A[7] -= 4; wm32( A[7], val ); }
unsigned M68000::popWord()  { unsigned v = rm16( A[7] ); A[7] += 2; return v; }
unsigned M68000::popLong()  { unsigned v = rm32( A[7] ); A[7] += 4; return v; }

// ---- Effective Address ----

unsigned M68000::computeEA( int mode, int reg, int size )
{
    switch( mode ) {
        case 0: // Dn (not address)
            return 0;
        case 1: // An (not address)
            return 0;
        case 2: // (An)
            return A[reg];
        case 3: // (An)+
        {
            unsigned ea = A[reg];
            A[reg] += (size == 1 && reg == 7) ? 2 : size;
            return ea;
        }
        case 4: // -(An)
        {
            A[reg] -= (size == 1 && reg == 7) ? 2 : size;
            return A[reg];
        }
        case 5: // d16(An)
        {
            int d16 = SIGN16( fetch16() );
            return A[reg] + d16;
        }
        case 6: // d8(An,Xn)
        {
            unsigned ext = fetch16();
            int d8 = SIGN8( ext & 0xFF );
            int xn = (ext >> 12) & 7;
            unsigned xval = (ext & 0x8000) ? A[xn] : D[xn];
            if( !(ext & 0x0800) ) xval = SIGN16( xval & 0xFFFF );
            return A[reg] + d8 + xval;
        }
        case 7:
            switch( reg ) {
                case 0: // abs.W
                    return (unsigned)(int)(short)fetch16();
                case 1: // abs.L
                    return fetch32();
                case 2: // d16(PC)
                {
                    unsigned base = PC;
                    int d16 = SIGN16( fetch16() );
                    return base + d16;
                }
                case 3: // d8(PC,Xn)
                {
                    unsigned base = PC;
                    unsigned ext = fetch16();
                    int d8 = SIGN8( ext & 0xFF );
                    int xn = (ext >> 12) & 7;
                    unsigned xval = (ext & 0x8000) ? A[xn] : D[xn];
                    if( !(ext & 0x0800) ) xval = SIGN16( xval & 0xFFFF );
                    return base + d8 + xval;
                }
                case 4: // #imm (not address, handled separately)
                    return 0;
            }
            break;
    }
    return 0;
}

unsigned M68000::readEA( int mode, int reg, int size )
{
    switch( mode ) {
        case 0: // Dn
            if( size == 1 ) return D[reg] & 0xFF;
            if( size == 2 ) return D[reg] & 0xFFFF;
            return D[reg];
        case 1: // An
            if( size == 2 ) return A[reg] & 0xFFFF;
            return A[reg];
        case 7:
            if( reg == 4 ) { // #imm
                if( size == 1 ) return fetch16() & 0xFF;
                if( size == 2 ) return fetch16();
                return fetch32();
            }
            // fall through to memory read
        default:
        {
            unsigned ea = computeEA( mode, reg, size );
            if( size == 1 ) return rm8( ea );
            if( size == 2 ) return rm16( ea );
            return rm32( ea );
        }
    }
}

void M68000::writeEA( int mode, int reg, int size, unsigned val )
{
    switch( mode ) {
        case 0: // Dn
            if( size == 1 ) { D[reg] = (D[reg] & 0xFFFFFF00) | (val & 0xFF); return; }
            if( size == 2 ) { D[reg] = (D[reg] & 0xFFFF0000) | (val & 0xFFFF); return; }
            D[reg] = val; return;
        case 1: // An
            if( size == 2 ) { A[reg] = SIGN16(val & 0xFFFF); return; }
            A[reg] = val; return;
        default:
        {
            unsigned ea = computeEA( mode, reg, size );
            if( size == 1 ) wm8( ea, val );
            else if( size == 2 ) wm16( ea, val );
            else wm32( ea, val );
        }
    }
}

// ---- Flag helpers ----

void M68000::setFlagNZ_B( unsigned val )
{
    SR &= ~(FlagN|FlagZ|FlagV|FlagC);
    if( (val & 0xFF) == 0 ) SR |= FlagZ;
    if( val & 0x80 ) SR |= FlagN;
}

void M68000::setFlagNZ_W( unsigned val )
{
    SR &= ~(FlagN|FlagZ|FlagV|FlagC);
    if( (val & 0xFFFF) == 0 ) SR |= FlagZ;
    if( val & 0x8000 ) SR |= FlagN;
}

void M68000::setFlagNZ_L( unsigned val )
{
    SR &= ~(FlagN|FlagZ|FlagV|FlagC);
    if( val == 0 ) SR |= FlagZ;
    if( val & 0x80000000 ) SR |= FlagN;
}

// ---- ALU operations ----

unsigned M68000::add_b( unsigned s, unsigned d )
{
    unsigned r = (d & 0xFF) + (s & 0xFF);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    if( (r & 0xFF) == 0 ) SR |= FlagZ;
    if( r & 0x80 ) SR |= FlagN;
    if( r & 0x100 ) SR |= (FlagC|FlagX);
    if( ((s^r) & (d^r) & 0x80) ) SR |= FlagV;
    return r & 0xFF;
}

unsigned M68000::add_w( unsigned s, unsigned d )
{
    unsigned r = (d & 0xFFFF) + (s & 0xFFFF);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    if( (r & 0xFFFF) == 0 ) SR |= FlagZ;
    if( r & 0x8000 ) SR |= FlagN;
    if( r & 0x10000 ) SR |= (FlagC|FlagX);
    if( ((s^r) & (d^r) & 0x8000) ) SR |= FlagV;
    return r & 0xFFFF;
}

unsigned M68000::add_l( unsigned s, unsigned d )
{
    unsigned long long r = (unsigned long long)(d) + (unsigned long long)(s);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    unsigned result = (unsigned)r;
    if( result == 0 ) SR |= FlagZ;
    if( result & 0x80000000 ) SR |= FlagN;
    if( r & 0x100000000ULL ) SR |= (FlagC|FlagX);
    if( ((s^result) & (d^result) & 0x80000000) ) SR |= FlagV;
    return result;
}

unsigned M68000::sub_b( unsigned s, unsigned d )
{
    unsigned r = (d & 0xFF) - (s & 0xFF);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    if( (r & 0xFF) == 0 ) SR |= FlagZ;
    if( r & 0x80 ) SR |= FlagN;
    if( r & 0x100 ) SR |= (FlagC|FlagX);
    if( ((d^s) & (d^r) & 0x80) ) SR |= FlagV;
    return r & 0xFF;
}

unsigned M68000::sub_w( unsigned s, unsigned d )
{
    unsigned r = (d & 0xFFFF) - (s & 0xFFFF);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    if( (r & 0xFFFF) == 0 ) SR |= FlagZ;
    if( r & 0x8000 ) SR |= FlagN;
    if( r & 0x10000 ) SR |= (FlagC|FlagX);
    if( ((d^s) & (d^r) & 0x8000) ) SR |= FlagV;
    return r & 0xFFFF;
}

unsigned M68000::sub_l( unsigned s, unsigned d )
{
    unsigned long long r = (unsigned long long)(d) - (unsigned long long)(s);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    unsigned result = (unsigned)r;
    if( result == 0 ) SR |= FlagZ;
    if( result & 0x80000000 ) SR |= FlagN;
    if( r & 0x100000000ULL ) SR |= (FlagC|FlagX);
    if( ((d^s) & (d^result) & 0x80000000) ) SR |= FlagV;
    return result;
}

unsigned M68000::addx_b( unsigned s, unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned r = (d & 0xFF) + (s & 0xFF) + x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    // Z flag is only cleared, never set
    if( (r & 0xFF) != 0 ) SR &= ~FlagZ;
    if( r & 0x80 ) SR |= FlagN;
    if( r & 0x100 ) SR |= (FlagC|FlagX);
    if( ((s^r) & (d^r) & 0x80) ) SR |= FlagV;
    return r & 0xFF;
}

unsigned M68000::addx_w( unsigned s, unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned r = (d & 0xFFFF) + (s & 0xFFFF) + x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    if( (r & 0xFFFF) != 0 ) SR &= ~FlagZ;
    if( r & 0x8000 ) SR |= FlagN;
    if( r & 0x10000 ) SR |= (FlagC|FlagX);
    if( ((s^r) & (d^r) & 0x8000) ) SR |= FlagV;
    return r & 0xFFFF;
}

unsigned M68000::addx_l( unsigned s, unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned long long r = (unsigned long long)(d) + (unsigned long long)(s) + x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    unsigned result = (unsigned)r;
    if( result != 0 ) SR &= ~FlagZ;
    if( result & 0x80000000 ) SR |= FlagN;
    if( r & 0x100000000ULL ) SR |= (FlagC|FlagX);
    if( ((s^result) & (d^result) & 0x80000000) ) SR |= FlagV;
    return result;
}

unsigned M68000::subx_b( unsigned s, unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned r = (d & 0xFF) - (s & 0xFF) - x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    if( (r & 0xFF) != 0 ) SR &= ~FlagZ;
    if( r & 0x80 ) SR |= FlagN;
    if( r & 0x100 ) SR |= (FlagC|FlagX);
    if( ((d^s) & (d^r) & 0x80) ) SR |= FlagV;
    return r & 0xFF;
}

unsigned M68000::subx_w( unsigned s, unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned r = (d & 0xFFFF) - (s & 0xFFFF) - x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    if( (r & 0xFFFF) != 0 ) SR &= ~FlagZ;
    if( r & 0x8000 ) SR |= FlagN;
    if( r & 0x10000 ) SR |= (FlagC|FlagX);
    if( ((d^s) & (d^r) & 0x8000) ) SR |= FlagV;
    return r & 0xFFFF;
}

unsigned M68000::subx_l( unsigned s, unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned long long r = (unsigned long long)(d) - (unsigned long long)(s) - x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    unsigned result = (unsigned)r;
    if( result != 0 ) SR &= ~FlagZ;
    if( result & 0x80000000 ) SR |= FlagN;
    if( r & 0x100000000ULL ) SR |= (FlagC|FlagX);
    if( ((d^s) & (d^result) & 0x80000000) ) SR |= FlagV;
    return result;
}

void M68000::cmp_b( unsigned s, unsigned d )
{
    unsigned r = (d & 0xFF) - (s & 0xFF);
    SR &= ~(FlagN|FlagZ|FlagV|FlagC);
    if( (r & 0xFF) == 0 ) SR |= FlagZ;
    if( r & 0x80 ) SR |= FlagN;
    if( r & 0x100 ) SR |= FlagC;
    if( ((d^s) & (d^r) & 0x80) ) SR |= FlagV;
}

void M68000::cmp_w( unsigned s, unsigned d )
{
    unsigned r = (d & 0xFFFF) - (s & 0xFFFF);
    SR &= ~(FlagN|FlagZ|FlagV|FlagC);
    if( (r & 0xFFFF) == 0 ) SR |= FlagZ;
    if( r & 0x8000 ) SR |= FlagN;
    if( r & 0x10000 ) SR |= FlagC;
    if( ((d^s) & (d^r) & 0x8000) ) SR |= FlagV;
}

void M68000::cmp_l( unsigned s, unsigned d )
{
    unsigned long long r = (unsigned long long)(d) - (unsigned long long)(s);
    SR &= ~(FlagN|FlagZ|FlagV|FlagC);
    unsigned result = (unsigned)r;
    if( result == 0 ) SR |= FlagZ;
    if( result & 0x80000000 ) SR |= FlagN;
    if( r & 0x100000000ULL ) SR |= FlagC;
    if( ((d^s) & (d^result) & 0x80000000) ) SR |= FlagV;
}

unsigned M68000::neg_b( unsigned d )
{
    unsigned r = 0 - (d & 0xFF);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    if( (r & 0xFF) == 0 ) SR |= FlagZ;
    else SR |= (FlagC|FlagX);
    if( r & 0x80 ) SR |= FlagN;
    if( (d & r & 0x80) ) SR |= FlagV;
    return r & 0xFF;
}

unsigned M68000::neg_w( unsigned d )
{
    unsigned r = 0 - (d & 0xFFFF);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    if( (r & 0xFFFF) == 0 ) SR |= FlagZ;
    else SR |= (FlagC|FlagX);
    if( r & 0x8000 ) SR |= FlagN;
    if( (d & r & 0x8000) ) SR |= FlagV;
    return r & 0xFFFF;
}

unsigned M68000::neg_l( unsigned d )
{
    unsigned long long r = 0ULL - (unsigned long long)(d);
    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
    unsigned result = (unsigned)r;
    if( result == 0 ) SR |= FlagZ;
    else SR |= (FlagC|FlagX);
    if( result & 0x80000000 ) SR |= FlagN;
    if( (d & result & 0x80000000) ) SR |= FlagV;
    return result;
}

unsigned M68000::negx_b( unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned r = 0 - (d & 0xFF) - x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    if( (r & 0xFF) != 0 ) SR &= ~FlagZ;
    if( r & 0x80 ) SR |= FlagN;
    if( r & 0x100 ) SR |= (FlagC|FlagX);
    if( (d & r & 0x80) ) SR |= FlagV;
    return r & 0xFF;
}

unsigned M68000::negx_w( unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned r = 0 - (d & 0xFFFF) - x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    if( (r & 0xFFFF) != 0 ) SR &= ~FlagZ;
    if( r & 0x8000 ) SR |= FlagN;
    if( r & 0x10000 ) SR |= (FlagC|FlagX);
    if( (d & r & 0x8000) ) SR |= FlagV;
    return r & 0xFFFF;
}

unsigned M68000::negx_l( unsigned d )
{
    unsigned x = (SR & FlagX) ? 1 : 0;
    unsigned long long r = 0ULL - (unsigned long long)(d) - x;
    SR &= ~(FlagN|FlagV|FlagC|FlagX);
    unsigned result = (unsigned)r;
    if( result != 0 ) SR &= ~FlagZ;
    if( result & 0x80000000 ) SR |= FlagN;
    if( r & 0x100000000ULL ) SR |= (FlagC|FlagX);
    if( (d & result & 0x80000000) ) SR |= FlagV;
    return result;
}

// ---- Condition test ----

bool M68000::testCondition( int cond )
{
    switch( cond ) {
        case 0x0: return true;                          // T
        case 0x1: return false;                         // F
        case 0x2: return !(SR & (FlagC|FlagZ));         // HI
        case 0x3: return  (SR & (FlagC|FlagZ)) != 0;    // LS
        case 0x4: return !(SR & FlagC);                 // CC/HS
        case 0x5: return  (SR & FlagC) != 0;            // CS/LO
        case 0x6: return !(SR & FlagZ);                 // NE
        case 0x7: return  (SR & FlagZ) != 0;            // EQ
        case 0x8: return !(SR & FlagV);                 // VC
        case 0x9: return  (SR & FlagV) != 0;            // VS
        case 0xA: return !(SR & FlagN);                 // PL
        case 0xB: return  (SR & FlagN) != 0;            // MI
        case 0xC: // GE
            return ((SR & FlagN) != 0) == ((SR & FlagV) != 0);
        case 0xD: // LT
            return ((SR & FlagN) != 0) != ((SR & FlagV) != 0);
        case 0xE: // GT
            return !(SR & FlagZ) && (((SR & FlagN) != 0) == ((SR & FlagV) != 0));
        case 0xF: // LE
            return (SR & FlagZ) || (((SR & FlagN) != 0) != ((SR & FlagV) != 0));
    }
    return false;
}

// ---- Exception processing ----

void M68000::exception( int vector )
{
    unsigned oldSR = SR;
    setSR( (SR | FlagS) & ~FlagT );  // Enter supervisor, clear trace
    pushLong( PC );
    pushWord( oldSR );
    PC = rm32( vector * 4 );
    icount_ -= 34;
}

void M68000::groupZeroException( int vector, unsigned addr, unsigned status )
{
    unsigned oldSR = SR;
    setSR( (SR | FlagS) & ~FlagT );
    // Group 0 pushes extra info
    pushWord( 0 );      // Function code
    pushLong( addr );    // Access address
    pushWord( 0 );       // Instruction register
    pushWord( oldSR );
    pushLong( PC );
    PC = rm32( vector * 4 );
    icount_ -= 50;
}

void M68000::checkInterrupts()
{
    if( irq_level_ > 0 ) {
        int mask = getIPM();
        if( irq_level_ > mask || irq_level_ == 7 ) {
            int vector = env_.interruptAcknowledge( irq_level_ );
            unsigned oldSR = SR;
            setSR( (SR & ~(MaskIPM|FlagT)) | FlagS | (irq_level_ << 8) );
            pushLong( PC );
            pushWord( oldSR );
            if( vector < 0 ) vector = 24 + irq_level_;  // autovector
            PC = rm32( vector * 4 );
            stopped_ = false;
            icount_ -= 44;
        }
    }
}

// ---- Main execution loop ----

unsigned M68000::run( unsigned cycles )
{
    icount_ = (int)cycles;

    checkInterrupts();

    while( icount_ > 0 ) {
        if( stopped_ ) {
            icount_ = 0;
            break;
        }
        executeOne();
        checkInterrupts();
    }

    cycles_ += cycles - icount_;
    return (icount_ < 0) ? (unsigned)(-icount_) : 0;
}

// ---- Main decode ----

void M68000::executeOne()
{
    unsigned op = fetch16();
    int group = (op >> 12) & 0xF;

    switch( group ) {
        case 0x0: group0000(op); break;
        case 0x1: group0001(op); break;
        case 0x2: group0010(op); break;
        case 0x3: group0011(op); break;
        case 0x4: group0100(op); break;
        case 0x5: group0101(op); break;
        case 0x6: group0110(op); break;
        case 0x7: group0111(op); break;
        case 0x8: group1000(op); break;
        case 0x9: group1001(op); break;
        case 0xA: exception(10); break; // Line-A
        case 0xB: group1011(op); break;
        case 0xC: group1100(op); break;
        case 0xD: group1101(op); break;
        case 0xE: group1110(op); break;
        case 0xF: exception(11); break; // Line-F
    }
}

// ======== Group 0: Bit manipulation/MOVEP/Immediate ========

void M68000::group0000( unsigned op )
{
    int dstMode = (op >> 3) & 7;
    int dstReg  = op & 7;

    if( op & 0x0100 ) {
        // Dynamic bit operations (BTST/BCHG/BCLR/BSET Dn,<ea>)
        // or MOVEP
        int srcReg = (op >> 9) & 7;
        int type   = (op >> 6) & 3;

        if( dstMode == 1 ) {
            // MOVEP
            unsigned ea = A[dstReg] + SIGN16(fetch16());
            if( type == 0 ) { // MOVEP.W (d16,Ay),Dx
                unsigned hi = rm8(ea);
                unsigned lo = rm8(ea+2);
                D[srcReg] = (D[srcReg] & 0xFFFF0000) | (hi << 8) | lo;
                icount_ -= 16;
            } else if( type == 1 ) { // MOVEP.L (d16,Ay),Dx
                unsigned b3 = rm8(ea);
                unsigned b2 = rm8(ea+2);
                unsigned b1 = rm8(ea+4);
                unsigned b0 = rm8(ea+6);
                D[srcReg] = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
                icount_ -= 24;
            } else if( type == 2 ) { // MOVEP.W Dx,(d16,Ay)
                unsigned val = D[srcReg];
                wm8(ea, (val >> 8) & 0xFF);
                wm8(ea+2, val & 0xFF);
                icount_ -= 16;
            } else { // MOVEP.L Dx,(d16,Ay)
                unsigned val = D[srcReg];
                wm8(ea, (val >> 24) & 0xFF);
                wm8(ea+2, (val >> 16) & 0xFF);
                wm8(ea+4, (val >> 8) & 0xFF);
                wm8(ea+6, val & 0xFF);
                icount_ -= 24;
            }
            return;
        }

        // BTST/BCHG/BCLR/BSET Dn,<ea>
        unsigned bit = D[srcReg];
        if( dstMode == 0 ) {
            // Register: 32-bit
            bit &= 31;
            unsigned mask = 1U << bit;
            SR &= ~FlagZ;
            if( !(D[dstReg] & mask) ) SR |= FlagZ;
            switch( type ) {
                case 0: icount_ -= 6; break;                          // BTST
                case 1: D[dstReg] ^= mask; icount_ -= 8; break;      // BCHG
                case 2: D[dstReg] &= ~mask; icount_ -= 10; break;    // BCLR
                case 3: D[dstReg] |= mask; icount_ -= 8; break;      // BSET
            }
        } else {
            // Memory: 8-bit
            bit &= 7;
            unsigned mask = 1U << bit;
            unsigned ea = computeEA(dstMode, dstReg, 1);
            unsigned val = rm8(ea);
            SR &= ~FlagZ;
            if( !(val & mask) ) SR |= FlagZ;
            switch( type ) {
                case 0: icount_ -= 4; break;                                  // BTST
                case 1: wm8(ea, val ^ mask); icount_ -= 8; break;            // BCHG
                case 2: wm8(ea, val & ~mask); icount_ -= 8; break;           // BCLR
                case 3: wm8(ea, val | mask); icount_ -= 8; break;            // BSET
            }
        }
        return;
    }

    // Static bit operations or immediate operations
    int subop = (op >> 9) & 7;

    if( subop == 4 ) {
        // Static bit operations with immediate bit number
        unsigned bit = fetch16() & 0xFF;
        int type = (op >> 6) & 3;

        if( dstMode == 0 ) {
            bit &= 31;
            unsigned mask = 1U << bit;
            SR &= ~FlagZ;
            if( !(D[dstReg] & mask) ) SR |= FlagZ;
            switch( type ) {
                case 0: icount_ -= 10; break;
                case 1: D[dstReg] ^= mask; icount_ -= 12; break;
                case 2: D[dstReg] &= ~mask; icount_ -= 14; break;
                case 3: D[dstReg] |= mask; icount_ -= 12; break;
            }
        } else {
            bit &= 7;
            unsigned mask = 1U << bit;
            unsigned ea = computeEA(dstMode, dstReg, 1);
            unsigned val = rm8(ea);
            SR &= ~FlagZ;
            if( !(val & mask) ) SR |= FlagZ;
            switch( type ) {
                case 0: icount_ -= 8; break;
                case 1: wm8(ea, val ^ mask); icount_ -= 12; break;
                case 2: wm8(ea, val & ~mask); icount_ -= 12; break;
                case 3: wm8(ea, val | mask); icount_ -= 12; break;
            }
        }
        return;
    }

    // Immediate operations: ORI, ANDI, SUBI, ADDI, EORI, CMPI
    int size_code = (op >> 6) & 3;
    int size = (size_code == 0) ? 1 : (size_code == 1) ? 2 : 4;

    unsigned imm;
    if( size == 1 ) imm = fetch16() & 0xFF;
    else if( size == 2 ) imm = fetch16();
    else imm = fetch32();

    // Special cases: ORI/ANDI/EORI to CCR/SR
    if( dstMode == 7 && dstReg == 4 ) {
        // Immediate to CCR or SR
        if( size == 1 ) {
            // to CCR
            switch( subop ) {
                case 0: setCCR( getCCR() | imm ); break;   // ORI to CCR
                case 1: setCCR( getCCR() & imm ); break;   // ANDI to CCR
                case 5: setCCR( getCCR() ^ imm ); break;   // EORI to CCR
            }
            icount_ -= 20;
        } else if( size == 2 ) {
            // to SR (supervisor only)
            if( isSupervisor() ) {
                switch( subop ) {
                    case 0: setSR( SR | imm ); break;   // ORI to SR
                    case 1: setSR( SR & imm ); break;   // ANDI to SR
                    case 5: setSR( SR ^ imm ); break;   // EORI to SR
                }
            } else {
                exception(8); // Privilege violation
            }
            icount_ -= 20;
        }
        return;
    }

    switch( subop ) {
        case 0: // ORI
        {
            unsigned val = readEA(dstMode, dstReg, size);
            unsigned r = val | imm;
            if( size == 1 ) setFlagNZ_B(r);
            else if( size == 2 ) setFlagNZ_W(r);
            else setFlagNZ_L(r);
            writeEA(dstMode, dstReg, size, r);
            icount_ -= (dstMode == 0) ? ((size == 4) ? 16 : 8) : ((size == 4) ? 20 : 12);
            break;
        }
        case 1: // ANDI
        {
            unsigned val = readEA(dstMode, dstReg, size);
            unsigned r = val & imm;
            if( size == 1 ) setFlagNZ_B(r);
            else if( size == 2 ) setFlagNZ_W(r);
            else setFlagNZ_L(r);
            writeEA(dstMode, dstReg, size, r);
            icount_ -= (dstMode == 0) ? ((size == 4) ? 16 : 8) : ((size == 4) ? 20 : 12);
            break;
        }
        case 2: // SUBI
        {
            unsigned val = readEA(dstMode, dstReg, size);
            unsigned r;
            if( size == 1 ) r = sub_b(imm, val);
            else if( size == 2 ) r = sub_w(imm, val);
            else r = sub_l(imm, val);
            writeEA(dstMode, dstReg, size, r);
            icount_ -= (dstMode == 0) ? ((size == 4) ? 16 : 8) : ((size == 4) ? 20 : 12);
            break;
        }
        case 3: // ADDI
        {
            unsigned val = readEA(dstMode, dstReg, size);
            unsigned r;
            if( size == 1 ) r = add_b(imm, val);
            else if( size == 2 ) r = add_w(imm, val);
            else r = add_l(imm, val);
            writeEA(dstMode, dstReg, size, r);
            icount_ -= (dstMode == 0) ? ((size == 4) ? 16 : 8) : ((size == 4) ? 20 : 12);
            break;
        }
        case 5: // EORI
        {
            unsigned val = readEA(dstMode, dstReg, size);
            unsigned r = val ^ imm;
            if( size == 1 ) setFlagNZ_B(r);
            else if( size == 2 ) setFlagNZ_W(r);
            else setFlagNZ_L(r);
            writeEA(dstMode, dstReg, size, r);
            icount_ -= (dstMode == 0) ? ((size == 4) ? 16 : 8) : ((size == 4) ? 20 : 12);
            break;
        }
        case 6: // CMPI
        {
            unsigned val = readEA(dstMode, dstReg, size);
            if( size == 1 ) cmp_b(imm, val);
            else if( size == 2 ) cmp_w(imm, val);
            else cmp_l(imm, val);
            icount_ -= (dstMode == 0) ? ((size == 4) ? 14 : 8) : ((size == 4) ? 12 : 8);
            break;
        }
        default:
            exception(4); // Illegal instruction
            break;
    }
}

// ======== Group 1: MOVE.B ========

void M68000::group0001( unsigned op )
{
    int srcMode = (op >> 3) & 7;
    int srcReg  = op & 7;
    int dstReg  = (op >> 9) & 7;
    int dstMode = (op >> 6) & 7;

    unsigned val = readEA(srcMode, srcReg, 1);
    setFlagNZ_B(val);

    writeEA(dstMode, dstReg, 1, val);
    icount_ -= 4;
}

// ======== Group 2: MOVE.L ========

void M68000::group0010( unsigned op )
{
    int srcMode = (op >> 3) & 7;
    int srcReg  = op & 7;
    int dstReg  = (op >> 9) & 7;
    int dstMode = (op >> 6) & 7;

    unsigned val = readEA(srcMode, srcReg, 4);

    if( dstMode == 1 ) {
        // MOVEA.L
        A[dstReg] = val;
    } else {
        setFlagNZ_L(val);
        writeEA(dstMode, dstReg, 4, val);
    }
    icount_ -= 4;
}

// ======== Group 3: MOVE.W ========

void M68000::group0011( unsigned op )
{
    int srcMode = (op >> 3) & 7;
    int srcReg  = op & 7;
    int dstReg  = (op >> 9) & 7;
    int dstMode = (op >> 6) & 7;

    unsigned val = readEA(srcMode, srcReg, 2);

    if( dstMode == 1 ) {
        // MOVEA.W
        A[dstReg] = SIGN16(val & 0xFFFF);
    } else {
        setFlagNZ_W(val);
        writeEA(dstMode, dstReg, 2, val);
    }
    icount_ -= 4;
}

// ======== Group 4: Miscellaneous ========

void M68000::group0100( unsigned op )
{
    int mode = (op >> 3) & 7;
    int reg  = op & 7;

    switch( (op >> 6) & 0x3F ) {
        case 0x00: // NEGX.B
        {
            unsigned val = readEA(mode, reg, 1);
            unsigned r = negx_b(val);
            writeEA(mode, reg, 1, r);
            icount_ -= (mode == 0) ? 4 : 8;
            return;
        }
        case 0x01: // NEGX.W
        {
            unsigned val = readEA(mode, reg, 2);
            unsigned r = negx_w(val);
            writeEA(mode, reg, 2, r);
            icount_ -= (mode == 0) ? 4 : 8;
            return;
        }
        case 0x02: // NEGX.L
        {
            unsigned val = readEA(mode, reg, 4);
            unsigned r = negx_l(val);
            writeEA(mode, reg, 4, r);
            icount_ -= (mode == 0) ? 6 : 12;
            return;
        }
        case 0x03: // MOVE from SR
        {
            writeEA(mode, reg, 2, SR);
            icount_ -= (mode == 0) ? 6 : 8;
            return;
        }
        case 0x08: // CLR.B
        {
            readEA(mode, reg, 1); // read before write (bus cycle)
            writeEA(mode, reg, 1, 0);
            SR &= ~(FlagN|FlagV|FlagC);
            SR |= FlagZ;
            icount_ -= (mode == 0) ? 4 : 8;
            return;
        }
        case 0x09: // CLR.W
        {
            readEA(mode, reg, 2);
            writeEA(mode, reg, 2, 0);
            SR &= ~(FlagN|FlagV|FlagC);
            SR |= FlagZ;
            icount_ -= (mode == 0) ? 4 : 8;
            return;
        }
        case 0x0A: // CLR.L
        {
            readEA(mode, reg, 4);
            writeEA(mode, reg, 4, 0);
            SR &= ~(FlagN|FlagV|FlagC);
            SR |= FlagZ;
            icount_ -= (mode == 0) ? 6 : 12;
            return;
        }
        case 0x0B: // MOVE to CCR
        {
            unsigned val = readEA(mode, reg, 2);
            setCCR(val);
            icount_ -= 12;
            return;
        }
        case 0x10: // NEG.B
        {
            unsigned val = readEA(mode, reg, 1);
            unsigned r = neg_b(val);
            writeEA(mode, reg, 1, r);
            icount_ -= (mode == 0) ? 4 : 8;
            return;
        }
        case 0x11: // NEG.W
        {
            unsigned val = readEA(mode, reg, 2);
            unsigned r = neg_w(val);
            writeEA(mode, reg, 2, r);
            icount_ -= (mode == 0) ? 4 : 8;
            return;
        }
        case 0x12: // NEG.L
        {
            unsigned val = readEA(mode, reg, 4);
            unsigned r = neg_l(val);
            writeEA(mode, reg, 4, r);
            icount_ -= (mode == 0) ? 6 : 12;
            return;
        }
        case 0x13: // MOVE to SR (supervisor)
        {
            if( !isSupervisor() ) { exception(8); return; }
            unsigned val = readEA(mode, reg, 2);
            setSR(val);
            icount_ -= 12;
            return;
        }
        case 0x18: // NOT.B
        {
            unsigned val = readEA(mode, reg, 1);
            unsigned r = (~val) & 0xFF;
            setFlagNZ_B(r);
            writeEA(mode, reg, 1, r);
            icount_ -= (mode == 0) ? 4 : 8;
            return;
        }
        case 0x19: // NOT.W
        {
            unsigned val = readEA(mode, reg, 2);
            unsigned r = (~val) & 0xFFFF;
            setFlagNZ_W(r);
            writeEA(mode, reg, 2, r);
            icount_ -= (mode == 0) ? 4 : 8;
            return;
        }
        case 0x1A: // NOT.L
        {
            unsigned val = readEA(mode, reg, 4);
            unsigned r = ~val;
            setFlagNZ_L(r);
            writeEA(mode, reg, 4, r);
            icount_ -= (mode == 0) ? 6 : 12;
            return;
        }
        case 0x20: // NBCD
        {
            // Not commonly used, stub
            icount_ -= 8;
            return;
        }
        case 0x21: // PEA or SWAP
        {
            if( mode == 0 ) {
                // SWAP Dn
                unsigned val = D[reg];
                D[reg] = ((val >> 16) & 0xFFFF) | ((val & 0xFFFF) << 16);
                setFlagNZ_L(D[reg]);
                icount_ -= 4;
            } else {
                // PEA <ea>
                unsigned ea = computeEA(mode, reg, 4);
                pushLong(ea);
                icount_ -= 12;
            }
            return;
        }
        case 0x22: // EXT.W or MOVEM.W reg-to-mem
        {
            if( mode == 0 ) {
                // EXT.W
                D[reg] = (D[reg] & 0xFFFF0000) | (SIGN8(D[reg] & 0xFF) & 0xFFFF);
                setFlagNZ_W(D[reg] & 0xFFFF);
                icount_ -= 4;
            } else {
                // MOVEM.W register list to memory
                unsigned mask = fetch16();
                unsigned ea;
                if( mode == 4 ) {
                    // -(An) mode: registers stored in reverse order
                    ea = A[reg];
                    for( int i = 15; i >= 0; i-- ) {
                        if( mask & (1 << (15 - i)) ) {
                            ea -= 2;
                            wm16(ea, (i < 8) ? D[i] : A[i-8]);
                            icount_ -= 4;
                        }
                    }
                    A[reg] = ea;
                } else {
                    ea = computeEA(mode, reg, 2);
                    for( int i = 0; i < 16; i++ ) {
                        if( mask & (1 << i) ) {
                            wm16(ea, (i < 8) ? D[i] : A[i-8]);
                            ea += 2;
                            icount_ -= 4;
                        }
                    }
                }
                icount_ -= 8;
            }
            return;
        }
        case 0x23: // EXT.L or MOVEM.L reg-to-mem
        {
            if( mode == 0 ) {
                // EXT.L
                D[reg] = (unsigned)(int)(short)(D[reg] & 0xFFFF);
                setFlagNZ_L(D[reg]);
                icount_ -= 4;
            } else {
                // MOVEM.L register list to memory
                unsigned mask = fetch16();
                unsigned ea;
                if( mode == 4 ) {
                    ea = A[reg];
                    for( int i = 15; i >= 0; i-- ) {
                        if( mask & (1 << (15 - i)) ) {
                            ea -= 4;
                            wm32(ea, (i < 8) ? D[i] : A[i-8]);
                            icount_ -= 8;
                        }
                    }
                    A[reg] = ea;
                } else {
                    ea = computeEA(mode, reg, 4);
                    for( int i = 0; i < 16; i++ ) {
                        if( mask & (1 << i) ) {
                            wm32(ea, (i < 8) ? D[i] : A[i-8]);
                            ea += 4;
                            icount_ -= 8;
                        }
                    }
                }
                icount_ -= 8;
            }
            return;
        }
        case 0x28: // TST.B
        {
            unsigned val = readEA(mode, reg, 1);
            setFlagNZ_B(val);
            icount_ -= 4;
            return;
        }
        case 0x29: // TST.W
        {
            unsigned val = readEA(mode, reg, 2);
            setFlagNZ_W(val);
            icount_ -= 4;
            return;
        }
        case 0x2A: // TST.L
        {
            unsigned val = readEA(mode, reg, 4);
            setFlagNZ_L(val);
            icount_ -= 4;
            return;
        }
        case 0x2B: // TAS
        {
            unsigned val = readEA(mode, reg, 1);
            setFlagNZ_B(val);
            writeEA(mode, reg, 1, val | 0x80);
            icount_ -= (mode == 0) ? 4 : 14;
            return;
        }
        case 0x32: // MOVEM.W memory-to-reg
        {
            unsigned mask = fetch16();
            unsigned ea;
            if( mode == 3 ) {
                // (An)+ mode
                ea = A[reg];
                for( int i = 0; i < 16; i++ ) {
                    if( mask & (1 << i) ) {
                        unsigned val = rm16(ea);
                        if( i < 8 ) D[i] = SIGN16(val);
                        else A[i-8] = SIGN16(val);
                        ea += 2;
                        icount_ -= 4;
                    }
                }
                A[reg] = ea;
            } else {
                ea = computeEA(mode, reg, 2);
                for( int i = 0; i < 16; i++ ) {
                    if( mask & (1 << i) ) {
                        unsigned val = rm16(ea);
                        if( i < 8 ) D[i] = SIGN16(val);
                        else A[i-8] = SIGN16(val);
                        ea += 2;
                        icount_ -= 4;
                    }
                }
            }
            icount_ -= 12;
            return;
        }
        case 0x33: // MOVEM.L memory-to-reg
        {
            unsigned mask = fetch16();
            unsigned ea;
            if( mode == 3 ) {
                ea = A[reg];
                for( int i = 0; i < 16; i++ ) {
                    if( mask & (1 << i) ) {
                        unsigned val = rm32(ea);
                        if( i < 8 ) D[i] = val;
                        else A[i-8] = val;
                        ea += 4;
                        icount_ -= 8;
                    }
                }
                A[reg] = ea;
            } else {
                ea = computeEA(mode, reg, 4);
                for( int i = 0; i < 16; i++ ) {
                    if( mask & (1 << i) ) {
                        unsigned val = rm32(ea);
                        if( i < 8 ) D[i] = val;
                        else A[i-8] = val;
                        ea += 4;
                        icount_ -= 8;
                    }
                }
            }
            icount_ -= 12;
            return;
        }
    }

    // More misc group 4 instructions
    int hi = (op >> 8) & 0xF;

    if( hi == 0xE ) {
        // JSR or JMP
        if( op & 0x0040 ) {
            // JMP <ea>
            unsigned ea = computeEA(mode, reg, 4);
            PC = ea;
            icount_ -= 8;
        } else {
            // JSR <ea>
            unsigned ea = computeEA(mode, reg, 4);
            pushLong(PC);
            PC = ea;
            icount_ -= 16;
        }
        return;
    }

    if( (op & 0xFFF0) == 0x4E70 ) {
        switch( op & 0xF ) {
            case 0: // RESET
                if( isSupervisor() ) icount_ -= 132;
                else exception(8);
                return;
            case 1: // NOP
                icount_ -= 4;
                return;
            case 2: // STOP
                if( isSupervisor() ) {
                    setSR(fetch16());
                    stopped_ = true;
                    icount_ -= 4;
                } else {
                    exception(8);
                }
                return;
            case 3: // RTE
                if( isSupervisor() ) {
                    unsigned newSR = popWord();
                    PC = popLong();
                    setSR(newSR);
                    icount_ -= 20;
                } else {
                    exception(8);
                }
                return;
            case 5: // RTS
                PC = popLong();
                icount_ -= 16;
                return;
            case 6: // TRAPV
                if( SR & FlagV ) exception(7);
                else icount_ -= 4;
                return;
            case 7: // RTR
            {
                unsigned newCCR = popWord();
                PC = popLong();
                setCCR(newCCR);
                icount_ -= 20;
                return;
            }
        }
    }

    // TRAP
    if( (op & 0xFFF0) == 0x4E40 ) {
        exception(32 + (op & 0xF));
        return;
    }

    // LINK
    if( (op & 0xFFF8) == 0x4E50 ) {
        pushLong(A[reg]);
        A[reg] = A[7];
        A[7] += SIGN16(fetch16());
        icount_ -= 16;
        return;
    }

    // UNLK
    if( (op & 0xFFF8) == 0x4E58 ) {
        A[7] = A[reg];
        A[reg] = popLong();
        icount_ -= 12;
        return;
    }

    // MOVE USP
    if( (op & 0xFFF0) == 0x4E60 ) {
        if( !isSupervisor() ) { exception(8); return; }
        if( op & 8 ) {
            // MOVE USP,An
            A[reg] = USP;
        } else {
            // MOVE An,USP
            USP = A[reg];
        }
        icount_ -= 4;
        return;
    }

    // LEA
    if( (op & 0xF1C0) == 0x41C0 ) {
        int dstReg2 = (op >> 9) & 7;
        unsigned ea = computeEA(mode, reg, 4);
        A[dstReg2] = ea;
        icount_ -= 4;
        return;
    }

    // CHK
    if( (op & 0xF1C0) == 0x4180 ) {
        int dstReg2 = (op >> 9) & 7;
        int val = (int)(short)(D[dstReg2] & 0xFFFF);
        int bound = (int)(short)(readEA(mode, reg, 2) & 0xFFFF);
        if( val < 0 ) {
            SR |= FlagN;
            exception(6);
        } else if( val > bound ) {
            SR &= ~FlagN;
            exception(6);
        }
        icount_ -= 10;
        return;
    }

    exception(4); // Illegal instruction
}

// ======== Group 5: ADDQ/SUBQ/Scc/DBcc ========

void M68000::group0101( unsigned op )
{
    int mode = (op >> 3) & 7;
    int reg  = op & 7;
    int data3 = (op >> 9) & 7;
    if( data3 == 0 ) data3 = 8;
    int size_code = (op >> 6) & 3;

    if( size_code == 3 ) {
        // Scc / DBcc
        int cond = (op >> 8) & 0xF;

        if( mode == 1 ) {
            // DBcc
            if( !testCondition(cond) ) {
                int disp = SIGN16(fetch16());
                D[reg] = (D[reg] & 0xFFFF0000) | ((D[reg] - 1) & 0xFFFF);
                if( (D[reg] & 0xFFFF) != 0xFFFF ) {
                    PC = PC - 2 + disp;
                    icount_ -= 10;
                } else {
                    icount_ -= 14;
                }
            } else {
                PC += 2; // skip displacement
                icount_ -= 12;
            }
        } else {
            // Scc
            if( testCondition(cond) ) {
                writeEA(mode, reg, 1, 0xFF);
                icount_ -= (mode == 0) ? 6 : 8;
            } else {
                writeEA(mode, reg, 1, 0);
                icount_ -= (mode == 0) ? 4 : 8;
            }
        }
        return;
    }

    int size = (size_code == 0) ? 1 : (size_code == 1) ? 2 : 4;

    if( op & 0x0100 ) {
        // SUBQ
        if( mode == 1 ) {
            // SUBQ to An: no flags affected, always .L
            A[reg] -= data3;
            icount_ -= 8;
        } else {
            unsigned val = readEA(mode, reg, size);
            unsigned r;
            if( size == 1 ) r = sub_b(data3, val);
            else if( size == 2 ) r = sub_w(data3, val);
            else r = sub_l(data3, val);
            writeEA(mode, reg, size, r);
            icount_ -= (mode == 0) ? ((size == 4) ? 8 : 4) : ((size == 4) ? 12 : 8);
        }
    } else {
        // ADDQ
        if( mode == 1 ) {
            A[reg] += data3;
            icount_ -= 8;
        } else {
            unsigned val = readEA(mode, reg, size);
            unsigned r;
            if( size == 1 ) r = add_b(data3, val);
            else if( size == 2 ) r = add_w(data3, val);
            else r = add_l(data3, val);
            writeEA(mode, reg, size, r);
            icount_ -= (mode == 0) ? ((size == 4) ? 8 : 4) : ((size == 4) ? 12 : 8);
        }
    }
}

// ======== Group 6: Bcc/BSR/BRA ========

void M68000::group0110( unsigned op )
{
    int cond = (op >> 8) & 0xF;
    int disp = SIGN8(op & 0xFF);
    unsigned base = PC;

    if( disp == 0 ) {
        disp = SIGN16(fetch16());
    }

    if( cond == 0 ) {
        // BRA
        PC = base + disp;
        icount_ -= 10;
    } else if( cond == 1 ) {
        // BSR
        pushLong(PC);
        PC = base + disp;
        icount_ -= 18;
    } else {
        // Bcc
        if( testCondition(cond) ) {
            PC = base + disp;
            icount_ -= 10;
        } else {
            icount_ -= 8;
        }
    }
}

// ======== Group 7: MOVEQ ========

void M68000::group0111( unsigned op )
{
    int reg = (op >> 9) & 7;
    unsigned val = (unsigned)(int)(signed char)(op & 0xFF);
    D[reg] = val;
    setFlagNZ_L(val);
    icount_ -= 4;
}

// ======== Group 8: OR/DIV/SBCD ========

void M68000::group1000( unsigned op )
{
    int dstReg = (op >> 9) & 7;
    int mode   = (op >> 3) & 7;
    int reg    = op & 7;
    int opmode = (op >> 6) & 7;

    switch( opmode ) {
        case 0: // OR.B <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 1);
            unsigned d = D[dstReg] & 0xFF;
            unsigned r = d | s;
            D[dstReg] = (D[dstReg] & 0xFFFFFF00) | (r & 0xFF);
            setFlagNZ_B(r);
            icount_ -= 4;
            break;
        }
        case 1: // OR.W <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 2);
            unsigned d = D[dstReg] & 0xFFFF;
            unsigned r = d | s;
            D[dstReg] = (D[dstReg] & 0xFFFF0000) | (r & 0xFFFF);
            setFlagNZ_W(r);
            icount_ -= 4;
            break;
        }
        case 2: // OR.L <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 4);
            unsigned r = D[dstReg] | s;
            D[dstReg] = r;
            setFlagNZ_L(r);
            icount_ -= (mode == 0 || mode == 1) ? 8 : 6;
            break;
        }
        case 3: // DIVU.W <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 2) & 0xFFFF;
            if( s == 0 ) {
                exception(5); // Division by zero
                return;
            }
            unsigned d = D[dstReg];
            unsigned quot = d / s;
            unsigned rem  = d % s;
            if( quot > 0xFFFF ) {
                SR |= FlagV;
                SR &= ~FlagC;
            } else {
                D[dstReg] = (rem << 16) | (quot & 0xFFFF);
                SR &= ~(FlagN|FlagZ|FlagV|FlagC);
                if( quot == 0 ) SR |= FlagZ;
                if( quot & 0x8000 ) SR |= FlagN;
            }
            icount_ -= 140;
            break;
        }
        case 4: // OR.B Dn,<ea> or SBCD
        {
            if( mode == 0 || mode == 1 ) {
                // SBCD
                icount_ -= 6;
            } else {
                unsigned s = D[dstReg] & 0xFF;
                unsigned ea = computeEA(mode, reg, 1);
                unsigned d = rm8(ea);
                unsigned r = d | s;
                setFlagNZ_B(r);
                wm8(ea, r);
                icount_ -= 8;
            }
            break;
        }
        case 5: // OR.W Dn,<ea>
        {
            unsigned s = D[dstReg] & 0xFFFF;
            unsigned ea = computeEA(mode, reg, 2);
            unsigned d = rm16(ea);
            unsigned r = d | s;
            setFlagNZ_W(r);
            wm16(ea, r);
            icount_ -= 8;
            break;
        }
        case 6: // OR.L Dn,<ea>
        {
            unsigned s = D[dstReg];
            unsigned ea = computeEA(mode, reg, 4);
            unsigned d = rm32(ea);
            unsigned r = d | s;
            setFlagNZ_L(r);
            wm32(ea, r);
            icount_ -= 12;
            break;
        }
        case 7: // DIVS.W <ea>,Dn
        {
            int s = (int)(short)(readEA(mode, reg, 2) & 0xFFFF);
            if( s == 0 ) {
                exception(5);
                return;
            }
            int d = (int)D[dstReg];
            int quot = d / s;
            int rem  = d % s;
            if( quot < -32768 || quot > 32767 ) {
                SR |= FlagV;
                SR &= ~FlagC;
            } else {
                D[dstReg] = ((unsigned)(rem & 0xFFFF) << 16) | (quot & 0xFFFF);
                SR &= ~(FlagN|FlagZ|FlagV|FlagC);
                if( (quot & 0xFFFF) == 0 ) SR |= FlagZ;
                if( quot & 0x8000 ) SR |= FlagN;
            }
            icount_ -= 158;
            break;
        }
    }
}

// ======== Group 9: SUB/SUBX ========

void M68000::group1001( unsigned op )
{
    int dstReg = (op >> 9) & 7;
    int mode   = (op >> 3) & 7;
    int reg    = op & 7;
    int opmode = (op >> 6) & 7;

    switch( opmode ) {
        case 0: // SUB.B <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 1);
            unsigned r = sub_b(s, D[dstReg] & 0xFF);
            D[dstReg] = (D[dstReg] & 0xFFFFFF00) | r;
            icount_ -= 4;
            break;
        }
        case 1: // SUB.W <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 2);
            unsigned r = sub_w(s, D[dstReg] & 0xFFFF);
            D[dstReg] = (D[dstReg] & 0xFFFF0000) | r;
            icount_ -= 4;
            break;
        }
        case 2: // SUB.L <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 4);
            D[dstReg] = sub_l(s, D[dstReg]);
            icount_ -= (mode == 0 || mode == 1) ? 8 : 6;
            break;
        }
        case 3: // SUBA.W
        {
            int s = SIGN16(readEA(mode, reg, 2));
            A[dstReg] -= s;
            icount_ -= 8;
            break;
        }
        case 4: // SUB.B Dn,<ea> or SUBX.B
        {
            if( mode == 0 ) {
                // SUBX.B Dy,Dx
                D[dstReg] = (D[dstReg] & 0xFFFFFF00) |
                    subx_b(D[reg] & 0xFF, D[dstReg] & 0xFF);
                icount_ -= 4;
            } else if( mode == 1 ) {
                // SUBX.B -(Ay),-(Ax)
                unsigned s = readEA(4, reg, 1);
                unsigned d = readEA(4, dstReg, 1);
                unsigned r = subx_b(s, d);
                wm8(A[dstReg], r);
                icount_ -= 18;
            } else {
                unsigned s = D[dstReg] & 0xFF;
                unsigned ea = computeEA(mode, reg, 1);
                unsigned d = rm8(ea);
                wm8(ea, sub_b(s, d));
                icount_ -= 8;
            }
            break;
        }
        case 5: // SUB.W Dn,<ea> or SUBX.W
        {
            if( mode == 0 ) {
                D[dstReg] = (D[dstReg] & 0xFFFF0000) |
                    subx_w(D[reg] & 0xFFFF, D[dstReg] & 0xFFFF);
                icount_ -= 4;
            } else if( mode == 1 ) {
                unsigned s = readEA(4, reg, 2);
                unsigned d = readEA(4, dstReg, 2);
                unsigned r = subx_w(s, d);
                wm16(A[dstReg], r);
                icount_ -= 18;
            } else {
                unsigned s = D[dstReg] & 0xFFFF;
                unsigned ea = computeEA(mode, reg, 2);
                unsigned d = rm16(ea);
                wm16(ea, sub_w(s, d));
                icount_ -= 8;
            }
            break;
        }
        case 6: // SUB.L Dn,<ea> or SUBX.L
        {
            if( mode == 0 ) {
                D[dstReg] = subx_l(D[reg], D[dstReg]);
                icount_ -= 8;
            } else if( mode == 1 ) {
                unsigned s = readEA(4, reg, 4);
                unsigned d = readEA(4, dstReg, 4);
                unsigned r = subx_l(s, d);
                wm32(A[dstReg], r);
                icount_ -= 30;
            } else {
                unsigned s = D[dstReg];
                unsigned ea = computeEA(mode, reg, 4);
                unsigned d = rm32(ea);
                wm32(ea, sub_l(s, d));
                icount_ -= 12;
            }
            break;
        }
        case 7: // SUBA.L
        {
            unsigned s = readEA(mode, reg, 4);
            A[dstReg] -= s;
            icount_ -= (mode == 0 || mode == 1) ? 8 : 6;
            break;
        }
    }
}

// ======== Group B: CMP/EOR ========

void M68000::group1011( unsigned op )
{
    int dstReg = (op >> 9) & 7;
    int mode   = (op >> 3) & 7;
    int reg    = op & 7;
    int opmode = (op >> 6) & 7;

    switch( opmode ) {
        case 0: // CMP.B
        {
            unsigned s = readEA(mode, reg, 1);
            cmp_b(s, D[dstReg] & 0xFF);
            icount_ -= 4;
            break;
        }
        case 1: // CMP.W
        {
            unsigned s = readEA(mode, reg, 2);
            cmp_w(s, D[dstReg] & 0xFFFF);
            icount_ -= 4;
            break;
        }
        case 2: // CMP.L
        {
            unsigned s = readEA(mode, reg, 4);
            cmp_l(s, D[dstReg]);
            icount_ -= 6;
            break;
        }
        case 3: // CMPA.W
        {
            int s = SIGN16(readEA(mode, reg, 2));
            cmp_l((unsigned)s, A[dstReg]);
            icount_ -= 6;
            break;
        }
        case 4: // EOR.B Dn,<ea> or CMPM.B
        {
            if( mode == 1 ) {
                // CMPM.B (Ay)+,(Ax)+
                unsigned s = readEA(3, reg, 1);
                unsigned d = readEA(3, dstReg, 1);
                cmp_b(s, d);
                icount_ -= 12;
            } else {
                unsigned d = readEA(mode, reg, 1);
                unsigned r = (D[dstReg] & 0xFF) ^ d;
                setFlagNZ_B(r);
                writeEA(mode, reg, 1, r);
                icount_ -= (mode == 0) ? 4 : 8;
            }
            break;
        }
        case 5: // EOR.W Dn,<ea> or CMPM.W
        {
            if( mode == 1 ) {
                unsigned s = readEA(3, reg, 2);
                unsigned d = readEA(3, dstReg, 2);
                cmp_w(s, d);
                icount_ -= 12;
            } else {
                unsigned d = readEA(mode, reg, 2);
                unsigned r = (D[dstReg] & 0xFFFF) ^ d;
                setFlagNZ_W(r);
                writeEA(mode, reg, 2, r);
                icount_ -= (mode == 0) ? 4 : 8;
            }
            break;
        }
        case 6: // EOR.L Dn,<ea> or CMPM.L
        {
            if( mode == 1 ) {
                unsigned s = readEA(3, reg, 4);
                unsigned d = readEA(3, dstReg, 4);
                cmp_l(s, d);
                icount_ -= 20;
            } else {
                unsigned d = readEA(mode, reg, 4);
                unsigned r = D[dstReg] ^ d;
                setFlagNZ_L(r);
                writeEA(mode, reg, 4, r);
                icount_ -= (mode == 0) ? 8 : 12;
            }
            break;
        }
        case 7: // CMPA.L
        {
            unsigned s = readEA(mode, reg, 4);
            cmp_l(s, A[dstReg]);
            icount_ -= 6;
            break;
        }
    }
}

// ======== Group C: AND/MUL/ABCD/EXG ========

void M68000::group1100( unsigned op )
{
    int dstReg = (op >> 9) & 7;
    int mode   = (op >> 3) & 7;
    int reg    = op & 7;
    int opmode = (op >> 6) & 7;

    switch( opmode ) {
        case 0: // AND.B <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 1);
            unsigned r = (D[dstReg] & 0xFF) & s;
            D[dstReg] = (D[dstReg] & 0xFFFFFF00) | r;
            setFlagNZ_B(r);
            icount_ -= 4;
            break;
        }
        case 1: // AND.W <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 2);
            unsigned r = (D[dstReg] & 0xFFFF) & s;
            D[dstReg] = (D[dstReg] & 0xFFFF0000) | r;
            setFlagNZ_W(r);
            icount_ -= 4;
            break;
        }
        case 2: // AND.L <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 4);
            D[dstReg] &= s;
            setFlagNZ_L(D[dstReg]);
            icount_ -= (mode == 0) ? 8 : 6;
            break;
        }
        case 3: // MULU.W <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 2) & 0xFFFF;
            unsigned d = D[dstReg] & 0xFFFF;
            D[dstReg] = s * d;
            setFlagNZ_L(D[dstReg]);
            icount_ -= 70;
            break;
        }
        case 4: // AND.B Dn,<ea> or ABCD or EXG
        {
            if( mode == 0 ) {
                // ABCD Dy,Dx
                icount_ -= 6;
            } else if( mode == 1 ) {
                // EXG Dx,Dy
                unsigned tmp = D[dstReg];
                D[dstReg] = D[reg];
                D[reg] = tmp;
                icount_ -= 6;
            } else {
                unsigned s = D[dstReg] & 0xFF;
                unsigned ea = computeEA(mode, reg, 1);
                unsigned d = rm8(ea);
                unsigned r = d & s;
                setFlagNZ_B(r);
                wm8(ea, r);
                icount_ -= 8;
            }
            break;
        }
        case 5: // AND.W Dn,<ea> or EXG
        {
            if( mode == 0 ) {
                // EXG Ax,Ay
                unsigned tmp = A[dstReg];
                A[dstReg] = A[reg];
                A[reg] = tmp;
                icount_ -= 6;
            } else if( mode == 1 ) {
                // EXG Dx,Ay
                unsigned tmp = D[dstReg];
                D[dstReg] = A[reg];
                A[reg] = tmp;
                icount_ -= 6;
            } else {
                unsigned s = D[dstReg] & 0xFFFF;
                unsigned ea = computeEA(mode, reg, 2);
                unsigned d = rm16(ea);
                unsigned r = d & s;
                setFlagNZ_W(r);
                wm16(ea, r);
                icount_ -= 8;
            }
            break;
        }
        case 6: // AND.L Dn,<ea>
        {
            unsigned s = D[dstReg];
            unsigned ea = computeEA(mode, reg, 4);
            unsigned d = rm32(ea);
            unsigned r = d & s;
            setFlagNZ_L(r);
            wm32(ea, r);
            icount_ -= 12;
            break;
        }
        case 7: // MULS.W <ea>,Dn
        {
            int s = (int)(short)(readEA(mode, reg, 2) & 0xFFFF);
            int d = (int)(short)(D[dstReg] & 0xFFFF);
            D[dstReg] = (unsigned)(s * d);
            setFlagNZ_L(D[dstReg]);
            icount_ -= 70;
            break;
        }
    }
}

// ======== Group D: ADD/ADDX ========

void M68000::group1101( unsigned op )
{
    int dstReg = (op >> 9) & 7;
    int mode   = (op >> 3) & 7;
    int reg    = op & 7;
    int opmode = (op >> 6) & 7;

    switch( opmode ) {
        case 0: // ADD.B <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 1);
            unsigned r = add_b(s, D[dstReg] & 0xFF);
            D[dstReg] = (D[dstReg] & 0xFFFFFF00) | r;
            icount_ -= 4;
            break;
        }
        case 1: // ADD.W <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 2);
            unsigned r = add_w(s, D[dstReg] & 0xFFFF);
            D[dstReg] = (D[dstReg] & 0xFFFF0000) | r;
            icount_ -= 4;
            break;
        }
        case 2: // ADD.L <ea>,Dn
        {
            unsigned s = readEA(mode, reg, 4);
            D[dstReg] = add_l(s, D[dstReg]);
            icount_ -= (mode == 0 || mode == 1) ? 8 : 6;
            break;
        }
        case 3: // ADDA.W
        {
            int s = SIGN16(readEA(mode, reg, 2));
            A[dstReg] += s;
            icount_ -= 8;
            break;
        }
        case 4: // ADD.B Dn,<ea> or ADDX.B
        {
            if( mode == 0 ) {
                D[dstReg] = (D[dstReg] & 0xFFFFFF00) |
                    addx_b(D[reg] & 0xFF, D[dstReg] & 0xFF);
                icount_ -= 4;
            } else if( mode == 1 ) {
                unsigned s = readEA(4, reg, 1);
                unsigned d = readEA(4, dstReg, 1);
                unsigned r = addx_b(s, d);
                wm8(A[dstReg], r);
                icount_ -= 18;
            } else {
                unsigned s = D[dstReg] & 0xFF;
                unsigned ea = computeEA(mode, reg, 1);
                unsigned d = rm8(ea);
                wm8(ea, add_b(s, d));
                icount_ -= 8;
            }
            break;
        }
        case 5: // ADD.W Dn,<ea> or ADDX.W
        {
            if( mode == 0 ) {
                D[dstReg] = (D[dstReg] & 0xFFFF0000) |
                    addx_w(D[reg] & 0xFFFF, D[dstReg] & 0xFFFF);
                icount_ -= 4;
            } else if( mode == 1 ) {
                unsigned s = readEA(4, reg, 2);
                unsigned d = readEA(4, dstReg, 2);
                unsigned r = addx_w(s, d);
                wm16(A[dstReg], r);
                icount_ -= 18;
            } else {
                unsigned s = D[dstReg] & 0xFFFF;
                unsigned ea = computeEA(mode, reg, 2);
                unsigned d = rm16(ea);
                wm16(ea, add_w(s, d));
                icount_ -= 8;
            }
            break;
        }
        case 6: // ADD.L Dn,<ea> or ADDX.L
        {
            if( mode == 0 ) {
                D[dstReg] = addx_l(D[reg], D[dstReg]);
                icount_ -= 8;
            } else if( mode == 1 ) {
                unsigned s = readEA(4, reg, 4);
                unsigned d = readEA(4, dstReg, 4);
                unsigned r = addx_l(s, d);
                wm32(A[dstReg], r);
                icount_ -= 30;
            } else {
                unsigned s = D[dstReg];
                unsigned ea = computeEA(mode, reg, 4);
                unsigned d = rm32(ea);
                wm32(ea, add_l(s, d));
                icount_ -= 12;
            }
            break;
        }
        case 7: // ADDA.L
        {
            unsigned s = readEA(mode, reg, 4);
            A[dstReg] += s;
            icount_ -= (mode == 0 || mode == 1) ? 8 : 6;
            break;
        }
    }
}

// ======== Group E: Shift/Rotate ========

void M68000::group1110( unsigned op )
{
    int mode = (op >> 3) & 7;
    int reg  = op & 7;
    int size_code = (op >> 6) & 3;

    if( size_code == 3 ) {
        // Memory shift/rotate (1-bit, word size)
        int type = (op >> 9) & 3;
        int dir  = (op >> 8) & 1;
        unsigned ea = computeEA(mode, reg, 2);
        unsigned val = rm16(ea);
        unsigned result;

        if( dir == 0 ) {
            // Right
            switch( type ) {
                case 0: // ASR
                    result = (val & 0x8000) | (val >> 1);
                    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
                    if( val & 1 ) SR |= (FlagC|FlagX);
                    if( result & 0x8000 ) SR |= FlagN;
                    if( (result & 0xFFFF) == 0 ) SR |= FlagZ;
                    break;
                case 1: // LSR
                    result = val >> 1;
                    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
                    if( val & 1 ) SR |= (FlagC|FlagX);
                    if( (result & 0xFFFF) == 0 ) SR |= FlagZ;
                    break;
                case 2: // ROXR
                {
                    unsigned x = (SR & FlagX) ? 1 : 0;
                    result = (x << 15) | (val >> 1);
                    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
                    if( val & 1 ) SR |= (FlagC|FlagX);
                    if( result & 0x8000 ) SR |= FlagN;
                    if( (result & 0xFFFF) == 0 ) SR |= FlagZ;
                    break;
                }
                case 3: // ROR
                    result = ((val & 1) << 15) | (val >> 1);
                    SR &= ~(FlagN|FlagZ|FlagV|FlagC);
                    if( val & 1 ) SR |= FlagC;
                    if( result & 0x8000 ) SR |= FlagN;
                    if( (result & 0xFFFF) == 0 ) SR |= FlagZ;
                    break;
                default: result = val; break;
            }
        } else {
            // Left
            switch( type ) {
                case 0: // ASL
                    result = (val << 1) & 0xFFFF;
                    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
                    if( val & 0x8000 ) SR |= (FlagC|FlagX);
                    if( result & 0x8000 ) SR |= FlagN;
                    if( (result & 0xFFFF) == 0 ) SR |= FlagZ;
                    if( (val ^ result) & 0x8000 ) SR |= FlagV;
                    break;
                case 1: // LSL
                    result = (val << 1) & 0xFFFF;
                    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
                    if( val & 0x8000 ) SR |= (FlagC|FlagX);
                    if( result & 0x8000 ) SR |= FlagN;
                    if( (result & 0xFFFF) == 0 ) SR |= FlagZ;
                    break;
                case 2: // ROXL
                {
                    unsigned x = (SR & FlagX) ? 1 : 0;
                    result = ((val << 1) | x) & 0xFFFF;
                    SR &= ~(FlagX|FlagN|FlagZ|FlagV|FlagC);
                    if( val & 0x8000 ) SR |= (FlagC|FlagX);
                    if( result & 0x8000 ) SR |= FlagN;
                    if( (result & 0xFFFF) == 0 ) SR |= FlagZ;
                    break;
                }
                case 3: // ROL
                    result = ((val << 1) | ((val >> 15) & 1)) & 0xFFFF;
                    SR &= ~(FlagN|FlagZ|FlagV|FlagC);
                    if( val & 0x8000 ) SR |= FlagC;
                    if( result & 0x8000 ) SR |= FlagN;
                    if( (result & 0xFFFF) == 0 ) SR |= FlagZ;
                    break;
                default: result = val; break;
            }
        }
        wm16(ea, result);
        icount_ -= 8;
        return;
    }

    // Register shift/rotate
    int count_reg = (op >> 9) & 7;
    int dir  = (op >> 8) & 1;
    int type = (op >> 3) & 3;
    int size = (size_code == 0) ? 1 : (size_code == 1) ? 2 : 4;

    unsigned count;
    if( op & 0x20 ) {
        count = D[count_reg] & 63;
    } else {
        count = count_reg;
        if( count == 0 ) count = 8;
    }

    unsigned val;
    if( size == 1 ) val = D[reg] & 0xFF;
    else if( size == 2 ) val = D[reg] & 0xFFFF;
    else val = D[reg];

    unsigned msb = (size == 1) ? 0x80 : (size == 2) ? 0x8000 : 0x80000000;
    unsigned mask = (size == 1) ? 0xFF : (size == 2) ? 0xFFFF : 0xFFFFFFFF;

    unsigned result = val;
    SR &= ~FlagC;  // Clear carry initially

    if( count == 0 ) {
        // No shift: just set NZ, clear C (V for ASL stays 0)
        if( dir == 0 && type == 0 ) {} // ASR with 0 count
        SR &= ~(FlagN|FlagZ|FlagV|FlagC);
        if( (result & mask) == 0 ) SR |= FlagZ;
        if( result & msb ) SR |= FlagN;
        // X flag unchanged when count is 0
    } else {
        if( dir == 0 ) {
            // Right shifts
            switch( type ) {
                case 0: // ASR
                {
                    SR &= ~(FlagV);
                    for( unsigned i = 0; i < count; i++ ) {
                        unsigned c = result & 1;
                        result = (result & msb) | ((result >> 1) & (mask >> 1));
                        SR &= ~(FlagC|FlagX);
                        if( c ) SR |= (FlagC|FlagX);
                    }
                    result &= mask;
                    SR &= ~(FlagN|FlagZ);
                    if( (result & mask) == 0 ) SR |= FlagZ;
                    if( result & msb ) SR |= FlagN;
                    break;
                }
                case 1: // LSR
                {
                    SR &= ~FlagV;
                    for( unsigned i = 0; i < count; i++ ) {
                        unsigned c = result & 1;
                        result >>= 1;
                        result &= mask;
                        SR &= ~(FlagC|FlagX);
                        if( c ) SR |= (FlagC|FlagX);
                    }
                    SR &= ~(FlagN|FlagZ);
                    if( (result & mask) == 0 ) SR |= FlagZ;
                    if( result & msb ) SR |= FlagN;
                    break;
                }
                case 2: // ROXR
                {
                    SR &= ~FlagV;
                    for( unsigned i = 0; i < count; i++ ) {
                        unsigned x = (SR & FlagX) ? 1 : 0;
                        unsigned c = result & 1;
                        result = (x ? msb : 0) | ((result >> 1) & (mask >> 1));
                        result &= mask;
                        SR &= ~(FlagC|FlagX);
                        if( c ) SR |= (FlagC|FlagX);
                    }
                    SR &= ~(FlagN|FlagZ);
                    if( (result & mask) == 0 ) SR |= FlagZ;
                    if( result & msb ) SR |= FlagN;
                    break;
                }
                case 3: // ROR
                {
                    SR &= ~FlagV;
                    for( unsigned i = 0; i < count; i++ ) {
                        unsigned c = result & 1;
                        result = (c ? msb : 0) | ((result >> 1) & (mask >> 1));
                        result &= mask;
                        SR &= ~FlagC;
                        if( c ) SR |= FlagC;
                    }
                    SR &= ~(FlagN|FlagZ);
                    if( (result & mask) == 0 ) SR |= FlagZ;
                    if( result & msb ) SR |= FlagN;
                    break;
                }
            }
        } else {
            // Left shifts
            switch( type ) {
                case 0: // ASL
                {
                    SR &= ~FlagV;
                    for( unsigned i = 0; i < count; i++ ) {
                        unsigned c = (result & msb) ? 1 : 0;
                        unsigned old_msb = result & msb;
                        result = (result << 1) & mask;
                        SR &= ~(FlagC|FlagX);
                        if( c ) SR |= (FlagC|FlagX);
                        if( (result & msb) != old_msb ) SR |= FlagV;
                    }
                    SR &= ~(FlagN|FlagZ);
                    if( (result & mask) == 0 ) SR |= FlagZ;
                    if( result & msb ) SR |= FlagN;
                    break;
                }
                case 1: // LSL
                {
                    SR &= ~FlagV;
                    for( unsigned i = 0; i < count; i++ ) {
                        unsigned c = (result & msb) ? 1 : 0;
                        result = (result << 1) & mask;
                        SR &= ~(FlagC|FlagX);
                        if( c ) SR |= (FlagC|FlagX);
                    }
                    SR &= ~(FlagN|FlagZ);
                    if( (result & mask) == 0 ) SR |= FlagZ;
                    if( result & msb ) SR |= FlagN;
                    break;
                }
                case 2: // ROXL
                {
                    SR &= ~FlagV;
                    for( unsigned i = 0; i < count; i++ ) {
                        unsigned x = (SR & FlagX) ? 1 : 0;
                        unsigned c = (result & msb) ? 1 : 0;
                        result = ((result << 1) | x) & mask;
                        SR &= ~(FlagC|FlagX);
                        if( c ) SR |= (FlagC|FlagX);
                    }
                    SR &= ~(FlagN|FlagZ);
                    if( (result & mask) == 0 ) SR |= FlagZ;
                    if( result & msb ) SR |= FlagN;
                    break;
                }
                case 3: // ROL
                {
                    SR &= ~FlagV;
                    for( unsigned i = 0; i < count; i++ ) {
                        unsigned c = (result & msb) ? 1 : 0;
                        result = ((result << 1) | c) & mask;
                        SR &= ~FlagC;
                        if( c ) SR |= FlagC;
                    }
                    SR &= ~(FlagN|FlagZ);
                    if( (result & mask) == 0 ) SR |= FlagZ;
                    if( result & msb ) SR |= FlagN;
                    break;
                }
            }
        }
    }

    // Write result back to register
    if( size == 1 ) D[reg] = (D[reg] & 0xFFFFFF00) | (result & 0xFF);
    else if( size == 2 ) D[reg] = (D[reg] & 0xFFFF0000) | (result & 0xFFFF);
    else D[reg] = result;

    icount_ -= (size == 4) ? 8 + 2 * count : 6 + 2 * count;
}
