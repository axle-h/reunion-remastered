"""Drives DOSBox-X's debugger (the curses one, needs a terminal) in a tmux
session, headless, for side_by_side.py.

Commands are typed into the debugger with tmux send-keys and its answers read
back from the pane and the log file. The game's addresses below are Ghidra's
(file segment + 1000h, as the port cites them); seg() moves them to where DOS
loaded the program.
"""
import os
import re
import subprocess
import time

SESSION = "reunion-dbx"

CONF = """\
[sdl]
output=surface
autolock=false
[dosbox]
machine=svga_s3
memsize=16
[cpu]
core=normal
cycles=fixed 30000
[log]
logfile={log}
debuggerrun=debugger
[sblaster]
sbtype=none
oplmode=none
[gus]
gus=false
[speaker]
pcspeaker=false
tandy=off
disney=false
[mixer]
nosound=true
[midi]
mpu401=none
mididevice=none
[dos]
xms=true
ems=true
[autoexec]
mount c "{drive}"
c:
cd REUNION
DEBUGBOX GRWAR\\REUNION.EXE sxca
"""
# No sound card: the game is set to none (SAVE\SETUP.SPD, side_by_side.py),
# as it waits for Sound Blaster playback that DOSBox-X never finishes
# headless. EMS on: without it the game stops with "Not enough memory".

# The segment REUNION.EXE's image was loaded at (its entry is LOAD:01A1);
# boot() reads it, as it moves with the memory DOSBox-X's drivers take.
LOAD = 0
DS = 0
WORK = ""


def seg(ghidra):
    return ghidra - 0x1000 + LOAD


RANDOMIZE = 0x48C7, 0x1948  # System.Randomize
MOUSE_CALLBACK = 0x4758, 0x0000  # the INT 33h event handler
RAND_SEED = 0x75DA  # DS:RandSeed
FRAME_END = 0x0068F  # the main loop's retrace wait ends (load-image offset)

STUB_SEG, STUB_OFS = 0x0000, 0x0520  # free bytes below DOS for injected code


def tmux(*args):
    return subprocess.run(["tmux", *args], check=True, capture_output=True, text=True).stdout


def screen():
    return tmux("capture-pane", "-t", SESSION, "-p")


def key(k):
    tmux("send-keys", "-t", SESSION, k)


def cmd(s):
    # Backspaces clear the input line: Escape would act as Alt on the next key.
    tmux("send-keys", "-t", SESSION, *["BSpace"] * 60)
    tmux("send-keys", "-t", SESSION, "-l", s)
    tmux("send-keys", "-t", SESSION, "Enter")


REG = re.compile(r"\b(E?[A-Z]{2}|[CDEFGS]S)=([0-9A-F]+)")


def regs():
    """The register overview, once it is drawn whole."""
    for _ in range(200):
        r = {}
        for line in screen().splitlines()[1:6]:
            for name, val in REG.findall(line):
                r[name] = int(val, 16)
        if all(k in r for k in ("CS", "EIP", "SS", "ESP", "DS", "EAX", "EDX")):
            return r
        time.sleep(0.01)
    raise RuntimeError("no register overview")


def at():
    r = regs()
    return r["CS"], r["EIP"]


_token = [0x1000]


def sync(timeout=30.0):
    """Waits until the debugger is stopped and has taken every command so
    far: a numbered EV echoes only once the debugger reads its input line.
    Keys typed while the CPU runs are lost, so the EV is sent again until
    it echoes."""
    _token[0] += 1
    t = f"{_token[0]:X}"
    end = time.time() + timeout
    while time.time() < end:
        cmd(f"EV {t}")
        for _ in range(30):
            if f"EV of '{t}' is:" in screen():
                return True
            time.sleep(0.01)
    return False


def resume(send, timeout=60.0):
    """Sends what makes the CPU run, then waits until it has stopped again:
    the terminal hands the debugger its keys in order, so an EV typed after
    the run echoes only once the debugger is back."""
    sync()
    send()
    return sync(timeout)


def vrt():
    """Runs to the next vertical retrace."""
    if not resume(lambda: cmd("VRT")):
        raise RuntimeError("VRT did not stop")


def run_until(cs, ip, timeout=60.0):
    """F5, then True once the debugger stops at CS:IP (a breakpoint)."""
    if not resume(lambda: key("F5"), timeout):
        return False
    s, o = at()
    return (s << 4) + o == (cs << 4) + ip


def dump(s, o, n):
    """n bytes at s:o (MEMDUMPBIN writes MEMDUMP.BIN in the working
    directory; its offset wraps at 64K and its segment is 16-bit)."""
    path = os.path.join(WORK, "MEMDUMP.BIN")
    if os.path.exists(path):
        os.remove(path)
    cmd(f"MEMDUMPBIN {s:X}:{o:X} {n:X}")
    end = time.time() + 10
    while time.time() < end:
        if os.path.exists(path) and os.path.getsize(path) == n:
            time.sleep(0.02)
            with open(path, "rb") as f:
                return f.read()
        time.sleep(0.01)
    raise RuntimeError(f"MEMDUMPBIN {s:X}:{o:X} {n:X} failed")


def setmem(s, o, data):
    for i in range(0, len(data), 16):
        chunk = " ".join(f"{b:02X}" for b in data[i:i + 16])
        cmd(f"SM {s:X}:{o + i:X} {chunk}")
    sync()


def bp(s, o):
    cmd(f"BP {s:X}:{o:X}")
    sync()


def bpdel():
    cmd("BPDEL *")
    sync()


def u16(b, i=0):
    return b[i] | b[i + 1] << 8


def run_to_return():
    """At a far procedure's entry: runs until it returns to its caller."""
    r = regs()
    ret = dump(r["SS"], r["ESP"] & 0xFFFF, 4)
    ip, cs = u16(ret, 0), u16(ret, 2)
    bpdel()
    bp(cs, ip)
    ok = run_until(cs, ip)
    bpdel()
    return ok


IFLAG = re.compile(r"\bI([01])\b")


def interrupts_on():
    for line in screen().splitlines()[1:6]:
        m = IFLAG.search(line)
        if m:
            return m.group(1) == "1"
    raise RuntimeError("no flags")


def mouse(x, y, buttons):
    """Calls the game's INT 33h handler as the driver would, from where the
    CPU stopped: a stub that keeps the flags and registers, then returns."""
    cs, ip = seg(MOUSE_CALLBACK[0]), MOUSE_CALLBACK[1]
    # The handler scrambles BP, SI, DI and ES on its way out (pop ax; popa;
    # retf): the driver saved them, so the stub saves everything.
    stub = bytes([0x9C, 0x50, 0x53, 0x51, 0x52, 0x56, 0x57, 0x55, 0x1E, 0x06,
                  0xB8, 0x1F, 0x00,
                  0xBB, buttons & 0xFF, buttons >> 8,
                  0xB9, x & 0xFF, x >> 8,
                  0xBA, y & 0xFF, y >> 8,
                  0x9A, ip & 0xFF, ip >> 8, cs & 0xFF, cs >> 8,
                  0x07, 0x1F, 0x5D, 0x5F, 0x5E, 0x5A, 0x59, 0x5B, 0x58, 0x9D, 0xCB])
    sync()
    # The driver calls back only while interrupts are on; with them off the
    # game may be using SP for data (blitters). Waits for a retrace where
    # they are on.
    for _ in range(200):
        if interrupts_on():
            break
        vrt()
    else:
        raise RuntimeError("interrupts stay off")
    setmem(STUB_SEG, STUB_OFS, stub)
    r = regs()
    sp = (r["ESP"] - 4) & 0xFFFF
    setmem(r["SS"], sp, bytes([r["EIP"] & 0xFF, r["EIP"] >> 8 & 0xFF, r["CS"] & 0xFF, r["CS"] >> 8]))
    cmd(f"SR SP {sp:X} CS {STUB_SEG:X} IP {STUB_OFS:X}")
    sync()
    for _ in range(100):
        r2 = regs()
        if r2["CS"] == STUB_SEG and r2["EIP"] == STUB_OFS and r2["ESP"] & 0xFFFF == sp:
            return
        time.sleep(0.01)
    raise RuntimeError(f"mouse injection failed: {r2}")


def ems_map():
    """The page frame's mapping from DOS EMS: {physical page: (handle, logical page)}."""
    log = os.path.join(WORK, "dosbox.log")
    n = os.path.getsize(log)
    sync()
    cmd("DOS EMS")
    sync()
    with open(log, "rb") as f:
        f.seek(n)
        text = f.read().decode("latin-1")
    m = {}
    for h, p, lp in re.findall(r"^\s+(\d+)\s+(\d+)/\s*(\d+) 000e", text, re.M):
        m[int(p)] = (int(h), int(lp))
    return m


def ems_dump(views):
    """Maps each view ([(handle, logical page), ...] to physical pages 0..)
    into the page frame in turn with INT 67h and dumps the frame, then
    restores the game's mapping and returns to where the CPU stopped.
    EMS memory lies above 1 MB, out of MEMDUMPBIN's reach."""
    old = ems_map()
    r = regs()
    code, stops = [0x9C, 0x50, 0x53, 0x52], []

    def mapping(pages):
        for phys, (h, lp) in pages:
            code.extend([0xB8, phys, 0x44, 0xBB, lp & 0xFF, lp >> 8, 0xBA, h & 0xFF, h >> 8, 0xCD, 0x67])
    for v in views:
        mapping(list(enumerate(v)))
        stops.append(STUB_OFS + len(code))
        code.append(0x90)
    mapping([(p, old[p]) for p in range(4) if p in old])
    code.extend([0x5A, 0x5B, 0x58, 0x9D, 0xEA, r["EIP"] & 0xFF, r["EIP"] >> 8, r["CS"] & 0xFF, r["CS"] >> 8])
    if len(code) > 0xE0:
        raise RuntimeError("EMS stub too long")
    setmem(STUB_SEG, STUB_OFS, bytes(code))
    cmd(f"SR CS {STUB_SEG:X} IP {STUB_OFS:X}")
    sync()
    out = []
    for s in stops:
        bpdel()
        bp(STUB_SEG, s)
        if not run_until(STUB_SEG, s):
            raise RuntimeError("EMS stub did not stop")
        out.append(dump(0xE000, 0, 0x10000))
    bpdel()
    bp(r["CS"], r["EIP"])
    if not run_until(r["CS"], r["EIP"]):
        raise RuntimeError("EMS stub did not return")
    bpdel()
    if ems_map() != old:
        raise RuntimeError("EMS mapping not restored")
    return out


def stop():
    subprocess.run(["tmux", "kill-session", "-t", SESSION], capture_output=True)


def boot(work, drive, seed):
    """Starts DOSBox-X on drive (holding REUNION\\), runs to REUNION.EXE's
    entry (DEBUGBOX) and seeds RandSeed as the port's seed:N does, right
    after Randomize. work holds the configuration, the log and the dumps."""
    global WORK, LOAD, DS
    WORK = work
    log = os.path.join(work, "dosbox.log")
    conf = os.path.join(work, "dosbox.conf")
    with open(conf, "w") as f:
        f.write(CONF.format(log=log, drive=drive))
    stop()
    tmux("new-session", "-d", "-s", SESSION, "-x", "200", "-y", "120", "-c", work,
         f"SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy dosbox-x -conf '{conf}' "
         "-fastlaunch -nomenu -break-start; sleep 600")
    end = time.time() + 30
    while "TYPE HELP" not in screen():
        if time.time() > end:
            raise RuntimeError("debugger did not start")
        time.sleep(0.2)
    resume(lambda: key("F5"))
    cs, ip = at()
    if ip != 0x01A1:
        raise RuntimeError(f"no DEBUGBOX entry: {cs:04X}:{ip:04X}")
    LOAD = cs
    DS = seg(0x4A81)
    bp(seg(RANDOMIZE[0]), RANDOMIZE[1])
    if not run_until(seg(RANDOMIZE[0]), RANDOMIZE[1]) or not run_to_return():
        raise RuntimeError("Randomize not reached")
    setmem(DS, RAND_SEED, seed.to_bytes(4, "little"))


def main_frame():
    return LOAD + (FRAME_END >> 4), FRAME_END & 15
