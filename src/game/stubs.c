// Stand-ins for routines whose unit is not ported yet. One function per stub,
// each with a `// TODO: port seg:off Name`; delete the stub when the unit lands.
#include "game/dseg.h"
#include "game/stub.h"


// --- stubs for Gfx (405f) ---
#include "platform/video.h"

// TODO: port 431a:017f Sound::SbBreakLoop
STUB void Sound_SbBreakLoop(void)
{
}


#include "game/stub.h"

// --- stubs for Satellite (28cd) callees ---

// --- stubs for Cargo (2583) ---
#include "game/stub.h"


// --- stubs for Groups (2b8d) callees ---

// --- stubs for Sim (1b8a) callees ---
#include "game/stub.h"

// --- stubs for Colonize (2841) callees ---

// --- stubs for Pub (1fd2) ---


// --- stubs for Main (1000) callees ---

// TODO: port 26eb:0000 Cheat::Apply (dead: Cheat_Requested is never set)
STUB void Cheat_Apply(void)
{
}
