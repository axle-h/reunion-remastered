// Unit Scale (3955): shrinks 32x32 sprites (320-wide rows) to 16..32 pixels
// square by dropping rows and columns.
#pragma once

#include <stdint.h>


// 3955:0000 .. 02f4: ScaleRowN copies the 32-pixel row at src into N pixels
// at dst, dropping 32 - N fixed columns (one unrolled movs sequence each).
void Scale_ScaleRow16(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow17(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow18(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow19(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow20(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow21(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow22(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow23(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow24(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow25(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow26(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow27(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow28(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow29(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow30(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow31(const uint8_t *src, uint8_t *dst);
void Scale_ScaleRow32(const uint8_t *src, uint8_t *dst);

typedef void (*ScaleRowProc)(const uint8_t *src, uint8_t *dst);

// Host stand-in for ds.Scale_RowProcs (array[16..32] of far code pointers):
// entry size - 16 is ScaleRow<size>.
extern const ScaleRowProc Scale_RowProcTable[17];

// 3955:030e: size is clamped to 16..32; source and destination rows are 320
// bytes apart. Row r of the 32 source rows is kept when a running error term
// (starting at 2 * size - 32) is non-negative.
void Scale_ScaleSprite(const uint8_t *src, uint8_t *dst, int16_t size);
