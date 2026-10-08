# Copy protection

How the original's copy-protection checks work, from the disassembly. The port removes them.

```
===== intel.txt: segments 29b9 (Intel), 2e4b (AlienTalk), 264a (Credits), 2693 (Protect2), 24eb (Protect) =====

===== COPY PROTECTION: how it works (the port should delete all of this) =====
There are two independent, obfuscated "challenge" schemes. Both use the same state machine:
  a "question index", an "answer key" (hash of what the user typed + a constant), a "lives" counter,
  and a table of expected answer keys. Wrong/unanswered => lives drops / memory is scribbled / game Halts.
All variables live in unused data slots: Key1/Key2 sit in the unused Races[1] record (4a81:6bba..6bc5);
PageIndex/PageCodes sit in the unused BuildingTypes[0] slot (4a81:6553..6591, Ghidra shows them as
"PageIndex+k*3Fh" when it really means BuildingTypes[k]); the date scheme uses 4a81:0856..0890.
Many compares use obfuscated arithmetic (x*7 == const, x+carry...); decoded values are given below.

--- Scheme A: "word at the bottom of page N" manual lookup (unit 24eb + 264a:034a + 2e4b:09d3 + 2841:0893) ---
 State (initial values read from the EXE data image):
   PageIndex 4a81:6564 longint = 62ECh  (62ECh = "not armed"; valid values 1..20 = manual page). NOTHING in the
      program ever stores another value here, so the whole scheme is inert in the shipped EXE.
   PageCodes 4a81:6566+2*PageIndex  word table, 20 entries (23A3 2BE1 2617 23A3 2C1C 29FE 2B9B 208D ...): expected
      Key2 for that page = hash(word)+1F5Fh.
   Key2 4a81:6bbe longint = 1F5Fh initial (= "hash 0" = unanswered). Key2 = hash(typed word) + 1F5Fh.
   Key1 4a81:6bba longint = 94h initial = "lives"; decremented once per prompt; at 91h the game Halts.
 24eb:00a5 Protect::CheckToken (called from Sim::AdvanceHour every game hour, result ignored):
   q = 505FCh div 13 (=25245). If PageIndex == q -> return. If (Key2-1F5Fh+0D80h) has a non-zero high word -> return.
   Otherwise it would fall into the prompt body at 24eb:0108, BUT in this EXE bytes 0100..0104 are NOPs followed by
   JMP to the epilogue, so the prompt body (0108..03B5) is DEAD CODE. CheckToken is effectively a no-op.
   The (dead) prompt: builds "| Security check | What is the word at the bottom of page <PageIndex> | Please type password
   &xxxxxxxx |" and calls the input dialog 34b0:0237 (title picture 'attentio', max 9 chars). The answer is trimmed,
   upper-cased and hashed: h=DS:759Ah; for i=1..len: h = ((i*2FFh)*ord(c) + h) mod 0D8Ah + 1 (empty answer: h=Random(50000)).
   Key2 = h+1F5Fh (also junk writes to 6bc2, 6514, 6524, 64ec). If the answer == 'JANI' (the author's name; a trap for
   people who find the string) it shows "| Warning !!! | Lamer detected |". Then if Key2 > 1F5Fh: Key1 := Key1-1.
 264a:034a (called from Sim hourly proc 1b8a:04d8) and 2e4b:09d3 (called from the Main loop) are the verifiers; both are
   skipped entirely while PageIndex == 62ECh. If Key2 > 1F5Fh (answered): Key2 == PageCodes[PageIndex] => correct:
   Key1 := Random(7)+479h (~1150 lives), and 2e4b also clears a flag byte.  Wrong: 264a only acts when Key1 == 91h
   (corrupts 18 Galaxy name-table entries with Random(10)+5); 2e4b resets the hash and Key2 := 1F5Fh (re-ask).
   264a:034a also: if Key2 == 1F5Fh (never answered) it writes 20 random bytes to random DS offsets (Random(FFFFh)) once
   (flag byte PageCodes[20]) -> random memory corruption/crash.
 2841:0893 (not in my range): if Key1 == 91h -> FUN_405f:1933 = fade palette to black, restore text mode, System.Halt.
 FAILURE RESULT: garbled galaxy data, random memory scribbles, finally silent Halt (exit to DOS after fade-out).
 PORT: delete CheckToken, 264a:034a, 2e4b:09d3, the calls from Sim::AdvanceHour/1b8a:04d8/Main, and Key1/Key2/PageIndex.

--- Scheme B: "Please type the date (xxx)" using TEXT\VALAMI.TXT (unit 2693; this one IS armed in the shipped EXE) ---
 VALAMI.TXT = 14 encrypted lines (read with TextFile::ReadTextLine = decrypting reader), one clue/question per index.
 State (initial EXE values):  QuestionIndex 4a81:0876 longint = 8615h ("not armed"; 1..14 = armed), 
   AnswerKey 4a81:0882 longint = 3421h ("unanswered"; = hash+3421h after an answer), AnswerHash 4a81:087a (scratch),
   Lives 4a81:088A longint = 10Ah (halts at 107h), ScribbleDone 4a81:088E byte,
   WordCodes 4a81:085A word[1..14] (40B7 3F6E 3901 3B93 ...): expected AnswerKey for question 1..14 (Protect::WordCodes).
 Flow:
  1. 2693:0000 (called from 1b8a:2a0e when a Kind-5 group "found new planets"): if AnswerKey == 3421h then
     QuestionIndex := Random(14)+1 (the store is disguised: es:[0756h] with es=ds+12h).  This ARMS the scheme.
  2. 2693:0085 (called from 1b8a:2178 when a building Health==0): if armed and AnswerKey == 3421h, shows
     "| Security check | <line QuestionIndex of valami.txt> | Please type the date &xxx |" via 34b0:0237, trims, upper-cases,
     hashes: h=AnswerHash; for i=1..len: h = ((i*149h)*ord(c)+h) mod 1104h + 0Dh; (empty answer: Random(D568h)).
     AnswerKey := h+3421h; then if AnswerKey > 3421h: Lives := Lives-1. (a debug/StrLong echo goes to 405f:11e8.)
  3. 2693:04AA (end of 1b8a:2837 daily step): if armed and AnswerKey*5 > 104A5h (answered): if AnswerKey == WordCodes[QuestionIndex]
     => correct: Lives := Random(7)+479h, clear BuildingTypes[21]._12[1] flag; else wrong: AnswerHash:=0, AnswerKey:=3421h (re-ask next time).
  4. 2693:0342 (start of 1b8a:25b5 colony step): if armed: when AnswerKey > 3421h and equals WordCodes[QuestionIndex] =>
     Lives := Random(7)+0D7Dh. Wrong: if Lives == 107h corrupt 18 Galaxy.OrionisNames entries (Random(10)+19h) and show the
     "!!!!!!!!!" box (2693:0563). If AnswerKey is still 3421h (armed but never answered) it writes 10 random bytes to
     random DS offsets once (ScribbleDone) and shows the box.
  5. 2693:0538 (end of 1b8a:006e hourly proc): if Lives == 107h -> FUN_405f:1933 (fade out + System.Halt).
 FAILURE RESULT: after ~3 prompts that are answered wrongly (Lives 10Ah -> 107h): corrupted names, "!!!!!!!!!" box, then Halt.
 A correct answer resets Lives to ~3450 so it effectively never fires again.
 PORT: remove calls 2693:0000/0085/0342/04AA/0538, the valami.txt dependency and all globals 4a81:0876..0890.

--- Races/force data layout used below (TRace at 4a81:6A06+race*E4h; forces at +0Ch+k*1Bh per types.pas TRaceForce) ---

===== Credits (264a) =====
===== Protect2 (2693) =====
===== Protect (24eb) =====
24eb contains only Protect::CheckToken (already named, spans 24eb:00a5..03b5) plus its string constants at 24eb:0000..00a4; no unnamed functions.
===== AlienTalk (2e4b) =====
===== Intel (29b9): alien races AI / story events / intel reports =====
```
