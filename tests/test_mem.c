// mem: far pointers into the emulated address space.
#include <stdio.h>

#include "game/dseg.h"
#include "game/mem.h"
#include "platform/video.h"

int main(void)
{
    mem_init();
    bool ok = true;

    // A far pointer to a global resolves to the same byte as the DS mirror.
    FarPtr credits = far_ptr(MEM_DS_SEG, 0x95BE);
    ds.Game_Credits = 120000;
    ok = ok && *(int32_t *)far_host(credits) == 120000;
    ok = ok && far_host(credits) == &ds.Game_Credits;
    ok = ok && ghidra_host(0x4A81, 0x95BE) == &ds.Game_Credits;

    // Video memory is A000:0000.
    vid_screen[1][2] = 7;
    ok = ok && mem[0xA0000 + 320 + 2] == 7;

    // Segment:offset aliasing works as in real mode.
    ok = ok && far_host(far_ptr(0x1234, 0x0010)) == far_host(far_ptr(0x1235, 0x0000));

    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
