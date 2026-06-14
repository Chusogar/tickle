/*
    M6809 emulator

    Based on the Motorola 6809 emulator by John Butler (1997)
    as found in MAME 0.37b7.

    Ported to C++ and adapted for tickle by Devin (2026).
    Original credits:
        6809 Simulator V09, By L.C. Benschop
        m6809: Portable 6809 emulator, DS
        References: 6809 Microcomputer Programming & Interfacing
                    by Andrew C. Staugaard, Jr.
*/
#ifndef M6809_H_
#define M6809_H_

class M6809Environment
{
public:
    M6809Environment() {}
    virtual ~M6809Environment() {}

    virtual unsigned char readByte( unsigned addr ) {
        return 0xFF;
    }

    virtual void writeByte( unsigned addr, unsigned char value ) {
    }
};

class M6809
{
public:
    // Condition code flags
    enum {
        FlagC  = 0x01,  // Carry
        FlagV  = 0x02,  // Overflow
        FlagZ  = 0x04,  // Zero
        FlagN  = 0x08,  // Negative
        FlagI  = 0x10,  // IRQ inhibit
        FlagH  = 0x20,  // Half carry
        FlagF  = 0x40,  // FIRQ inhibit
        FlagE  = 0x80   // Entire state pushed
    };

    // Internal state flags
    enum {
        StateCWAI = 0x08,
        StateSYNC = 0x10,
        StateLDS  = 0x20
    };

    // Registers (public for driver access)
    unsigned char   A;
    unsigned char   B;
    unsigned char   CC;
    unsigned char   DP;
    unsigned        X;
    unsigned        Y;
    unsigned        S;
    unsigned        U;
    unsigned        PC;

    M6809( M6809Environment & env );
    virtual ~M6809() {}

    virtual void reset();
    virtual unsigned run( unsigned cycles );
    void irq();
    void firq();
    void nmi();

    unsigned D() const { return ((unsigned)A << 8) | B; }
    void setD( unsigned val ) { A = (val >> 8) & 0xFF; B = val & 0xFF; }

    unsigned getCycles() const { return cycles_; }
    void setCycles( unsigned c ) { cycles_ = c; }

private:
    M6809Environment & env_;

    unsigned        cycles_;
    int             icount_;
    int             extra_cycles_;
    unsigned char   int_state_;
    unsigned char   nmi_state_;
    unsigned char   irq_state_;
    unsigned char   firq_state_;

    unsigned        ea_;    // effective address

    // Memory access helpers
    unsigned char rm( unsigned addr );
    void wm( unsigned addr, unsigned char val );
    unsigned rm16( unsigned addr );
    void wm16( unsigned addr, unsigned val );

    // Immediate / addressing helpers
    unsigned char imm8();
    unsigned imm16();
    void direct();
    void extended();
    void indexed();

    // Stack operations
    void pushByteS( unsigned char b );
    void pushWordS( unsigned val );
    unsigned char pullByteS();
    unsigned pullWordS();
    void pushByteU( unsigned char b );
    void pushWordU( unsigned val );
    unsigned char pullByteU();
    unsigned pullWordU();

    // Flag helpers
    void clr_nzvc() { CC &= ~(FlagN|FlagZ|FlagV|FlagC); }
    void clr_nzv()  { CC &= ~(FlagN|FlagZ|FlagV); }
    void clr_nzc()  { CC &= ~(FlagN|FlagZ|FlagC); }
    void clr_hnzvc(){ CC &= ~(FlagH|FlagN|FlagZ|FlagV|FlagC); }
    void clr_z()    { CC &= ~FlagZ; }
    void set_z8( unsigned val )  { if(!(val&0xFF)) CC|=FlagZ; }
    void set_z16( unsigned val ) { if(!(val&0xFFFF)) CC|=FlagZ; }
    void set_n8( unsigned val )  { CC|=((val&0x80)>>4); }
    void set_n16( unsigned val ) { CC|=((val&0x8000)>>12); }
    void set_h( unsigned a, unsigned b, unsigned r ) { CC|=(((a^b^r)&0x10)<<1); }
    void set_c8( unsigned val )  { CC|=((val&0x100)>>8); }
    void set_c16( unsigned val ) { CC|=((val&0x10000)>>16); }
    void set_v8( unsigned a, unsigned b, unsigned r ) { CC|=(((a^b^r^(r>>1))&0x80)>>6); }
    void set_v16( unsigned a, unsigned b, unsigned r ){ CC|=(((a^b^r^(r>>1))&0x8000)>>14); }
    void set_nz8( unsigned val )  { set_n8(val); set_z8(val); }
    void set_nz16( unsigned val ) { set_n16(val); set_z16(val); }
    void set_flags8( unsigned a, unsigned b, unsigned r ) {
        set_n8(r); set_z8(r); set_v8(a,b,r); set_c8(r);
    }
    void set_flags16( unsigned a, unsigned b, unsigned r ) {
        set_n16(r); set_z16(r); set_v16(a,b,r); set_c16(r);
    }

    // Flag increment/decrement tables
    static const unsigned char flags8i_[256];
    static const unsigned char flags8d_[256];

    // IRQ check
    void check_irq_lines();
    void do_irq();
    void do_firq();

    // Opcode execution
    void execute_one();
    void pref10();
    void pref11();

    // Page 1 opcodes
    void neg_di(); void com_di(); void lsr_di(); void ror_di();
    void asr_di(); void asl_di(); void rol_di(); void dec_di();
    void inc_di(); void tst_di(); void jmp_di(); void clr_di();
    void neg_ix(); void com_ix(); void lsr_ix(); void ror_ix();
    void asr_ix(); void asl_ix(); void rol_ix(); void dec_ix();
    void inc_ix(); void tst_ix(); void jmp_ix(); void clr_ix();
    void neg_ex(); void com_ex(); void lsr_ex(); void ror_ex();
    void asr_ex(); void asl_ex(); void rol_ex(); void dec_ex();
    void inc_ex(); void tst_ex(); void jmp_ex(); void clr_ex();

    void nega(); void coma(); void lsra(); void rora();
    void asra(); void asla(); void rola(); void deca();
    void inca(); void tsta(); void clra();
    void negb(); void comb(); void lsrb(); void rorb();
    void asrb(); void aslb(); void rolb(); void decb();
    void incb(); void tstb(); void clrb();

    void nop_(); void sync_(); void lbra(); void lbsr();
    void daa(); void orcc(); void andcc(); void sex();
    void exg(); void tfr();

    void bra(); void brn(); void bhi(); void bls();
    void bcc(); void bcs(); void bne(); void beq();
    void bvc(); void bvs(); void bpl(); void bmi();
    void bge(); void blt(); void bgt(); void ble();
    void bsr();

    void leax(); void leay(); void leas(); void leau();
    void pshs(); void puls(); void pshu(); void pulu();
    void rts(); void abx(); void rti(); void cwai();
    void mul(); void swi();

    void suba_im(); void cmpa_im(); void sbca_im(); void subd_im();
    void anda_im(); void bita_im(); void lda_im(); void sta_im();
    void eora_im(); void adca_im(); void ora_im(); void adda_im();
    void cmpx_im(); void ldx_im(); void stx_im();

    void suba_di(); void cmpa_di(); void sbca_di(); void subd_di();
    void anda_di(); void bita_di(); void lda_di(); void sta_di();
    void eora_di(); void adca_di(); void ora_di(); void adda_di();
    void cmpx_di(); void jsr_di(); void ldx_di(); void stx_di();

    void suba_ix(); void cmpa_ix(); void sbca_ix(); void subd_ix();
    void anda_ix(); void bita_ix(); void lda_ix(); void sta_ix();
    void eora_ix(); void adca_ix(); void ora_ix(); void adda_ix();
    void cmpx_ix(); void jsr_ix(); void ldx_ix(); void stx_ix();

    void suba_ex(); void cmpa_ex(); void sbca_ex(); void subd_ex();
    void anda_ex(); void bita_ex(); void lda_ex(); void sta_ex();
    void eora_ex(); void adca_ex(); void ora_ex(); void adda_ex();
    void cmpx_ex(); void jsr_ex(); void ldx_ex(); void stx_ex();

    void subb_im(); void cmpb_im(); void sbcb_im(); void addd_im();
    void andb_im(); void bitb_im(); void ldb_im(); void stb_im();
    void eorb_im(); void adcb_im(); void orb_im(); void addb_im();
    void ldd_im(); void std_im(); void ldu_im(); void stu_im();

    void subb_di(); void cmpb_di(); void sbcb_di(); void addd_di();
    void andb_di(); void bitb_di(); void ldb_di(); void stb_di();
    void eorb_di(); void adcb_di(); void orb_di(); void addb_di();
    void ldd_di(); void std_di(); void ldu_di(); void stu_di();

    void subb_ix(); void cmpb_ix(); void sbcb_ix(); void addd_ix();
    void andb_ix(); void bitb_ix(); void ldb_ix(); void stb_ix();
    void eorb_ix(); void adcb_ix(); void orb_ix(); void addb_ix();
    void ldd_ix(); void std_ix(); void ldu_ix(); void stu_ix();

    void subb_ex(); void cmpb_ex(); void sbcb_ex(); void addd_ex();
    void andb_ex(); void bitb_ex(); void ldb_ex(); void stb_ex();
    void eorb_ex(); void adcb_ex(); void orb_ex(); void addb_ex();
    void ldd_ex(); void std_ex(); void ldu_ex(); void stu_ex();

    // Page 2 (0x10 prefix) opcodes
    void lbrn(); void lbhi(); void lbls(); void lbcc();
    void lbcs(); void lbne(); void lbeq(); void lbvc();
    void lbvs(); void lbpl(); void lbmi(); void lbge();
    void lblt(); void lbgt(); void lble();
    void cmpd_im(); void cmpy_im(); void ldy_im(); void sty_im();
    void cmpd_di(); void cmpy_di(); void ldy_di(); void sty_di();
    void cmpd_ix(); void cmpy_ix(); void ldy_ix(); void sty_ix();
    void cmpd_ex(); void cmpy_ex(); void ldy_ex(); void sty_ex();
    void lds_im(); void sts_im();
    void lds_di(); void sts_di();
    void lds_ix(); void sts_ix();
    void lds_ex(); void sts_ex();
    void swi2();

    // Page 3 (0x11 prefix) opcodes
    void cmpu_im(); void cmps_im();
    void cmpu_di(); void cmps_di();
    void cmpu_ix(); void cmps_ix();
    void cmpu_ex(); void cmps_ex();
    void swi3();
};

#endif // M6809_H_
