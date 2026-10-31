# Dancing Stage Fusion (PS1, SLES-04163, Europe 2004) -- Behavior Reference

Key: dsf-game-research. Status: IN PROGRESS (resumed run 2026-10-16; earlier run left only raw dumps).
Confidence tags: **verified** = multiple independent sources agree, or directly observed on the user's disc
(disc observations are labelled "disc"); **sourced** = one cited source; **uncertain** = inference.
NOTE ON SOURCES: in this environment WebFetch/curl to remywiki.com, wikipedia.org, psxdatacenter.com,
gamefaqs, mobygames, fandom, zenius-i-vanisher, leadbman.com are all BLOCKED (DNS/403). Web facts below come
from WebSearch result snippets (search-engine summaries of those pages) -> treat single-snippet facts as
"sourced" at best; numbers quoted from RemyWiki table rows were copied from snippet text.
Disc-derived images/dumps live ONLY in /home/user/work/analysis/dsf-game-research/ (never in engine/deliverables).

## 0. Inputs reused from other agents / previous run (read-only)
- READ_DT.BIN parser lib: /home/user/work/analysis/disc-readdt/scripts/readdt_lib.py (+tim2png.py).
- EXE string scan: /home/user/work/analysis/disc-exe/strings_all.tsv; master song-name table
  /home/user/work/analysis/disc-exe/songtable.tsv (that agent's table starts at 0x800A56C4; the 20 DSF songs
  are in EARLIER records of the same 20-byte-stride table, 0x800A3EB4..0x800A4034, see sec. 3).
- Previous run: text leaves of READ_DT entries 21-25 -> analysis/dsf-game-research/disc/text_e02[1-5].txt
  (lesson-mode instruction strings; e021 = English; e022..e025 = the other 4 languages).
- DuckStation gamedb copy: analysis/dsf-game-research/gh/gamedb.yaml (from github.com/stenzek/duckstation).
- My scripts: analysis/dsf-game-research/scripts/{sheet.py (TIM -> PNG contact sheets), ptrs.py, dumptab.py,
  dump_text.py, dump_e021.py}. Rendered sheets: analysis/dsf-game-research/png/sheet_eNNN.png.

## 1. Identity and release facts
| Fact | Value | Confidence / source |
|---|---|---|
| Title / serial | Dancing Stage Fusion, SLES-04163, PAL Europe, 1 disc | verified (disc SYSTEM.CNF BOOT=cdrom:\SLES_041.63;1; Redump DAT; DuckStation gamedb) |
| Release date | 5 Nov 2004 (Europe), same day as the PS2 version | verified (RemyWiki CS_DS_Fusion_PS1, Wikipedia "Dancing Stage Fusion", DuckStation gamedb releaseDate 2004-11-05, GameFAQs PS data page via search) |
| Developer | Konami Computer Entertainment Tokyo (KCET); publisher Konami | verified: title-screen TIM on disc reads "(c)1998 2004 KONAMI & Konami Computer Entertainment Tokyo"; boot logos KONAMI / KONAMI COMPUTER ENTERTAINMENT TOKYO / KONAMI SPORTS (READ_DT e026, e221); DuckStation gamedb developer "KCET"; PCSX2 wiki/PSXDataCenter per search snippet. (Wikipedia infobox says "Konami/Bemani".) |
| Historical note | Last BEMANI title released on PS1 in any region; final Dancing Stage on PS1 | sourced (RemyWiki CS_DS_Fusion_PS1) |
| Languages | English, French, German, Italian, Spanish (5). | verified (Redump name "(En,Fr,De,Es,It)"; DuckStation gamedb; disc: 5 copies of every text/graphic pack, e.g. READ_DT e021..e025, options text e027/0..4) |
| How language is chosen | In-game: OPTIONS > LANGUAGE OPTION ("Select language. / Régler la langue. / Wähle Sprache. / Scegli il linguaggio. / Seleccionar el idioma."). Language names stored in EXE in all 5 languages (ENGLISH/ANGLAIS/ENGLISCH/INGLESE/INGLÉS ...) plus unused Japanese labels ("NIHON-GO", "EI-GO"...). Whether a language prompt also appears on first boot: uncertain (not observed). | verified (disc text e027/0; EXE strings 0x8001E390..) / uncertain (boot prompt) |
| Video mode | PAL. Startup code calls PsyQ SetVideoMode(1 = MODE_PAL) exactly once (call site 0x800261D4, SetVideoMode @0x80038B68, only caller). No 60 Hz / NTSC strings or option texts found on disc. => no 60 Hz option. | verified (disasm) for PAL init; "no 60 Hz option" = verified-by-absence in options text e027 (screen adjustment + FLICKER FREE only), still uncertain until observed |
| Players | 1-2 (2P versus/simultaneous via two controllers; DOUBLE MODE = one player on two mats/pads) | verified (disc: 1P/2P graphics, "CONTROLLER 1/2", DOUBLE PLAY settings; DuckStation gamedb maxPlayers 2) |
| Memory card | 1 block for "system data"; memory card slot 1 only; AUTO SAVE is OFF by default (prompt "Do you wish to AUTO SAVE?") | verified (EXE strings: "1 empty block needed to save system data.", "The game does not AUTO SAVE at default.", "MEMORY CARD is not inserted in MEMORY CARD slot 1."; DuckStation gamedb min/maxBlocks 1) |
| Controllers | Digital pad, DualShock/Analog (analog sticks unused), Dance Mat. Options contain "DANCE PLAY SETTINGS" (accept input from non-directional buttons per 1P/2P), "DOUBLE MODE SETTINGS" (L1/L2/R1/R2 mapping during DOUBLE PLAY, 1P/2P button settings). Controller-config screen shows a DualShock (ANALOG) image. | verified (disc text e027, graphic e027/20); DuckStation gamedb: Analog+Digital, trait DisableAutoAnalogMode "Analog sticks do nothing" |
| Vibration | OPTIONS > CONTROLLER SETTINGS > VIBRATION: NONE / BUTTON / MISS, plus a DOUBLE PLAY vibration setting. EXE links LIBPAD (PadInfoMode/PadSetAct/PadSetActAlign called from 0x8002FC8C..0x8002FD70). Actual rumble behaviour not observed. NOTE DuckStation gamedb says vibration:false -> contradicted by disc text. | verified (option text + code present) / uncertain (runtime behaviour) |
| Multitap | No evidence of multitap use; PadChkMtap is linked but has no direct caller. | uncertain (DuckStation gamedb: multitap false) |
| Link cable | No | sourced (DuckStation gamedb) |
| PEGI | 3 | sourced (gamescrobbler via search) - low confidence |

## 2. Song list (20 songs, all available from the start, no unlocks)
Count 20 = verified (RemyWiki "Total songs: 20", Wikipedia "20 of the PS2 version's 54 songs", PSXDataCenter
via snippet; disc: 20-record song table @0x800AFE38, 20 banners/text plates, 20 song groups READ_DT e068..e167).
Order below = order of the EXE song table @0x800AFE38 (field1 = 1..20) = order of banners in READ_DT e181/e201
and title plates in e201/e206 = Konami originals A-Z by title, then licensed songs A-Z. That this is the on-screen
song-wheel order is **uncertain** (very likely; confirm from video/emulator).
Ratings: SP = Beginner/Standard/Difficult/Expert, DP = Standard/Difficult/Expert ("-" = no chart).
Ratings source: **disc** (EXE song table @0x800AFE38, u32 at +0x10 = SP, +0x14 = DP, one nibble per chart:
bits0-3 Standard, 4-7 Difficult, 8-11 Expert, 12-15 Beginner) cross-checked against RemyWiki CS_DS_Fusion_PS1
table rows quoted in search snippets. Where both exist they agree, except Mickey SP-Difficult (disc 6, RemyWiki
snippet 5). BPM = RemyWiki (snippets); BPM is NOT in this EXE table (must come from chart data -> disc-readdt).
Title/artist spelling = exactly as on the disc's text plates (READ_DT e201/e206).

| # | id | Title (disc) | Artist (disc) | BPM (RemyWiki) | SP B/S/D/E (disc) | DP S/D/E (disc) | Origin / notes ("new" = new to the console series per RemyWiki 'New CS song') |
|---|---|---|---|---|---|---|---|
| 1 | 0x01D7 | 321STARS | DJ SIMON | 192 | 2/4/6/8 | 4/7/8 | Konami original |
| 2 | 0x701C | A Stupid Barber | Sho-T | 132 | 1/4/6/7 | 3/7/8 | Konami original; first playable in DDR EXTREME CS (Japan) per RemyWiki A_Stupid_Barber (sourced) |
| 3 | 0x0019 | AM-3P | KTz | 130 | 1/5/6/8 | 5/6/7 | Konami original |
| 4 | 0x010E | CAN'T STOP FALLIN' IN LOVE (SPEED MIX) | NAOKI | 170 | 1/4/6/9 | 4/6/9 | Konami original |
| 5 | 0x0109 | DIVE | Be For U (banner: BeForU) | 155 | 1/4/5/8 | 4/6/7 | Konami original |
| 6 | 0x012B | Healing Vision (Angelic mix) | 2MB | 46-196 | 2/5/7/9 | 5/7/9 | Konami original; BPM changes (stops uncertain) |
| 7 | 0x0173 | KAKUMEI | dj TAKA with NAOKI | 83-148 | 1/4/6/9 | 3/5/9 | Konami original; BPM changes. DP word has an extra nibble=1 in the Beginner slot (0x1953) - meaning unknown (uncertain) |
| 8 | 0x002E | KEEP ON MOVIN' | N.M.R | 132 | 1/4/5/6 | 4/6/7 | Konami original |
| 9 | 0x000E | MAKE IT BETTER | mitsu-O! | 119 | 1/4/5/7 | 5/7/7 | Konami original |
| 10 | 0x00D5 | ORION.78 (civilization mix) | 2MB | 200 | 2/6/8/9 | 6/8/9 | Konami original |
| 11 | 0x001D | PARANOiA MAX (DIRTY MIX) | 190 | 190 | 2/6/8/8 | 7/8/9 | Konami original |
| 12 | 0x00EA | STILL IN MY HEART | NAOKI | 150 | 1/4/6/7 | 4/5/7 | Konami original |
| 13 | 0x6001 | Try 2 Luv. U | S.F.M.P. | 115 (RemyWiki song page/PS2 page; PS1 row not retrieved) | 1/3/4/5 | 2/4/6 | Konami original |
| 14 | 0x802F | Come Into My World | Kylie Minogue | 125 | 1/3/6/7 | - | licensed, new |
| 15 | 0x700E | Freak Like Me | Sugababes | 92 | 1/2/3/4 | 2/4/4 | licensed, new |
| 16 | 0x8025 | Gotta Get Thru This | Daniel Beddingfield (sic; real name Bedingfield) | 134 | 1/4/5/7 | - | licensed, new |
| 17 | 0x8023 | I BELIEVE IN A THING CALLED LOVE | The Darkness | 68-128 | 1/4/6/8 | - | licensed, new; BPM change |
| 18 | 0x800F | Mickey | Toni Basil | 151 | 1/4/6/7 (RemyWiki: 1/4/5/7) | 3/7/8 | licensed, new |
| 19 | 0x8018 | Move Your Feet | Junior Senior | 118 | 1/3/5/7 | 3/5/7 | licensed; from DDR EXTREME (North America) per RemyWiki |
| 20 | 0x8015 | Sunlight | DJ SAMMY | 140 | 1/3/5/7 | 3/6/7 | licensed, new |

Notes:
- Licensed songs without Double charts: Come Into My World, Gotta Get Thru This, I BELIEVE IN A THING CALLED
  LOVE (verified: disc DP word = 0, RemyWiki "- - -").
- No CHALLENGE difficulty; no song rated 10 although the scale goes to 10 (RemyWiki); the song-select star
  sprite sheet (e201/4) has 4 colours of star (one per difficulty) + empty slot and digits 0-9.
- Freeze arrows: none (all charts that had freeze arrows lost them) - sourced RemyWiki + PSXDataCenter/Wikipedia
  snippets ("None of the songs contain Freeze Arrows") => verified (2 sources); disc chart decode pending (disc-readdt).
- Stops: supported by this engine (TCRF "Dancing Stage Fusion (PlayStation)": heavily modified DDR 4thMIX engine
  with stop support, one of only two PS1 DDR releases with stops, with 5thMIX). Which songs use stops: uncertain
  (likely Healing Vision Angelic mix, KAKUMEI - check charts).
- Other EXE table fields (@0x800AFE38, 44 B/rec): +8 = a permutation of 1..20 (roughly descending BPM:
  ORION=1, Healing=2, 321STARS=3, PARANOiA=4 ... Freak=20; maybe a sort key, uncertain); +0xC = values 28..49
  (unknown index); +0x20 low half = 35..80 (ORION 80, PARANOiA 77 ... Freak 35; maybe calorie/intensity
  coefficient, uncertain); +0x24 ptr to 24-byte per-song record (language group -> READ_DT entries).
- The EXE also contains a huge master song-name table (~300 titles: DDR 1st..5th/Extra/Konamix/Club/EuroMix/
  Party Edition/MegaMix/DS Fusion PS2-era titles, e.g. "Can Be Real", "Chihuahua", "PARANOiA BOSS") = shared
  Konami database leftovers; none of those are playable (only 20 song packs exist on disc).

### 2a. BPM / stops verified from the disc tempo maps
Tempo map = leaf e<NNN>/0/0 of each song group (layout decoded by the disc-readdt agent: (u32 pos in 1/4096
measure, u32 time in 1/75 s) pairs). BPM per segment = 4*dMeasure/(dTime/60), computed by me from
/home/user/work/analysis/disc-readdt/tempo.csv. All agree with RemyWiki display BPMs => **verified**, except:
- Healing Vision (Angelic mix): disc 49.0 BPM (intro m0-2 and outro m73.75-75.75) / 196 main, **STOP 0.31 s at
  measure 57.5**; RemyWiki snippet says "46-196" -> disc says 49-196 (49 = 196/4). Treat the low BPM as 49.
- 321STARS: 192 BPM with **STOP 0.47 s at measure 50.5**.
- KAKUMEI: 148 BPM with a gradual slow-down in measures 6.5-9.75 down to ~83 and back (no stop).
- I BELIEVE IN A THING CALLED LOVE: 128 BPM, final measures 49-53 slow to ~52-67 (ritardando ending).
- Mickey, Move Your Feet: live-played tempo drift (Mickey ~147-153, display 151; Move Your Feet ~112-119, display 118).
- A Stupid Barber: first two tempo-map measures at 116 BPM then 132 (lead-in; display 132).
- MAKE IT BETTER tempo map has bit31 flags on two time entries (0x80000000, 0x80000097) - meaning unknown.
Only two songs have stops (321STARS, Healing Vision) => matches TCRF "stop support" claim (verified).

### 2b. Chart inventory (disc, READ_DT first language copy of each song group; script scripts/chart_inventory.py)
Chart kind byte1: d4 = BEGINNER, d0 = STANDARD, d1 = DIFFICULT, d2 = EXPERT (inferred from step counts and from the
rating table; d3 [would be CHALLENGE] never occurs). Format "step-rows/jump-rows" (single: low nibble L/D/U/R,
high nibble always mirrors it; 0 exceptions):
```
I BELIEVE            SP B:94/9   S:154/17  D:188/43  E:312/41   (no DP)
Come Into My World   SP B:87/5   S:119/6   D:208/11  E:282/30   (no DP)
DIVE                 SP B:122/0  S:204/16  D:272/20  E:354/49   DP S:191 D:268 E:322
Freak Like Me        SP B:43/0   S:44/2    D:99/8    E:118/15   DP S:79  D:100 E:122
Gotta Get Thru This  SP B:91/2   S:125/22  D:196/26  E:301/40   (no DP)
Healing Vision       SP B:150/6  S:269/9   D:372/40  E:472/23   DP S:268 D:372 E:464
KAKUMEI              SP B:103/15 S:128/5   D:237/4   E:371/6    DP S:109 D:193 E:365   (no DP Beginner chart)
KEEP ON MOVIN'       SP B:51/15  S:110/17  D:140/16  E:183/12   DP S:113 D:162 E:164
CAN'T STOP (SPEED)   SP B:133/9  S:238/15  D:321/13  E:443/26   DP S:241 D:323 E:432
MAKE IT BETTER       SP B:67/8   S:143/40  D:188/24  E:212/46   DP S:130 D:181 E:220
Mickey               SP B:117/7  S:159/10  D:221/53  E:275/58   DP S:137 D:237 E:289
Move Your Feet       SP B:83/1   S:120/4   D:155/29  E:231/26   DP S:114 D:156 E:210
ORION.78 civ         SP B:69/1   S:242/51  D:348/88  E:439/81   DP S:242 D:312 E:424
PARANOiA MAX         SP B:142/8  S:268/37  D:288/49  E:340/56   DP S:274 D:311 E:363
AM-3P                SP B:74/3   S:164/25  D:195/25  E:244/29   DP S:166 D:194 E:240
STILL IN MY HEART    SP B:117/4  S:180/21  D:255/17  E:290/32   DP S:169 D:230 E:276
A Stupid Barber      SP B:108/9  S:152/9   D:215/30  E:271/14   DP S:126 D:225 E:271
Sunlight             SP B:91/1   S:119/1   D:193/15  E:274/14   DP S:104 D:183 E:244
321STARS             SP B:95/2   S:146/4   D:239/4   E:307/13   DP S:147 D:241 E:306
Try 2 Luv. U         SP B:66/4   S:73/4    D:125/14  E:150/13   DP S:88  D:127 E:169
```
Chart presence matches the rating table exactly (17 songs with DP, 3 single-only) -> verified. The chart
format has no hold/freeze encoding -> consistent with "no freeze arrows" (verified with RemyWiki/PSXDataCenter).
These step counts are useful as exactness checks (e.g. max combo shown on the results screen of a full combo
should equal step-rows if a jump counts as one combo step - the PS1 4thMix-engine rule is uncertain).
