# Voicecard-4P-SMD — Stage A (packages only)

Stage A of `docs/VOICECARD_SMD_PLAN.md`: the same 4P circuit in SMD packages,
for JLCPCB assembly. Same nets, same values, same firmware.

Provenance: copied from `../KiCad/` (the Eagle→KiCad 10 import baseline) at
commit `7cec6fe`. Only the project files were renamed; the imported symbol and
footprint libraries keep their `Voicecard-4P-v01-*` names, so every `lib_id`
in the schematic and board is unchanged. `../KiCad/` stays untouched as the
reference for the original through-hole board.

State: schematic footprints and LCSC fields assigned, ERC clean. **No board
work done** —
`Voicecard-4P-SMD.kicad_pcb` is still the original through-hole layout.

## Pin renumbering (symbol pads, not nets)

Eight Eagle symbols numbered their pins for the through-hole footprint. The
standard-library SMD footprints number pads differently, so the pin *numbers*
in the symbols were changed. Schematic wires attach to pin positions, not pin
numbers, so this changes which pad a pin lands on and nothing else; the
exported netlist is identical to the baseline's, net for net.

| Symbol | Refs | Change |
|---|---|---|
| `C-US` | C1 C27 | `+`/`-` → `1`/`2` |
| `CPOL-USE2.5-6` | C14 C25 C33 | `+`/`-` → `1`/`2` (KiCad pad 1 is +) |
| `1N47XXDO35` | D1 D2 D3 | `C`/`A` → `1`/`2` (SOD-123 pad 1 is the cathode) |
| `LED3MM` | LED1 LED2 | `K`/`A` → `1`/`2` (LED_0805 pad 1 is the cathode) |
| `2N3906-` | Q1 Q2 | TO-92 C,B,E = 1,2,3 → SOT-23 B=1, E=2, C=3 |
| `C-2.5-5/4` | C7 C13 C19 C23 | pin 3 → pad `2`; the Eagle symbol carried two alternate pads for one terminal, and the schematic wires both |
| `CRYSTALHC49U-V` | Q3 | pin 2 → pad `3`; the 3225 has the crystal on pads 1 and 3 |
| `MEGA8-P` | IC7 | DIP-28 → TQFP-32 numbering throughout |

Added pins, all of them pads the DIP package did not have:

- IC7 VCC and GND use KiCad 10 stacked numbers `[4,6]` and `[3,5]`, so the
  TQFP's second VCC and GND pads join +5V and GND. Both must be connected;
  the datasheet requires it.
- IC7 ADC6 (19) and ADC7 (22): TQFP-only ADC inputs, left unconnected.
- Q3 pads 2 and 4: the 3225's case pads, grounded (Carey, 2026-09-16) as
  convention for the package. They are one pin with the stacked number
  `[2,4]`, wired to the C31/C32 ground rail.

## Footprints and parts

JLC-assembled parts carry an `LCSC` field. Part numbers, stock and
basic/extended status were read from JLC's parts API on 2026-09-16; verify
again at order time.

| Refs | Value | Footprint | LCSC | Part | JLC |
|---|---|---|---|---|---|
| R1–R27, R29–R34 | 220/470/2.2k/10k/22k/33k | `Resistor_SMD:R_0603_1608Metric` | C22962 / C23179 / C4190 / C25804 / C31850 / C4216 | UNI-ROYAL 0603WAF, 1 % | basic |
| 16× 100n | 100n | `Capacitor_SMD:C_0603_1608Metric` | C14663 | CC0603KRX7R9BB104, 50 V X7R | basic |
| C31 C32 | 20p | `C_0603_1608Metric` | C1648 | CL10C200JB8NNNC, C0G | basic |
| C4 | 100p | `C_0603_1608Metric` | C14858 | CL10C101JB8NNNC, C0G | basic |
| C26 | 220n | `C_0603_1608Metric` | C21120 | CL10B224KA8NNNC, 25 V X7R | basic |
| C9 C10 C21 C22 | 560p | `C_0603_1608Metric` | C107054 | CC0603JRNPO9BN561, 50 V NP0 | extended |
| C7 C13 C19 C23 | 220p | `C_0603_1608Metric` | C106210 | CC0603JRNPO9BN221, 50 V NP0 | extended |
| C1 C27 | 4.7u | `Capacitor_SMD:C_1206_3216Metric` | C29823 | 1206B475K500NT, 50 V X7R | basic |
| C14 C25 C33 | 100u | `Capacitor_SMD:CP_Elec_6.3x7.7` | C72477 | RVT1E101M0607, 25 V, 6.3 × 7.7 mm | extended |
| D1 D2 | 3.6V | `Diode_SMD:D_SOD-123` | C173412 | BZT52C3V6 | extended |
| D3 | 4.7V | `Diode_SMD:D_SOD-123` | C173408 | BZT52C4V7 | extended |
| IC1 | TL074P | `Package_SO:SOIC-14_3.9x8.7mm_P1.27mm` | C12594 | TL074CDR | extended |
| IC2 | LM13700N | `Package_SO:SOIC-16_3.9x9.9mm_P1.27mm` | C174050 | LM13700MX/NOPB | extended |
| IC3 IC5 | TL072P | `Package_SO:SOIC-8_3.9x4.9mm_P1.27mm` | C6961 | TL072CDT | basic |
| IC4 | V2164D | `Package_SO:SOIC-16_3.9x9.9mm_P1.27mm` | — | SSI2164, hand-soldered | not ordered from JLC |
| IC6 | MCP4822 | `Package_SO:SOIC-8_3.9x4.9mm_P1.27mm` | C16040 | MCP4822-E/SN | extended |
| IC7 | ATMega328p | `Package_QFP:TQFP-32_7x7mm_P0.8mm` | C14877 | ATMEGA328P-AU | extended |
| Q1 Q2 | 2N3906 | `Package_TO_SOT_SMD:SOT-23` | C53444 | MMBT3906LT1G | extended |
| Q3 | 20MHz | `Crystal:Crystal_SMD_3225-4Pin_3.2x2.5mm` | C255914 | X1E0000210624, **CL 12 pF** | extended |
| LED1 LED2 | LED3MM | `LED_SMD:LED_0805_2012Metric` | C84256 | NCD0805R1, red | basic |

Value fields still read as the original parts (`TL074P`, `2N3906`,
`LED3MM`, …) because Stage A does not change values; the `LCSC` field carries
the part that is actually fitted.

Crystal load capacitance: C31/C32 stay at 20 p — two 20 p in series is 10 pF,
plus a few pF of stray, so 12–14 pF. Hence the 12 pF-load crystal. A 20 pF-load
part such as C9004 would mean changing those values, which Stage A does not do.

### Kept through-hole

JP1–JP4, ISP0 and R28 keep their imported Eagle footprints. JP1–JP4 are
mechanically locked to the motherboard, and the imported footprints are
anchored the way the baseline board places them; the standard-library pin
headers are anchored on pad 1 and would shift the connectors. Per the plan
these six parts (four headers, the trimmer, the SSI2164) are hand-soldered on
arrival, so they carry no LCSC field.

R28's imported footprint is the triangular Bourns 3296P pad pattern, matching
KiCad's `Potentiometer_Bourns_3296P_Horizontal`, not the 3296W named in the
plan. Kept through-hole for Stage A (Carey, 2026-09-16): it is the pattern the
original used, and the card is already being finished by hand. An SMD 3224W
is a question for the STM32 board.

## ERC

`kicad-cli sch erc` reports **0 errors and 1 warning**: IC2 has unplaced units
A and B, the LM13700's two Darlington buffers, unused in the 4P (pins 7–10).
That warning is inherent to using half of an LM13700 and is left alone; it is
also what Update PCB from Schematic reports about pads 7–10.

The Eagle import arrived with 20 ERC errors, all of them missing annotation
rather than circuit faults, and all cleared here (Carey, 2026-09-16) so that a
real error is visible later. No net and no value changed:

- 13 no-connect flags on pins that are unused by design: IC7 AREF, PC0–PC3,
  PD0, PD3, PD7, PB0, PB1; IC2 pins 2 and 15 (LM13700 diode-bias inputs);
  IC4 pin 1 (2164 MODE). ADC6/ADC7 on the TQFP need no flag — those pins are
  declared as no-connect in the symbol.
- 4 PWR_FLAGs, on +5V, VCC, VEE and GND, telling ERC where power enters the
  card. They sit on the existing supply symbols' pins; their value text is
  hidden so it does not cover JP2's pin numbers. PWR_FLAG lives in
  `Voicecard-4P-SMD-extras.kicad_sym`, a project library, so the schematic
  does not depend on KiCad's global symbol libraries. Flags carry no
  footprint and stay out of the BOM.

These flags exist only in KiCad-SMD; `../KiCad/` keeps its original ERC state.

## Reproducing the checks

```sh
KC=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
$KC sch erc --severity-all -o erc.rpt Voicecard-4P-SMD.kicad_sch
$KC sch export netlist --format kicadsexpr -o smd.net Voicecard-4P-SMD.kicad_sch
$KC sch export bom --fields "Reference,Value,Footprint,LCSC,\${QUANTITY}" \
    --group-by "Value,Footprint,LCSC" -o bom.csv Voicecard-4P-SMD.kicad_sch
```

## Board preparation (done, nothing placed)

The board was updated from the schematic with footprints replaced in place:
each footprint sits at its predecessor's position, orientation and layer, and
keeps its schematic path. kicad-cli has no update-from-schematic command and
pcbnew does not wrap `BOARD_NETLIST_UPDATER`, so `board_prep.py` does it
explicitly through pcbnew and the result is checked against the schematic
netlist rather than trusted:

| Check | Result |
|---|---|
| Footprints | 92 (81 replaced, 5 board-only: U$1 U$2 U$4 U$5 U$6, 6 kept THT) |
| Pad nets vs schematic netlist | 277 of 277, no mismatch, nothing extra |
| Nets | same set as the schematic, none added or lost |
| Library ids | all library-qualified (`Resistor_SMD:R_0603...`) |
| Tracks / vias / zones | 445 segments, 20 vias and 1 zone deleted; 0 remain |
| Locked | Edge.Cuts (8 segments), U$1 U$2 U$4 U$5, JP1–JP4, ISP0 |
| Outline | 118.8 × 60.38 mm, unchanged |

IC2 pads 7–10 carry no net: the LM13700 buffer half the 4P does not use.

### Design rules and net classes

JLCPCB 2-layer standard, set in the `.kicad_pro`:

| Rule | Was | Now |
|---|---|---|
| Minimum track width | 0.2 | **0.127 mm** |
| Minimum clearance | 0.2032 | **0.127 mm** |
| Minimum via drill | 0.3 | 0.3 mm |
| Minimum via diameter | 0.5 | **0.6 mm** |

| Net class | Track | Via | Nets |
|---|---|---|---|
| Default | 0.2 mm | 0.6 / 0.3 mm | everything else |
| Power | 0.4 mm | 0.6 / 0.3 mm | +5V, VCC, VEE, GND |

### Light placement pass

Olivier's arrangement is kept: nothing was compacted or regrouped, and the
rows of passives stay where they were. Only these moved (`place.py`):

**Decoupling.** Every 100n sat 5.97–13.99 mm from the supply pins it serves,
because the SOIC and TQFP bodies are so much smaller than the DIPs they
replaced. Each is now beside its IC's supply pad, assigned to minimise total
travel:

| Cap | Serves | Gap |
|---|---|---|
| C16 / C3 | IC1 VCC pad 4 / VEE pad 11 | 2.88 / 2.88 mm |
| C5 / C18 | IC2 VCC pad 11 / VEE pad 6 | 1.32 / 1.32 mm |
| C6 / C20 | IC3 VCC pad 8 / VEE pad 4 | 1.32 / 1.32 mm |
| C8 / C11 | IC4 VCC pad 16 / VEE pad 9 | 1.28 / 1.55 mm |
| C12 / C24 | IC5 VCC pad 8 / VEE pad 4 | 1.32 / 1.32 mm |
| C29 | IC6 VDD pad 1 | 1.58 mm |
| C30 | IC7 VCC pads 4 and 6 | 2.42 / 2.83 mm |
| C28 | IC7 AVCC pad 18 | 0.88 mm |

**Crystal loop.** Q3, C31 and C32 moved from 10–17 mm away to sit against
IC7's XTAL pins: Q3's terminals are now 1.65 mm (XTAL2, pad 8) and 3.35 mm
(XTAL1, pad 7) from the MCU, with the load caps flanking the crystal. The
3225's two terminals are diagonally opposite, so one of them is always the
far one; the two placements are within 0.05 mm of each other in total length.

**C33** moved from the bottom-right corner, where its pad crossed the board
edge, to the free band at (191.5, 113.5). It could not simply move inboard:
that corner is boxed in by the U$5 mounting hole, ISP0, R33 and JP4, and the
9.5 mm can does not fit between them.

**Q1 and Q2 are not an expo pair, so they were left alone.** Read from the
netlist: each is a separate op-amp servo current source. Q1's base is driven
by IC1D's output with R1 from its emitter back to IC1D's inverting input, and
its collector feeds I_GAIN (IC2 pin 16); Q2 is the same topology around IC1A
with R12, feeding I_RESO (IC2 pin 1). They share no node, and neither is an
exponential converter — the V/Oct exponential conversion is internal to the
SSI2164, whose four CTRL pins are driven from F_CV via IC1B, with R28 (the
trimmer silkscreened V/Oct) setting the scale through R15. Placing them
touching would buy nothing.

**Grid and orientation.** All unlocked footprints are on a 0.5 mm grid, except
C25, which has 0.06 mm of slack between JP1 and JP3 and so keeps its imported
position rather than being nudged into a locked connector. 24 0603 parts had
their orientation normalised to their own axis (-90 to 90, 180 to 0) so each
group agrees; nothing was rotated by 90 degrees, so no pad moved.

Locked parts — connectors, mounting holes, the outline — did not move.

### DRC after placement

```
** Found 88 DRC violations **
** Found 196 unconnected items **

 196  [unconnected_items]      error    <- nothing routed yet
  51  [silk_over_copper]       warning
  25  [silk_overlap]           warning
   6  [silk_edge_clearance]    warning
   4  [lib_footprint_mismatch] warning
   2  [text_height]            warning
```

`copper_edge_clearance` is **zero**, as is `courtyards_overlap`. The
silkscreen warnings rose from 57 to 82 because parts are now closer together;
that is for the silkscreen pass after routing, where plan §3 turns values off
and leaves reference designators on for JLC's assembly drawing. The 4 library
mismatches are still the Eagle-imported mounting holes.

Renders and the ratsnest drawing are written to `build/` (gitignored):
`voicecard-4p-smd-front.png`, `-back.png`, `-ratsnest.png`.

## Next (not done)

Placement, then routing. The outline, mounting holes and connectors are
locked, so placement starts from them. Plan §3 layout rules apply: analog
section together and away from the AVR clock and DAC lines, GND pour both
sides stitched, values off the silkscreen.
