# Original sound drivers and timer

How the DOS game drives sound and the PIT, from the disassembly (segments `424a`, `4265`, `428e`, `431a`, `43d5`, `46ca`, `474f`). The port replaces all of this (see docs/audio.md); the "fragile" list is the main suspect for the crashes on newer systems.

```
Device type: byte 0 of save\setup.spd -> 4a81:89cf (also 4a81:8a15 in SoundDrv::Init). 1 = Gravis Ultrasound (GUS),
  2/3/6 = Sound Blaster family (digital fx via Creative CT-VOICE-style driver + AdLib music), 4 = AdLib music only, other = none.
  Sound::Init (431a:09c1) reads setup.spd (type byte + 3 words at 89d0/89d2/89d4 = SB port/irq/dma? presumably), sets
  89ce (effects usable) for types 1,2,3,6. For SB it loads grwar\alfa1.pic (un-obfuscated, <=6000 bytes: this IS the CT-VOICE driver
  image, far entry stored at DS:8A60) via 431a:000d and prints "Sound Blaster detected"/"not detected". The driver is called via
  474f:xxxx thunks (BX=function no: 1 set port, 2 set IRQ, 3 init, 4 speaker, 5 set status word addr(->89cc), 6 play VOC (ES:DI), 8 stop, 9 break loop).
Music backend selected in 424a:00b2: 4a81:89be = 0 none / 1 AdLib / 2 GUS MOD. Game::MusicTrack = (89be>0).
  Start a tune: 424a:000f PlayTrack(name) -> StopMusic then AdlibMusic::PlayMusic (grwar\<name>.pic) or SoundDrv::LoadMusic (anim\<name>.spd).
  Stop: 424a:0095 StopMusic -> 4265:022d (AdLib) or 43d5:020d (GUS). 424a:0137 picks the track from Game::MusicTrack (1 = 'main1'-style name at 424a:012B, 2 = ...:0131).
Effects: Sound::PlaySfx(name)/PlayWarSnd/PlayGrSnd LOAD the .SMP file (SB: to a heap buffer 89c6; GUS: uploaded into GF1 DRAM at 5cf8:5cfa via
  43d5:02ab, byte-wise, XOR 80h to convert unsigned->signed) then 431a:03a8 StartSample starts it (SB: driver fn 6; GUS: 43d5:0420 -> voice 5, 8-bit, no loop, pan 8, vol 0xEFF).
  SB stop = driver fn 8 (431a:059e). "IsPlaying" = word status 4a81:89cc != 0 (431a:0192).
---- PIT / TIMER ----
(a) GUS MOD player (43d5): StartPlayer 43d5:2a40 saves INT 08h vector, installs its own ISR at 43d5:1df7 and programs PIT ch0:
    out 43h,36h ; out 40h,A9h ; out 40h,04h  => divisor 0x04A9 = 1193 => ~1000.15 Hz (vs BIOS 18.2 Hz). 43d5:1d3f StopPlayer
    writes divisor 0 (=65536, 18.2 Hz) and restores the INT 08h vector (saved at CS:1B9D/1B9F).
    ISR per 1 kHz tick: pusha, set DS=code seg, mini volume/ramp pass, sends EOI (out 20h,20h) ITSELF and does STI, bumps tick counter (CS:1BAD cycles 1..5
    for GF1 per-voice updates on 4 channels: volume smoothing + frequency/position regs), adds 0x0CCCCCCC to a 32-bit accumulator (CS:1BA5..1BAB);
    every carry = exactly 1/20 ticks = 50 Hz MOD sequencer tick (speed 6, tempo 125 style). It does NOT chain to the old INT 08h:
    BIOS tick count at 0040:006C, INT 1Ch and DOS clock freeze while GUS music plays! Any wait loop on the BIOS tick will hang. Port this as a 1 kHz timer thread or drop to 50 Hz.
(b) AdLib player (428e): does NOT reprogram the PIT. Hooks INT 1Ch (user tick, 18.2 Hz): handler 428e:0059 decrements row counter CS:873,
    reloading from speed CS:875 (default 2; module effect Fxx changes it) -> at 0 it runs 428e:008c (read next 9-channel row, 64 rows/pattern,
    pattern = 0x480 bytes, 18 bytes/row-channel group), 428e:04f3 (volume update) and 428e:054e (frequency/key-on update), then chains to the old INT 1Ch. So AdLib tempo is locked to 18.2 Hz BIOS ticks.
---- FRAGILE ON MODERN MACHINES / LIKELY CRASH CAUSES ----
1. GUS: detection (46ca:00e7) scans bases 200h..2A0h step 10h and fills port vars (8a3c mixer=base, 8a46=base+102h voice, 8a48=base+103h regsel, 8a4a=base+107h DRAM), BUT
   ULTRASND env var parsing is dead code (blocks removed), and the player code (ISR 43d5:1df7, 43d5:02ab GusUploadSample, 43d5:2b4e, 43d5:1d3f, 43d5:2c5d) hard-codes ports 0x220/0x322/0x323/0x324/0x325/0x327
   i.e. assumes GUS at base 220h. A GUS at 240h (DOSBox default) works only partially; port 220h clashes with Sound Blaster base 220h (writes of 1/3 to 220h hit SB/OPL).
2. SoundDrv::Init ignores a failed GUS detect: if 8a36=false, the port vars stay 0 and Gus* routines OUT to ports 0,1,2,... (DMA controller) and 20h (PIC). Stub sound or force 'no device' on failed detect.
3. GUS probe 46ca:0084 and delays: 43d5:2f50 = 80 IN's from GUS DRAM port as a short delay; 43d5:2b4e uses 6 IN's of port 220h as a delay after GF1 reset. Pure I/O timing, harmless in an emulator but they spin forever when ports float (0FFh) on real HW.
4. AdLib detect (428e:065d) tries ports 218h,288h,318h,388h; for each it writes OPL timer regs and waits FOUR VGA vertical-retrace edges (poll port 3DAh bit 3, twice for each phase,
   ~4 frames per phase) -- infinite loop if 3DAh never toggles (mono 3BAh, headless/no VGA emulation, some DOS shims). Result port stored in CS:0823; OplWrite then uses 6 / 27 dummy INs as 3.3us / 23us delays.
5. Timer ISR hogs: 1 kHz INT 08h without chaining (see above); EOI sent manually. Hard-coded code segment constant (mov ax,33D5h) in the ISR is relocated by the EXE loader -- a rewrite must not copy the code.
6. Self-modifying style data in code segment (CS: variables 428e:0743..08B7, 43d5:053A etc.): any port that moves code needs those as globals.
7. Sound Blaster driver is a raw 16-bit x86 blob (grwar\alfa1.pic, called via far pointer at DS:8A60) -- cannot be loaded in a port; replace 474f thunks with a native SB/OpenAL layer.
8. VOC/SMP: .SMP time constant TC = byte at offset 4 of the loaded buffer; GUS rate = 1000000/(256-TC) Hz (stored 89f2; default 0xE210 before first upload).
9. 431a:0209 LoadSample aborts via FUN_431a_01a8 when MaxAvail < 64K; 431a:03e8 AllocWarSndBuf busy-waits on 89cc (SB status word) until playback ends -> infinite loop if driver never clears it.

```
