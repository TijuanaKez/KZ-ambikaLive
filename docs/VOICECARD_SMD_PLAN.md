# Voice card SMD rebuild — plan

Drafted September 15, 2026. This is the hardware track that runs alongside
`VOICECARD_V2_PLAN.md` (the firmware track). It is deliberately staged so that
each board can be built and tested against the *current* firmware before the
next stage changes anything else.

Every part value and pin below was read from the imported KiCad files, not
from memory.

---

## 0. Where things are

```
voicecard/hardware_design/pcb/
├── Voicecard-4P-v01.sch / .brd     original Eagle 5 binaries (untouched)
├── eagle-xml/                      Eagle 9.7.0 XML resaves of the same files
│   └── README.md                   provenance (MD5-verified against the binaries)
└── KiCad/                          KiCad 10 import — the BASELINE
    ├── Voicecard-4P-v01.kicad_pro / .kicad_sch / .kicad_pcb
    ├── Voicecard-4P-v01-eagle-import.kicad_sym   symbols pulled from the Eagle file
    └── Voicecard-4P-v01-import-fps.pretty/       footprints pulled from the Eagle file
```

The Eagle 5 binaries cannot be read by KiCad; the XML pair came from
rhinocerose/synth-mutable-ambika, whose binaries are byte-identical to ours.

### Baseline verification (done, September 15, 2026)

| Check | Eagle | KiCad |
|---|---|---|
| Schematic parts | 131 | 131 (145 placements incl. op-amp units) |
| Board footprints | 92 | 92 |
| Nets / signals | 66 | 66 (+13 `unconnected-` pin nets, normal) |
| Track segments | — | 425, all carrying nets |
| Vias | 20 | 20 |
| Copper zones | 1 | 1 |
| Board outline | — | 118.75 × 60.33 mm, rounded corners intact |

Update PCB from Schematic was run with "re-link by reference designator"
(required after an Eagle import — without it KiCad duplicates every
footprint). Second run reports zero changes. The only warnings are IC2 pads
7–10 (LM13700 Darlington buffers, unused, unconnected in the original too).
U$1/U$2/U$4/U$5 are the 3.0/3.3 mm mounting holes and U$6 is the logo; they are
board-only and have no schematic symbol. The 200bmp layer (Eagle bitmap logo)
was dropped on import — decorative only.

**Commit this state before touching anything.** Add to `.gitignore` first:

```
voicecard/hardware_design/pcb/KiCad/.history/
*.lck
```

(`.history/` is KiCad's local backup and contains its own nested `.git`.)

---

## 1. Staging — three boards, not one

The temptation is to jump straight to STM32 + PCM5102A. Don't. Each stage
below changes exactly one class of thing, so a silent card can be blamed on
one cause.

| Stage | What changes | What stays | Tests against |
|---|---|---|---|
| **A. SMD-4P** | Packages only. Same circuit, same ATmega328P, same MCP4822. | Every net, every value | Current firmware, unmodified |
| **B. SMD-4P + PCM5102A** | DAC swapped: SPI MCP4822 → I²S PCM5102A. AVR I²S is not native; see §5. | Analog section from A | Firmware with new DAC driver only |
| **C. STM32 voice card** | MCU swapped. 3.3 V logic, level shifting to the 5 V mobo bus. | Analog section from A | Ported firmware |

Stage A is the one this plan details. It is the least glamorous and the most
valuable: it validates the JLCPCB assembly flow, the footprint choices and
the analog layout with zero firmware risk. If A works, everything downstream
has a known-good analog reference to compare against.

Stage B may be skipped in favour of going straight to C — the PCM5102A wants
I²S and the AVR has no I²S peripheral. Decide after A ships.

---

## 2. Stage A — part substitution table

Values from `Voicecard-4P-v01.kicad_sch`. "Basic" = JLCPCB basic part
(no per-reel loading fee); verify basic/extended status in the JLC parts
library at order time — it shifts.

### Passives

| Original | Qty | Values | SMD choice | Notes |
|---|---|---|---|---|
| 0204/7 resistor | 33 | 220 ×4, 470 ×8, 2.2k ×3, 10k ×3, 22k ×5, 33k ×10 | **0603**, 1 % | All basic. 0805 if hand-rework matters more than density. |
| C025 ceramic | 24 | 100n ×16, 20p ×2, 100p, 220n, 560p ×4 | **0603** | 100n: X7R basic. 20p/100p/560p: **C0G/NP0** — these are timing/filter caps. |
| C2.5-5/4 | 4 | 220p (C7 C13 C19 C23) | **0603 C0G/NP0** | These are the 4-pole filter integrator caps. C0G is not optional; X7R here changes the filter with temperature and voltage. Consider 0805 C0G for tolerance selection. |
| E2.5-6 electrolytic | 3 | 100µ (C14 C25 C33) | SMD alu electrolytic, 6.3 mm can, or 2× 47µ 1206 X5R | Check rail voltage on each before picking a 6.3 V/10 V/16 V part. |
| ENP11-6.3 | 2 | 4.7µ (C1 C27) | **1206 X7R** or 0805 | Ceramics fine here (decoupling/filter). |
| 3296P trimmer | 1 | 5k (R28) | Bourns **3224W** (SMD, top-adjust) or keep THT 3296W | R28 is the filter tune trim. THT trimmer is one hand-solder joint and is easier to adjust in a rack of six cards; decide by preference. |

### Semiconductors

| Ref | Original | SMD part | Package | Notes |
|---|---|---|---|---|
| IC1 | TL074P | TL074CDR | SOIC-14 | Basic. |
| IC3, IC5 | TL072P | TL072CDR | SOIC-8 | Basic. |
| IC2 | LM13700N | LM13700MX | SOIC-16 | Extended. Pins 7–10 (buffers) stay unconnected. |
| IC4 | V2164D | **SSI2164** (Sound Semiconductor) | SOIC-16 | Pin-compatible SSM2164 successor, lower noise. **Not assembled by JLC** — V2164M is perpetually out of stock at LCSC and dear when it isn't. Ordered ×12 from Cabintech (Sep 2026), hand-soldered on arrival. Leave IC4 out of the JLC BOM (`DNP`/no LCSC field). |
| IC6 | MCP4822 | MCP4822-E/SN | SOIC-8 | Extended. Stage A keeps it. |
| IC7 | ATmega328P (DIP-28) | ATMEGA328P-AU | **TQFP-32** | Extended. TQFP-32 has two extra ADC pins (ADC6/ADC7) vs DIP-28 — leave NC. Fuses/bootloader identical. |
| Q1, Q2 | 2N3906 | MMBT3906 | SOT-23 | Basic. Check the Eagle symbol pin order vs SOT-23 EBC when assigning. |
| D1, D2 | 3.6 V zener | BZT52C3V6 | SOD-123 | Basic-ish. |
| D3 | 4.7 V zener | BZT52C4V7 | SOD-123 | |
| Q3 | 20 MHz HC49U | 20 MHz **3225** SMD crystal | 4-pad | Keep C31/C32 (20p) — re-check the load-capacitance spec of the chosen crystal; 20p per side implies ~12 pF CL with stray. |
| LED1, LED2 | 3 mm | 0805 LED, or keep 3 mm THT | | Decide by how visible you want them through the case. |

### Keep through-hole (mechanical / interface)

| Ref | Part | Why |
|---|---|---|
| JP1, JP3 | 1×3 SIP | Voice card ↔ motherboard connector. Pitch and position are fixed by the mobo; do not move. |
| JP2, JP4 | 1×6 SIP | Same. |
| ISP0 | 2×3 AVR ISP | Keep THT 2×3, or replace with a Tag-Connect TC2030 footprint (no part fitted). Tag-Connect is nicer for a production run. |
| U$1/2/4/5 | M3 holes | Positions fixed by the mobo standoffs. |

JLCPCB assembles THT on request but it is a separate, dearer service. The
clean option for Stage A: SMD-assembled by JLC, then four SIP headers, the
trimmer and the SSI2164 hand-soldered on arrival. That's six parts per card.

---

## 3. Stage A — layout rules

- **Outline, mounting holes and JP1–JP4 positions are locked** to the original
  (118.75 × 60.33 mm). Copy them from the baseline board verbatim; everything
  else is placed fresh.
- Two-layer is fine; the original is. Four-layer is cheap at JLC and buys a
  solid ground plane under the filter — worth it if the first two-layer
  build shows noise, not before.
- Keep the analog section (IC1–IC5, filter caps, expo converter Q1/Q2, R28)
  physically together and away from the AVR clock and the DAC SPI lines, as
  the original does. The 4P is an OTA cascade; the 220 p integrator caps and
  their 33 k resistors want short, tidy paths.
- GND pour on both sides, stitched.
- Silkscreen: reference designators on, values off (JLC's assembly drawing
  wants refs). Reinstate a logo on F.SilkS if wanted — the Eagle bitmap is
  gone; draw or import a fresh one.
- Design rules for JLC 2-layer standard: 0.127 mm (5 mil) min trace/space,
  0.3 mm min via drill, 0.6 mm via pad. Set these in the board setup before
  routing, not after.

---

## 4. Stage A — JLCPCB production files

`kicad-cli` (KiCad 10, on the Mac at
`/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`) produces everything:

```
kicad-cli sch erc  Voicecard-4P-SMD.kicad_sch
kicad-cli pcb drc  Voicecard-4P-SMD.kicad_pcb
kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts ...
kicad-cli pcb export drill --format excellon --excellon-units mm ...
kicad-cli pcb export pos --format csv --units mm --side both --use-drill-file-origin ...
kicad-cli sch export bom --fields "Reference,Value,Footprint,LCSC" --group-by Value,Footprint ...
```

Two schematic-side requirements for the BOM to be useful:

1. Every assembled part gets an **`LCSC` field** in its symbol (e.g. `C6961`).
   That is the column JLC matches on. Do this during the footprint swap,
   not afterwards.
2. JLC's CPL wants columns `Designator, Mid X, Mid Y, Layer, Rotation`.
   `kicad-cli pcb export pos` gives `Ref, PosX, PosY, Rot, Side`; a five-line
   script renames them. Rotation offsets for SOIC/SOT-23 vs JLC's convention
   are the classic gotcha — check the first assembly preview on the JLC site
   pad by pad.

The jhbruhn/ambika-smr4-multimode repo is the reference for this workflow
(it ships JLC production files). Its **circuit** is not a reference — it is
an LM13700 SMR-4, not the SSM2164 4P.

---

## 5. Stages B and C — decisions to make later, recorded now

**PCM5102A on an AVR (Stage B).** The PCM5102A needs I²S (BCK, LRCK, DIN) at
a standard audio rate; the ATmega328P has no I²S. Bit-banging I²S at 39.2 kHz
(the current sample rate, timer2 phase-correct PWM at 20 MHz / 510) is not
realistic inside the existing 510-cycle budget. Options: (a) skip B and do
the DAC change as part of C, where the STM32 has I²S in hardware; (b) use a
cheap I²S bridge — not worth it. **Recommendation: skip B.** The analog
section only cares that *something* delivers an audio-band voltage at OUT
and a control voltage on F_CV/RES_PWM.

**STM32 voice card (Stage C).** Not designed here; constraints to carry:

- The motherboard talks to the card over SPI-ish lines (SCK, MOSI, MISO, SS,
  SSA, SSB) at 5 V from the controller's ATmega644P. The STM32 needs 5 V-
  tolerant inputs (many STM32 pins are FT — pick a part and check each pin)
  or a level shifter, and its 3.3 V outputs must be readable by the 644P
  (3.3 V is above the 644P's V_IH at 5 V; usually fine, verify).
- 3.3 V regulator on the card from the mobo's +5 V.
- The PCM5102A output is a line-level, DC-coupled-capable audio signal; the
  MCP4822 currently provides both the audio (OUT) and the control voltages
  F_CV / RES_PWM / VCA_CONTROL. Those CVs need a home: STM32 DAC channels
  (F4/G4/H7 have two 12-bit DACs) or PWM + RC as now. Keep the CV path
  separate from the I²S audio path.
- The audio path after the DAC: the current OUT node expects a 0–4.096 V-ish
  unipolar signal from the MCP4822 into C4/R1 etc. The PCM5102A output is
  ±~1 V around 0 V. The input stage of the filter will need re-biasing.
  Measure the original OUT node and the first op-amp stage before redesigning.

Candidate MCU: STM32F4 (F405/F411, cheap, I²S, two DACs, plenty of 5 V-tolerant
pins) or G4 (better DACs). Decide when the firmware plan for the port
(`VOICECARD_V2_PLAN.md`, oscillator rewrite) has settled what the CPU budget
looks like on an M4.

---

## 6. Working split: Cowork vs Claude Code

- **Claude Code on the Mac** owns the KiCad file work: footprint swaps in the
  `.kicad_sch` (text), LCSC field population, `kicad-cli` ERC/DRC/exports,
  BOM/CPL scripts, and the `git` history. It can run `kicad-cli` directly.
  With KiCad 10's IPC API it can also drive placement in the open board via
  the `kicad-python` package when we reach layout.
- **Cowork** for the GUI-only moments (import dialogs, visual inspection via
  screen control) and for planning.
- **Both machines share the repo via iCloud.** Two rules: never have the
  project open in KiCad on both Macs at once (`.lck` files and iCloud do not
  mix), and let iCloud finish syncing before switching machines. The
  `.history/` folder is per-machine noise; it is gitignored.

### First tasks for Claude Code

1. Confirm the baseline commit exists and is clean (`git status`).
2. Copy `KiCad/` → `KiCad-SMD/` (or branch; a copy is simpler with iCloud),
   rename the project files to `Voicecard-4P-SMD`.
3. In the schematic, apply §2: change each symbol's Footprint field to the
   KiCad standard library footprint (`Resistor_SMD:R_0603_1608Metric`,
   `Package_SO:SOIC-8_3.9x4.9mm_P1.27mm`, `Package_QFP:TQFP-32_7x7mm_P0.8mm`,
   …) and add the `LCSC` field. Do not touch nets or values.
4. Run `kicad-cli sch erc`; expect the same LM13700 buffer-pin warnings only.
5. Open the board, Update PCB from Schematic *with* "replace footprints" —
   this drops in the SMD footprints on top of the old positions with the
   ratsnest intact — then delete all tracks/zones and start placement from
   the locked outline/connectors.
