#include "game/adlib.h"

#include <stddef.h>
#include <string.h>

#include "game/sound.h"
#include "platform/audio.h"

// The player keeps all its state in its own code segment (428e:0743..08B7).
// That area is mirrored byte for byte, initialised from REUNION.PRG, because
// two routines index past their tables (a note above 96 reads beyond
// FreqTable; key-off writes 156h bytes too high) and land in the following
// bytes, which belong to segment 431a. Those are included up to 431a:0140, as
// stored in the file (far-call segments not relocated).
#pragma pack(push, 1)
typedef struct {
    uint8_t Playing;           // 0743
    uint8_t Hooked;            // 0744: INT 1Ch handler installed
    int16_t PortTable[5];      // 0745: 218h, 288h, 318h, 388h, -1 (detection)
    uint16_t FreqTable[97];    // 074f: by note-1; A0h value in the low byte, B0h (with key-on) in the high
    uint8_t OpOffset[9][2];    // 0811: per channel, op A (the carrier) and op B (the modulator)
    uint16_t Port;             // 0823
    uint8_t DetectStatus[2];   // 0825 (detection)
    uint16_t ChanFreq[9];      // 0827: A0h (low) / B0h (high) values
    uint16_t ChanFreqDirty[9]; // 0839
    uint8_t Level[2][9];       // 084b: 40h value (KSL | TL) of op A / op B
    uint8_t LevelDirty[2][9];  // 085d
    uint32_t OldInt1C;         // 086f
    uint8_t TickCount;         // 0873: ticks left until the next row
    uint8_t NoteIndex;         // 0874: note-1 of the last note-on
    uint8_t Speed;             // 0875: ticks per row
    uint8_t Attenuation;       // 0876: 0..3Fh; always 0 (nothing sets it)
    uint8_t FreqOffset[9];     // 0877: from the instrument; added to A0h on every tick
    uint16_t TuneSeg;          // 0880
    uint16_t InstrOff;         // 0882
    uint16_t OrderOff;         // 0884
    uint16_t PatternOff;       // 0886
    uint8_t Note[9];           // 0888
    uint8_t NoteTriggered[9];  // 0891
    uint8_t Instrument[9];     // 089a: FFh = none
    uint8_t Meter[9];          // 08a3: level meter, decays per tick (nothing reads it)
    uint8_t MeterHit[9];       // 08ac
    uint8_t OrderPos;          // 08b5
    uint8_t Pattern;           // 08b6
    uint8_t Row;               // 08b7
    uint8_t Unused[8];         // 08b8
    uint8_t Seg431a[0x140];    // 08c0 = 431a:0000
} AdlibCs;
#pragma pack(pop)

enum : uint16_t { CS_BASE = 0x0743, CS_END = 0x0A00 };
static_assert(offsetof(AdlibCs, FreqTable) == 0x074F - CS_BASE);
static_assert(offsetof(AdlibCs, ChanFreq) == 0x0827 - CS_BASE);
static_assert(offsetof(AdlibCs, OldInt1C) == 0x086F - CS_BASE);
static_assert(offsetof(AdlibCs, TuneSeg) == 0x0880 - CS_BASE);
static_assert(offsetof(AdlibCs, Note) == 0x0888 - CS_BASE);
static_assert(offsetof(AdlibCs, OrderPos) == 0x08B5 - CS_BASE);
static_assert(offsetof(AdlibCs, Seg431a) == 0x08C0 - CS_BASE);
static_assert(sizeof(AdlibCs) == CS_END - CS_BASE);

static union {
    AdlibCs v;
    uint8_t b[sizeof(AdlibCs)];
} cs_image = { .b = {
        0x00, 0x00, 0x18, 0x02, 0x88, 0x02, 0x18, 0x03, 0x88, 0x03, 0xff, 0xff, 0x00, 0x00, 0x6b, 0x21,  // 0743
        0x81, 0x21, 0x98, 0x21, 0xb0, 0x21, 0xca, 0x21, 0xe5, 0x21, 0x02, 0x22, 0x20, 0x22, 0x41, 0x22,  // 0753
        0x63, 0x22, 0x87, 0x22, 0xae, 0x22, 0x6b, 0x25, 0x81, 0x25, 0x98, 0x25, 0xb0, 0x25, 0xca, 0x25,  // 0763
        0xe5, 0x25, 0x02, 0x26, 0x20, 0x26, 0x41, 0x26, 0x63, 0x26, 0x87, 0x26, 0xae, 0x26, 0x6b, 0x29,  // 0773
        0x81, 0x29, 0x98, 0x29, 0xb0, 0x29, 0xca, 0x29, 0xe5, 0x29, 0x02, 0x2a, 0x20, 0x2a, 0x41, 0x2a,  // 0783
        0x63, 0x2a, 0x87, 0x2a, 0xae, 0x2a, 0x6b, 0x2d, 0x81, 0x2d, 0x98, 0x2d, 0xb0, 0x2d, 0xca, 0x2d,  // 0793
        0xe5, 0x2d, 0x02, 0x2e, 0x20, 0x2e, 0x41, 0x2e, 0x63, 0x2e, 0x87, 0x2e, 0xae, 0x2e, 0x6b, 0x31,  // 07a3
        0x81, 0x31, 0x98, 0x31, 0xb0, 0x31, 0xca, 0x31, 0xe5, 0x31, 0x02, 0x32, 0x20, 0x32, 0x41, 0x32,  // 07b3
        0x63, 0x32, 0x87, 0x32, 0xae, 0x32, 0x6b, 0x35, 0x81, 0x35, 0x98, 0x35, 0xb0, 0x35, 0xca, 0x35,  // 07c3
        0xe5, 0x35, 0x02, 0x36, 0x20, 0x36, 0x41, 0x36, 0x63, 0x36, 0x87, 0x36, 0xae, 0x36, 0x6b, 0x39,  // 07d3
        0x81, 0x39, 0x98, 0x39, 0xb0, 0x39, 0xca, 0x39, 0xe5, 0x39, 0x02, 0x3a, 0x20, 0x3a, 0x41, 0x3a,  // 07e3
        0x63, 0x3a, 0x87, 0x3a, 0xae, 0x3a, 0x6b, 0x3d, 0x81, 0x3d, 0x98, 0x3d, 0xb0, 0x3d, 0xca, 0x3d,  // 07f3
        0xe5, 0x3d, 0x02, 0x3e, 0x20, 0x3e, 0x41, 0x3e, 0x63, 0x3e, 0x87, 0x3e, 0xae, 0x3e, 0x03, 0x00,  // 0803
        0x04, 0x01, 0x05, 0x02, 0x0b, 0x08, 0x0c, 0x09, 0x0d, 0x0a, 0x13, 0x10, 0x14, 0x11, 0x15, 0x12,  // 0813
        0x88, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0823
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,  // 0833
        0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0843
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  // 0853
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x3d, 0x3f,  // 0863
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x3f, 0x00,  // 0873
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0883
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 0893
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 08a3
        0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x43, 0x61,  // 08b3
        0x6e, 0x6e, 0x6f, 0x74, 0x20, 0x4f, 0x70, 0x65, 0x6e, 0x20, 0xc8, 0x8c, 0x02, 0x00, 0x8c, 0xd3,  // 08c3
        0x8e, 0xc3, 0x8c, 0xdb, 0xfc, 0x8d, 0xbe, 0x00, 0xff, 0xc5, 0x76, 0x06, 0xac, 0xaa, 0x91, 0x30,  // 08d3
        0xed, 0xf3, 0xa4, 0x8e, 0xdb, 0x80, 0x3e, 0xcf, 0x89, 0x01, 0x75, 0x03, 0xe9, 0x49, 0x01, 0x68,  // 08e3
        0x70, 0x17, 0x9a, 0x8a, 0x02, 0xc7, 0x38, 0x89, 0x86, 0x78, 0xfe, 0x89, 0x96, 0x7a, 0xfe, 0xa1,  // 08f3
        0x5c, 0x8a, 0x50, 0x8b, 0x3e, 0x5e, 0x8a, 0x07, 0x89, 0xf8, 0x8c, 0xc2, 0x89, 0x86, 0x74, 0xfe,  // 0903
        0x89, 0x96, 0x76, 0xfe, 0x8d, 0xbe, 0x80, 0xfe, 0x16, 0x57, 0x8d, 0xbe, 0x00, 0xff, 0x16, 0x57,  // 0913
        0x9a, 0x91, 0x09, 0xc7, 0x38, 0x8d, 0xbe, 0x80, 0xfe, 0x16, 0x57, 0x6a, 0x01, 0x9a, 0xcc, 0x09,  // 0923
        0xc7, 0x38, 0x9a, 0xed, 0x04, 0xc7, 0x38, 0x09, 0xc0, 0x74, 0x38, 0xbf, 0xf6, 0xa3, 0x1e, 0x57,  // 0933
        0x8d, 0xbe, 0x74, 0xfd, 0x16, 0x57, 0xbf, 0x00, 0x00, 0x0e, 0x57, 0x9a, 0x7c, 0x0c, 0xc7, 0x38,  // 0943
        0x8d, 0xbe, 0x00, 0xff, 0x16, 0x57, 0x9a, 0xfb, 0x0c, 0xc7, 0x38, 0x6a, 0x00, 0x9a, 0x64, 0x09,  // 0953
        0xc7, 0x38, 0x9a, 0x40, 0x08, 0xc7, 0x38, 0x9a, 0xf4, 0x04, 0xc7, 0x38, 0x31, 0xc0, 0x9a, 0x16,  // 0963
        0x01, 0xc7, 0x38, 0x8d, 0xbe, 0x80, 0xfe, 0x16, 0x57, 0x9a, 0xcb, 0x1a, 0xc7, 0x38, 0x9a, 0xf4,  // 0973
        0x04, 0xc7, 0x38, 0x89, 0x86, 0x7c, 0xfe, 0x8d, 0xbe, 0x80, 0xfe, 0x16, 0x57, 0xc4, 0xbe, 0x78,  // 0983
        0xfe, 0x06, 0x57, 0xff, 0xb6, 0x7c, 0xfe, 0x31, 0xc0, 0x50, 0x50, 0x9a, 0xb7, 0x0a, 0xc7, 0x38,  // 0993
        0x9a, 0xf4, 0x04, 0xc7, 0x38, 0x8d, 0xbe, 0x80, 0xfe, 0x16, 0x57, 0x9a, 0x4d, 0x0a, 0xc7, 0x38,  // 09a3
        0x9a, 0xf4, 0x04, 0xc7, 0x38, 0x8b, 0x86, 0x7c, 0xfe, 0x89, 0x86, 0x72, 0xfe, 0xb8, 0x01, 0x00,  // 09b3
        0x3b, 0x86, 0x72, 0xfe, 0x7f, 0x62, 0x89, 0x86, 0x7e, 0xfe, 0xeb, 0x04, 0xff, 0x86, 0x7e, 0xfe,  // 09c3
        0x8b, 0x86, 0x7e, 0xfe, 0x99, 0x8b, 0xc8, 0x8b, 0xda, 0x8b, 0x86, 0x7c, 0xfe, 0x40, 0x31, 0xd2,  // 09d3
        0x2b, 0xc1, 0x1b, 0xd3, 0x52, 0x50, 0x8b, 0x86, 0x7e, 0xfe, 0x99, 0x8b, 0xc8, 0x8b, 0xda, 0x8b,  // 09e3
        0x86, 0x7c, 0xfe, 0x40, 0x31, 0xd2, 0x2b, 0xc1, 0x1b, 0xd3, 0xc4, 0xbe, 0x78,  // 09f3
} };
static AdlibCs *const cs = &cs_image.v;

// The tune's segment (the player's DS), copied when a tune starts so the
// audio thread never reads the buffer the game loads the next tune into.
static uint8_t tune[0x10000];

static uint16_t cs_word(uint16_t off)
{
    off -= CS_BASE;
    return (uint16_t)(cs_image.b[off] | cs_image.b[off + 1] << 8);
}

static void cs_set_word(uint16_t off, uint16_t v)
{
    off -= CS_BASE;
    cs_image.b[off] = (uint8_t)v;
    cs_image.b[off + 1] = (uint8_t)(v >> 8);
}

// 428e:0716: AL = register, AH = value. The 3.3 / 23 us delay reads aren't
// needed by the emulator.
static void OplWrite(uint8_t reg, uint8_t value)
{
    audio_opl_write(reg, value);
}

// 40h value scaled by the master attenuation. BUG? The KSL bits come from
// 3Fh - value, so KSL 1 and 3 swap (and the effects' levels clear KSL).
static uint8_t scale_level(uint8_t level)
{
    uint8_t al = (uint8_t)(0x3F - level);
    uint8_t ksl = al & 0xC0;
    uint16_t q = (uint16_t)((al & 0x3F) * (uint8_t)(0x3F - cs->Attenuation)) / 0x3F;
    return (uint8_t)(((uint8_t)(0x3F - (uint8_t)q) & 0x3F) | ksl);
}

// Byte 8 (C0h: feedback / connection) of the channel's instrument.
static bool additive(int ch)
{
    uint16_t off = (uint16_t)(cs->InstrOff + cs->Instrument[ch] * 12);
    return tune[(uint16_t)(off + 8)] & 1;
}

// 428e:02c0: keys off every channel, sets the fastest release and resets the
// channel state.
static void SilenceAll(void)
{
    OplWrite(0x01, 0x00);
    for (uint8_t r = 0xB0; r <= 0xB8; r++)
        OplWrite(r, 0x00);
    static const uint8_t sr[] = { 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x88, 0x89, 0x8A,
                                  0x8B, 0x8C, 0x8D, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95 };
    for (size_t i = 0; i < sizeof sr; i++)
        OplWrite(sr[i], 0xFF);
    for (int ch = 0; ch < 9; ch++) {
        cs->ChanFreq[ch] = 0;
        cs->ChanFreqDirty[ch] = 1;
        cs->Level[0][ch] = cs->Level[1][ch] = 0;
        cs->LevelDirty[0][ch] = cs->LevelDirty[1][ch] = 1;
        cs->Instrument[ch] = 0xFF;
        cs->Note[ch] = 0;
        cs->NoteTriggered[ch] = 0;
        cs->Meter[ch] = 0xFF;
    }
}

// 428e:039c: AL = instrument, AH = channel. Skipped if already set.
static void SetInstrument(uint8_t ins, uint8_t ch)
{
    if (cs->Instrument[ch] == ins)
        return;
    cs->Instrument[ch] = ins;
    uint16_t si = (uint16_t)(cs->InstrOff + ins * 12);
#define INS(i) tune[(uint16_t)(si + (i))]
    uint8_t a = cs->OpOffset[ch][0], b = cs->OpOffset[ch][1];
    OplWrite(0xB0 + ch, 0x00);
    OplWrite(0xC0 + ch, INS(8));
    OplWrite(0x20 + a, INS(0));
    OplWrite(0x20 + b, INS(1));
    OplWrite(0x60 + a, INS(4));
    OplWrite(0x60 + b, INS(5));
    OplWrite(0x80 + a, INS(6));
    OplWrite(0x80 + b, INS(7));
    OplWrite(0xE0 + a, INS(9));
    OplWrite(0xE0 + b, INS(10));
    OplWrite(0x40 + a, INS(2));
    OplWrite(0x40 + b, INS(3));
    cs->Level[0][ch] = INS(2);
    cs->LevelDirty[0][ch] = 1;
    cs->Level[1][ch] = INS(3);
    cs->LevelDirty[1][ch] = 1;
    cs->FreqOffset[ch] = (INS(11) & 0xF0) >> 4;
#undef INS
}

// 428e:047d: AL = note, AH = channel. Keys the channel off now; the next tick
// writes the new frequency with the key-on bit (all FreqTable values have it).
static void NoteOn(uint8_t note, uint8_t ch)
{
    if (note == 0x7F) {
        // 428e:04d5, key off. BUG? It clears the key-on bit at CS:097D+2*ch
        // and flags CS:098F+2*ch, 156h past ChanFreq / ChanFreqDirty: the key
        // off never reaches the OPL and bytes of 431a:00BD.. (LoadSbDriver,
        // already run) are patched instead.
        uint16_t off = (uint16_t)(0x097D + ch * 2);
        cs_set_word(off, cs_word(off) & 0xDFFF);
        cs_set_word((uint16_t)(off + 0x12), 1);
        return;
    }
    cs->Note[ch] = note;
    cs->NoteTriggered[ch] = 1;
    cs->NoteIndex = (uint8_t)(note - 1);
    OplWrite(0xB0 + ch, 0x00);
    // Notes above 97 read past the table (see the top of the file).
    cs->ChanFreq[ch] = cs_word((uint16_t)(0x074F + cs->NoteIndex * 2));
    cs->ChanFreqDirty[ch] = 1;
}

// 428e:00f2 (part of AdvanceRow): one channel's cell at tune[si]; returns
// the next cell's offset.
static uint16_t ReadCell(uint16_t si, uint8_t ch)
{
    uint8_t al = tune[si++];
    if (al == 0x80) {
        SetInstrument(tune[si++] & 0x7F, ch);
        return si;
    }
    if (al != 0)
        NoteOn(al, ch);
    al = tune[si++];
    if (al == 0)
        return si;
    if (al == 0x01) {
        // Pattern break: the row increment after the cells moves on.
        cs->Row = 0x3F;
        return si;
    }
    uint8_t param = al & 0x0F;
    switch (al >> 4) {
    case 0x1: // slide up; BUG? only the low byte of the F-number, no carry
        cs->ChanFreq[ch] = (uint16_t)((cs->ChanFreq[ch] & 0xFF00) | (uint8_t)(cs->ChanFreq[ch] + param + 1));
        cs->ChanFreqDirty[ch] = 1;
        break;
    case 0x2: // slide down, same
        cs->ChanFreq[ch] = (uint16_t)((cs->ChanFreq[ch] & 0xFF00) | (uint8_t)(cs->ChanFreq[ch] - param - 1));
        cs->ChanFreqDirty[ch] = 1;
        break;
    case 0xA: // op A level
        cs->Level[0][ch] = (uint8_t)(param << 2);
        cs->LevelDirty[0][ch] = 1;
        break;
    case 0xB: // op B level
        cs->Level[1][ch] = (uint8_t)(param << 2);
        cs->LevelDirty[1][ch] = 1;
        break;
    case 0xC: // channel volume: op A, plus op B for an additive instrument
        cs->Level[0][ch] = (uint8_t)(param << 2);
        cs->LevelDirty[0][ch] = 1;
        if (additive(ch)) {
            cs->Level[1][ch] = (uint8_t)(param << 2);
            cs->LevelDirty[1][ch] = 1;
        }
        break;
    default: // Fxx, and every other effect (the compare for F has no jump)
        cs->Speed = (uint8_t)(param + 1);
        cs->TickCount = (uint8_t)(param + 1);
        break;
    }
    return si;
}

// 428e:008c: plays the next row and moves to the next order after row 63.
static void AdvanceRow(void)
{
    uint8_t pat = tune[(uint16_t)(cs->OrderOff + cs->OrderPos)];
    cs->Pattern = pat;
    uint16_t si = (uint16_t)(cs->PatternOff + pat * 0x480 + cs->Row * 18);
    for (int cx = 9; cx > 0; cx--)
        si = ReadCell(si, (uint8_t)(9 - cx));
    cs->Row++;
    if (cs->Row == 0x40) {
        cs->Row = 0;
        cs->OrderPos++;
        if (tune[(uint16_t)(cs->OrderOff + cs->OrderPos)] == 0xFF)
            cs->OrderPos = 0;
    }
}

// 428e:04f3: level meters (not audible; kept because it clears NoteTriggered).
static void UpdateMeters(void)
{
    for (int ch = 8; ch >= 0; ch--) {
        cs->MeterHit[ch] = 0;
        if (cs->NoteTriggered[ch]) {
            uint8_t al = (uint8_t)(0x80 - (cs->Level[0][ch] & 0x3F) - (cs->Level[1][ch] & 0x3F));
            al >>= 3;
            if (al > 0x0F)
                al = 0x0F;
            cs->Meter[ch] = (uint8_t)(al + 1);
            cs->NoteTriggered[ch] = 0;
            cs->MeterHit[ch] = 0xFF;
        }
        if (cs->Meter[ch] != 0xFF)
            cs->Meter[ch]--;
    }
}

// 428e:055b: writes a channel's changed frequency and levels. Op B's level is
// scaled only for an additive instrument (where it is heard directly).
static void UpdateChannel(uint8_t ch)
{
    if (cs->FreqOffset[ch] != 0 || cs->ChanFreqDirty[ch] != 0) {
        cs->ChanFreqDirty[ch] = 0;
        OplWrite(0xA0 + ch, (uint8_t)(cs->ChanFreq[ch] + cs->FreqOffset[ch]));
        OplWrite(0xB0 + ch, (uint8_t)(cs->ChanFreq[ch] >> 8));
    }
    if (cs->LevelDirty[0][ch]) {
        cs->LevelDirty[0][ch] = 0;
        OplWrite(0x40 + cs->OpOffset[ch][0], scale_level(cs->Level[0][ch]));
    }
    if (cs->LevelDirty[1][ch]) {
        cs->LevelDirty[1][ch] = 0;
        uint8_t v = cs->Level[1][ch];
        OplWrite(0x40 + cs->OpOffset[ch][1], additive(ch) ? scale_level(v) : v);
    }
}

// 428e:054e
static void UpdateChannels(void)
{
    for (int cx = 9; cx > 0; cx--)
        UpdateChannel((uint8_t)(cx - 1));
}

// 428e:0059: INT 1Ch handler, 18.2 Hz. The original then chained to the old
// handler; the port's timer has no chain.
static void TimerTickIsr(void)
{
    cs->TickCount--;
    if (cs->TickCount == 0) {
        cs->TickCount = cs->Speed;
        AdvanceRow();
    }
    UpdateMeters();
    UpdateChannels();
}

// 428e:01f2
static void StopPlayerInternal(void)
{
    if (cs->Hooked) {
        audio_set_timer(nullptr, 0); // INT 21h/25h: restore INT 1Ch
        cs->Hooked = 0;
    }
    SilenceAll();
    cs->Playing = 0;
}

// 428e:021d: ES:SI = tune. The tune is at offset 0 of the player's copy
// (TuneSeg is left 0); beyond it the copy is zero.
static void StartPlayerInternal(const uint8_t *buf, size_t size)
{
    size = size < sizeof tune ? size : sizeof tune;
    memcpy(tune, buf, size);
    memset(tune + size, 0, sizeof tune - size);
    cs->TuneSeg = 0;
    cs->OrderOff = 0;
    cs->InstrOff = 0x33;
    cs->PatternOff = 0x633;
    SilenceAll();
    OplWrite(0x01, 0x20); // waveform select on
    OplWrite(0x08, 0x40); // keyboard split by F-number bit 8
    OplWrite(0xBD, 0x00); // melodic mode, no deep AM/vibrato
    for (uint8_t i = 0; i < 9; i++)
        SetInstrument(i, i);
    cs->Attenuation = 0;
    cs->TickCount = 1;
    cs->OrderPos = 0;
    cs->Pattern = 0;
    cs->Row = 0;
    cs->Speed = 2;
    memset(cs->LevelDirty, 1, sizeof cs->LevelDirty);
    memset(cs->Meter, 0xFF, sizeof cs->Meter);
    // INT 21h/35h + 25h: save INT 1Ch, install 428e:0059. BIOS tick = PIT
    // divisor 65536.
    cs->OldInt1C = 0;
    audio_set_timer(TimerTickIsr, 65536);
    cs->Playing = 0xFF;
    cs->Hooked = 0xFF;
}

// 428e:000c -> 428e:003a. ScanAdlibPorts (428e:065d) is replaced by the
// audio device check; an OPL at 388h is what it would find under DOSBox.
int16_t Adlib_DetectAdlib(void)
{
    if (!Sound_DeviceOpen())
        return -1;
    cs->Port = 0x388;
    return (int16_t)cs->Port;
}

// 428e:0012. The lock stands in for the original's single CPU: the timer
// can't run in the middle.
void Adlib_StartPlayer(const uint8_t *tune_buf, size_t size)
{
    audio_lock();
    if (cs->Playing)
        StopPlayerInternal();
    StartPlayerInternal(tune_buf, size);
    audio_unlock();
}

// 428e:0029
void Adlib_StopPlayer(void)
{
    audio_lock();
    if (cs->Playing)
        StopPlayerInternal();
    audio_unlock();
}

bool Adlib_Playing(void)
{
    audio_lock();
    bool playing = cs->Playing != 0;
    audio_unlock();
    return playing;
}
