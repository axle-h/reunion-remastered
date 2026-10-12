// Unit VecLoad (3d38): loads VECTORS\Vn.VEC models and draws the rotating
// solid view (3D research screen) and the wireframe hologram, through Vec3D.
//
// State: the VecLoad_* and Vec3D_* fields of DS (game/dseg.h), arrays
// indexed by object slot 1..17. VecLoad_Buffer, the 320-wide off-screen
// buffer, is allocated by the caller as a whole segment: AF00h bytes in the
// original for the solid view (Shop::ShopScreen; its 114x115 window is at
// offset 0CCCh), 8980h for the wireframe (ShowHologram).
#pragma once

#include <stdint.h>


// 3d38:0000 (near): projects object slot's vertices (zoom) and normals (40h)
// and draws its faces into the buffer.
void VecLoad_DrawObject(int16_t slot);
// 3d38:00e5 (nested in DrawFrame): selection sort of the draw order by
// projected centroid depth, furthest first. count is DrawFrame's local copy
// of the object count (the parent frame's [bp-0Eh]).
void VecLoad_SortByDepth(int16_t count);
// 3d38:01aa: copies the window to the screen (the previous frame), clears
// it, draws the model and advances the angles.
void VecLoad_DrawFrame(void);
// 3d38:03b3: sets Gfx_Palette C0h..EFh to the blue ramp (not written to the
// DAC), draws a frame and copies it to the screen.
void VecLoad_DrawBlueFrame(void);
// 3d38:0413 (near)
void VecLoad_DrawWireObject(int16_t idx);
// 3d38:0490
void VecLoad_DrawWireScene(void);
// 3d38:04f9: palette entry 41h = (0, 38h, 0), written to the DAC with 40h.
void VecLoad_SetWirePalette(void);
// 3d38:051c / 05d8: heap blocks for 17 objects.
void VecLoad_AllocVecBuffers(void);
void VecLoad_FreeVecBuffers(void);
// 3d38:0696 (nested in LoadVec): reads the next object of the open file
// into slot; false at the FFFFh terminator.
bool VecLoad_ReadObject(int16_t model, int16_t slot);
// 3d38:084a: vectors\v<n>.vec, PrepareObject for every object, view setup.
// Does nothing if the file can't be opened.
void VecLoad_LoadVec(int16_t n);
// 3d38:0a00 / 0a96: AllocVecBuffers without the face blocks.
void VecLoad_AllocWireBuffers(void);
void VecLoad_FreeWireBuffers(void);
// 3d38:0b2e (nested in LoadWireVec): like ReadObject, but the faces go to
// the DS:8584 buffer and the model number isn't stored.
bool VecLoad_ReadVecObject(int16_t n, int16_t slot);
// 3d38:0cd6
void VecLoad_LoadWireVec(int16_t n);
// 3d38:0dd7: unit initialisation (builds the cosine table).
void VecLoad_Init(void);
