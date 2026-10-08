# Original input handling

How the DOS game reads the mouse and keyboard, from the disassembly. The port's input layer emulates this.

```
---- Crt (4857). Ghidra split only part of this unit; the extra entry points below are real routine starts.
---- Dos (48b9)
---- System (48c7)
---- MinMax (484a)
---- EMS (4719). TEmsBlock layout: +0 state (0 none,1 heap copy,2 in EMS), +1 ptr (heap data), +5 size:word, +7 handle:word, +9 pageFrameSeg:word, +B pageCount:word
---- 474f: Sound Blaster CT-VOICE driver thunks (far call through DS:8A60, BX = function number)
---- Mouse (4758)
---- SaveLoad (1a3e)
---- notes for the input layer
DS:918E-9190 and cursor position are written ONLY by MouseCallback (4758:0000), registered via INT 33h AX=0Ch, CX=7Fh (move + all button press/release), ES:DX=4758:0000.
The driver calls it with AX=cond mask, BX=button state (bit0 left, bit1 right), CX=x, DX=y; the handler pushes everything and sets DS itself.
Callback body: 918A:=CX (Mouse::X, 0..318), 918C:=DX (Mouse::Y, 0..198) [skipped if 9182=2, i.e. cursor busy];
  918E := (BX and 3) <> 0   (any button down)   -> Mouse::BtnDown
  918F := (BX and 1) <> 0   (left)   -> BtnRight? name; per symbols.txt this is the byte copied to 8A7A; semantically LEFT button
  9190 := (BX and 2) <> 0   (right)  -> copied to 8A7B
  if no button down: 8A84 := 0
  HitTestCursor(var 9192); if 9192>=20: 9198 := byte at Hotspots[9192-20] offset 1Ah (hotspot's cursor shape), else 9198 := 0
  then software-cursor maintenance: if cursor is not inside the blit exclusion rect (DS:9270 vs Y+11) and 9182=0, hide cursor via CursorRestore (0294) when 919E=0; show again via 0231,01AA when 919E=1;
  if X/Y changed vs 9186/9188 (last drawn pos) and not busy: CursorRestore, CursorSave, CursorDraw. 9182 is a busy flag (0 idle, 2 inside hook).
Mouse::Poll (4758:042D) is the Pascal-side consumer: snapshots 918E->Buttons, X/Y->ClickX/Y, 918F/9190->8A7A/8A7B while pressed.
Poll also drains the keyboard: if Crt KeyPressed then ReadKey (and a second ReadKey for extended keys), discarding keys.
DS:91E7 (Cheat::Requested): NO writer exists anywhere in the executable. Searched all instructions for ds:91E7 (only 'mov byte,0' at 1000:04C6 and 26EB:0004, reads at 1000:0A19/066F), indexed addressing,
  every INT 21h vector set (only 1Bh Crt break, 1Ch AdLib timer, 23h/24h System, INT 8 GUS/PIT in 43d5), and port 60h reads: there is no INT 9 keyboard hook, no keyboard code touches it.
  So the cheat (26EB Cheat::Apply: +10000 credits, or the big cheat once via 91E6) is unreachable in the shipped game; the port can ignore it (or bind it to a debug key).
The only ISRs: Crt INT 1Bh (4857:0138), mouse callback (4758:0000), 1Ch timer in 428e, GUS/PIT timer in 43d5 (not input).
```
