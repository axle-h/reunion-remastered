// video: mode 12h state and framebuffer helpers (no window).
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/video.h"

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAILED line %d: %s\n", __LINE__, #cond);              \
            ok = false;                                                     \
        }                                                                   \
    } while (0)

int main(void)
{
    bool ok = true;
    static uint8_t strip[100 * VID12_WIDTH];

    CHECK(vid_mode() == VID_MODE_13H);
    CHECK(vid_screen != nullptr);

    vid12_screen[5][5] = 9;
    vid_set_mode(VID_MODE_12H);
    CHECK(vid_mode() == VID_MODE_12H);
    CHECK(vid12_screen[5][5] == 0);

    CHECK(vid_attr[6] == 0x14 && vid_attr[7] == 7 && vid_attr[8] == 0x38 && vid_attr[15] == 0x3f);

    for (size_t i = 0; i < sizeof strip; i++)
        strip[i] = (uint8_t)(i % 16 + 16); // high bits must be dropped
    CHECK(vid12_blit_strip(100, 100, strip));
    CHECK(vid12_screen[99][0] == 0);
    CHECK(vid12_screen[100][0] == 0 && vid12_screen[100][3] == 3);
    CHECK(vid12_screen[199][639] == (639 + 99 * 640) % 16);
    CHECK(vid12_screen[200][0] == 0);
    CHECK(vid12_blit_strip(400, 80, strip));
    CHECK(!vid12_blit_strip(401, 80, strip));
    CHECK(!vid12_blit_strip(-1, 1, strip));

    // Write mode 2: colour 0xA to the 4 leftmost pixels of byte 0, all planes.
    vid12_planar_write(0, 0xA, 0xF0, 0x0F);
    CHECK(vid12_screen[0][0] == 0xA && vid12_screen[0][3] == 0xA && vid12_screen[0][4] == 0);
    // Only planes 0 and 1 (map mask 3): colour 0xF changes the low two bits.
    vid12_planar_write(0, 0xF, 0x80, 0x03);
    CHECK(vid12_screen[0][0] == 0xB);
    vid12_planar_write(80, 5, 0xFF, 0x0F);
    CHECK(vid12_screen[1][0] == 5 && vid12_screen[1][7] == 5 && vid12_screen[1][8] == 0);

    vid_set_mode(VID_MODE_13H);
    CHECK(vid_mode() == VID_MODE_13H);

    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
