#include "game/crt.h"

#include "platform/input.h"
#include "platform/timing.h"

// 4857:0308 (the pending-scancode byte at DS:A2F3 lives in input.c's queue)
bool Crt_KeyPressed(void)
{
    return input_key_pressed();
}

// 4857:031a
char Crt_ReadKey(void)
{
    int k;
    while ((k = input_read_key()) < 0) {
        if (!timing_wait_retrace())
            return 0;
    }
    return (char)k;
}

// Crt.Delay (not named in symbols.txt)
void Crt_Delay(uint16_t ms)
{
    // 70.086 Hz -> 14.27 ms per frame.
    int frames = (int)(((uint32_t)ms * 70086 + 999999) / 1000000);
    if (frames > 0)
        timing_wait_frames(frames);
}
