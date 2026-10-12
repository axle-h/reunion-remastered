// UI drivers for the dialogue screens: alien talks (screen 34), the pub (23) and its
// questions (25), and the advisor (24) for university courses. Story scenes
// (33) are interrupts: ui_message.h.
//
// The talk, pub and advisor screens draw a menu of questions as rows of
// text; a driver picks a question by its drawn text (case and spacing
// ignored, the path item a prefix of the question), never by a fixed row.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum : int {
    TALK_QUESTIONS_MAX = 5,  // questions in one menu (pub and advisor; talks have 3)
    TALK_TEXT_MAX = 256,
};

// A question menu as drawn: each question's text (its rows joined by single
// spaces, without the answer code) and the panel row it starts on (1 = top).
typedef struct {
    int count;
    char text[TALK_QUESTIONS_MAX][TALK_TEXT_MAX];
    int row[TALK_QUESTIONS_MAX];
} TalkMenu;

// --- Observers (read-only; each reads what the screen shows) ---

// Screen 34 is up with its question panel.
bool ui_talk_shown(void);
// The alien whose portrait the talk screen shows (alien\alien<n> by race), as
// the talk number (talk 2 = Jaanosian meeting 1, ...; ui_talk.c).
int ui_talk_alien(void);
// The question rows drawn in the talk panel. False while a reply is shown in
// their place, or off screen 34.
bool ui_talk_menu(TalkMenu *m);
// The alien's reply drawn in the panel ("" while the questions are shown).
void ui_talk_reply(char *out, size_t size);
// The pub (screen 23) shows a person captioned `name`.
bool ui_pub_present(const char *name);
// Logs the people the pub shows (their hotspot captions).
void ui_pub_log_people(void);
// The pub questions (screen 25) drawn, like ui_talk_menu.
bool ui_pub_menu(TalkMenu *m);
// The person's reply drawn on screen 25 ("" while the questions are shown).
void ui_pub_reply(char *out, size_t size);
// The advisor questions (screen 24) drawn.
bool ui_advisor_menu(TalkMenu *m);
// The advisor's answer drawn under the question on screen 24.
void ui_advisor_reply(char *out, size_t size);
// The last reply a talk or pub driver saw drawn, or the advisor's refusal
// of a course ("" before any).
const char *ui_last_reply(void);

// --- Drivers ---

// Plays the talk on screen 34 along `path` (nullptr-terminated): for each
// menu clicks the question whose text starts with the next path item, then
// clicks the reply away, until the talk ends and the command centre is back
// (closing the box some talks end with). Fails the run if a menu lacks the
// next item or the path ends before the talk.
void ui_alien_talk_path(const char *const path[]);
#define ui_alien_talk(...) ui_alien_talk_path((const char *const[]){__VA_ARGS__, nullptr})
// Waits for the talk screen, recognises the talk by the alien shown and its
// first question, and plays the policy's path (ui_talk.c).
// Returns the talk (race) number. Fails on a talk the policy has no path for.
int ui_alien_talk_policy(void);
// From the command centre or the pub: asks `person` (pub caption) the
// questions along `path` to the end of the conversation and returns to the
// pub. The conversation may end before the path (e.g. the Informer's "I
// can't tell you anything new."; see ui_last_reply). Returns the questions
// asked, 0 if the person isn't in the pub.
int ui_pub_ask_path(const char *person, const char *const path[]);
#define ui_pub_ask(person, ...) ui_pub_ask_path(person, (const char *const[]){__VA_ARGS__, nullptr})
// From the command centre: sends staff member `who` (caption) to university;
// `course` is the developer's course ("Maths & physics." ...), nullptr for the
// others. Pays when asked. True if the advisor goes ("All right, I'll go."),
// false on a refusal (logged); either way ends on the command centre.
bool ui_university(const char *who, const char *course);
// ui_university that declines ("You shouldn't go.") a course quoted above
// `max_price`; *price (may be nullptr) gets the quote, 0 if none was made.
bool ui_university_upto(const char *who, const char *course, int32_t max_price, int32_t *price);
