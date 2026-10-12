// WAR\ANIM\ANIM.DEF: scripts for the space battle background props.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum : int {
    ANIMDEF_SCRIPT_SIZE = 560,
    ANIMDEF_MAX_STEPS = 140,
    ANIMDEF_END = 0xFF, // delay value that ends a script
};

typedef struct {
    uint8_t anim;  // war-anim id, 0 = nothing drawn
    uint8_t frame;
    uint8_t sound; // 0 = none
    uint8_t delay; // ticks until the next step; ANIMDEF_END ends the script
} AnimDefStep;

// Steps run up to and including the end marker; script 24 of the original
// file carries it only in its 140th step.
typedef struct {
    int num_steps;
    AnimDefStep steps[ANIMDEF_MAX_STEPS];
} AnimDefScript;

typedef struct {
    int num_scripts; // script n of the game is scripts[n - 1]
    AnimDefScript *scripts;
} AnimDef;

bool animdef_decode(const uint8_t *data, size_t size, AnimDef *out);
bool animdef_load(const char *dos_path, AnimDef *out);
void animdef_free(AnimDef *def);
