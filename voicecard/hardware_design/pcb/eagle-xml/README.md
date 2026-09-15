# Eagle XML resaves of the original voicecard designs

These are the original Mutable Instruments Ambika voicecard files (Eagle 5 binary,
in the parent directory) resaved as Eagle 9.7.0 XML. They come from
https://github.com/rhinocerose/synth-mutable-ambika (voicecard/hardware_design/pcb/*/eagle-xml/).

Provenance check, 2026-09-15: the binary originals in that repository are
byte-identical (MD5) to the ones in this tree, and the XML pair for the 4P is a
straight resave: 131 parts / 66 nets in the schematic, 92 elements / 66 signals on
the board. No design changes.

These XML files are what KiCad's Eagle importer needs (it cannot read the Eagle 5
binaries). Voicecard-4P-v01 is the source for the SMD/KiCad rebuild.
