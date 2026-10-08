// Unit Dos (48b9): Intr's callers are replaced at their call sites (INT 33h by
// input.h, INT 16h by Crt, ...), so only GetEnv remains.
#pragma once

#include <stdint.h>

// 48b9:0000: result := value of environment variable name (Pascal strings,
// result is string[255]); empty if unset. The name is uppercased and cut to
// 31 characters as in the original.
void Dos_GetEnv(uint8_t *result, const uint8_t *name);
