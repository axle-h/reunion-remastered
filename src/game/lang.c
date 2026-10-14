#include "game/lang.h"

#include <SDL3/SDL.h>

static Language language = LANGUAGE_ENGLISH;

// The code texts tools/cd_texts.py cannot pair: where the other builds'
// code differs, what each does instead (by the English code's address).
// Most drop a piece of a message ("") where the translation words it
// differently; nullptr: as lang_texts.c has it.
static const LangTextAt manual[] = {
    // Sim (1b8a): messages.
    {0x1b8a03cf, "s", {"", nullptr, nullptr}},  // "You got 5 pieces of Hunters"
    {0x1b8a1e5b, " system", {nullptr, "", nullptr}},
    {0x1b8a1f6e, " Your ", {"", "", ""}},
    {0x1b8a2048, " system", {nullptr, "", nullptr}},
    {0x1b8a2d1a, " |    They found new planets", {nullptr, "", nullptr}},
    {0x1b8a2e8b, "Your ", {"", "", ""}},
    {0x1b8a2ea0, " explored ", {"", "", ""}},
    {0x1b8a2ece, "'s moons", {nullptr, nullptr, ""}},
    {0x1b8a3027, " |", {nullptr, "", nullptr}},
    {0x1b8a314a, " |", {nullptr, "", nullptr}},
    {0x1b8a3a45, "        is under attack by ", {nullptr, "", nullptr}},
    // Pub (1fd2): the German trader names a price one way only.
    {0x1fd206bd, "09 It will be ", {"09 Das kostet ", nullptr, nullptr}},
    {0x1fd206d2, " credits", {" Kredite", nullptr, nullptr}},
    // Colonize (2841): the German race buttons have no captions.
    {0x284104e6, "Change people", {"", nullptr, nullptr}},
    {0x28410517, "Previous people", {"", nullptr, nullptr}},
    {0x28410548, "Next people", {"", nullptr, nullptr}},
    // Intel (29b9): the Italian weapons line has no gap before a ship class.
    {0x29b91b78, "  ", {nullptr, nullptr, ""}},
    // Shop (32b4): "Ore needed: (5 Stueck)", with no plural.
    {0x32b40e72, " piece", {"", "", ""}},
    {0x32b40ea8, "s )", {" St\201ck)", " unit\202s )", " unit\205 )"}},
    // Hire (3fab): the Italian text moved.
    {0x3fab08e5, "I'm not skilled enough for the job.",
     {nullptr, nullptr, "Non sono all'altezza di questo lavoro."}},
};

// The port's own texts (game/improvements.h).
static const LangText port_texts[] = {
    {" Miner Stations", {" Bergbaustationen", " stations mini\212res", " stazioni minerarie"}},
    // Trade routes (game/routes.h, game/portcmd.h).
    {"ROUTE            ", {"ROUTE            ", "ROUTE            ", "ROTTA            "}},
    {"ADD STOP         ", {"HALT DAZU        ", "AJOUTER ESCALE   ", "AGG. TAPPA       "}},
    {"REMOVE STOP      ", {"HALT ENTFERNEN   ", "RETIRER ESCALE   ", "TOGLI TAPPA      "}},
    {"START ROUTE      ", {"ROUTE STARTEN    ", "LANCER ROUTE     ", "AVVIA ROTTA      "}},
    {"END ROUTE        ", {"ROUTE BEENDEN    ", "ARRETER ROUTE    ", "FERMA ROTTA      "}},
    {" The route has all its 6 stops! ",
     {" Die Route hat schon 6 Halte! ", " La route a d\202j\205 ses 6 escales! ",
      " La rotta ha gi\205 6 tappe! "}},
    {" New Earth is where the route unloads! ",
     {" Dort entl\204dt die Route schon! ", " La route d\202charge d\202j\205 l\205! ",
      " La rotta scarica gi\205 l\205! "}},
    {" Only your colonies can be stops! ",
     {" Nur Ihre Kolonien k\224nnen Halte sein! ",
      " Seules vos colonies peuvent \210tre des escales! ",
      " Solo le tue colonie possono essere tappe! "}},
    {" This stop is on the route already! ",
     {" Dieser Halt ist schon auf der Route! ", " Cette escale est d\202j\205 sur la route! ",
      " Questa tappa \212 gi\205 sulla rotta! "}},
    {"another order", {"anderer Befehl", "autre ordre", "altro ordine"}},
    {"battle lost", {"Schlacht verloren", "bataille perdue", "battaglia persa"}},
    {"stop %d lost", {"Halt %d verloren", "escale %d perdue", "tappa %d persa"}},
    {"stop %d out of reach", {"Halt %d unerreichbar", "escale %d hors de port\202e", "tappa %d irraggiungibile"}},
    {"disbanded", {"aufgel\224st", "dissoute", "sciolta"}},
    {": route ended, ", {": Route beendet, ", ": route termin\202e, ", ": rotta finita, "}},
    {"Flying home to unload", {"Fliegt heim zum Entladen", "Rentre pour d\202charger", "Torna a scaricare"}},
    {"Flying to stop %d", {"Fliegt zu Halt %d", "Vole vers l'escale %d", "Vola alla tappa %d"}},
    {"Unloading at home, %d h left", {"Entl\204dt daheim, noch %d h", "D\202charge, encore %d h", "Scarica, ancora %d h"}},
    {"Loading at stop %d, %d h left",
     {"L\204dt an Halt %d, noch %d h", "Charge \205 l'escale %d, encore %d h", "Carica alla tappa %d, ancora %d h"}},
    {"Route ended: ", {"Route beendet: ", "Route termin\202e: ", "Rotta finita: "}},
    {"Route not running", {"Route l\204uft nicht", "Route arr\210t\202e", "Rotta ferma"}},
    {"Unload at ", {"Entladen auf ", "D\202charger sur ", "Scaricare su "}},
    {"Route: unloading", {"Route: entl\204dt", "Route: d\202charge", "Rotta: scarica"}},
    {"Route: homeward", {"Route: heimw\204rts", "Route: retour", "Rotta: ritorno"}},
    {"Route: at stop %d/%d", {"Route: an Halt %d/%d", "Route: escale %d/%d", "Rotta: tappa %d/%d"}},
    {"Route: to stop %d/%d", {"Route: zu Halt %d/%d", "Route: vers %d/%d", "Rotta: verso %d/%d"}},
    {"Move stop up", {"Halt nach oben", "Escale vers haut", "Tappa in su"}},
    {"Move stop down", {"Halt nach unten", "Escale vers bas", "Tappa in gi\227"}},
    {"Select stop", {"Halt w\204hlen", "Choisir escale", "Scegli tappa"}},
    // The port's saves (game/saveload.h).
    {" This save is from a newer version of the game! ",
     {" Dieser Spielstand ist von einer neueren Version! ",
      " Cette sauvegarde vient d'une version plus r\202cente! ",
      " Questo salvataggio \212 di una versione pi\227 recente! "}},
    // Autosave (game/autosave.h). The name with a date fits the slot's 19.
    {"Autosave", {"Autosave", "Auto", "Auto"}},
    {" The autosave can only be loaded! ",
     {" Diese Sicherung kann nur geladen werden! ",
      " Cette sauvegarde ne peut qu'\210tre charg\202e! ",
      " Questo salvataggio si pu\225 solo caricare! "}},
    {"Game saved", {"Spiel gesichert", "Partie sauvegard\202e", "Partita salvata"}},
    {"Can't save now", {"Nicht speicherbar", "Pas de sauvegarde", "Non salvabile ora"}},
    // Storage full message (game/storefull.h).
    {"Storage is full on ", {"Lager ist voll auf ", "Entrep\223t plein sur ", "Magazzino pieno su "}},
    // Universities that count (game/courses.h): the advisor's answer.
    {"We have no University I could go to.",
     {"Wir haben keine Universit\204t,|an die ich gehen k\224nnte.",
      "Nous n'avons aucune universit\202 o\227 aller.",
      "Non abbiamo un'universit\205 dove andare."}},
};

// printf formats where a translation changes the order of the pieces.
static const LangTextAt formats[] = {
    {0x28410228, "%s forces", {"%s Garnison", "Force %s", "Forze %s"}},
    {0x29b90a1f, "%s forces", {"%s Garnison", "Force %s", "Forze %s"}},
};

// The texts longer than their slot, whole (Lang_Slot).
enum : int { LONG_MAX_SLOTS = 16 };
static struct {
    const void *slot;
    PString text;
} long_slots[LONG_MAX_SLOTS];
static int long_count;

void Lang_Set(Language l)
{
    language = l < LANGUAGE_COUNT ? l : LANGUAGE_ENGLISH;
}

Language Lang_Get(void)
{
    return language;
}

static const char *pick(const char *const text[3], const char *english)
{
    const char *t = language != LANGUAGE_ENGLISH ? text[language - 1] : nullptr;
    return t ? t : english;
}

void Lang_PatchDS(void)
{
    long_count = 0;
    if (language == LANGUAGE_ENGLISH)
        return;
    for (int i = 0; i < lang_slot_count; i++) {
        const LangSlot *s = &lang_slots[i];
        const char *t = s->text[language - 1];
        if (!t)
            continue;
        size_t n = SDL_strlen(t);
        if (n > s->cap && long_count < LONG_MAX_SLOTS) {
            long_slots[long_count].slot = s->slot;
            long_slots[long_count].text.len = (uint8_t)n;
            SDL_memcpy(long_slots[long_count].text.s, t, n);
            long_count++;
        }
        n = SDL_min(n, (size_t)s->cap);
        s->slot[0] = (uint8_t)n;
        SDL_memcpy(s->slot + 1, t, n);
        SDL_memset(s->slot + 1 + n, 0, s->cap - n);
    }
}

static int text_cmp(const void *key, const void *entry)
{
    return SDL_strcmp(key, ((const LangText *)entry)->english);
}

static const LangText *find_text(const char *english)
{
    const LangText *t =
        SDL_bsearch(english, lang_texts, (size_t)lang_text_count, sizeof *lang_texts, text_cmp);
    for (size_t i = 0; !t && i < SDL_arraysize(port_texts); i++)
        if (SDL_strcmp(port_texts[i].english, english) == 0)
            t = &port_texts[i];
    return t;
}

const char *Lang_Text(const char *english)
{
    if (language == LANGUAGE_ENGLISH)
        return english;
    const LangText *t = find_text(english);
#ifndef NDEBUG
    if (!t) {
        for (int i = 0; i < lang_text_at_count; i++)
            if (SDL_strcmp(lang_texts_at[i].english, english) == 0)
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Lang_Text(\"%s\"): translated by address (Lang_TextAt)", english);
    }
#endif
    return t ? pick(t->text, english) : english;
}

const char *Lang_TextAt(uint32_t addr, const char *english)
{
    if (language == LANGUAGE_ENGLISH)
        return english;
    for (size_t i = 0; i < SDL_arraysize(manual); i++)
        if (manual[i].addr == addr && manual[i].text[language - 1])
            return manual[i].text[language - 1];
    for (int i = 0; i < lang_text_at_count; i++)
        if (lang_texts_at[i].addr == addr)
            return pick(lang_texts_at[i].text, english);
    // Not listed by address: as the text is everywhere, or as in English.
    const LangText *t = find_text(english);
    return t ? pick(t->text, english) : english;
}

const char *Lang_Format(uint32_t addr, const char *english)
{
    for (size_t i = 0; language != LANGUAGE_ENGLISH && i < SDL_arraysize(formats); i++)
        if (formats[i].addr == addr)
            return pick(formats[i].text, english);
    return english;
}

const PString *Lang_Slot(const void *slot)
{
    for (int i = 0; i < long_count; i++)
        if (long_slots[i].slot == slot)
            return &long_slots[i].text;
    return slot;
}
