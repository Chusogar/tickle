#include "m6809.h"

/*
 * NOTE:
 * This file is adapted from the uploaded Oldspark Opc_Std.cpp.
 * The opcode semantics are preserved.
 * It expects:
 *   - class M6809
 *   - env.rd8 / env.wr8 / env.rd16 / env.wr16
 *   - helper methods implemented in m6809.cpp
 *   - dispatch tables M6809Opc_std / M6809Opc_10xx / M6809Opc_11xx
 */

// -----------------------------------------------------------------------------
// Opcode   : NEG (0x00)
// Ad. Mode : DIRECT
// Cycles   : 6
// Opc size : 2
// -----------------------------------------------------------------------------
void M6809::Opc_std_00()
{
    dpr();
    env.wr8(dppr, neg(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_01()
{
    illegal_std(0x01);
    cycle_counter -= 1;
}

void M6809::Opc_std_02()
{
    illegal_std(0x02);
    cycle_counter -= 1;
}

void M6809::Opc_std_03()
{
    dpr();
    env.wr8(dppr, com(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_04()
{
    dpr();
    env.wr8(dppr, lsr(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_05()
{
    illegal_std(0x05);
    cycle_counter -= 1;
}

void M6809::Opc_std_06()
{
    dpr();
    env.wr8(dppr, ror(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_07()
{
    dpr();
    env.wr8(dppr, asr(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_08()
{
    dpr();
    env.wr8(dppr, asl(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_09()
{
    dpr();
    env.wr8(dppr, rol(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_0a()
{
    dpr();
    env.wr8(dppr, dec(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_0b()
{
    illegal_std(0x0B);
    cycle_counter -= 1;
}

void M6809::Opc_std_0c()
{
    dpr();
    env.wr8(dppr, inc(env.rd8(dppr)));
    cycle_counter -= 6;
}

void M6809::Opc_std_0d()
{
    dpr();
    tst(env.rd8(dppr));
    cycle_counter -= 6;
}

void M6809::Opc_std_0e()
{
    dpr();
    PC = dppr;
    cycle_counter -= 3;
}

void M6809::Opc_std_0f()
{
    dpr();
    env.wr8(dppr, clr());
    cycle_counter -= 6;
}

// PAGE 0x10
void M6809::Opc_std_10()
{
    u8 opcode = env.rd8(PC++);

#if M6809_OPC_STATS
    opc_10xx_used[opcode] += 1;
#endif

    cycle_counter -= 1;
    (this->*M6809Opc_10xx[opcode])();
}

// PAGE 0x11
void M6809::Opc_std_11()
{
    u8 opcode = env.rd8(PC++);

#if M6809_OPC_STATS
    opc_11xx_used[opcode] += 1;
#endif

    cycle_counter -= 1;
    (this->*M6809Opc_11xx[opcode])();
}

void M6809::Opc_std_12()
{
    cycle_counter -= 2; // NOP
}

void M6809::Opc_std_13()
{
    unimplemented(0x13); // SYNC
    cycle_counter -= 2;
}

void M6809::Opc_std_14()
{
    illegal_std(0x14);
    cycle_counter -= 1;
}

void M6809::Opc_std_15()
{
    illegal_std(0x15);
    cycle_counter -= 1;
}

void M6809::Opc_std_16()
{
    // LBRA
    PC += env.rd16(PC);
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_std_17()
{
    // LBSR
    extended();
    pshs(0x80);
    PC += epr;
    cycle_counter -= 9;
}

void M6809::Opc_std_18()
{
    illegal_std(0x18);
    cycle_counter -= 1;
}

void M6809::Opc_std_19()
{
    // DAA
    setA(daa(getA()));
    cycle_counter -= 2;
}

void M6809::Opc_std_1a()
{
    // ORCC
    CC |= env.rd8(PC++);
    cycle_counter -= 3;
}

void M6809::Opc_std_1b()
{
    illegal_std(0x1B);
    cycle_counter -= 1;
}

void M6809::Opc_std_1c()
{
    // ANDCC
    CC &= env.rd8(PC++);
    cycle_counter -= 3;
}

void M6809::Opc_std_1d()
{
    // SEX
    if (getB() & 0x80) {
        setA(0xFF);
    } else {
        setA(0x00);
    }
    tst(D);
    cycle_counter -= 2;
}

void M6809::Opc_std_1e()
{
    // EXG
    exg(env.rd8(PC++));
    cycle_counter -= 8;
}

void M6809::Opc_std_1f()
{
    // TFR
    tfr(env.rd8(PC++));
    cycle_counter -= 7;
}

// -----------------------------------------------------------------------------
// Short branches
// -----------------------------------------------------------------------------
void M6809::Opc_std_20()
{
    // BRA
    PC += (s8)env.rd8(PC);
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_21()
{
    // BRN
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_22()
{
    // BHI
    if ((CC & ZeroCarry) == 0) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_23()
{
    // BLS
    if (((CC & ZeroCarry) == Zero) || ((CC & ZeroCarry) == Carry)) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_24()
{
    // BHS/BCC
    if ((CC & Carry) == 0) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_25()
{
    // BLO/BCS
    if ((CC & Carry) == Carry) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_26()
{
    // BNE
    if ((CC & Zero) == 0) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_27()
{
    // BEQ
    if ((CC & Zero) == Zero) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_28()
{
    // BVC
    if ((CC & Overflow) == 0) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_29()
{
    // BVS
    if ((CC & Overflow) == Overflow) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_2a()
{
    // BPL
    if ((CC & Negative) == 0) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_2b()
{
    // BMI
    if ((CC & Negative) == Negative) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_2c()
{
    // BGE
    if (((CC & NegativeOverflow) == 0) || ((CC & NegativeOverflow) == NegativeOverflow)) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_2d()
{
    // BLT
    if (((CC & NegativeOverflow) == Negative) || ((CC & NegativeOverflow) == Overflow)) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_2e()
{
    // BGT
    if (((CC & 0x0E) == 0) || ((CC & 0x0E) == NegativeOverflow)) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

void M6809::Opc_std_2f()
{
    // BLE
    if (((CC & 0x0E) == Negative) ||
        ((CC & 0x0E) == Zero) ||
        ((CC & 0x0E) == Overflow) ||
        ((CC & 0x0E) == 0x0E)) {
        PC += (s8)env.rd8(PC);
    }
    PC++;
    cycle_counter -= 3;
}

// -----------------------------------------------------------------------------
// LEA / push / pull / RTS / RTI / SWI etc.
// -----------------------------------------------------------------------------
void M6809::Opc_std_30()
{
    // LEAX
    CC &= (u8)~Zero;
    indexed();
    X = ipr;
    if (X == 0) CC |= Zero;
    cycle_counter -= 4;
}

void M6809::Opc_std_31()
{
    // LEAY
    CC &= (u8)~Zero;
    indexed();
    Y = ipr;
    if (Y == 0) CC |= Zero;
    cycle_counter -= 4;
}

void M6809::Opc_std_32()
{
    // LEAS
    indexed();
    S = ipr;
    cycle_counter -= 4;
}

void M6809::Opc_std_33()
{
    // LEAU
    indexed();
    U = ipr;
    cycle_counter -= 4;
}

void M6809::Opc_std_34()
{
    pshs(env.rd8(PC++));
    cycle_counter -= 5;
}

void M6809::Opc_std_35()
{
    puls(env.rd8(PC++));
    cycle_counter -= 5;
}

void M6809::Opc_std_36()
{
    pshu(env.rd8(PC++));
    cycle_counter -= 5;
}

void M6809::Opc_std_37()
{
    pulu(env.rd8(PC++));
    cycle_counter -= 5;
}

void M6809::Opc_std_38()
{
    illegal_std(0x38);
    cycle_counter -= 1;
}

void M6809::Opc_std_39()
{
    // RTS
    puls(0x80);
    cycle_counter -= 5;
}

void M6809::Opc_std_3a()
{
    // ABX
    X = (u16)(X + (u16)getB());
    cycle_counter -= 3;
}

void M6809::Opc_std_3b()
{
    // RTI
    puls(0x01);
    if (CC & Entire) {
        puls(0xFE);
        cycle_counter -= 15;
    } else {
        puls(0x80);
        cycle_counter -= 6;
    }
}

void M6809::Opc_std_3c()
{
    // CWAI
    CC &= env.rd8(PC++);
    CC |= Entire;
    cycle_counter -= 21;
}

void M6809::Opc_std_3d()
{
    // MUL
    mul();
    cycle_counter -= 11;
}

void M6809::Opc_std_3e()
{
    illegal_std(0x3E);
}

void M6809::Opc_std_3f()
{
    // SWI
    swi1();
    cycle_counter -= 19;
}

// -----------------------------------------------------------------------------
// A/B inherent ops
// -----------------------------------------------------------------------------
void M6809::Opc_std_40() { setA(neg(getA())); cycle_counter -= 2; }
void M6809::Opc_std_41() { illegal_std(0x41); cycle_counter -= 1; }
void M6809::Opc_std_42() { illegal_std(0x42); cycle_counter -= 1; }
void M6809::Opc_std_43() { setA(com(getA())); cycle_counter -= 2; }
void M6809::Opc_std_44() { setA(lsr(getA())); cycle_counter -= 2; }
void M6809::Opc_std_45() { illegal_std(0x45); cycle_counter -= 1; }
void M6809::Opc_std_46() { setA(ror(getA())); cycle_counter -= 2; }
void M6809::Opc_std_47() { setA(asr(getA())); cycle_counter -= 2; }
void M6809::Opc_std_48() { setA(asl(getA())); cycle_counter -= 2; }
void M6809::Opc_std_49() { setA(rol(getA())); cycle_counter -= 2; }
void M6809::Opc_std_4a() { setA(dec(getA())); cycle_counter -= 2; }
void M6809::Opc_std_4b() { illegal_std(0x4B); cycle_counter -= 1; }
void M6809::Opc_std_4c() { setA(inc(getA())); cycle_counter -= 2; }
void M6809::Opc_std_4d() { tst(getA()); cycle_counter -= 2; }
void M6809::Opc_std_4e() { illegal_std(0x4E); cycle_counter -= 1; }
void M6809::Opc_std_4f() { setA(clr()); cycle_counter -= 2; }

void M6809::Opc_std_50() { setB(neg(getB())); cycle_counter -= 2; }
void M6809::Opc_std_51() { illegal_std(0x51); cycle_counter -= 1; }
void M6809::Opc_std_52() { illegal_std(0x52); cycle_counter -= 1; }
void M6809::Opc_std_53() { setB(com(getB())); cycle_counter -= 2; }
void M6809::Opc_std_54() { setB(lsr(getB())); cycle_counter -= 2; }
void M6809::Opc_std_55() { illegal_std(0x55); cycle_counter -= 1; }
void M6809::Opc_std_56() { setB(ror(getB())); cycle_counter -= 2; }
void M6809::Opc_std_57() { setB(asr(getB())); cycle_counter -= 2; }
void M6809::Opc_std_58() { setB(asl(getB())); cycle_counter -= 2; }
void M6809::Opc_std_59() { setB(rol(getB())); cycle_counter -= 2; }
void M6809::Opc_std_5a() { setB(dec(getB())); cycle_counter -= 2; }
void M6809::Opc_std_5b() { illegal_std(0x5B); cycle_counter -= 1; }
void M6809::Opc_std_5c() { setB(inc(getB())); cycle_counter -= 2; }
void M6809::Opc_std_5d() { tst(getB()); cycle_counter -= 2; }
void M6809::Opc_std_5e() { illegal_std(0x5E); cycle_counter -= 1; }
void M6809::Opc_std_5f() { setB(clr()); cycle_counter -= 2; }

// -----------------------------------------------------------------------------
// Indexed memory ops 0x60-0x6F
// -----------------------------------------------------------------------------
void M6809::Opc_std_60() { indexed(); env.wr8(ipr, neg(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_61() { illegal_std(0x61); cycle_counter -= 1; }
void M6809::Opc_std_62() { illegal_std(0x62); cycle_counter -= 1; }
void M6809::Opc_std_63() { indexed(); env.wr8(ipr, com(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_64() { indexed(); env.wr8(ipr, lsr(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_65() { illegal_std(0x65); cycle_counter -= 1; }
void M6809::Opc_std_66() { indexed(); env.wr8(ipr, ror(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_67() { indexed(); env.wr8(ipr, asr(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_68() { indexed(); env.wr8(ipr, asl(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_69() { indexed(); env.wr8(ipr, rol(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_6a() { indexed(); env.wr8(ipr, dec(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_6b() { illegal_std(0x6B); cycle_counter -= 1; }
void M6809::Opc_std_6c() { indexed(); env.wr8(ipr, inc(env.rd8(ipr))); cycle_counter -= 6; }
void M6809::Opc_std_6d() { indexed(); tst(env.rd8(ipr)); cycle_counter -= 6; }
void M6809::Opc_std_6e() { indexed(); PC = ipr; cycle_counter -= 3; }
void M6809::Opc_std_6f() { indexed(); env.wr8(ipr, clr()); cycle_counter -= 6; }

// -----------------------------------------------------------------------------
// Extended memory ops 0x70-0x7F
// -----------------------------------------------------------------------------
void M6809::Opc_std_70() { extended(); env.wr8(epr, neg(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_71() { illegal_std(0x71); cycle_counter -= 1; }
void M6809::Opc_std_72() { illegal_std(0x72); cycle_counter -= 1; }
void M6809::Opc_std_73() { extended(); env.wr8(epr, com(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_74() { extended(); env.wr8(epr, lsr(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_75() { illegal_std(0x75); cycle_counter -= 1; }
void M6809::Opc_std_76() { extended(); env.wr8(epr, ror(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_77() { extended(); env.wr8(epr, asr(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_78() { extended(); env.wr8(epr, asl(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_79() { extended(); env.wr8(epr, rol(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_7a() { extended(); env.wr8(epr, dec(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_7b() { illegal_std(0x7B); cycle_counter -= 1; }
void M6809::Opc_std_7c() { extended(); env.wr8(epr, inc(env.rd8(epr))); cycle_counter -= 7; }
void M6809::Opc_std_7d() { extended(); tst(env.rd8(epr)); cycle_counter -= 7; }
void M6809::Opc_std_7e() { PC = env.rd16(PC); cycle_counter -= 3; } // JMP ext
void M6809::Opc_std_7f() { extended(); env.wr8(epr, clr()); cycle_counter -= 7; }

// -----------------------------------------------------------------------------
// A register group 0x80-0xBF
// -----------------------------------------------------------------------------
void M6809::Opc_std_80() { setA(sub(getA(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_81() { cmp(getA(), env.rd8(PC++)); cycle_counter -= 2; }
void M6809::Opc_std_82() { setA(sbc(getA(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_83() { D = sub(D, env.rd16(PC)); PC += 2; cycle_counter -= 4; }
void M6809::Opc_std_84() { setA(and8(getA(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_85() { and8(getA(), env.rd8(PC++)); cycle_counter -= 2; }
void M6809::Opc_std_86() { setA(env.rd8(PC++)); tst(getA()); cycle_counter -= 2; }
void M6809::Opc_std_87() { illegal_std(0x87); cycle_counter -= 1; }
void M6809::Opc_std_88() { setA(xor8(getA(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_89() { setA(adc(getA(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_8a() { setA(or8(getA(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_8b() { setA(add(getA(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_8c() { extended(); cmp(X, epr); cycle_counter -= 4; }
void M6809::Opc_std_8d() { u16 ea = PC++; pshs(0x80); PC += (s8)env.rd8(ea); cycle_counter -= 7; }
void M6809::Opc_std_8e() { extended(); X = epr; tst(X); cycle_counter -= 3; }
void M6809::Opc_std_8f() { illegal_std(0x8F); cycle_counter -= 1; }

void M6809::Opc_std_90() { dpr(); setA(sub(getA(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_91() { dpr(); cmp(getA(), env.rd8(dppr)); cycle_counter -= 4; }
void M6809::Opc_std_92() { dpr(); setA(sbc(getA(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_93() { dpr(); D = sub(D, env.rd16(dppr)); cycle_counter -= 6; }
void M6809::Opc_std_94() { dpr(); setA(and8(getA(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_95() { dpr(); and8(getA(), env.rd8(dppr)); cycle_counter -= 4; }
void M6809::Opc_std_96() { dpr(); setA(env.rd8(dppr)); tst(getA()); cycle_counter -= 4; }
void M6809::Opc_std_97() { dpr(); env.wr8(dppr, getA()); tst(getA()); cycle_counter -= 4; }
void M6809::Opc_std_98() { dpr(); setA(xor8(getA(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_99() { dpr(); setA(adc(getA(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_9a() { dpr(); setA(or8(getA(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_9b() { dpr(); setA(add(getA(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_9c() { dpr(); cmp(X, env.rd16(dppr)); cycle_counter -= 6; }
void M6809::Opc_std_9d() { dpr(); pshs(0x80); PC = dppr; cycle_counter -= 7; }
void M6809::Opc_std_9e() { dpr(); X = env.rd16(dppr); tst(X); cycle_counter -= 5; }
void M6809::Opc_std_9f() { dpr(); env.wr16(dppr, X); tst(X); cycle_counter -= 5; }

void M6809::Opc_std_a0() { indexed(); setA(sub(getA(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_a1() { indexed(); cmp(getA(), env.rd8(ipr)); cycle_counter -= 4; }
void M6809::Opc_std_a2() { indexed(); setA(sbc(getA(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_a3() { indexed(); D = sub(D, env.rd16(ipr)); cycle_counter -= 6; }
void M6809::Opc_std_a4() { indexed(); setA(and8(getA(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_a5() { indexed(); and8(getA(), env.rd8(ipr)); cycle_counter -= 4; }
void M6809::Opc_std_a6() { indexed(); setA(env.rd8(ipr)); tst(getA()); cycle_counter -= 4; }
void M6809::Opc_std_a7() { indexed(); env.wr8(ipr, getA()); tst(getA()); cycle_counter -= 4; }
void M6809::Opc_std_a8() { indexed(); setA(xor8(getA(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_a9() { indexed(); setA(adc(getA(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_aa() { indexed(); setA(or8(getA(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_ab() { indexed(); setA(add(getA(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_ac() { indexed(); cmp(X, env.rd16(ipr)); cycle_counter -= 6; }
void M6809::Opc_std_ad() { indexed(); pshs(0x80); PC = ipr; cycle_counter -= 7; }
void M6809::Opc_std_ae() { indexed(); X = env.rd16(ipr); tst(X); cycle_counter -= 5; }
void M6809::Opc_std_af() { indexed(); env.wr16(ipr, X); tst(X); cycle_counter -= 5; }

void M6809::Opc_std_b0() { extended(); setA(sub(getA(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_b1() { extended(); cmp(getA(), env.rd8(epr)); cycle_counter -= 5; }
void M6809::Opc_std_b2() { extended(); setA(sbc(getA(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_b3() { extended(); D = sub(D, env.rd16(epr)); cycle_counter -= 7; }
void M6809::Opc_std_b4() { extended(); setA(and8(getA(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_b5() { extended(); and8(getA(), env.rd8(epr)); cycle_counter -= 5; }
void M6809::Opc_std_b6() { extended(); setA(env.rd8(epr)); tst(getA()); cycle_counter -= 5; }
void M6809::Opc_std_b7() { extended(); env.wr8(epr, getA()); tst(getA()); cycle_counter -= 5; }
void M6809::Opc_std_b8() { extended(); setA(xor8(getA(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_b9() { extended(); setA(adc(getA(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_ba() { extended(); setA(or8(getA(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_bb() { extended(); setA(add(getA(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_bc() { extended(); cmp(X, env.rd16(epr)); cycle_counter -= 7; }
void M6809::Opc_std_bd() { extended(); pshs(0x80); PC = epr; cycle_counter -= 8; }
void M6809::Opc_std_be() { extended(); X = env.rd16(epr); tst(X); cycle_counter -= 6; }
void M6809::Opc_std_bf() { extended(); env.wr16(epr, X); tst(X); cycle_counter -= 6; }

// -----------------------------------------------------------------------------
// B register group 0xC0-0xFF
// -----------------------------------------------------------------------------
void M6809::Opc_std_c0() { setB(sub(getB(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_c1() { cmp(getB(), env.rd8(PC++)); cycle_counter -= 2; }
void M6809::Opc_std_c2() { setB(sbc(getB(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_c3() { extended(); D = add(D, epr); cycle_counter -= 4; }
void M6809::Opc_std_c4() { setB(and8(getB(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_c5() { and8(getB(), env.rd8(PC++)); cycle_counter -= 2; }
void M6809::Opc_std_c6() { setB(env.rd8(PC++)); tst(getB()); cycle_counter -= 2; }
void M6809::Opc_std_c7() { illegal_std(0xC7); cycle_counter -= 1; }
void M6809::Opc_std_c8() { setB(xor8(getB(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_c9() { setB(adc(getB(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_ca() { setB(or8(getB(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_cb() { setB(add(getB(), env.rd8(PC++))); cycle_counter -= 2; }
void M6809::Opc_std_cc() { extended(); D = epr; tst(D); cycle_counter -= 3; }
void M6809::Opc_std_cd() { illegal_std(0xCD); cycle_counter -= 1; }
void M6809::Opc_std_ce() { extended(); U = epr; tst(U); cycle_counter -= 3; }
void M6809::Opc_std_cf() { illegal_std(0xCF); cycle_counter -= 1; }

void M6809::Opc_std_d0() { dpr(); setB(sub(getB(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_d1() { dpr(); cmp(getB(), env.rd8(dppr)); cycle_counter -= 4; }
void M6809::Opc_std_d2() { dpr(); setB(sbc(getB(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_d3() { dpr(); D = add(D, env.rd16(dppr)); cycle_counter -= 6; }
void M6809::Opc_std_d4() { dpr(); setB(and8(getB(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_d5() { dpr(); and8(getB(), env.rd8(dppr)); cycle_counter -= 4; }
void M6809::Opc_std_d6() { dpr(); setB(env.rd8(dppr)); tst(getB()); cycle_counter -= 4; }
void M6809::Opc_std_d7() { dpr(); env.wr8(dppr, getB()); tst(getB()); cycle_counter -= 4; }
void M6809::Opc_std_d8() { dpr(); setB(xor8(getB(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_d9() { dpr(); setB(adc(getB(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_da() { dpr(); setB(or8(getB(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_db() { dpr(); setB(add(getB(), env.rd8(dppr))); cycle_counter -= 4; }
void M6809::Opc_std_dc() { dpr(); D = env.rd16(dppr); tst(D); cycle_counter -= 5; }
void M6809::Opc_std_dd() { dpr(); env.wr16(dppr, D); tst(D); cycle_counter -= 5; }
void M6809::Opc_std_de() { dpr(); U = env.rd16(dppr); tst(U); cycle_counter -= 5; }
void M6809::Opc_std_df() { dpr(); env.wr16(dppr, U); tst(U); cycle_counter -= 5; }

void M6809::Opc_std_e0() { indexed(); setB(sub(getB(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_e1() { indexed(); cmp(getB(), env.rd8(ipr)); cycle_counter -= 4; }
void M6809::Opc_std_e2() { indexed(); setB(sbc(getB(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_e3() { indexed(); D = add(D, env.rd16(ipr)); cycle_counter -= 6; }
void M6809::Opc_std_e4() { indexed(); setB(and8(getB(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_e5() { indexed(); and8(getB(), env.rd8(ipr)); cycle_counter -= 4; }
void M6809::Opc_std_e6() { indexed(); setB(env.rd8(ipr)); tst(getB()); cycle_counter -= 4; }
void M6809::Opc_std_e7() { indexed(); env.wr8(ipr, getB()); tst(getB()); cycle_counter -= 4; }
void M6809::Opc_std_e8() { indexed(); setB(xor8(getB(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_e9() { indexed(); setB(adc(getB(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_ea() { indexed(); setB(or8(getB(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_eb() { indexed(); setB(add(getB(), env.rd8(ipr))); cycle_counter -= 4; }
void M6809::Opc_std_ec() { indexed(); D = env.rd16(ipr); tst(D); cycle_counter -= 5; }
void M6809::Opc_std_ed() { indexed(); env.wr16(ipr, D); tst(D); cycle_counter -= 5; }
void M6809::Opc_std_ee() { indexed(); U = env.rd16(ipr); tst(U); cycle_counter -= 5; }
void M6809::Opc_std_ef() { indexed(); env.wr16(ipr, U); tst(U); cycle_counter -= 5; }

void M6809::Opc_std_f0() { extended(); setB(sub(getB(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_f1() { extended(); cmp(getB(), env.rd8(epr)); cycle_counter -= 5; }
void M6809::Opc_std_f2() { extended(); setB(sbc(getB(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_f3() { extended(); D = add(D, env.rd16(epr)); cycle_counter -= 7; }
void M6809::Opc_std_f4() { extended(); setB(and8(getB(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_f5() { extended(); and8(getB(), env.rd8(epr)); cycle_counter -= 5; }
void M6809::Opc_std_f6() { extended(); setB(env.rd8(epr)); tst(getB()); cycle_counter -= 5; }
void M6809::Opc_std_f7() { extended(); env.wr8(epr, getB()); tst(getB()); cycle_counter -= 5; }
void M6809::Opc_std_f8() { extended(); setB(xor8(getB(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_f9() { extended(); setB(adc(getB(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_fa() { extended(); setB(or8(getB(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_fb() { extended(); setB(add(getB(), env.rd8(epr))); cycle_counter -= 5; }
void M6809::Opc_std_fc() { extended(); D = env.rd16(epr); tst(D); cycle_counter -= 6; }
void M6809::Opc_std_fd() { extended(); env.wr16(epr, D); tst(D); cycle_counter -= 6; }
void M6809::Opc_std_fe() { extended(); U = env.rd16(epr); tst(U); cycle_counter -= 6; }
void M6809::Opc_std_ff() { extended(); env.wr16(epr, U); tst(U); cycle_counter -= 6; }

// -----------------------------------------------------------------------------
// Jump table
// -----------------------------------------------------------------------------
M6809Opc_handler M6809Opc_std[256] = {
    &M6809::Opc_std_00, &M6809::Opc_std_01, &M6809::Opc_std_02, &M6809::Opc_std_03,
    &M6809::Opc_std_04, &M6809::Opc_std_05, &M6809::Opc_std_06, &M6809::Opc_std_07,
    &M6809::Opc_std_08, &M6809::Opc_std_09, &M6809::Opc_std_0a, &M6809::Opc_std_0b,
    &M6809::Opc_std_0c, &M6809::Opc_std_0d, &M6809::Opc_std_0e, &M6809::Opc_std_0f,
    &M6809::Opc_std_10, &M6809::Opc_std_11, &M6809::Opc_std_12, &M6809::Opc_std_13,
    &M6809::Opc_std_14, &M6809::Opc_std_15, &M6809::Opc_std_16, &M6809::Opc_std_17,
    &M6809::Opc_std_18, &M6809::Opc_std_19, &M6809::Opc_std_1a, &M6809::Opc_std_1b,
    &M6809::Opc_std_1c, &M6809::Opc_std_1d, &M6809::Opc_std_1e, &M6809::Opc_std_1f,
    &M6809::Opc_std_20, &M6809::Opc_std_21, &M6809::Opc_std_22, &M6809::Opc_std_23,
    &M6809::Opc_std_24, &M6809::Opc_std_25, &M6809::Opc_std_26, &M6809::Opc_std_27,
    &M6809::Opc_std_28, &M6809::Opc_std_29, &M6809::Opc_std_2a, &M6809::Opc_std_2b,
    &M6809::Opc_std_2c, &M6809::Opc_std_2d, &M6809::Opc_std_2e, &M6809::Opc_std_2f,
    &M6809::Opc_std_30, &M6809::Opc_std_31, &M6809::Opc_std_32, &M6809::Opc_std_33,
    &M6809::Opc_std_34, &M6809::Opc_std_35, &M6809::Opc_std_36, &M6809::Opc_std_37,
    &M6809::Opc_std_38, &M6809::Opc_std_39, &M6809::Opc_std_3a, &M6809::Opc_std_3b,
    &M6809::Opc_std_3c, &M6809::Opc_std_3d, &M6809::Opc_std_3e, &M6809::Opc_std_3f,
    &M6809::Opc_std_40, &M6809::Opc_std_41, &M6809::Opc_std_42, &M6809::Opc_std_43,
    &M6809::Opc_std_44, &M6809::Opc_std_45, &M6809::Opc_std_46, &M6809::Opc_std_47,
    &M6809::Opc_std_48, &M6809::Opc_std_49, &M6809::Opc_std_4a, &M6809::Opc_std_4b,
    &M6809::Opc_std_4c, &M6809::Opc_std_4d, &M6809::Opc_std_4e, &M6809::Opc_std_4f,
    &M6809::Opc_std_50, &M6809::Opc_std_51, &M6809::Opc_std_52, &M6809::Opc_std_53,
    &M6809::Opc_std_54, &M6809::Opc_std_55, &M6809::Opc_std_56, &M6809::Opc_std_57,
    &M6809::Opc_std_58, &M6809::Opc_std_59, &M6809::Opc_std_5a, &M6809::Opc_std_5b,
    &M6809::Opc_std_5c, &M6809::Opc_std_5d, &M6809::Opc_std_5e, &M6809::Opc_std_5f,
    &M6809::Opc_std_60, &M6809::Opc_std_61, &M6809::Opc_std_62, &M6809::Opc_std_63,
    &M6809::Opc_std_64, &M6809::Opc_std_65, &M6809::Opc_std_66, &M6809::Opc_std_67,
    &M6809::Opc_std_68, &M6809::Opc_std_69, &M6809::Opc_std_6a, &M6809::Opc_std_6b,
    &M6809::Opc_std_6c, &M6809::Opc_std_6d, &M6809::Opc_std_6e, &M6809::Opc_std_6f,
    &M6809::Opc_std_70, &M6809::Opc_std_71, &M6809::Opc_std_72, &M6809::Opc_std_73,
    &M6809::Opc_std_74, &M6809::Opc_std_75, &M6809::Opc_std_76, &M6809::Opc_std_77,
    &M6809::Opc_std_78, &M6809::Opc_std_79, &M6809::Opc_std_7a, &M6809::Opc_std_7b,
    &M6809::Opc_std_7c, &M6809::Opc_std_7d, &M6809::Opc_std_7e, &M6809::Opc_std_7f,
    &M6809::Opc_std_80, &M6809::Opc_std_81, &M6809::Opc_std_82, &M6809::Opc_std_83,
    &M6809::Opc_std_84, &M6809::Opc_std_85, &M6809::Opc_std_86, &M6809::Opc_std_87,
    &M6809::Opc_std_88, &M6809::Opc_std_89, &M6809::Opc_std_8a, &M6809::Opc_std_8b,
    &M6809::Opc_std_8c, &M6809::Opc_std_8d, &M6809::Opc_std_8e, &M6809::Opc_std_8f,
    &M6809::Opc_std_90, &M6809::Opc_std_91, &M6809::Opc_std_92, &M6809::Opc_std_93,
    &M6809::Opc_std_94, &M6809::Opc_std_95, &M6809::Opc_std_96, &M6809::Opc_std_97,
    &M6809::Opc_std_98, &M6809::Opc_std_99, &M6809::Opc_std_9a, &M6809::Opc_std_9b,
    &M6809::Opc_std_9c, &M6809::Opc_std_9d, &M6809::Opc_std_9e, &M6809::Opc_std_9f,
    &M6809::Opc_std_a0, &M6809::Opc_std_a1, &M6809::Opc_std_a2, &M6809::Opc_std_a3,
    &M6809::Opc_std_a4, &M6809::Opc_std_a5, &M6809::Opc_std_a6, &M6809::Opc_std_a7,
    &M6809::Opc_std_a8, &M6809::Opc_std_a9, &M6809::Opc_std_aa, &M6809::Opc_std_ab,
    &M6809::Opc_std_ac, &M6809::Opc_std_ad, &M6809::Opc_std_ae, &M6809::Opc_std_af,
    &M6809::Opc_std_b0, &M6809::Opc_std_b1, &M6809::Opc_std_b2, &M6809::Opc_std_b3,
    &M6809::Opc_std_b4, &M6809::Opc_std_b5, &M6809::Opc_std_b6, &M6809::Opc_std_b7,
    &M6809::Opc_std_b8, &M6809::Opc_std_b9, &M6809::Opc_std_ba, &M6809::Opc_std_bb,
    &M6809::Opc_std_bc, &M6809::Opc_std_bd, &M6809::Opc_std_be, &M6809::Opc_std_bf,
    &M6809::Opc_std_c0, &M6809::Opc_std_c1, &M6809::Opc_std_c2, &M6809::Opc_std_c3,
    &M6809::Opc_std_c4, &M6809::Opc_std_c5, &M6809::Opc_std_c6, &M6809::Opc_std_c7,
    &M6809::Opc_std_c8, &M6809::Opc_std_c9, &M6809::Opc_std_ca, &M6809::Opc_std_cb,
    &M6809::Opc_std_cc, &M6809::Opc_std_cd, &M6809::Opc_std_ce, &M6809::Opc_std_cf,
    &M6809::Opc_std_d0, &M6809::Opc_std_d1, &M6809::Opc_std_d2, &M6809::Opc_std_d3,
    &M6809::Opc_std_d4, &M6809::Opc_std_d5, &M6809::Opc_std_d6, &M6809::Opc_std_d7,
    &M6809::Opc_std_d8, &M6809::Opc_std_d9, &M6809::Opc_std_da, &M6809::Opc_std_db,
    &M6809::Opc_std_dc, &M6809::Opc_std_dd, &M6809::Opc_std_de, &M6809::Opc_std_df,
    &M6809::Opc_std_e0, &M6809::Opc_std_e1, &M6809::Opc_std_e2, &M6809::Opc_std_e3,
    &M6809::Opc_std_e4, &M6809::Opc_std_e5, &M6809::Opc_std_e6, &M6809::Opc_std_e7,
    &M6809::Opc_std_e8, &M6809::Opc_std_e9, &M6809::Opc_std_ea, &M6809::Opc_std_eb,
    &M6809::Opc_std_ec, &M6809::Opc_std_ed, &M6809::Opc_std_ee, &M6809::Opc_std_ef,
    &M6809::Opc_std_f0, &M6809::Opc_std_f1, &M6809::Opc_std_f2, &M6809::Opc_std_f3,
    &M6809::Opc_std_f4, &M6809::Opc_std_f5, &M6809::Opc_std_f6, &M6809::Opc_std_f7,
    &M6809::Opc_std_f8, &M6809::Opc_std_f9, &M6809::Opc_std_fa, &M6809::Opc_std_fb,
    &M6809::Opc_std_fc, &M6809::Opc_std_fd, &M6809::Opc_std_fe, &M6809::Opc_std_ff
};