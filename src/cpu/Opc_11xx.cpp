#include "m6809.h"

/*
 * Adapted from the uploaded Oldspark Opc_11xx.cpp.
 * Keeps the original opcode semantics for page-0x11 opcodes.
 */

#define OP11_ILLEGAL(hexname, value)                \
void M6809::Opc_11xx_##hexname()                    \
{                                                   \
    illegal_11xx(value);                            \
}

// -----------------------------------------------------------------------------
// 0x00 - 0x3E : mostly illegal
// -----------------------------------------------------------------------------
OP11_ILLEGAL(00, 0x00)
OP11_ILLEGAL(01, 0x01)
OP11_ILLEGAL(02, 0x02)
OP11_ILLEGAL(03, 0x03)
OP11_ILLEGAL(04, 0x04)
OP11_ILLEGAL(05, 0x05)
OP11_ILLEGAL(06, 0x06)
OP11_ILLEGAL(07, 0x07)
OP11_ILLEGAL(08, 0x08)
OP11_ILLEGAL(09, 0x09)
OP11_ILLEGAL(0a, 0x0A)
OP11_ILLEGAL(0b, 0x0B)
OP11_ILLEGAL(0c, 0x0C)
OP11_ILLEGAL(0d, 0x0D)
OP11_ILLEGAL(0e, 0x0E)
OP11_ILLEGAL(0f, 0x0F)

OP11_ILLEGAL(10, 0x10)
OP11_ILLEGAL(11, 0x11)
OP11_ILLEGAL(12, 0x12)
OP11_ILLEGAL(13, 0x13)
OP11_ILLEGAL(14, 0x14)
OP11_ILLEGAL(15, 0x15)
OP11_ILLEGAL(16, 0x16)
OP11_ILLEGAL(17, 0x17)
OP11_ILLEGAL(18, 0x18)
OP11_ILLEGAL(19, 0x19)
OP11_ILLEGAL(1a, 0x1A)
OP11_ILLEGAL(1b, 0x1B)
OP11_ILLEGAL(1c, 0x1C)
OP11_ILLEGAL(1d, 0x1D)
OP11_ILLEGAL(1e, 0x1E)
OP11_ILLEGAL(1f, 0x1F)

OP11_ILLEGAL(20, 0x20)
OP11_ILLEGAL(21, 0x21)
OP11_ILLEGAL(22, 0x22)
OP11_ILLEGAL(23, 0x23)
OP11_ILLEGAL(24, 0x24)
OP11_ILLEGAL(25, 0x25)
OP11_ILLEGAL(26, 0x26)
OP11_ILLEGAL(27, 0x27)
OP11_ILLEGAL(28, 0x28)
OP11_ILLEGAL(29, 0x29)
OP11_ILLEGAL(2a, 0x2A)
OP11_ILLEGAL(2b, 0x2B)
OP11_ILLEGAL(2c, 0x2C)
OP11_ILLEGAL(2d, 0x2D)
OP11_ILLEGAL(2e, 0x2E)
OP11_ILLEGAL(2f, 0x2F)

OP11_ILLEGAL(30, 0x30)
OP11_ILLEGAL(31, 0x31)
OP11_ILLEGAL(32, 0x32)
OP11_ILLEGAL(33, 0x33)
OP11_ILLEGAL(34, 0x34)
OP11_ILLEGAL(35, 0x35)
OP11_ILLEGAL(36, 0x36)
OP11_ILLEGAL(37, 0x37)
OP11_ILLEGAL(38, 0x38)
OP11_ILLEGAL(39, 0x39)
OP11_ILLEGAL(3a, 0x3A)
OP11_ILLEGAL(3b, 0x3B)
OP11_ILLEGAL(3c, 0x3C)
OP11_ILLEGAL(3d, 0x3D)
OP11_ILLEGAL(3e, 0x3E)

// -----------------------------------------------------------------------------
// 0x3F : SWI3
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_3f()
{
    swi3();
    cycle_counter -= 20;
}

// -----------------------------------------------------------------------------
// 0x40 - 0x82 : illegal
// -----------------------------------------------------------------------------
OP11_ILLEGAL(40, 0x40)
OP11_ILLEGAL(41, 0x41)
OP11_ILLEGAL(42, 0x42)
OP11_ILLEGAL(43, 0x43)
OP11_ILLEGAL(44, 0x44)
OP11_ILLEGAL(45, 0x45)
OP11_ILLEGAL(46, 0x46)
OP11_ILLEGAL(47, 0x47)
OP11_ILLEGAL(48, 0x48)
OP11_ILLEGAL(49, 0x49)
OP11_ILLEGAL(4a, 0x4A)
OP11_ILLEGAL(4b, 0x4B)
OP11_ILLEGAL(4c, 0x4C)
OP11_ILLEGAL(4d, 0x4D)
OP11_ILLEGAL(4e, 0x4E)
OP11_ILLEGAL(4f, 0x4F)

OP11_ILLEGAL(50, 0x50)
OP11_ILLEGAL(51, 0x51)
OP11_ILLEGAL(52, 0x52)
OP11_ILLEGAL(53, 0x53)
OP11_ILLEGAL(54, 0x54)
OP11_ILLEGAL(55, 0x55)
OP11_ILLEGAL(56, 0x56)
OP11_ILLEGAL(57, 0x57)
OP11_ILLEGAL(58, 0x58)
OP11_ILLEGAL(59, 0x59)
OP11_ILLEGAL(5a, 0x5A)
OP11_ILLEGAL(5b, 0x5B)
OP11_ILLEGAL(5c, 0x5C)
OP11_ILLEGAL(5d, 0x5D)
OP11_ILLEGAL(5e, 0x5E)
OP11_ILLEGAL(5f, 0x5F)

OP11_ILLEGAL(60, 0x60)
OP11_ILLEGAL(61, 0x61)
OP11_ILLEGAL(62, 0x62)
OP11_ILLEGAL(63, 0x63)
OP11_ILLEGAL(64, 0x64)
OP11_ILLEGAL(65, 0x65)
OP11_ILLEGAL(66, 0x66)
OP11_ILLEGAL(67, 0x67)
OP11_ILLEGAL(68, 0x68)
OP11_ILLEGAL(69, 0x69)
OP11_ILLEGAL(6a, 0x6A)
OP11_ILLEGAL(6b, 0x6B)
OP11_ILLEGAL(6c, 0x6C)
OP11_ILLEGAL(6d, 0x6D)
OP11_ILLEGAL(6e, 0x6E)
OP11_ILLEGAL(6f, 0x6F)

OP11_ILLEGAL(70, 0x70)
OP11_ILLEGAL(71, 0x71)
OP11_ILLEGAL(72, 0x72)
OP11_ILLEGAL(73, 0x73)
OP11_ILLEGAL(74, 0x74)
OP11_ILLEGAL(75, 0x75)
OP11_ILLEGAL(76, 0x76)
OP11_ILLEGAL(77, 0x77)
OP11_ILLEGAL(78, 0x78)
OP11_ILLEGAL(79, 0x79)
OP11_ILLEGAL(7a, 0x7A)
OP11_ILLEGAL(7b, 0x7B)
OP11_ILLEGAL(7c, 0x7C)
OP11_ILLEGAL(7d, 0x7D)
OP11_ILLEGAL(7e, 0x7E)
OP11_ILLEGAL(7f, 0x7F)

OP11_ILLEGAL(80, 0x80)
OP11_ILLEGAL(81, 0x81)
OP11_ILLEGAL(82, 0x82)

// -----------------------------------------------------------------------------
// 0x83 : CMPU immediate
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_83()
{
    extended();
    cmp(U, epr);
    cycle_counter -= 5;
}

// -----------------------------------------------------------------------------
OP11_ILLEGAL(84, 0x84)
OP11_ILLEGAL(85, 0x85)
OP11_ILLEGAL(86, 0x86)
OP11_ILLEGAL(87, 0x87)
OP11_ILLEGAL(88, 0x88)
OP11_ILLEGAL(89, 0x89)
OP11_ILLEGAL(8a, 0x8A)
OP11_ILLEGAL(8b, 0x8B)

// -----------------------------------------------------------------------------
// 0x8C : CMPS immediate
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_8c()
{
    extended();
    cmp(S, epr);
    cycle_counter -= 5;
}

// -----------------------------------------------------------------------------
OP11_ILLEGAL(8d, 0x8D)
OP11_ILLEGAL(8e, 0x8E)
OP11_ILLEGAL(8f, 0x8F)

OP11_ILLEGAL(90, 0x90)
OP11_ILLEGAL(91, 0x91)
OP11_ILLEGAL(92, 0x92)

// -----------------------------------------------------------------------------
// 0x93 : CMPU direct
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_93()
{
    dpr();
    cmp(U, env.rd16(dppr));
    cycle_counter -= 7;
}

// -----------------------------------------------------------------------------
OP11_ILLEGAL(94, 0x94)
OP11_ILLEGAL(95, 0x95)
OP11_ILLEGAL(96, 0x96)
OP11_ILLEGAL(97, 0x97)
OP11_ILLEGAL(98, 0x98)
OP11_ILLEGAL(99, 0x99)
OP11_ILLEGAL(9a, 0x9A)
OP11_ILLEGAL(9b, 0x9B)

// -----------------------------------------------------------------------------
// 0x9C : CMPS direct
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_9c()
{
    dpr();
    cmp(S, env.rd16(dppr));
    cycle_counter -= 7;
}

// -----------------------------------------------------------------------------
OP11_ILLEGAL(9d, 0x9D)
OP11_ILLEGAL(9e, 0x9E)
OP11_ILLEGAL(9f, 0x9F)

OP11_ILLEGAL(a0, 0xA0)
OP11_ILLEGAL(a1, 0xA1)
OP11_ILLEGAL(a2, 0xA2)

// -----------------------------------------------------------------------------
// 0xA3 : CMPU indexed
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_a3()
{
    indexed();
    cmp(U, env.rd16(ipr));
    cycle_counter -= 7;
}

// -----------------------------------------------------------------------------
OP11_ILLEGAL(a4, 0xA4)
OP11_ILLEGAL(a5, 0xA5)
OP11_ILLEGAL(a6, 0xA6)
OP11_ILLEGAL(a7, 0xA7)
OP11_ILLEGAL(a8, 0xA8)
OP11_ILLEGAL(a9, 0xA9)
OP11_ILLEGAL(aa, 0xAA)
OP11_ILLEGAL(ab, 0xAB)

// -----------------------------------------------------------------------------
// 0xAC : CMPS indexed
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_ac()
{
    indexed();
    cmp(S, env.rd16(ipr));
    cycle_counter -= 7;
}

// -----------------------------------------------------------------------------
OP11_ILLEGAL(ad, 0xAD)
OP11_ILLEGAL(ae, 0xAE)
OP11_ILLEGAL(af, 0xAF)

OP11_ILLEGAL(b0, 0xB0)
OP11_ILLEGAL(b1, 0xB1)
OP11_ILLEGAL(b2, 0xB2)

// -----------------------------------------------------------------------------
// 0xB3 : CMPU extended
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_b3()
{
    extended();
    cmp(U, env.rd16(epr));
    cycle_counter -= 8;
}

// -----------------------------------------------------------------------------
OP11_ILLEGAL(b4, 0xB4)
OP11_ILLEGAL(b5, 0xB5)
OP11_ILLEGAL(b6, 0xB6)
OP11_ILLEGAL(b7, 0xB7)
OP11_ILLEGAL(b8, 0xB8)
OP11_ILLEGAL(b9, 0xB9)
OP11_ILLEGAL(ba, 0xBA)
OP11_ILLEGAL(bb, 0xBB)

// -----------------------------------------------------------------------------
// 0xBC : CMPS extended
// -----------------------------------------------------------------------------
void M6809::Opc_11xx_bc()
{
    extended();
    cmp(S, env.rd16(epr));
    cycle_counter -= 8;
}

// -----------------------------------------------------------------------------
OP11_ILLEGAL(bd, 0xBD)
OP11_ILLEGAL(be, 0xBE)
OP11_ILLEGAL(bf, 0xBF)

OP11_ILLEGAL(c0, 0xC0)
OP11_ILLEGAL(c1, 0xC1)
OP11_ILLEGAL(c2, 0xC2)
OP11_ILLEGAL(c3, 0xC3)
OP11_ILLEGAL(c4, 0xC4)
OP11_ILLEGAL(c5, 0xC5)
OP11_ILLEGAL(c6, 0xC6)
OP11_ILLEGAL(c7, 0xC7)
OP11_ILLEGAL(c8, 0xC8)
OP11_ILLEGAL(c9, 0xC9)
OP11_ILLEGAL(ca, 0xCA)
OP11_ILLEGAL(cb, 0xCB)
OP11_ILLEGAL(cc, 0xCC)
OP11_ILLEGAL(cd, 0xCD)
OP11_ILLEGAL(ce, 0xCE)
OP11_ILLEGAL(cf, 0xCF)

OP11_ILLEGAL(d0, 0xD0)
OP11_ILLEGAL(d1, 0xD1)
OP11_ILLEGAL(d2, 0xD2)
OP11_ILLEGAL(d3, 0xD3)
OP11_ILLEGAL(d4, 0xD4)
OP11_ILLEGAL(d5, 0xD5)
OP11_ILLEGAL(d6, 0xD6)
OP11_ILLEGAL(d7, 0xD7)
OP11_ILLEGAL(d8, 0xD8)
OP11_ILLEGAL(d9, 0xD9)
OP11_ILLEGAL(da, 0xDA)
OP11_ILLEGAL(db, 0xDB)
OP11_ILLEGAL(dc, 0xDC)
OP11_ILLEGAL(dd, 0xDD)
OP11_ILLEGAL(de, 0xDE)
OP11_ILLEGAL(df, 0xDF)

OP11_ILLEGAL(e0, 0xE0)
OP11_ILLEGAL(e1, 0xE1)
OP11_ILLEGAL(e2, 0xE2)
OP11_ILLEGAL(e3, 0xE3)
OP11_ILLEGAL(e4, 0xE4)
OP11_ILLEGAL(e5, 0xE5)
OP11_ILLEGAL(e6, 0xE6)
OP11_ILLEGAL(e7, 0xE7)
OP11_ILLEGAL(e8, 0xE8)
OP11_ILLEGAL(e9, 0xE9)
OP11_ILLEGAL(ea, 0xEA)
OP11_ILLEGAL(eb, 0xEB)
OP11_ILLEGAL(ec, 0xEC)
OP11_ILLEGAL(ed, 0xED)
OP11_ILLEGAL(ee, 0xEE)
OP11_ILLEGAL(ef, 0xEF)

OP11_ILLEGAL(f0, 0xF0)
OP11_ILLEGAL(f1, 0xF1)
OP11_ILLEGAL(f2, 0xF2)
OP11_ILLEGAL(f3, 0xF3)
OP11_ILLEGAL(f4, 0xF4)
OP11_ILLEGAL(f5, 0xF5)
OP11_ILLEGAL(f6, 0xF6)
OP11_ILLEGAL(f7, 0xF7)
OP11_ILLEGAL(f8, 0xF8)
OP11_ILLEGAL(f9, 0xF9)
OP11_ILLEGAL(fa, 0xFA)
OP11_ILLEGAL(fb, 0xFB)
OP11_ILLEGAL(fc, 0xFC)
OP11_ILLEGAL(fd, 0xFD)
OP11_ILLEGAL(fe, 0xFE)
OP11_ILLEGAL(ff, 0xFF)

#undef OP11_ILLEGAL

// -----------------------------------------------------------------------------
// Jump table
// -----------------------------------------------------------------------------
M6809Opc_handler M6809Opc_11xx[256] = {
    &M6809::Opc_11xx_00, &M6809::Opc_11xx_01, &M6809::Opc_11xx_02, &M6809::Opc_11xx_03,
    &M6809::Opc_11xx_04, &M6809::Opc_11xx_05, &M6809::Opc_11xx_06, &M6809::Opc_11xx_07,
    &M6809::Opc_11xx_08, &M6809::Opc_11xx_09, &M6809::Opc_11xx_0a, &M6809::Opc_11xx_0b,
    &M6809::Opc_11xx_0c, &M6809::Opc_11xx_0d, &M6809::Opc_11xx_0e, &M6809::Opc_11xx_0f,
    &M6809::Opc_11xx_10, &M6809::Opc_11xx_11, &M6809::Opc_11xx_12, &M6809::Opc_11xx_13,
    &M6809::Opc_11xx_14, &M6809::Opc_11xx_15, &M6809::Opc_11xx_16, &M6809::Opc_11xx_17,
    &M6809::Opc_11xx_18, &M6809::Opc_11xx_19, &M6809::Opc_11xx_1a, &M6809::Opc_11xx_1b,
    &M6809::Opc_11xx_1c, &M6809::Opc_11xx_1d, &M6809::Opc_11xx_1e, &M6809::Opc_11xx_1f,
    &M6809::Opc_11xx_20, &M6809::Opc_11xx_21, &M6809::Opc_11xx_22, &M6809::Opc_11xx_23,
    &M6809::Opc_11xx_24, &M6809::Opc_11xx_25, &M6809::Opc_11xx_26, &M6809::Opc_11xx_27,
    &M6809::Opc_11xx_28, &M6809::Opc_11xx_29, &M6809::Opc_11xx_2a, &M6809::Opc_11xx_2b,
    &M6809::Opc_11xx_2c, &M6809::Opc_11xx_2d, &M6809::Opc_11xx_2e, &M6809::Opc_11xx_2f,
    &M6809::Opc_11xx_30, &M6809::Opc_11xx_31, &M6809::Opc_11xx_32, &M6809::Opc_11xx_33,
    &M6809::Opc_11xx_34, &M6809::Opc_11xx_35, &M6809::Opc_11xx_36, &M6809::Opc_11xx_37,
    &M6809::Opc_11xx_38, &M6809::Opc_11xx_39, &M6809::Opc_11xx_3a, &M6809::Opc_11xx_3b,
    &M6809::Opc_11xx_3c, &M6809::Opc_11xx_3d, &M6809::Opc_11xx_3e, &M6809::Opc_11xx_3f,
    &M6809::Opc_11xx_40, &M6809::Opc_11xx_41, &M6809::Opc_11xx_42, &M6809::Opc_11xx_43,
    &M6809::Opc_11xx_44, &M6809::Opc_11xx_45, &M6809::Opc_11xx_46, &M6809::Opc_11xx_47,
    &M6809::Opc_11xx_48, &M6809::Opc_11xx_49, &M6809::Opc_11xx_4a, &M6809::Opc_11xx_4b,
    &M6809::Opc_11xx_4c, &M6809::Opc_11xx_4d, &M6809::Opc_11xx_4e, &M6809::Opc_11xx_4f,
    &M6809::Opc_11xx_50, &M6809::Opc_11xx_51, &M6809::Opc_11xx_52, &M6809::Opc_11xx_53,
    &M6809::Opc_11xx_54, &M6809::Opc_11xx_55, &M6809::Opc_11xx_56, &M6809::Opc_11xx_57,
    &M6809::Opc_11xx_58, &M6809::Opc_11xx_59, &M6809::Opc_11xx_5a, &M6809::Opc_11xx_5b,
    &M6809::Opc_11xx_5c, &M6809::Opc_11xx_5d, &M6809::Opc_11xx_5e, &M6809::Opc_11xx_5f,
    &M6809::Opc_11xx_60, &M6809::Opc_11xx_61, &M6809::Opc_11xx_62, &M6809::Opc_11xx_63,
    &M6809::Opc_11xx_64, &M6809::Opc_11xx_65, &M6809::Opc_11xx_66, &M6809::Opc_11xx_67,
    &M6809::Opc_11xx_68, &M6809::Opc_11xx_69, &M6809::Opc_11xx_6a, &M6809::Opc_11xx_6b,
    &M6809::Opc_11xx_6c, &M6809::Opc_11xx_6d, &M6809::Opc_11xx_6e, &M6809::Opc_11xx_6f,
    &M6809::Opc_11xx_70, &M6809::Opc_11xx_71, &M6809::Opc_11xx_72, &M6809::Opc_11xx_73,
    &M6809::Opc_11xx_74, &M6809::Opc_11xx_75, &M6809::Opc_11xx_76, &M6809::Opc_11xx_77,
    &M6809::Opc_11xx_78, &M6809::Opc_11xx_79, &M6809::Opc_11xx_7a, &M6809::Opc_11xx_7b,
    &M6809::Opc_11xx_7c, &M6809::Opc_11xx_7d, &M6809::Opc_11xx_7e, &M6809::Opc_11xx_7f,
    &M6809::Opc_11xx_80, &M6809::Opc_11xx_81, &M6809::Opc_11xx_82, &M6809::Opc_11xx_83,
    &M6809::Opc_11xx_84, &M6809::Opc_11xx_85, &M6809::Opc_11xx_86, &M6809::Opc_11xx_87,
    &M6809::Opc_11xx_88, &M6809::Opc_11xx_89, &M6809::Opc_11xx_8a, &M6809::Opc_11xx_8b,
    &M6809::Opc_11xx_8c, &M6809::Opc_11xx_8d, &M6809::Opc_11xx_8e, &M6809::Opc_11xx_8f,
    &M6809::Opc_11xx_90, &M6809::Opc_11xx_91, &M6809::Opc_11xx_92, &M6809::Opc_11xx_93,
    &M6809::Opc_11xx_94, &M6809::Opc_11xx_95, &M6809::Opc_11xx_96, &M6809::Opc_11xx_97,
    &M6809::Opc_11xx_98, &M6809::Opc_11xx_99, &M6809::Opc_11xx_9a, &M6809::Opc_11xx_9b,
    &M6809::Opc_11xx_9c, &M6809::Opc_11xx_9d, &M6809::Opc_11xx_9e, &M6809::Opc_11xx_9f,
    &M6809::Opc_11xx_a0, &M6809::Opc_11xx_a1, &M6809::Opc_11xx_a2, &M6809::Opc_11xx_a3,
    &M6809::Opc_11xx_a4, &M6809::Opc_11xx_a5, &M6809::Opc_11xx_a6, &M6809::Opc_11xx_a7,
    &M6809::Opc_11xx_a8, &M6809::Opc_11xx_a9, &M6809::Opc_11xx_aa, &M6809::Opc_11xx_ab,
    &M6809::Opc_11xx_ac, &M6809::Opc_11xx_ad, &M6809::Opc_11xx_ae, &M6809::Opc_11xx_af,
    &M6809::Opc_11xx_b0, &M6809::Opc_11xx_b1, &M6809::Opc_11xx_b2, &M6809::Opc_11xx_b3,
    &M6809::Opc_11xx_b4, &M6809::Opc_11xx_b5, &M6809::Opc_11xx_b6, &M6809::Opc_11xx_b7,
    &M6809::Opc_11xx_b8, &M6809::Opc_11xx_b9, &M6809::Opc_11xx_ba, &M6809::Opc_11xx_bb,
    &M6809::Opc_11xx_bc, &M6809::Opc_11xx_bd, &M6809::Opc_11xx_be, &M6809::Opc_11xx_bf,
    &M6809::Opc_11xx_c0, &M6809::Opc_11xx_c1, &M6809::Opc_11xx_c2, &M6809::Opc_11xx_c3,
    &M6809::Opc_11xx_c4, &M6809::Opc_11xx_c5, &M6809::Opc_11xx_c6, &M6809::Opc_11xx_c7,
    &M6809::Opc_11xx_c8, &M6809::Opc_11xx_c9, &M6809::Opc_11xx_ca, &M6809::Opc_11xx_cb,
    &M6809::Opc_11xx_cc, &M6809::Opc_11xx_cd, &M6809::Opc_11xx_ce, &M6809::Opc_11xx_cf,
    &M6809::Opc_11xx_d0, &M6809::Opc_11xx_d1, &M6809::Opc_11xx_d2, &M6809::Opc_11xx_d3,
    &M6809::Opc_11xx_d4, &M6809::Opc_11xx_d5, &M6809::Opc_11xx_d6, &M6809::Opc_11xx_d7,
    &M6809::Opc_11xx_d8, &M6809::Opc_11xx_d9, &M6809::Opc_11xx_da, &M6809::Opc_11xx_db,
    &M6809::Opc_11xx_dc, &M6809::Opc_11xx_dd, &M6809::Opc_11xx_de, &M6809::Opc_11xx_df,
    &M6809::Opc_11xx_e0, &M6809::Opc_11xx_e1, &M6809::Opc_11xx_e2, &M6809::Opc_11xx_e3,
    &M6809::Opc_11xx_e4, &M6809::Opc_11xx_e5, &M6809::Opc_11xx_e6, &M6809::Opc_11xx_e7,
    &M6809::Opc_11xx_e8, &M6809::Opc_11xx_e9, &M6809::Opc_11xx_ea, &M6809::Opc_11xx_eb,
    &M6809::Opc_11xx_ec, &M6809::Opc_11xx_ed, &M6809::Opc_11xx_ee, &M6809::Opc_11xx_ef,
    &M6809::Opc_11xx_f0, &M6809::Opc_11xx_f1, &M6809::Opc_11xx_f2, &M6809::Opc_11xx_f3,
    &M6809::Opc_11xx_f4, &M6809::Opc_11xx_f5, &M6809::Opc_11xx_f6, &M6809::Opc_11xx_f7,
    &M6809::Opc_11xx_f8, &M6809::Opc_11xx_f9, &M6809::Opc_11xx_fa, &M6809::Opc_11xx_fb,
    &M6809::Opc_11xx_fc, &M6809::Opc_11xx_fd, &M6809::Opc_11xx_fe, &M6809::Opc_11xx_ff
};