// .SMP sound effects: 8-bit unsigned mono PCM in a headerless VOC stream.
#pragma once

#include <stddef.h>
#include <stdint.h>

struct smp {
    uint8_t *samples; // unsigned 8-bit, 0x80 = silence
    size_t length;
    uint32_t rate; // Hz
};

bool smp_decode(const uint8_t *data, size_t size, struct smp *out);
bool smp_load(const char *dos_path, struct smp *out);
void smp_free(struct smp *smp);
