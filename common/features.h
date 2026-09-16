// -----------------------------------------------------------------------------
//
// KEZ MOD: Switches to disable features and recover some bytes.
//
// -----------------------------------------------------------------------------

// Switch audit, 2026-09-15. Measured by building with each flipped and by
// grepping for consumers. Several of these do nothing at all -- do not assume a
// switch works because of its name.
//
//   DISABLE_SNAPSHOT          works; removes the deepest stack path
//   DISABLE_CARD_INFO_PAGE    works
//   DISABLE_VERSION_MANAGER   works, but see the page-table static_assert
//   DISABLE_RAGAS             only hides the raga control on the PART page. The
//                             raga tables stay linked, so it saves NOTHING --
//                             not one byte of flash or SRAM.
//   DISABLE_GROOVE_TEMPLATES  NO CONSUMER. Does nothing.
//   DISABLE_SEQUENCER         works, but see the page-table static_assert
//   DISABLE_ARPEGGIO          works
//   LFO_MEMORY_DIET           NO CONSUMER. Does nothing.
//   BANDLIMITED_TRIANGLE      NO CONSUMER. Does nothing.
//   DIRTY_CC_LOOKUP           works
//   DISABLE_CC_MAPS           NO CONSUMER. Does nothing.
//   DISABLE_LAUNCHKEY_MODE    works
//   DISABLE_PART_MUTES        works
//   POLIVOKS_FILTERBOARD      had NO CONSUMER in this tree until 2026-09-15;
//                             the resonance inversion it is meant to apply was
//                             lost in the MachFour merge and is now restored in
//                             voicecard/voice.cc.

// KZ MOD 2026-09-16: removes the undo snapshot that Storage::Load writes
// whenever the edit buffer is dirty. That snapshot is a full Storage::Save plus
// an Unlink -- f_open 53, f_mkdir 66, f_unlink 66, the three deepest stack
// frames in the firmware -- and it runs on every patch load after you touch a
// knob, which is ordinary browsing. It drove the controller's stack low
// watermark to 0, confirmed on hardware by the 'sav' context tag on the
// diagnostic page.
//
// Defined by default: a feature that can collide the stack with static data is
// not worth offering. Undefining it restores the snapshot and exposes an `undo`
// switch on preferences page B, for anyone who wants undo and has the headroom.
// The version manager page still builds either way, but with snapshots off
// there is nothing for it to step back to.
#define DISABLE_SNAPSHOT

#define DISABLE_CARD_INFO_PAGE
//#define DISABLE_VERSION_MANAGER
//#define DISABLE_RAGAS
//#define DISABLE_GROOVE_TEMPLATES
//#define DISABLE_SEQUENCER
//#define DISABLE_ARPEGGIO
//#define LFO_MEMORY_DIET
//#define BANDLIMITED_TRIANGLE
#define DIRTY_CC_LOOKUP
#define DISABLE_CC_MAPS
#define DISABLE_LAUNCHKEY_MODE
#define DISABLE_PART_MUTES
// Carey does not have Polivoks filter boards; left here for others. Enabling
// it inverts the resonance CV in voicecard/voice.cc.
//#define POLIVOKS_FILTERBOARD
