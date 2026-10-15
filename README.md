# Reunion port

A native port of *Reunion* (Amnesty Design / Grandslam, 1994), the DOS space
strategy game, for modern systems. It is the original game rewritten in C
from the DOS release: it plays the same, with the original's bugs fixed and
a few improvements, each of which can be switched off.

The port comes without the game itself. It plays your own copy of the DOS
game, which it imports the first time it starts.

## Getting it

Download the build for your system from the releases page, or build it
yourself (see "Building from source" at the end). The program is
`reunion` (`reunion.exe` on Windows).

You also need a copy of the DOS game. The port takes either of these:

- the English floppy release
- the European CD, with its English, German, French and Italian games

## Importing your copy

On the first start the port asks for your copy of the DOS game. Choose one
of these, or drop it onto the window:

- the game's folder, the one that holds `GRAFIKA`, `GRWAR`, `PICS`, ...
- a ZIP of that folder
- the CD's ISO image

On Linux the file dialogs need xdg-desktop-portal or zenity; dropping the
file or folder onto the window works without them.

The port checks every file it needs. If any file is missing or different,
the port lists them and copies nothing. Otherwise it copies the game into
your player folder, and the game starts. Saves in the DOS game's `SAVE`
folder come along into slots that are still empty.

To import from the command line, without a window:

```sh
reunion --import=PATH
```

To import another copy later, use the port menu (System, Import game
data).

## Playing

The port plays the credits and the intro, then the game, as the DOS game
did. Everything else is as in the original, apart from the fixes and
improvements below.

| Key | |
|---|---|
| F1 | the port menu |
| Alt+Enter | full screen on or off |
| F11 | save to the autosave (Autosave improvement) |
| F12 | a screenshot, to `screenshots/` |

Everything the port keeps is in your player folder:

- Linux: `~/.local/share/Reunion/Reunion`
- Windows: `%APPDATA%\Reunion\Reunion`
- macOS: `~/Library/Application Support/Reunion/Reunion`

It holds `reunion.ini` (the settings), `save/` (the saves),
`screenshots/` and `data/` (your imported copy).

The DOS game's saves load in the port, and the port's saves load in the
DOS game, unless an improvement needs more than the DOS format holds (see
Improvements).

[docs/walkthrough.md](docs/walkthrough.md) is a guide to the game itself.

### Languages

An import of the CD has its English, German, French and Italian games.
The port menu (Game, Language) picks one, from the next restart. The
port's own menus and messages are translated too. `--language=english`,
`german`, `french` or `italian` starts the port in that language instead.

### Cut-scenes

`reunion --cutscene=credits`, `intro` or `victory` plays one of the
cut-scenes and exits.

## The port menu

F1 opens the port menu over the game, and F1 or Escape closes it. Pick a
section on the left with the mouse, Tab or Page Up/Down; click a setting,
or use the arrow keys and Enter. The menu's help line says what the
highlighted setting does. Every change is saved at once.

- **Cheats:** freeze money (purchases cost nothing), freeze ore (New
  Earth's ore never falls), add credits, add ore.
- **Sound:** the music (None, Sound Blaster or UltraSound), sound effects,
  and both volumes. Sound Blaster plays the game's FM tunes and UltraSound
  its music modules; the cut-scenes sound the same on both.
- **Video:** a window, full screen (changes the display's mode) or
  desktop (full screen at the desktop's resolution), and the size.
- **Game:**
  - Skip intro: start at the title screen.
  - Colony stats: on the planet info screen of one of your colonies,
    click the POP row for its housing, food and health, the disease
    timer, how close the people are to demanding a Stadium or a
    University, and where their mood and numbers are heading.
  - Detoxin rate: the droids screen and a Derrick's info show how much
    Detoxin the body's Derricks produce (the original shows `--`).
  - Language: the CD's games (see Languages).
- **Improvements:** each improvement on or off (below).
- **Keys:** the keys of keyboard play.
- **Fixes:** each bug fix on or off (below). "All off (original)"
  switches off every fix and every improvement: the original game, bugs
  and all.
- **System:** import game data, restart (back to the title screen) and
  quit.

### Improvements

Changes to the game's design, all on unless you switch them off:

- **More Miner stations:** up to four Miner stations on a body, mining
  every hour and kept when a colony is founded there.
- **Repeat build:** on a planet's surface, Shift+click places a building
  and stays in build mode for another of the same kind; a plain click ends
  it.
- **Trade routes:** ROUTE on a trade group's control panel opens its
  route: up to six of your colonies or Miner-station bodies to load ore
  at, and New Earth to unload it. Once started, the group flies the loop on
  its own, docking 5 game hours at each stop and at home, until you end it
  or give the group another order.
- **Keyboard play:** every screen can be played with keys alone. Tab and
  Shift+Tab move the pointer from one clickable thing to the next, with a
  frame around it; Enter clicks there and Space right-clicks (held, the
  button is held). On a map or a list the arrow keys go one cell at a time
  (off the surface map's edge they scroll it); elsewhere they go to the
  nearest thing that way, and Shift+arrows move the pointer a little. 1-6
  press the toolbar's buttons and 7 turns its page, Escape leaves the
  screen, H held runs the clock, F2 opens the command centre and F3-F10
  its rooms. The mouse works alongside.
- **No building limit:** more than the original's 1,000 buildings across
  all colonies.
- **Autosave:** one save of its own, besides the 12 slots. It is written as
  a battle or an alien talk starts, holding the game as it was at the start
  of that hour, so loading it leads to the same battle or talk again; also
  once a week of game time, and when you press F11 (not in battles, talks
  and other screens where the clock stops). The disk screen lists it under
  the slots, to load only.
- **Storage full message:** when a body's store of an ore is full and its
  production stops, the message log says "Storage is full on" the body.
  It says so once: not again while that or another ore there stays full,
  but again for the next ore to fill after one has gone below its cap.
- **Universities count:** a university course needs a finished, powered
  University at one of your colonies (New Earth starts with one), and
  each University lets one more staff member be away at once, up to all
  four. Without one, the staff member says there is none to go to.

The DOS game can't load a save that holds what only the port has: a trade
route, more than 1,000 buildings or more than one staff member at the
university. While Trade routes or No building limit is on, or the game
holds any of those, the port saves in its own format (the port menu says
so), as `save/reunion.<n>` instead of the DOS game's `save/spidysav.<n>`.
The autosave is always in the port's format.

### Fixes

Bugs of the original, all fixed unless you switch the fix off:

- **Jade found in time:** Jade is found by 1 April 2928 at the latest, so
  the game can be won.
- **All systems mine:** every system's colonies and droids mine, not only
  the selected system's.
- **Meteorite droids:** a meteorite takes a droid from the colony it hit.
- **Building on moons:** buildings on a moon are built at the moon's speed.
- **Derricks on moons:** a Derrick on a moon fills the moon's store, not
  the planet's.
- **Drifting ship:** the drifting Morgrul ship is found by a group arriving
  at Phoenix 1.
- **Disbanding groups:** disbanding a group no longer wipes or doubles
  other fleets.
- **Annihilation:** the annihilation first leaves screens showing what it
  destroys.
- **New colony buildings:** a new colony's buildings are kept when its
  surface is shown and full.
- **Staff at university:** a staff member at the university stays out of
  the command centre.
- **Loading cargo:** a ship takes on as much ore as it has room for,
  whatever the storage.
- **Miner stations:** bodies with only a Miner station keep mining after a
  conquest.
- **Garrison losses:** after a space battle each garrison loses the ships
  it really lost.
- **Colony set-up:** two new colonies get their starter buildings on time.
- **Demolishing mines:** a demolished mine's droid goes back into the
  colony's stock.
- **Building limit:** at 1,000 buildings a new one is refused before it is
  paid for.
- **Spy reports:** spy reports on armies, garrisons and weapons name the
  race.
- **League tip:** the bartender still sells the League tip after the
  League attacks.

## reunion.ini

The port menu writes `reunion.ini` in your player folder whenever a
setting changes, so you rarely need to edit it. To change it by hand,
quit the game first. A setting that's missing gets its default; a `;`
starts a comment.

| Section | Setting | Values |
|---|---|---|
| `[cheats]` | `freeze_money`, `freeze_ore` | `on`, `off` |
| `[sound]` | `music` | `none`, `soundblaster`, `gus` |
| | `effects` | `on`, `off` |
| | `music_volume`, `effects_volume` | 0-100 (percent) |
| `[video]` | `fullscreen` | `off` (a window), `on`, `desktop` |
| | `window_width`, `window_height` | the window's size (1280×960) |
| | `fullscreen_width`, `fullscreen_height` | the full-screen mode; 0 is the desktop's |
| | `fullscreen_refresh` | its refresh rate in Hz; 0 is any |
| `[game]` | `skip_intro`, `colony_stats`, `detoxin_rate` | `on`, `off` |
| | `language` | `english`, `german`, `french`, `italian` |
| `[improvements]` | `more_stations`, `repeat_build`, `trade_routes`, `keyboard_play`, `no_building_limit`, `autosave`, `storage_full_message`, `university_courses` | `on`, `off` |
| `[fixes]` | `find_jade`, `mine_all_systems`, `meteorite_droids`, `build_on_moons`, `derricks_on_moons`, `drifting_ship`, `disband_list_count`, `annihilate_leaves_screen`, `autoplace_when_full`, `away_staff_hidden`, `cargo_planet_cap`, `station_storage`, `garrison_losses`, `colony_setups`, `demolish_droids`, `building_limit`, `spy_reports`, `league_tip` | `on`, `off` |
| `[keys]` | see below | |

`[keys]` changes the keys of keyboard play: an action, then its keys by
SDL's key names, with `Shift+`, `Ctrl+` or `Alt+` in front and a comma
between two keys. The port menu (Keys) shows them as they are now.

```ini
[keys]
click = Return, Keypad Enter
clock = H
quick_save = F11
```

The actions are `next`, `previous`, `up`, `down`, `left`, `right`,
`nudge_up`, `nudge_down`, `nudge_left`, `nudge_right` (move the pointer a
little), `click`, `right_click`, `back`, `clock`, `toolbar_1` to
`toolbar_6`, `toolbar_page`, `command_centre`, `star_map`, `planet`,
`ship_info`, `space_local`, `research`, `commanders`, `info_buy`,
`messages` and `quick_save`.

## Building from source

You need a C23 compiler (GCC 14+ or Clang 18+), CMake 3.25+ and Ninja.
SDL3 is taken from the system when it is installed (`dnf install
SDL3-devel`); otherwise it is downloaded and built, as are libxmp, Nuked
OPL3 and miniz. On Linux, building SDL needs `libdecor-devel` for the
window's title bar under GNOME on Wayland; without it the window has none
and can't be moved. On Windows, build with clang-cl or MinGW, as MSVC's C23
support is incomplete.

```sh
cmake --preset release
cmake --build --preset release
build/release/reunion
```

For the other presets, the tests and how the port is put together, see
[docs/development.md](docs/development.md).
