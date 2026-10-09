// UI drivers and observers for the office screens: research (screen 3, cmd
// 2), info/buy production (screen 5, cmd 23) and the message log (screen 11,
// cmd 4), plus the status bar every screen shows.
//
// Observers read what the screen shows (ui_read_text, the game font matched
// in video memory) unless noted:
// - research: the icon drawn for each product (matched against the icon
//   sheet grafika\cds the screen draws from) and the info panel of the
//   selected product (name, "Can be developed" ..., "Completed: n%");
// - shop: the product list (selected row highlighted), the queue panel
//   ("Bought items:", "Time to go:", "Stores:", "Total price:") and, in the
//   quantity editor, "Ores in stock";
// - message log: its 15 rows.
// The status bar (credits, date) is in ui_message.h.
#pragma once

#include <stddef.h>
#include <stdint.h>

// --- Navigation ---

// Goes to the research (3), info/buy (5), message log (11) or command
// centre (1) screen by the toolbar, via the command centre if needed, and
// waits until it takes input.
void ui_goto(int screen);

// --- Research (screen 3) ---

enum : int {
    UI_RS_UNKNOWN = 0,     // no icon, or one that is changing
    UI_RS_CAN_DEVELOP = 1,
    UI_RS_DEVELOPING = 2,  // the icon shows the project paused or not yet animated
    UI_RS_CAN_ANALYSE = 3,
    UI_RS_ANALYSING = 4,
    UI_RS_DONE = 5,
    UI_RS_RUNNING = 6,     // the working animation (developing or analysing)
};

typedef struct {
    char name[20];     // "Project name :" panel
    char status[20];   // "Can be developed", "Under development", ...
    int percent;       // "Completed: n%" (the part still to do; -1 if not shown)
    bool no_developer; // "No developer" in place of the percentage
} UiResearchPanel;

// The product's icon state (UI_RS_*) on the research screen; UI_RS_UNKNOWN
// if no icon carries that caption.
int ui_research_icon(const char *name);
// The info panel of the selected product.
bool ui_research_panel(UiResearchPanel *panel);
// Starts researching `name` (goes to screen 3): clicks its icon if it shows
// "Can be developed" or "Can be analysed", then checks that the panel says
// "Under development"/"Under analysis". Does nothing if it is already
// running (a click would pause it). Starting a project stops the one
// running. False if the product is not available or done.
bool ui_research_start(const char *name);

// --- Info / buy (screen 5) ---

typedef struct {
    bool shown;          // the queue panel has numbers (else `note`)
    int32_t queued;      // "Bought items:"
    int32_t hours;       // "Time to go:"
    int32_t stores;      // "Stores:"
    int32_t price;       // "Total price:"
    char note[48];       // e.g. "This can be built on your colonies"
} UiShopQueue;

// Selects `name` in the shop's product list (goes to screen 5, list mode).
// False if the list does not have it (not researched).
bool ui_shop_select(const char *name);
// The queue panel of the selected product.
bool ui_shop_queue(UiShopQueue *queue);
// Opens the quantity editor for `name`, reads "Ores in stock" (Detoxin,
// Energon, Kremir, Lepitium, Raenium, Texon; the queue's ores included) and
// leaves it unchanged. False if the item can't be bought.
bool ui_shop_ores(const char *name, int32_t ores[6]);
// Buys `n` more of `name`: BUY ITEM, +10/+1 up to the queue + n, OK BUY.
// The game silently cuts the quantity to what credits and New Earth's ores
// allow; returns how many were added (read from "Bought items:"), or -1 if
// the item can't be bought.
int ui_buy(const char *name, int n);

// --- Message log (screen 11) ---

enum : int { UI_LOG_ROWS = 15, UI_LOG_COLS = 50 };

// Reads the message log rows (goes to screen 11); returns the number of
// rows with text, oldest first.
int ui_message_log(char rows[UI_LOG_ROWS][UI_LOG_COLS + 1]);
// The newest message log row ("" if empty), read on screen 11.
void ui_last_message(char *out, size_t size);
// The message log has a row containing `text`. Read from the log screen 11
// draws (Message_Log), so it works on any screen, e.g. as a wait's predicate.
bool ui_log_has(const char *text);
