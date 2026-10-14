#include "play/ui_talk.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/mouse.h"
#include "game/textfile.h"
#include "play/lang.h"
#include "play/play.h"
#include "play/ui.h"

enum : int {
    SCREEN_COMMAND = 1,
    SCREEN_PUB = 23,
    SCREEN_ADVISOR = 24,
    SCREEN_PUB_TALK = 25,
    SCREEN_TALK = 34,
    // Talk and pub panels (alientalk.c, pub.c): hotspot "Talking" at
    // (6,134) 210x63; question rows 9 pixels high from y 134, row r hovered
    // while (y - 125) / 9 == r and 5 < x <= 216.
    PANEL_X = 60,
    PANEL_ROW_Y = 129,      // + 9 * row: the middle of panel row `row`
    PANEL_CONTINUE_Y = 192, // in the panel, below the rows a menu can have
    // Advisor menu (advisor.c): rows from y 93, row r while (y - 84) / 9 == r
    // and 111 < x <= 208.
    ADVISOR_X = 160,
    ADVISOR_ROW_Y = 88,     // + 9 * row
    TIMEOUT = 3000,
};

// --- Text helpers ---

// A row text as drawn by TextFile_DrawReply: without the answer code
// ("NN "), '|' line breaks as spaces, runs of blanks as one.
static void drawn_text(const char *p, int skip, char *out, size_t size)
{
    size_t n = 0;
    bool blank = true;  // drops leading blanks
    size_t len = SDL_strlen(p);
    for (size_t i = (size_t)skip; i < len && n + 1 < size; i++) {
        char c = p[i] == '|' ? ' ' : p[i];
        if (c == ' ') {
            if (!blank)
                out[n++] = ' ';
            blank = true;
        } else {
            out[n++] = c;
            blank = false;
        }
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = 0;
}

static int text_lines(const char *p)
{
    return TextFile_CountLines(p);
}

// `want` (normalised like drawn_text) starts `text`, ignoring case.
static bool text_starts(const char *text, const char *want)
{
    char w[TALK_TEXT_MAX];
    drawn_text(want, 0, w, sizeof w);
    return SDL_strncasecmp(text, w, SDL_strlen(w)) == 0;
}

static int find_question(const TalkMenu *m, const char *want)
{
    for (int k = 0; k < m->count; k++)
        if (text_starts(m->text[k], want))
            return k;
    return -1;
}

static void log_menu(const char *what, const TalkMenu *m)
{
    for (int k = 0; k < m->count; k++)
        play_log("  %s row %d: \"%s\"", what, m->row[k], m->text[k]);
}

// TEXT\<name><num><ext>.
static void text_file_name(const char *name, const PString *num, const char *ext, char *out,
                           size_t size)
{
    SDL_snprintf(out, size, "%s%.*s%s", name, num->len, num->s, ext);
}

// Line `line` of TEXT\<name><num><ext>, as the game reads it, in English
// (lang.h).
static void text_file_line(const char *name, const PString *num, const char *ext, int line,
                           char out[256])
{
    char file[64], text[256] = "";
    text_file_name(name, num, ext, file, sizeof file);
    if (line > 0)
        TextFile_ReadTextLine(file, (int16_t)line, text, sizeof text);
    lang_line(file, line, text, out, 256);
}

// A question as the game holds it, a line of TEXT\<name><num><ext>, in
// English, as drawn (drawn_text).
static void question_text(const char *q, const char *name, const PString *num, const char *ext,
                          char *out, size_t size)
{
    char file[64], english[256];
    text_file_name(name, num, ext, file, sizeof file);
    lang_line_of(file, q, english, sizeof english);
    drawn_text(english, 3, out, size);
}

// --- Alien talk (screen 34): alientalk.c, main_program.c screen_alien_talk ---

static bool on_screen(int screen)
{
    return ds.Game_CurrentScreen == screen && !ds.Game_ScreenSwitch;
}

bool ui_talk_shown(void)
{
    return on_screen(SCREEN_TALK) && ds.Mouse_HotspotCount == 1;
}

int ui_talk_alien(void)
{
    return ui_talk_shown() ? ds.AlienTalk_Race : 0;
}

bool ui_talk_menu(TalkMenu *m)
{
    m->count = 0;
    if (!ui_talk_shown() || ds.AlienTalk_Locked)
        return false;
    int row = 1;
    for (int k = 0; k < ds.Alien_CurMenu[0] && k < TALK_QUESTIONS_MAX; k++) {
        const char *q = (const char *)ds.AlienTalk_QuestionBuf + k * 0x100;
        question_text(q, "kerdes", (const PString *)&ds.AlienTalk_RaceStr, ".at", m->text[k],
                      TALK_TEXT_MAX);
        m->row[k] = row;
        row += text_lines(q);
        m->count++;
    }
    return true;
}

void ui_talk_reply(char *out, size_t size)
{
    out[0] = 0;
    if (!ui_talk_shown() || !ds.AlienTalk_Locked)
        return;
    char line[256];
    text_file_line("valasz", (const PString *)&ds.AlienTalk_RaceStr, ".at", ds.AlienTalk_ReplyLine,
                   line);
    drawn_text(line, 3, out, size);
}

// The highlighted question (1..), drawn in colour FC.
static int talk_hovered(void)
{
    return (int16_t)ds.AlienTalk_SelectedLine;
}

static bool talk_replied(void)
{
    return ds.AlienTalk_Locked != 0;
}

// --- Pub questions (screen 25): pub.c, main_program.c screen_pub_dialog ---

static const char *pub_line(int i)
{
    return (const char *)ds.Pub_Lines + (i - 1) * 256;
}

bool ui_pub_menu(TalkMenu *m)
{
    m->count = 0;
    if (!on_screen(SCREEN_PUB_TALK) || ds.Mouse_HotspotCount != 1 || ds.Pub_Replied)
        return false;
    int row = 1;
    for (int k = 0; k < ds.Pub_CurMenu.Count && k < TALK_QUESTIONS_MAX; k++) {
        question_text(pub_line(k + 1), "kerdes", (const PString *)&ds.Pub_PersonStr, ".loc",
                      m->text[k], TALK_TEXT_MAX);
        m->row[k] = row;
        row += text_lines(pub_line(k + 1));
        m->count++;
    }
    return true;
}

void ui_pub_reply(char *out, size_t size)
{
    out[0] = 0;
    if (!on_screen(SCREEN_PUB_TALK) || !ds.Pub_Replied)
        return;
    // The trader's prices are made up in the first question line (pub.c
    // Pub_ShowReply); every other reply is the file line.
    if (ds.Pub_Person == 2 && ds.Pub_ReplyLine > 9 && ds.Pub_ReplyLine < 14) {
        drawn_text(pub_line(1), 3, out, size);
        return;
    }
    char line[256];
    text_file_line("valasz", (const PString *)&ds.Pub_PersonStr, ".loc", ds.Pub_ReplyLine, line);
    drawn_text(line, 3, out, size);
}

// The highlighted question (1..).
static int pub_hovered(void)
{
    return ds.Pub_HoverItem;
}

static bool pub_replied(void)
{
    return ds.Pub_Replied != 0;
}

bool ui_pub_present(const char *name)
{
    // Hotspots 1..10 are the people (Pub_SetupPubHotspots).
    int j = ui_find_hotspot(name, 1);
    return on_screen(SCREEN_PUB) && j >= 1 && j <= 10;
}

void ui_pub_log_people(void)
{
    char line[256] = "";
    for (int j = 1; j <= 10 && j <= ds.Mouse_HotspotCount; j++) {
        const THotspot *h = Mouse_Hotspot((int16_t)j);
        if (h->W > 0 && h->Caption.len > 0)
            SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), "'%.*s' ",
                         h->Caption.len, h->Caption.s);
    }
    play_log("pub people: %s", line);
}

// --- The question-and-reply engine shared by screens 34 and 25 ---

typedef struct {
    int screen;
    const char *what;
    bool (*menu)(TalkMenu *m);
    void (*reply)(char *out, size_t size);
    int (*hovered)(void);
    bool (*replied)(void);
} Dialog;

static const Dialog *cur;

static bool dialog_left(void)
{
    return ds.Game_CurrentScreen != cur->screen || ds.Game_ScreenSwitch;
}

// The questions are drawn, or the dialogue is over.
static bool menu_or_left(void *unused)
{
    (void)unused;
    TalkMenu m;
    return dialog_left() || cur->menu(&m);
}

static bool reply_or_left(void *unused)
{
    (void)unused;
    return dialog_left() || cur->replied();
}

static bool hovered_is(void *arg)
{
    return cur->hovered() == *(int *)arg;
}

static char last_reply[TALK_TEXT_MAX];

const char *ui_last_reply(void)
{
    return last_reply;
}

// Plays `path` to the end of the dialogue (the screen leaves cur->screen)
// and returns the questions asked. With `may_end` the dialogue may end
// before the path does.
static int run_dialog(const Dialog *d, const char *const path[], bool may_end)
{
    cur = d;
    last_reply[0] = 0;
    for (int step = 0;; step++) {
        if (!ui_wait_until(menu_or_left, nullptr, TIMEOUT))
            play_fail("%s: no questions drawn", d->what);
        // Over: the next screen may first show a box, so no idle wait here.
        if (dialog_left()) {
            if (path[step] && !may_end)
                play_fail("%s: ended before \"%s\"", d->what, path[step]);
            return step;
        }
        if (!ui_wait_idle(TIMEOUT))
            play_fail("%s: busy with its questions drawn", d->what);
        TalkMenu m;
        d->menu(&m);
        log_menu(d->what, &m);
        if (!path[step])
            play_fail("%s: the path ended but the questions go on", d->what);
        int k = find_question(&m, path[step]);
        if (k < 0)
            play_fail("%s: no question \"%s\"", d->what, path[step]);
        // Hover the question's first row until it is highlighted, then click.
        int x = PANEL_X, y = PANEL_ROW_Y + 9 * m.row[k], want = k + 1;
        play_mouse(x, y, 0);
        if (!play_wait_until(hovered_is, &want, 50))
            play_fail("%s: \"%s\" did not highlight", d->what, m.text[k]);
        ui_click(x, y);
        play_log("%s: asked \"%s\"", d->what, m.text[k]);
        if (!ui_wait_until(reply_or_left, nullptr, TIMEOUT) || !ui_wait_idle(TIMEOUT))
            play_fail("%s: no reply to \"%s\"", d->what, m.text[k]);
        if (dialog_left())
            continue;
        d->reply(last_reply, sizeof last_reply);
        play_log("%s: reply \"%s\"", d->what, last_reply);
        play_dump("%s-reply", d->what);
        ui_click(PANEL_X, PANEL_CONTINUE_Y);
    }
}

static const Dialog talk_dialog = {
    SCREEN_TALK, "talk", ui_talk_menu, ui_talk_reply, talk_hovered, talk_replied,
};
static const Dialog pub_dialog = {
    SCREEN_PUB_TALK, "pub", ui_pub_menu, ui_pub_reply, pub_hovered, pub_replied,
};

static bool talk_menu_shown(void *unused)
{
    (void)unused;
    TalkMenu m;
    return ui_talk_menu(&m);
}

void ui_alien_talk_path(const char *const path[])
{
    // Talk 6 opens with the tractor-beam scene before its questions.
    if (!ui_wait_until(talk_menu_shown, nullptr, 5000))
        play_fail("no alien talk");
    play_log("alien talk %d", ui_talk_alien());
    play_dump("talk%d", ui_talk_alien());
    run_dialog(&talk_dialog, path, false);
    if (!ui_wait_screen(SCREEN_COMMAND, TIMEOUT))
        play_fail("no command centre after the talk");
}

// The policy's path per talk.
static const struct {
    int race;
    const char *path[8];
} talk_paths[] = {
    {2, {"We are glad", "Could you be more specific ?", "What can I do ?", "O.K."}},
    {3, {"Greetings Alien !", "What is it ?", "What is the price ?", "Yes."}},
    {4, {"Greetings Alien.", "What shall we fight with?", "O.K. We will do our best to help."}},
    {5, {"Greetings Alien.", "That is right.", "When are we receiving your weapons?"}},
    {6, {"Tell me, what are your plans?", "You will send a message to your leaders", "All right."}},
    {7, {"Greetings Phelonians.", "What will you give in return?", "I think something more valuable",
         "It's a pleasure doing business with you."}},
    {9, {"Greetings Erans.", "Yes, we are sure.", "Yes ! My people will welcome you."}},
    {10, {"Greetings Syonians !", "What do you know about the destiny of earth ?", "Go on!",
          "How could he control the rebellious leaders ?", "Why did you let this happen ?",
          "Thank you for your confidence in us."}},
};

int ui_alien_talk_policy(void)
{
    if (!ui_wait_until(talk_menu_shown, nullptr, 5000) || !ui_wait_idle(TIMEOUT))
        play_fail("no alien talk");
    int race = ui_talk_alien();
    TalkMenu m;
    ui_talk_menu(&m);
    for (size_t i = 0; i < SDL_arraysize(talk_paths); i++) {
        if (talk_paths[i].race != race)
            continue;
        if (find_question(&m, talk_paths[i].path[0]) < 0) {
            log_menu("talk", &m);
            play_fail("talk %d: unexpected first question", race);
        }
        ui_alien_talk_path(talk_paths[i].path);
        return race;
    }
    log_menu("talk", &m);
    play_fail("talk %d: no policy path", race);
}

// --- Pub (screen 23) ---

static const char *const PUB_LABEL = "SPACE LOCAL";  // toolbar button, command 45

int ui_pub_ask_path(const char *person, const char *const path[])
{
    if (ui_screen() != SCREEN_PUB) {
        if (!ui_toolbar(PUB_LABEL) || !ui_wait_screen(SCREEN_PUB, TIMEOUT))
            play_fail("ui_pub_ask: no pub");
    }
    ui_pub_log_people();
    if (!ui_pub_present(person)) {
        play_log("ui_pub_ask: %s is not in the pub", person);
        return 0;
    }
    ui_hotspot(person);
    if (!ui_wait_screen(SCREEN_PUB_TALK, TIMEOUT))
        play_fail("ui_pub_ask: %s did not answer", person);
    play_log("pub: talking to %s", person);
    int asked = run_dialog(&pub_dialog, path, true);
    if (!ui_wait_screen(SCREEN_PUB, TIMEOUT))
        play_fail("ui_pub_ask: no pub after talking to %s", person);
    return asked;
}

// --- Advisor (screen 24): advisor.c ---

bool ui_advisor_menu(TalkMenu *m)
{
    m->count = 0;
    if (!on_screen(SCREEN_ADVISOR) || ds.Mouse_HotspotCount != 1)
        return false;
    for (int k = 0; k < ds.Advisor_CurMenu.Count && k < TALK_QUESTIONS_MAX; k++) {
        const char *p = (const char *)ds.Advisor_MenuLines + k * 0x3D;
        char english[256];
        lang_line_of("rkerdes1.sp", p, english, sizeof english);
        drawn_text(english, 0, m->text[k], TALK_TEXT_MAX);
        m->row[k] = k + 1;
        m->count++;
    }
    return true;
}

void ui_advisor_reply(char *out, size_t size)
{
    // Answer 10 names the price (advisor.c Advisor_ShowAdvisorAnswer); 7
    // and 8 name an invention, logged here as the file's placeholder.
    if (ds.Advisor_Answer == 10) {
        SDL_snprintf(out, size, "It will be %d credits", (int)ds.Advisor_TrainingPrice);
        return;
    }
    char line[256];
    PString none = {0};
    text_file_line("valasz1.sp", &none, "", ds.Advisor_Answer, line);
    drawn_text(line, 3, out, size);
}

static bool advisor_menu_or_left(void *unused)
{
    (void)unused;
    TalkMenu m;
    return !on_screen(SCREEN_ADVISOR) || ui_advisor_menu(&m);
}

// Clicks the advisor question `want`; false if the menu lacks it.
static bool advisor_ask(const char *want)
{
    if (!ui_wait_until(advisor_menu_or_left, nullptr, TIMEOUT) || !ui_wait_idle(TIMEOUT)
        || !on_screen(SCREEN_ADVISOR))
        play_fail("advisor: no questions drawn");
    TalkMenu m;
    ui_advisor_menu(&m);
    log_menu("advisor", &m);
    int k = find_question(&m, want);
    if (k < 0)
        return false;
    // The row under the mouse at the release is the one asked (the menu
    // polls while the button is down).
    ui_click(ADVISOR_X, ADVISOR_ROW_Y + 9 * m.row[k]);
    if (!ui_wait_until(advisor_menu_or_left, nullptr, TIMEOUT))
        play_fail("advisor: no answer to \"%s\"", want);
    ui_wait_idle(TIMEOUT);
    char reply[TALK_TEXT_MAX];
    ui_advisor_reply(reply, sizeof reply);
    play_log("advisor: asked \"%s\", answer \"%s\"", m.text[k], reply);
    return true;
}

bool ui_university(const char *who, const char *course)
{
    return ui_university_upto(who, course, INT32_MAX, nullptr);
}

bool ui_university_upto(const char *who, const char *course, int32_t max_price, int32_t *price)
{
    if (price)
        *price = 0;
    if (ui_screen() != SCREEN_COMMAND)
        play_fail("ui_university: not on the command centre");
    if (!ui_hotspot(who) || !ui_wait_screen(SCREEN_ADVISOR, TIMEOUT))
        play_fail("ui_university: %s does not answer", who);
    play_dump("advisor");
    if (!advisor_ask("Go to university?"))
        play_fail("ui_university: no \"Go to university?\"");
    if (course && on_screen(SCREEN_ADVISOR) && !advisor_ask(course))
        play_log("ui_university: no course \"%s\" offered", course);
    char quote[TALK_TEXT_MAX];
    int quoted = 0;
    ui_advisor_reply(quote, sizeof quote);
    if (on_screen(SCREEN_ADVISOR) && SDL_sscanf(quote, "It will be %d credits", &quoted) == 1) {
        if (price)
            *price = quoted;
        if (quoted > max_price)
            advisor_ask("You shouldn't go.");
        else
            advisor_ask("I will pay.");
    }
    // Only "All right, I'll go." closes the screen (VALASZ1.SP line 13, menu 00).
    if (!on_screen(SCREEN_ADVISOR)) {
        if (!ui_wait_screen(SCREEN_COMMAND, TIMEOUT))
            play_fail("ui_university: no command centre after the advisor");
        play_log("ui_university: %s goes", who);
        return true;
    }
    // Refused: the first menu is back. The menu bar leads out.
    ui_advisor_reply(last_reply, sizeof last_reply);
    play_log("ui_university: %s refused: \"%s\"", who, last_reply);
    if (!ui_toolbar("BACK TO M.SCREEN") || !ui_wait_screen(SCREEN_COMMAND, TIMEOUT))
        play_fail("ui_university: no way back to the command centre");
    return false;
}
