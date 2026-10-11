// Unit ShipMove (22ca): screen 0x11, a group at a planet (dock/orbit view),
// its animations (lights, blink, spinner, star field when travelling) and the
// launch/docking sequences.
#pragma once

#include <stdint.h>


// 22ca:0105: screen 0x11 handler
void ShipMove_GroupScreen(void);
// 22ca:065e
void ShipMove_SetupGroupScreenHotspots(void);
// 22ca:08d4 / 09f9 / 0b32
void ShipMove_InitOrbitView(void);
void ShipMove_InitDockView(int16_t surfaceType);
void ShipMove_RedrawBackdrop(void);
// 22ca:0bbc / 0fd3
void ShipMove_LaunchAnimation(int16_t surfaceType);
void ShipMove_DockingAnimation(int16_t surfaceType);
// 22ca:132c: copies the planet-view shape (213 runs, 19423 bytes) from the
// 320-wide image at src to screen row 49. The original is an unrolled
// rep-movs sequence; ShipMove_ViewRuns holds its runs.
void ShipMove_BlitViewport(const uint8_t *src);
typedef struct {
    uint16_t offset; // from the start of the image
    uint16_t length;
} ShipMoveRun;
extern const ShipMoveRun ShipMove_ViewRuns[];
extern const int ShipMove_ViewRunCount;
// 22ca:1d78 / 1db0
void ShipMove_DrawButtonAnimFrame(int16_t frame);
void ShipMove_DrawDockDoorFrame(int16_t frame);
// 22ca:1de8 / 1e2f / 1e78 (near)
void ShipMove_AnimateBlinkLight(void);
void ShipMove_AnimateSpinner(void);
void ShipMove_AnimateLights(void);
// 22ca:1fb5 / 20f3 / 21ef
void ShipMove_AnimateStarfield(void);
void ShipMove_InitStarfield(void);
// Port addition: frees the backdrop's segment, which holds the star table
// too (main's FreeMem of both).
void ShipMove_FreeBackdrop(void);
void ShipMove_AnimateGroupView(void);
