#include "m6809.h"

/*
 * Adapted from the uploaded Oldspark Opc_10xx.cpp.
 * Keeps the original opcode semantics for page-0x10 opcodes.
 */

// -----------------------------------------------------------------------------
void M6809::Opc_10xx_00() { illegal_10xx(0x00); }
void M6809::Opc_10xx_01() { illegal_10xx(0x01); }
void M6809::Opc_10xx_02() { illegal_10xx(0x02); }
void M6809::Opc_10xx_03() { illegal_10xx(0x03); }
void M6809::Opc_10xx_04() { illegal_10xx(0x04); }
void M6809::Opc_10xx_05() { illegal_10xx(0x05); }
void M6809::Opc_10xx_06() { illegal_10xx(0x06); }
void M6809::Opc_10xx_07() { illegal_10xx(0x07); }
void M6809::Opc_10xx_08() { illegal_10xx(0x08); }
void M6809::Opc_10xx_09() { illegal_10xx(0x09); }
void M6809::Opc_10xx_0a() { illegal_10xx(0x0A); }
void M6809::Opc_10xx_0b() { illegal_10xx(0x0B); }
void M6809::Opc_10xx_0c() { illegal_10xx(0x0C); }
void M6809::Opc_10xx_0d() { illegal_10xx(0x0D); }
void M6809::Opc_10xx_0e() { illegal_10xx(0x0E); }
void M6809::Opc_10xx_0f() { illegal_10xx(0x0F); }

void M6809::Opc_10xx_10() { illegal_10xx(0x10); }
void M6809::Opc_10xx_11() { illegal_10xx(0x11); }
void M6809::Opc_10xx_12() { illegal_10xx(0x12); }
void M6809::Opc_10xx_13() { illegal_10xx(0x13); }
void M6809::Opc_10xx_14() { illegal_10xx(0x14); }
void M6809::Opc_10xx_15() { illegal_10xx(0x15); }
void M6809::Opc_10xx_16() { illegal_10xx(0x16); }
void M6809::Opc_10xx_17() { illegal_10xx(0x17); }
void M6809::Opc_10xx_18() { illegal_10xx(0x18); }
void M6809::Opc_10xx_19() { illegal_10xx(0x19); }
void M6809::Opc_10xx_1a() { illegal_10xx(0x1A); }
void M6809::Opc_10xx_1b() { illegal_10xx(0x1B); }
void M6809::Opc_10xx_1c() { illegal_10xx(0x1C); }
void M6809::Opc_10xx_1d() { illegal_10xx(0x1D); }
void M6809::Opc_10xx_1e() { illegal_10xx(0x1E); }
void M6809::Opc_10xx_1f() { illegal_10xx(0x1F); }

// -----------------------------------------------------------------------------
// Long branches
// -----------------------------------------------------------------------------
void M6809::Opc_10xx_20()
{
    illegal_10xx(0x20);
}

void M6809::Opc_10xx_21()
{
    // LBRN
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_22()
{
    // LBHI
    if ((CC & ZeroCarry) == 0) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_23()
{
    // LBLS
    if (((CC & ZeroCarry) == Zero) || ((CC & ZeroCarry) == Carry)) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_24()
{
    // LBHS / LBCC
    if ((CC & Carry) == 0) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_25()
{
    // LBLO / LBCS
    if ((CC & Carry) == Carry) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_26()
{
    // LBNE
    if ((CC & Zero) == 0) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_27()
{
    // LBEQ
    if ((CC & Zero) == Zero) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_28()
{
    // LBVC
    if ((CC & Overflow) == 0) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_29()
{
    // LBVS
    if ((CC & Overflow) == Overflow) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_2a()
{
    // LBPL
    if ((CC & Negative) == 0) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_2b()
{
    // LBMI
    if ((CC & Negative) == Negative) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_2c()
{
    // LBGE
    if (((CC & NegativeOverflow) == 0) ||
        ((CC & NegativeOverflow) == NegativeOverflow)) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_2d()
{
    // LBLT
    if (((CC & NegativeOverflow) == Negative) ||
        ((CC & NegativeOverflow) == Overflow)) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_2e()
{
    // LBGT
    if (((CC & 0x0E) == 0) || ((CC & 0x0E) == NegativeOverflow)) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_2f()
{
    // LBLE
    if (((CC & 0x0E) == Negative) ||
        ((CC & 0x0E) == Zero) ||
        ((CC & 0x0E) == Overflow) ||
        ((CC & 0x0E) == 0x0E)) {
        PC += (s16)env.rd16(PC);
    }
    PC += 2;
    cycle_counter -= 5;
}

void M6809::Opc_10xx_30() { illegal_10xx(0x30); }
void M6809::Opc_10xx_31() { illegal_10xx(0x31); }
void M6809::Opc_10xx_32() { illegal_10xx(0x32); }
void M6809::Opc_10xx_33() { illegal_10xx(0x33); }
void M6809::Opc_10xx_34() { illegal_10xx(0x34); }
void M6809::Opc_10xx_35() { illegal_10xx(0x35); }
void M6809::Opc_10xx_36() { illegal_10xx(0x36); }
void M6809::Opc_10xx_37() { illegal_10xx(0x37); }
void M6809::Opc_10xx_38() { illegal_10xx(0x38); }
void M6809::Opc_10xx_39() { illegal_10xx(0x39); }
void M6809::Opc_10xx_3a() { illegal_10xx(0x3A); }
void M6809::Opc_10xx_3b() { illegal_10xx(0x3B); }
void M6809::Opc_10xx_3c() { illegal_10xx(0x3C); }
void M6809::Opc_10xx_3d() { illegal_10xx(0x3D); }
void M6809::Opc_10xx_3e() { illegal_10xx(0x3E); }

void M6809::Opc_10xx_3f()
{
    // SWI2
    swi2();
    cycle_counter -= 20;
}

// -----------------------------------------------------------------------------
// 0x40-0x7F mostly illegal in this page
// -----------------------------------------------------------------------------
void M6809::Opc_10xx_40() { illegal_10xx(0x40); }
void M6809::Opc_10xx_41() { illegal_10xx(0x41); }
void M6809::Opc_10xx_42() { illegal_10xx(0x42); }
void M6809::Opc_10xx_43() { illegal_10xx(0x43); }
void M6809::Opc_10xx_44() { illegal_10xx(0x44); }
void M6809::Opc_10xx_45() { illegal_10xx(0x45); }
void M6809::Opc_10xx_46() { illegal_10xx(0x46); }
void M6809::Opc_10xx_47() { illegal_10xx(0x47); }
void M6809::Opc_10xx_48() { illegal_10xx(0x48); }
void M6809::Opc_10xx_49() { illegal_10xx(0x49); }
void M6809::Opc_10xx_4a() { illegal_10xx(0x4A); }
void M6809::Opc_10xx_4b() { illegal_10xx(0x4B); }
void M6809::Opc_10xx_4c() { illegal_10xx(0x4C); }
void M6809::Opc_10xx_4d() { illegal_10xx(0x4D); }
void M6809::Opc_10xx_4e() { illegal_10xx(0x4E); }
void M6809::Opc_10xx_4f() { illegal_10xx(0x4F); }

void M6809::Opc_10xx_50() { illegal_10xx(0x50); }
void M6809::Opc_10xx_51() { illegal_10xx(0x51); }
void M6809::Opc_10xx_52() { illegal_10xx(0x52); }
void M6809::Opc_10xx_53() { illegal_10xx(0x53); }
void M6809::Opc_10xx_54() { illegal_10xx(0x54); }
void M6809::Opc_10xx_55() { illegal_10xx(0x55); }
void M6809::Opc_10xx_56() { illegal_10xx(0x56); }
void M6809::Opc_10xx_57() { illegal_10xx(0x57); }
void M6809::Opc_10xx_58() { illegal_10xx(0x58); }
void M6809::Opc_10xx_59() { illegal_10xx(0x59); }
void M6809::Opc_10xx_5a() { illegal_10xx(0x5A); }
void M6809::Opc_10xx_5b() { illegal_10xx(0x5B); }
void M6809::Opc_10xx_5c() { illegal_10xx(0x5C); }
void M6809::Opc_10xx_5d() { illegal_10xx(0x5D); }
void M6809::Opc_10xx_5e() { illegal_10xx(0x5E); }
void M6809::Opc_10xx_5f() { illegal_10xx(0x5F); }

void M6809::Opc_10xx_60() { illegal_10xx(0x60); }
void M6809::Opc_10xx_61() { illegal_10xx(0x61); }
void M6809::Opc_10xx_62() { illegal_10xx(0x62); }
void M6809::Opc_10xx_63() { illegal_10xx(0x63); }
void M6809::Opc_10xx_64() { illegal_10xx(0x64); }
void M6809::Opc_10xx_65() { illegal_10xx(0x65); }
void M6809::Opc_10xx_66() { illegal_10xx(0x66); }
void M6809::Opc_10xx_67() { illegal_10xx(0x67); }
void M6809::Opc_10xx_68() { illegal_10xx(0x68); }
void M6809::Opc_10xx_69() { illegal_10xx(0x69); }
void M6809::Opc_10xx_6a() { illegal_10xx(0x6A); }
void M6809::Opc_10xx_6b() { illegal_10xx(0x6B); }
void M6809::Opc_10xx_6c() { illegal_10xx(0x6C); }
void M6809::Opc_10xx_6d() { illegal_10xx(0x6D); }
void M6809::Opc_10xx_6e() { illegal_10xx(0x6E); }
void M6809::Opc_10xx_6f() { illegal_10xx(0x6F); }

void M6809::Opc_10xx_70() { illegal_10xx(0x70); }
void M6809::Opc_10xx_71() { illegal_10xx(0x71); }
void M6809::Opc_10xx_72() { illegal_10xx(0x72); }
void M6809::Opc_10xx_73() { illegal_10xx(0x73); }
void M6809::Opc_10xx_74() { illegal_10xx(0x74); }
void M6809::Opc_10xx_75() { illegal_10xx(0x75); }
void M6809::Opc_10xx_76() { illegal_10xx(0x76); }
void M6809::Opc_10xx_77() { illegal_10xx(0x77); }
void M6809::Opc_10xx_78() { illegal_10xx(0x78); }
void M6809::Opc_10xx_79() { illegal_10xx(0x79); }
void M6809::Opc_10xx_7a() { illegal_10xx(0x7A); }
void M6809::Opc_10xx_7b() { illegal_10xx(0x7B); }
void M6809::Opc_10xx_7c() { illegal_10xx(0x7C); }
void M6809::Opc_10xx_7d() { illegal_10xx(0x7D); }
void M6809::Opc_10xx_7e() { illegal_10xx(0x7E); }
void M6809::Opc_10xx_7f() { illegal_10xx(0x7F); }

// -----------------------------------------------------------------------------
// CMPD / CMPY / LDY / STY / LDS / STS
// -----------------------------------------------------------------------------
void M6809::Opc_10xx_80() { illegal_10xx(0x80); }
void M6809::Opc_10xx_81() { illegal_10xx(0x81); }
void M6809::Opc_10xx_82() { illegal_10xx(0x82); }

void M6809::Opc_10xx_83()
{
    // CMPD immediate
    extended();
    cmp(D, epr);
    cycle_counter -= 5;
}

void M6809::Opc_10xx_84() { illegal_10xx(0x84); }
void M6809::Opc_10xx_85() { illegal_10xx(0x85); }
void M6809::Opc_10xx_86() { illegal_10xx(0x86); }
void M6809::Opc_10xx_87() { illegal_10xx(0x87); }
void M6809::Opc_10xx_88() { illegal_10xx(0x88); }
void M6809::Opc_10xx_89() { illegal_10xx(0x89); }
void M6809::Opc_10xx_8a() { illegal_10xx(0x8A); }
void M6809::Opc_10xx_8b() { illegal_10xx(0x8B); }

void M6809::Opc_10xx_8c()
{
    // CMPY immediate
    extended();
    cmp(Y, epr);
    cycle_counter -= 5;
}

void M6809::Opc_10xx_8d() { illegal_10xx(0x8D); }

void M6809::Opc_10xx_8e()
{
    // LDY immediate
    extended();
    Y = epr;
    tst(Y);
    cycle_counter -= 4;
}

void M6809::Opc_10xx_8f() { illegal_10xx(0x8F); }

void M6809::Opc_10xx_90() { illegal_10xx(0x90); }
void M6809::Opc_10xx_91() { illegal_10xx(0x91); }
void M6809::Opc_10xx_92() { illegal_10xx(0x92); }

void M6809::Opc_10xx_93()
{
    // CMPD direct
    dpr();
    cmp(D, env.rd16(dppr));
    cycle_counter -= 7;
}

void M6809::Opc_10xx_94() { illegal_10xx(0x94); }
void M6809::Opc_10xx_95() { illegal_10xx(0x95); }
void M6809::Opc_10xx_96() { illegal_10xx(0x96); }
void M6809::Opc_10xx_97() { illegal_10xx(0x97); }
void M6809::Opc_10xx_98() { illegal_10xx(0x98); }
void M6809::Opc_10xx_99() { illegal_10xx(0x99); }
void M6809::Opc_10xx_9a() { illegal_10xx(0x9A); }
void M6809::Opc_10xx_9b() { illegal_10xx(0x9B); }

void M6809::Opc_10xx_9c()
{
    // CMPY direct
    dpr();
    cmp(Y, env.rd16(dppr));
    cycle_counter -= 7;
}

void M6809::Opc_10xx_9d() { illegal_10xx(0x9D); }

void M6809::Opc_10xx_9e()
{
    // LDY direct
    dpr();
    Y = env.rd16(dppr);
    tst(Y);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_9f()
{
    // STY direct
    dpr();
    env.wr16(dppr, Y);
    tst(Y);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_a0() { illegal_10xx(0xA0); }
void M6809::Opc_10xx_a1() { illegal_10xx(0xA1); }
void M6809::Opc_10xx_a2() { illegal_10xx(0xA2); }

void M6809::Opc_10xx_a3()
{
    // CMPD indexed
    indexed();
    cmp(D, env.rd16(ipr));
    cycle_counter -= 7;
}

void M6809::Opc_10xx_a4() { illegal_10xx(0xA4); }
void M6809::Opc_10xx_a5() { illegal_10xx(0xA5); }
void M6809::Opc_10xx_a6() { illegal_10xx(0xA6); }
void M6809::Opc_10xx_a7() { illegal_10xx(0xA7); }
void M6809::Opc_10xx_a8() { illegal_10xx(0xA8); }
void M6809::Opc_10xx_a9() { illegal_10xx(0xA9); }
void M6809::Opc_10xx_aa() { illegal_10xx(0xAA); }
void M6809::Opc_10xx_ab() { illegal_10xx(0xAB); }

void M6809::Opc_10xx_ac()
{
    // CMPY indexed
    indexed();
    cmp(Y, env.rd16(ipr));
    cycle_counter -= 7;
}

void M6809::Opc_10xx_ad() { illegal_10xx(0xAD); }

void M6809::Opc_10xx_ae()
{
    // LDY indexed
    indexed();
    Y = env.rd16(ipr);
    tst(Y);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_af()
{
    // STY indexed
    indexed();
    env.wr16(ipr, Y);
    tst(Y);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_b0() { illegal_10xx(0xB0); }
void M6809::Opc_10xx_b1() { illegal_10xx(0xB1); }
void M6809::Opc_10xx_b2() { illegal_10xx(0xB2); }

void M6809::Opc_10xx_b3()
{
    // CMPD extended
    extended();
    cmp(D, env.rd16(epr));
    cycle_counter -= 8;
}

void M6809::Opc_10xx_b4() { illegal_10xx(0xB4); }
void M6809::Opc_10xx_b5() { illegal_10xx(0xB5); }
void M6809::Opc_10xx_b6() { illegal_10xx(0xB6); }
void M6809::Opc_10xx_b7() { illegal_10xx(0xB7); }
void M6809::Opc_10xx_b8() { illegal_10xx(0xB8); }
void M6809::Opc_10xx_b9() { illegal_10xx(0xB9); }
void M6809::Opc_10xx_ba() { illegal_10xx(0xBA); }
void M6809::Opc_10xx_bb() { illegal_10xx(0xBB); }

void M6809::Opc_10xx_bc()
{
    // CMPY extended
    extended();
    cmp(Y, env.rd16(epr));
    cycle_counter -= 8;
}

void M6809::Opc_10xx_bd() { illegal_10xx(0xBD); }

void M6809::Opc_10xx_be()
{
    // LDY extended
    extended();
    Y = env.rd16(epr);
    tst(Y);
    cycle_counter -= 7;
}

void M6809::Opc_10xx_bf()
{
    // STY extended
    extended();
    env.wr16(epr, Y);
    tst(Y);
    cycle_counter -= 7;
}

void M6809::Opc_10xx_c0() { illegal_10xx(0xC0); }
void M6809::Opc_10xx_c1() { illegal_10xx(0xC1); }
void M6809::Opc_10xx_c2() { illegal_10xx(0xC2); }
void M6809::Opc_10xx_c3() { illegal_10xx(0xC3); }
void M6809::Opc_10xx_c4() { illegal_10xx(0xC4); }
void M6809::Opc_10xx_c5() { illegal_10xx(0xC5); }
void M6809::Opc_10xx_c6() { illegal_10xx(0xC6); }
void M6809::Opc_10xx_c7() { illegal_10xx(0xC7); }
void M6809::Opc_10xx_c8() { illegal_10xx(0xC8); }
void M6809::Opc_10xx_c9() { illegal_10xx(0xC9); }
void M6809::Opc_10xx_ca() { illegal_10xx(0xCA); }
void M6809::Opc_10xx_cb() { illegal_10xx(0xCB); }
void M6809::Opc_10xx_cc() { illegal_10xx(0xCC); }
void M6809::Opc_10xx_cd() { illegal_10xx(0xCD); }

void M6809::Opc_10xx_ce()
{
    // LDS immediate
    extended();
    S = epr;
    tst(S);
    cycle_counter -= 4;
}

void M6809::Opc_10xx_cf() { illegal_10xx(0xCF); }

void M6809::Opc_10xx_d0() { illegal_10xx(0xD0); }
void M6809::Opc_10xx_d1() { illegal_10xx(0xD1); }
void M6809::Opc_10xx_d2() { illegal_10xx(0xD2); }
void M6809::Opc_10xx_d3() { illegal_10xx(0xD3); }
void M6809::Opc_10xx_d4() { illegal_10xx(0xD4); }
void M6809::Opc_10xx_d5() { illegal_10xx(0xD5); }
void M6809::Opc_10xx_d6() { illegal_10xx(0xD6); }
void M6809::Opc_10xx_d7() { illegal_10xx(0xD7); }
void M6809::Opc_10xx_d8() { illegal_10xx(0xD8); }
void M6809::Opc_10xx_d9() { illegal_10xx(0xD9); }
void M6809::Opc_10xx_da() { illegal_10xx(0xDA); }
void M6809::Opc_10xx_db() { illegal_10xx(0xDB); }
void M6809::Opc_10xx_dc() { illegal_10xx(0xDC); }
void M6809::Opc_10xx_dd() { illegal_10xx(0xDD); }

void M6809::Opc_10xx_de()
{
    // LDS direct
    dpr();
    S = env.rd16(dppr);
    tst(S);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_df()
{
    // STS direct
    dpr();
    env.wr16(dppr, S);
    tst(S);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_e0() { illegal_10xx(0xE0); }
void M6809::Opc_10xx_e1() { illegal_10xx(0xE1); }
void M6809::Opc_10xx_e2() { illegal_10xx(0xE2); }
void M6809::Opc_10xx_e3() { illegal_10xx(0xE3); }
void M6809::Opc_10xx_e4() { illegal_10xx(0xE4); }
void M6809::Opc_10xx_e5() { illegal_10xx(0xE5); }
void M6809::Opc_10xx_e6() { illegal_10xx(0xE6); }
void M6809::Opc_10xx_e7() { illegal_10xx(0xE7); }
void M6809::Opc_10xx_e8() { illegal_10xx(0xE8); }
void M6809::Opc_10xx_e9() { illegal_10xx(0xE9); }
void M6809::Opc_10xx_ea() { illegal_10xx(0xEA); }
void M6809::Opc_10xx_eb() { illegal_10xx(0xEB); }
void M6809::Opc_10xx_ec() { illegal_10xx(0xEC); }
void M6809::Opc_10xx_ed() { illegal_10xx(0xED); }

void M6809::Opc_10xx_ee()
{
    // LDS indexed
    indexed();
    S = env.rd16(ipr);
    tst(S);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_ef()
{
    // STS indexed
    indexed();
    env.wr16(ipr, S);
    tst(S);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_f0() { illegal_10xx(0xF0); }
void M6809::Opc_10xx_f1() { illegal_10xx(0xF1); }
void M6809::Opc_10xx_f2() { illegal_10xx(0xF2); }
void M6809::Opc_10xx_f3() { illegal_10xx(0xF3); }
void M6809::Opc_10xx_f4() { illegal_10xx(0xF4); }
void M6809::Opc_10xx_f5() { illegal_10xx(0xF5); }
void M6809::Opc_10xx_f6() { illegal_10xx(0xF6); }
void M6809::Opc_10xx_f7() { illegal_10xx(0xF7); }
void M6809::Opc_10xx_f8() { illegal_10xx(0xF8); }
void M6809::Opc_10xx_f9() { illegal_10xx(0xF9); }
void M6809::Opc_10xx_fa() { illegal_10xx(0xFA); }
void M6809::Opc_10xx_fb() { illegal_10xx(0xFB); }
void M6809::Opc_10xx_fc() { illegal_10xx(0xFC); }
void M6809::Opc_10xx_fd() { illegal_10xx(0xFD); }

void M6809::Opc_10xx_fe()
{
    // LDS extended
    extended();
    S = env.rd16(epr);
    tst(S);
    cycle_counter -= 6;
}

void M6809::Opc_10xx_ff()
{
    // STS extended
    extended();
    env.wr16(epr, S);
    tst(S);
    cycle_counter -= 6;
}

// -----------------------------------------------------------------------------
// Jump table
// -----------------------------------------------------------------------------
M6809Opc_handler M6809Opc_10xx[256] = {
    &M6809::Opc_10xx_00, &M6809::Opc_10xx_01, &M6809::Opc_10xx_02, &M6809::Opc_10xx_03,
    &M6809::Opc_10xx_04, &M6809::Opc_10xx_05, &M6809::Opc_10xx_06, &M6809::Opc_10xx_07,
    &M6809::Opc_10xx_08, &M6809::Opc_10xx_09, &M6809::Opc_10xx_0a, &M6809::Opc_10xx_0b,
    &M6809::Opc_10xx_0c, &M6809::Opc_10xx_0d, &M6809::Opc_10xx_0e, &M6809::Opc_10xx_0f,
    &M6809::Opc_10xx_10, &M6809::Opc_10xx_11, &M6809::Opc_10xx_12, &M6809::Opc_10xx_13,
    &M6809::Opc_10xx_14, &M6809::Opc_10xx_15, &M6809::Opc_10xx_16, &M6809::Opc_10xx_17,
    &M6809::Opc_10xx_18, &M6809::Opc_10xx_19, &M6809::Opc_10xx_1a, &M6809::Opc_10xx_1b,
    &M6809::Opc_10xx_1c, &M6809::Opc_10xx_1d, &M6809::Opc_10xx_1e, &M6809::Opc_10xx_1f,
    &M6809::Opc_10xx_20, &M6809::Opc_10xx_21, &M6809::Opc_10xx_22, &M6809::Opc_10xx_23,
    &M6809::Opc_10xx_24, &M6809::Opc_10xx_25, &M6809::Opc_10xx_26, &M6809::Opc_10xx_27,
    &M6809::Opc_10xx_28, &M6809::Opc_10xx_29, &M6809::Opc_10xx_2a, &M6809::Opc_10xx_2b,
    &M6809::Opc_10xx_2c, &M6809::Opc_10xx_2d, &M6809::Opc_10xx_2e, &M6809::Opc_10xx_2f,
    &M6809::Opc_10xx_30, &M6809::Opc_10xx_31, &M6809::Opc_10xx_32, &M6809::Opc_10xx_33,
    &M6809::Opc_10xx_34, &M6809::Opc_10xx_35, &M6809::Opc_10xx_36, &M6809::Opc_10xx_37,
    &M6809::Opc_10xx_38, &M6809::Opc_10xx_39, &M6809::Opc_10xx_3a, &M6809::Opc_10xx_3b,
    &M6809::Opc_10xx_3c, &M6809::Opc_10xx_3d, &M6809::Opc_10xx_3e, &M6809::Opc_10xx_3f,
    &M6809::Opc_10xx_40, &M6809::Opc_10xx_41, &M6809::Opc_10xx_42, &M6809::Opc_10xx_43,
    &M6809::Opc_10xx_44, &M6809::Opc_10xx_45, &M6809::Opc_10xx_46, &M6809::Opc_10xx_47,
    &M6809::Opc_10xx_48, &M6809::Opc_10xx_49, &M6809::Opc_10xx_4a, &M6809::Opc_10xx_4b,
    &M6809::Opc_10xx_4c, &M6809::Opc_10xx_4d, &M6809::Opc_10xx_4e, &M6809::Opc_10xx_4f,
    &M6809::Opc_10xx_50, &M6809::Opc_10xx_51, &M6809::Opc_10xx_52, &M6809::Opc_10xx_53,
    &M6809::Opc_10xx_54, &M6809::Opc_10xx_55, &M6809::Opc_10xx_56, &M6809::Opc_10xx_57,
    &M6809::Opc_10xx_58, &M6809::Opc_10xx_59, &M6809::Opc_10xx_5a, &M6809::Opc_10xx_5b,
    &M6809::Opc_10xx_5c, &M6809::Opc_10xx_5d, &M6809::Opc_10xx_5e, &M6809::Opc_10xx_5f,
    &M6809::Opc_10xx_60, &M6809::Opc_10xx_61, &M6809::Opc_10xx_62, &M6809::Opc_10xx_63,
    &M6809::Opc_10xx_64, &M6809::Opc_10xx_65, &M6809::Opc_10xx_66, &M6809::Opc_10xx_67,
    &M6809::Opc_10xx_68, &M6809::Opc_10xx_69, &M6809::Opc_10xx_6a, &M6809::Opc_10xx_6b,
    &M6809::Opc_10xx_6c, &M6809::Opc_10xx_6d, &M6809::Opc_10xx_6e, &M6809::Opc_10xx_6f,
    &M6809::Opc_10xx_70, &M6809::Opc_10xx_71, &M6809::Opc_10xx_72, &M6809::Opc_10xx_73,
    &M6809::Opc_10xx_74, &M6809::Opc_10xx_75, &M6809::Opc_10xx_76, &M6809::Opc_10xx_77,
    &M6809::Opc_10xx_78, &M6809::Opc_10xx_79, &M6809::Opc_10xx_7a, &M6809::Opc_10xx_7b,
    &M6809::Opc_10xx_7c, &M6809::Opc_10xx_7d, &M6809::Opc_10xx_7e, &M6809::Opc_10xx_7f,
    &M6809::Opc_10xx_80, &M6809::Opc_10xx_81, &M6809::Opc_10xx_82, &M6809::Opc_10xx_83,
    &M6809::Opc_10xx_84, &M6809::Opc_10xx_85, &M6809::Opc_10xx_86, &M6809::Opc_10xx_87,
    &M6809::Opc_10xx_88, &M6809::Opc_10xx_89, &M6809::Opc_10xx_8a, &M6809::Opc_10xx_8b,
    &M6809::Opc_10xx_8c, &M6809::Opc_10xx_8d, &M6809::Opc_10xx_8e, &M6809::Opc_10xx_8f,
    &M6809::Opc_10xx_90, &M6809::Opc_10xx_91, &M6809::Opc_10xx_92, &M6809::Opc_10xx_93,
    &M6809::Opc_10xx_94, &M6809::Opc_10xx_95, &M6809::Opc_10xx_96, &M6809::Opc_10xx_97,
    &M6809::Opc_10xx_98, &M6809::Opc_10xx_99, &M6809::Opc_10xx_9a, &M6809::Opc_10xx_9b,
    &M6809::Opc_10xx_9c, &M6809::Opc_10xx_9d, &M6809::Opc_10xx_9e, &M6809::Opc_10xx_9f,
    &M6809::Opc_10xx_a0, &M6809::Opc_10xx_a1, &M6809::Opc_10xx_a2, &M6809::Opc_10xx_a3,
    &M6809::Opc_10xx_a4, &M6809::Opc_10xx_a5, &M6809::Opc_10xx_a6, &M6809::Opc_10xx_a7,
    &M6809::Opc_10xx_a8, &M6809::Opc_10xx_a9, &M6809::Opc_10xx_aa, &M6809::Opc_10xx_ab,
    &M6809::Opc_10xx_ac, &M6809::Opc_10xx_ad, &M6809::Opc_10xx_ae, &M6809::Opc_10xx_af,
    &M6809::Opc_10xx_b0, &M6809::Opc_10xx_b1, &M6809::Opc_10xx_b2, &M6809::Opc_10xx_b3,
    &M6809::Opc_10xx_b4, &M6809::Opc_10xx_b5, &M6809::Opc_10xx_b6, &M6809::Opc_10xx_b7,
    &M6809::Opc_10xx_b8, &M6809::Opc_10xx_b9, &M6809::Opc_10xx_ba, &M6809::Opc_10xx_bb,
    &M6809::Opc_10xx_bc, &M6809::Opc_10xx_bd, &M6809::Opc_10xx_be, &M6809::Opc_10xx_bf,
    &M6809::Opc_10xx_c0, &M6809::Opc_10xx_c1, &M6809::Opc_10xx_c2, &M6809::Opc_10xx_c3,
    &M6809::Opc_10xx_c4, &M6809::Opc_10xx_c5, &M6809::Opc_10xx_c6, &M6809::Opc_10xx_c7,
    &M6809::Opc_10xx_c8, &M6809::Opc_10xx_c9, &M6809::Opc_10xx_ca, &M6809::Opc_10xx_cb,
    &M6809::Opc_10xx_cc, &M6809::Opc_10xx_cd, &M6809::Opc_10xx_ce, &M6809::Opc_10xx_cf,
    &M6809::Opc_10xx_d0, &M6809::Opc_10xx_d1, &M6809::Opc_10xx_d2, &M6809::Opc_10xx_d3,
    &M6809::Opc_10xx_d4, &M6809::Opc_10xx_d5, &M6809::Opc_10xx_d6, &M6809::Opc_10xx_d7,
    &M6809::Opc_10xx_d8, &M6809::Opc_10xx_d9, &M6809::Opc_10xx_da, &M6809::Opc_10xx_db,
    &M6809::Opc_10xx_dc, &M6809::Opc_10xx_dd, &M6809::Opc_10xx_de, &M6809::Opc_10xx_df,
    &M6809::Opc_10xx_e0, &M6809::Opc_10xx_e1, &M6809::Opc_10xx_e2, &M6809::Opc_10xx_e3,
    &M6809::Opc_10xx_e4, &M6809::Opc_10xx_e5, &M6809::Opc_10xx_e6, &M6809::Opc_10xx_e7,
    &M6809::Opc_10xx_e8, &M6809::Opc_10xx_e9, &M6809::Opc_10xx_ea, &M6809::Opc_10xx_eb,
    &M6809::Opc_10xx_ec, &M6809::Opc_10xx_ed, &M6809::Opc_10xx_ee, &M6809::Opc_10xx_ef,
    &M6809::Opc_10xx_f0, &M6809::Opc_10xx_f1, &M6809::Opc_10xx_f2, &M6809::Opc_10xx_f3,
    &M6809::Opc_10xx_f4, &M6809::Opc_10xx_f5, &M6809::Opc_10xx_f6, &M6809::Opc_10xx_f7,
    &M6809::Opc_10xx_f8, &M6809::Opc_10xx_f9, &M6809::Opc_10xx_fa, &M6809::Opc_10xx_fb,
    &M6809::Opc_10xx_fc, &M6809::Opc_10xx_fd, &M6809::Opc_10xx_fe, &M6809::Opc_10xx_ff
};
