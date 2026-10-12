// Unit Message (34b0): message boxes, the message log, the planet naming
// dialog and the satellite fly-by animation.
//
// Strings are C strings, except the dialog's edit buffer, a Pascal string
// of the game's data (a planet name); `pointer` parameters are host
// pointers.
#pragma once

#include <stdint.h>

// 34b0:0001: appends an entry (kind, text) to the log (15 at most; the oldest
// goes first), plays the voice sample unless it is empty and flashes the
// palette; redraws the log on screen 0Bh.
void Message_AddMessageLogEntry(int16_t kind, const char *text, const char *voice);
// 34b0:011a: line lineNo of message.txt, "attentio" sound, logged.
void Message_ShowMessageLine(int16_t lineNo);
// 34b0:0158: "message" sound, logged.
void Message_ShowMessage(const char *text);
// 34b0:0197: "hiba" sound, not logged.
void Message_ShowMessageNoLog(const char *text);
// 34b0:01df: line lineNo of kitalal.txt with a space on each side, via ShowMessage.
void Message_ShowKitalalLine(int16_t lineNo);
// 34b0:0237: centred box of up to 6 lines ('|' = line break, '&' = where the
// edit field goes); saves and restores the background if memory allows.
// Without edit it waits for a mouse click or key ("Press mouse button"
// blinking in the status bar); with edit it runs TextFile_InputLine on buf
// (a Pascal string, maxLen characters). logIt appends the lines to the
// message log as kind 99.
void Message_MessageBox(const char *text, const char *voice, bool logIt, uint8_t *buf,
                        bool edit, uint8_t maxLen);
// 34b0:0953
void Message_PlaySatelliteAnim(int16_t satNo);
// 34b0:0bbe: asks for the name of planet `planet` of system `sys`.
void Message_PromptPlanetName(int16_t sys, int16_t planet);
// 34b0:0c14
void Message_DrawMessageLog(void);
// 34b0:0c8e
void Message_Init(void);
