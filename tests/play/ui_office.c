#include "play/ui_office.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/mem.h"
#include "game/mouse.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"

enum : int {
    TIMEOUT = 3000,
    SCREEN_W = 320,
    // Research info panel (research.c Research_DrawInventionInfo).
    PANEL_X = 0xBE, PANEL_NAME_X = 0xC8, PANEL_NAME_Y = 0x58, PANEL_STATUS_Y = 0x64,
    PANEL_PERCENT_Y = 0x70,
    ICON_W = 0x1F, ICON_H = 0xE, ICON_TOLERANCE = 4,  // the blinking selection mark
    // Shop (shop.c): list rows, queue panel, editor ore panel.
    LIST_X = 0x10, LIST_ROWS = 13, LIST_CLICK_X = 0x40, HIGHLIGHT = 0xFC,
    QUEUE_LABEL_X = 0xE, QUEUE_Y1 = 0xB4, QUEUE_Y2 = 0xBD,
    QUEUE_LEFT_X = 100, QUEUE_RIGHT_X = 0x108,
    // Message log (message.c Message_DrawMessageLog).
    LOG_X = 10, LOG_Y = 0x30,
};

#define CDS_BUF (*(const FarPtr *)ds_at(0x78AC))  // research icon sheet (research.c)

// --- Navigation ---

static const char *screen_label(int screen)
{
    switch (screen) {
    case 1: return "BACK TO M.SCREEN";
    case 3: return "RESEARCH-DESIGN";
    case 5: return "INFO-BUY";
    case 11: return "MESSAGES";
    default: return nullptr;
    }
}

void ui_goto(int screen)
{
    const char *label = screen_label(screen);
    if (!label)
        play_fail("ui_goto: no route to screen %d", screen);
    ui_open_screen(label, screen);
}

// --- Research ---

// Hotspot j of the research screen is product j's icon, captioned with its
// name (research.c Research_SetupResearchHotspots).
static int icon_index(const char *name)
{
    if (ui_screen() != 3)
        return 0;
    for (int j = 1; j <= ds.Mouse_HotspotCount; j++) {
        const THotspot *h = Mouse_Hotspot((int16_t)j);
        if ((size_t)h->Caption.len == SDL_strlen(name)
            && SDL_strncasecmp(h->Caption.s, name, h->Caption.len) == 0)
            return j;
    }
    return 0;
}

// Pixels of icon k that differ from frame `frame` of the icon sheet
// (research.c Research_DrawIconFrame), the cursor's excluded.
static int icon_mismatch(int k, int frame)
{
    FarPtr base = CDS_BUF;
    uint16_t off = (uint16_t)((uint16_t)base + frame / 10 * 0x12C0 + frame * 0x20);
    const uint8_t *src = far_host(far_ptr((uint16_t)(base >> 16), off));
    int dest = (k - 1) / 5 * 0x1400 + 0x4100 + (k - 1) % 5 * 0x20 + 0x140;
    const uint8_t *screen = mem + MEM_VIDEO_SEG * 16 + dest;
    int x0 = dest % SCREEN_W, y0 = dest / SCREEN_W;
    int cx = ds.Mouse_CursorSaveX, cy = ds.Mouse_CursorSaveY;
    bool cursor = ds.Mouse_CursorState == 0 && !ds.Mouse_CursorFrozen;
    int bad = 0;
    for (int r = 0; r < ICON_H; r++) {
        for (int c = 0; c < ICON_W; c++) {
            if (cursor && x0 + c >= cx && x0 + c < cx + 11 && y0 + r >= cy && y0 + r < cy + 11)
                continue;
            bad += screen[r * SCREEN_W + c] != src[r * SCREEN_W + c];
        }
    }
    return bad;
}

int ui_research_icon(const char *name)
{
    int k = icon_index(name);
    if (k == 0 || CDS_BUF == 0)
        return UI_RS_UNKNOWN;
    // research.c: frame of each state, then the working animation.
    static const struct { int frame, state; } frames[] = {
        {10, UI_RS_CAN_DEVELOP}, {0x13, UI_RS_DEVELOPING}, {0x1E, UI_RS_CAN_ANALYSE},
        {0x27, UI_RS_ANALYSING}, {0, UI_RS_DONE},
        {0x28, UI_RS_RUNNING}, {0x29, UI_RS_RUNNING}, {0x2A, UI_RS_RUNNING}, {0x2B, UI_RS_RUNNING},
        {0x2C, UI_RS_RUNNING}, {0x2D, UI_RS_RUNNING}, {0x2E, UI_RS_RUNNING}, {0x2F, UI_RS_RUNNING},
        {0x30, UI_RS_RUNNING},
    };
    int best = UI_RS_UNKNOWN, least = ICON_TOLERANCE + 1;
    for (size_t i = 0; i < SDL_arraysize(frames); i++) {
        int bad = icon_mismatch(k, frames[i].frame);
        if (bad < least) {
            least = bad;
            best = frames[i].state;
        }
    }
    return best;
}

bool ui_research_panel(UiResearchPanel *p)
{
    if (ui_screen() != 3)
        return false;
    char t[32];
    *p = (UiResearchPanel){.percent = -1};
    ui_read_text(PANEL_NAME_X, PANEL_NAME_Y, 16, p->name, sizeof p->name);
    ui_read_text(PANEL_X, PANEL_STATUS_Y, 18, p->status, sizeof p->status);
    ui_read_text(PANEL_X, PANEL_PERCENT_Y, 18, t, sizeof t);
    p->no_developer = SDL_strcmp(t, "No developer") == 0;
    int n;
    if (SDL_sscanf(t, "Completed: %d%%", &n) == 1)
        p->percent = n;
    return p->name[0] != 0;
}

bool ui_developer_skills(int skills[4])
{
    if (ui_screen() != 3)
        return false;
    // Research_DrawDeveloperSkills: at the x of the layout table DS:0C24
    // (entries 1..4) + 100, y BCh.
    const int16_t *xs = (const int16_t *)ds_at(0xC24);
    for (int k = 1; k <= 4; k++) {
        int32_t v;
        if (!ui_read_number(xs[k] + 100, 0xBC, 2, &v))
            return false;
        skills[k - 1] = v;
    }
    return true;
}

static bool panel_running(const char *name)
{
    UiResearchPanel p;
    return ui_research_panel(&p) && SDL_strcasecmp(p.name, name) == 0
           && SDL_strncmp(p.status, "Under ", 6) == 0;
}

static bool icon_settled(void *arg)
{
    return ui_research_icon(arg) != UI_RS_UNKNOWN;
}

bool ui_research_start(const char *name)
{
    ui_goto(3);
    if (icon_index(name) == 0)
        return false;
    ui_wait_until(icon_settled, (void *)name, 200);
    int st = ui_research_icon(name);
    play_log("research %s: icon state %d", name, st);
    if (st == UI_RS_RUNNING || st == UI_RS_DEVELOPING || st == UI_RS_ANALYSING)
        return true;
    if (st != UI_RS_CAN_DEVELOP && st != UI_RS_CAN_ANALYSE)
        return false;
    ui_hotspot(name);
    ui_wait_idle(TIMEOUT);
    if (!panel_running(name)) {
        play_dump("research-start");
        play_fail("research %s did not start", name);
    }
    UiResearchPanel p;
    ui_research_panel(&p);
    play_log("research %s: %s, completed %d%%", name, p.status, p.percent);
    return true;
}

// --- Info / buy ---

static bool list_mode(void)
{
    return ui_toolbar_has("SELECT OFF");
}

static void list_row(int i, char *out, size_t size)
{
    ui_read_text(LIST_X, i * 9 + 0x2E, 16, out, size);
}

static bool rows_equal(char a[LIST_ROWS][20], char b[LIST_ROWS][20])
{
    for (int i = 0; i < LIST_ROWS; i++)
        if (SDL_strcmp(a[i], b[i]) != 0)
            return false;
    return true;
}

static void read_rows(char rows[LIST_ROWS][20])
{
    for (int i = 0; i < LIST_ROWS; i++)
        list_row(i + 1, rows[i], 20);
}

// Clicks a scroll hotspot; false if the list did not move.
static bool scroll(const char *caption)
{
    char before[LIST_ROWS][20], after[LIST_ROWS][20];
    read_rows(before);
    if (!ui_hotspot(caption))
        return false;
    ui_wait_idle(TIMEOUT);
    read_rows(after);
    return !rows_equal(before, after);
}

// The list row (1..13) showing `name`, 0 if none.
static int find_row(const char *name)
{
    for (int i = 1; i <= LIST_ROWS; i++) {
        char row[20];
        list_row(i, row, sizeof row);
        if (SDL_strcasecmp(row, name) == 0)
            return i;
    }
    return 0;
}

bool ui_shop_select(const char *name)
{
    ui_goto(5);
    if (!list_mode()) {
        if (!ui_toolbar("SELECT"))
            play_fail("ui_shop_select: no SELECT button");
        ui_wait_idle(TIMEOUT);
    }
    int i = find_row(name);
    for (int guard = 0; i == 0 && guard < 40 && scroll("Scroll up"); guard++)
        i = find_row(name);
    for (int guard = 0; i == 0 && guard < 40 && scroll("Scroll down"); guard++)
        i = find_row(name);
    if (i == 0)
        return false;
    int y = i * 9 + 0x2E;
    if (ui_text_colour(LIST_X, y) != HIGHLIGHT) {
        // shop.c Shop_HandleListClick: row = (y - 30h) / 9.
        ui_click(LIST_CLICK_X, 0x30 + i * 9 + 4);
        ui_wait_idle(TIMEOUT);
    }
    if (ui_text_colour(LIST_X, y) != HIGHLIGHT)
        play_fail("ui_shop_select: %s not highlighted after the click", name);
    return true;
}

bool ui_shop_queue(UiShopQueue *q)
{
    *q = (UiShopQueue){0};
    if (ui_screen() != 5)
        return false;
    char label[32];
    ui_read_text(QUEUE_LABEL_X, QUEUE_Y1, 13, label, sizeof label);
    if (SDL_strcmp(label, "Bought items:") == 0) {
        q->shown = ui_read_number(QUEUE_LEFT_X, QUEUE_Y1, 5, &q->queued)
                   && ui_read_number(QUEUE_RIGHT_X, QUEUE_Y1, 6, &q->hours)
                   && ui_read_number(QUEUE_RIGHT_X, QUEUE_Y2, 8, &q->price);
    }
    ui_read_text(QUEUE_LABEL_X, QUEUE_Y2, 7, label, sizeof label);
    if (SDL_strcmp(label, "Stores:") == 0)
        ui_read_number(QUEUE_LEFT_X, QUEUE_Y2, 10, &q->stores);
    else
        ui_read_text(QUEUE_LABEL_X, QUEUE_Y2, 40, q->note, sizeof q->note);
    return true;
}

// The quantity editor (toolbar 0Fh, shop.c cmd 12h): the clock stops and the
// toolbar offers the quantity buttons.
static bool editor_open(void)
{
    return ui_toolbar_has("OK,BUY");
}

// Opens it for the product selected; false if BUY ITEM does nothing (an
// item that is built, not bought: shop.c Produced).
static bool open_editor(void)
{
    if (!ui_toolbar("BUY ITEM"))
        play_fail("ui_buy: no BUY ITEM button");
    ui_wait_idle(TIMEOUT);
    return editor_open();
}

static int32_t editor_quantity(void)
{
    int32_t n;
    if (!ui_read_number(QUEUE_LEFT_X, QUEUE_Y1, 5, &n))
        play_fail("ui_buy: no quantity in the editor");
    return n;
}

static void editor_button(const char *label)
{
    if (!ui_toolbar(label)) {
        ui_log_screen();
        play_fail("ui_buy: no %s button in the editor", label);
    }
    ui_wait_idle(TIMEOUT);
}

// "Ores in stock" lines in the editor (shop.c Shop_ShowOreStock): ores 1, 3,
// 2, 6, 5, 4 in two columns; the number is right-aligned after the name.
bool ui_shop_ores(const char *name, int32_t ores[6])
{
    if (!ui_shop_select(name) || !open_editor())
        return false;
    static const struct { int ore, x, y, w; } lines[6] = {
        {1, 0x8E, 0x52, 0xD}, {3, 0xE1, 0x52, 0xE}, {2, 0x8E, 0x5B, 0xD},
        {6, 0xE1, 0x5B, 0xE}, {5, 0x8E, 0x64, 0xD}, {4, 0xE1, 0x64, 0xE},
    };
    char t[32];
    ui_read_text(0x88, 0x49, 0x1D, t, sizeof t);
    bool ok = SDL_strcmp(t, "Ores in stock") == 0;
    for (int i = 0; i < 6; i++) {
        ui_read_text(lines[i].x, lines[i].y, lines[i].w, t, sizeof t);
        size_t end = SDL_strlen(t), start = end;
        while (start > 0 && (SDL_isdigit((unsigned char)t[start - 1]) || t[start - 1] == 'O'))
            start--;
        for (size_t c = start; c < end; c++)
            t[c] = t[c] == 'O' ? '0' : t[c];
        ok = ok && start < end;
        ores[lines[i].ore - 1] = (int32_t)SDL_strtol(t + start, nullptr, 10);
    }
    editor_button("CANCEL BUY");
    return ok;
}

int ui_buy(const char *name, int n)
{
    UiShopQueue q;
    if (!ui_shop_select(name) || !ui_shop_queue(&q))
        return -1;
    int32_t before = q.shown ? q.queued : 0;
    if (!open_editor())
        return -1;
    int32_t qty = editor_quantity(), target = before + n;
    while (qty < target) {
        editor_button(target - qty >= 10 ? "ADD TEN" : "ADD ONE");
        int32_t now = editor_quantity();
        if (now <= qty)
            break;  // cut by credits or ore
        qty = now;
    }
    editor_button("OK,BUY");
    ui_shop_queue(&q);
    int32_t after = q.shown ? q.queued : 0;
    play_log("bought %d %s (asked %d): queue %d, %d h, stores %d", after - before, name, n, after,
             q.hours, q.stores);
    return after - before;
}

// --- Message log ---

int ui_message_log(char rows[UI_LOG_ROWS][UI_LOG_COLS + 1])
{
    ui_goto(11);
    int n = 0;
    for (int i = 1; i <= UI_LOG_ROWS; i++) {
        ui_read_text(LOG_X, i * 9 + LOG_Y, UI_LOG_COLS, rows[i - 1], UI_LOG_COLS + 1);
        if (rows[i - 1][0])
            n = i;
    }
    return n;
}

void ui_last_message(char *out, size_t size)
{
    char rows[UI_LOG_ROWS][UI_LOG_COLS + 1];
    int n = ui_message_log(rows);
    SDL_strlcpy(out, n ? rows[n - 1] : "", size);
}

bool ui_log_has(const char *text)
{
    // Message_DrawMessageLog draws these entries ({kind, string[50]}).
    for (int i = 1; i <= ds.Message_LogCount; i++) {
        const uint8_t *e = (const uint8_t *)far_host(ds.Message_Log) + (i - 1) * 0x35 + 2;
        char s[64];
        SDL_snprintf(s, sizeof s, "%.*s", e[0], (const char *)e + 1);
        if (SDL_strstr(s, text))
            return true;
    }
    return false;
}
