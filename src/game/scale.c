#include "game/scale.h"

// Copies src[cols[i]] to dst[i]; offsets wrap inside each segment like the
// original's si/di.
static void copy_row(FarPtr src, FarPtr dst, const uint8_t *cols, int n)
{
    for (int i = 0; i < n; i++) {
        uint8_t *d = far_host(far_ptr((uint16_t)(dst >> 16), (uint16_t)((uint16_t)dst + i)));
        *d = *(uint8_t *)far_host(
            far_ptr((uint16_t)(src >> 16), (uint16_t)((uint16_t)src + cols[i])));
    }
}

// The column lists below are the movsb/movsw/inc si sequences of the
// original, as source offsets per destination pixel.

// 3955:0000
void Scale_ScaleRow16(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[16] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
    copy_row(src, dst, cols, 16);
}

// 3955:0034
void Scale_ScaleRow17(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[17] = {0, 2, 4, 6, 8, 10, 12, 14, 15, 17, 19, 21, 23, 25, 27, 29, 31};
    copy_row(src, dst, cols, 17);
}

// 3955:0068
void Scale_ScaleRow18(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[18] = {0,  2,  4,  6,  7,  9,  11, 13, 15,
                                     16, 18, 20, 22, 23, 25, 27, 29, 31};
    copy_row(src, dst, cols, 18);
}

// 3955:009a
void Scale_ScaleRow19(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[19] = {0,  2,  4,  5,  7,  9,  10, 12, 14, 15,
                                     17, 19, 21, 22, 24, 26, 27, 29, 31};
    copy_row(src, dst, cols, 19);
}

// 3955:00ca
void Scale_ScaleRow20(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[20] = {0,  2,  3,  5,  7,  8,  10, 11, 13, 15,
                                     16, 18, 19, 21, 23, 24, 26, 27, 29, 31};
    copy_row(src, dst, cols, 20);
}

// 3955:00f8
void Scale_ScaleRow21(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[21] = {0,  2,  3,  5,  6,  8,  9,  11, 12, 14, 15,
                                     17, 19, 20, 22, 23, 25, 26, 28, 29, 31};
    copy_row(src, dst, cols, 21);
}

// 3955:0124
void Scale_ScaleRow22(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[22] = {0,  2,  3,  5,  6,  7,  9,  10, 12, 13, 15,
                                     16, 18, 19, 21, 22, 23, 25, 26, 28, 29, 31};
    copy_row(src, dst, cols, 22);
}

// 3955:0150
void Scale_ScaleRow23(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[23] = {0,  2,  3,  4,  6,  7,  9,  10, 11, 13, 14, 15,
                                     17, 18, 20, 21, 22, 24, 25, 27, 28, 29, 31};
    copy_row(src, dst, cols, 23);
}

// 3955:017d
void Scale_ScaleRow24(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[24] = {0,  1,  3,  4,  5,  7,  8,  9,  11, 12, 13, 15,
                                     16, 17, 19, 20, 21, 23, 24, 25, 27, 28, 29, 31};
    copy_row(src, dst, cols, 24);
}

// 3955:01aa
void Scale_ScaleRow25(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[25] = {0,  1,  3,  4,  5,  7,  8,  9,  10, 12, 13, 14, 15,
                                     17, 18, 19, 21, 22, 23, 24, 26, 27, 28, 30, 31};
    copy_row(src, dst, cols, 25);
}

// 3955:01dd
void Scale_ScaleRow26(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[26] = {0,  1,  3,  4,  5,  6,  7,  9,  10, 11, 12, 14, 15,
                                     16, 17, 19, 20, 21, 22, 23, 25, 26, 27, 28, 30, 31};
    copy_row(src, dst, cols, 26);
}

// 3955:0215
void Scale_ScaleRow27(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[27] = {0,  1,  2,  4,  5,  6,  7,  8,  10, 11, 12, 13, 14, 15,
                                     17, 18, 19, 20, 21, 23, 24, 25, 26, 27, 29, 30, 31};
    copy_row(src, dst, cols, 27);
}

// 3955:024a
void Scale_ScaleRow28(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[28] = {0,  1,  2,  3,  5,  6,  7,  8,  9,  10, 11, 13, 14, 15,
                                     16, 17, 18, 19, 21, 22, 23, 24, 25, 26, 27, 29, 30, 31};
    copy_row(src, dst, cols, 28);
}

// 3955:027c
void Scale_ScaleRow29(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[29] = {0,  1,  2,  3,  4,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
                                     17, 18, 19, 20, 21, 22, 23, 24, 25, 27, 28, 29, 30, 31};
    copy_row(src, dst, cols, 29);
}

// 3955:02ab
void Scale_ScaleRow30(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[30] = {0,  1,  2,  3,  4,  5,  6,  7,  9,  10, 11, 12, 13, 14, 15,
                                     16, 17, 18, 19, 20, 21, 22, 23, 25, 26, 27, 28, 29, 30, 31};
    copy_row(src, dst, cols, 30);
}

// 3955:02d3
void Scale_ScaleRow31(FarPtr src, FarPtr dst)
{
    static const uint8_t cols[31] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
                                     17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    copy_row(src, dst, cols, 31);
}

// 3955:02f4
void Scale_ScaleRow32(FarPtr src, FarPtr dst)
{
    // rep movsw of 16 words: a plain copy.
    static const uint8_t cols[32] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                     11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                     22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    copy_row(src, dst, cols, 32);
}

const ScaleRowProc Scale_RowProcTable[17] = {
    Scale_ScaleRow16, Scale_ScaleRow17, Scale_ScaleRow18, Scale_ScaleRow19, Scale_ScaleRow20,
    Scale_ScaleRow21, Scale_ScaleRow22, Scale_ScaleRow23, Scale_ScaleRow24, Scale_ScaleRow25,
    Scale_ScaleRow26, Scale_ScaleRow27, Scale_ScaleRow28, Scale_ScaleRow29, Scale_ScaleRow30,
    Scale_ScaleRow31, Scale_ScaleRow32,
};

// 3955:030e
void Scale_ScaleSprite(FarPtr src, FarPtr dst, int16_t size)
{
    if (size < 16)
        size = 16;
    if (size > 32)
        size = 32;
    int16_t keep_step = (int16_t)(size * 2 - 64);
    int16_t drop_step = (int16_t)(size * 2);
    int16_t err = (int16_t)(size * 2 - 32);
    uint16_t s = (uint16_t)src, d = (uint16_t)dst;
    for (int16_t row = 1;; row++) {
        if (err >= 0) {
            // call far [Scale_RowProcs + (size - 16) * 4]
            Scale_RowProcTable[size - 16](far_ptr((uint16_t)(src >> 16), s),
                                          far_ptr((uint16_t)(dst >> 16), d));
            d += 320;
            err = (int16_t)(err + keep_step);
        } else {
            err = (int16_t)(err + drop_step);
        }
        s += 320;
        if (row == 32)
            break;
    }
}
