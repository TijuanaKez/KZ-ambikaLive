// Compile the actual page implementations against checked host substitutes.
// This validates navigation/rendering, not AVR peripherals or interrupt timing.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "controller/deferred_load.h"

#define PSTR(s) (s)
#define PROGMEM
#define strncpy_P std::strncpy
#define memcpy_P std::memcpy
#define IGNORE_UNUSED(x) (void)(x)
#define DIAGNOSTIC_BUILD

namespace ambika {
constexpr uint8_t kNumParameters = 79, kNumParametersPerPage = 8;
constexpr uint8_t kLcdWidth = 40, kDelimiter = 7;
constexpr uint8_t PAGE_ENV_LFO = 3, PAGE_SYSTEM_SETTINGS = 15;
constexpr uint8_t PAGE_SYSTEM_SETTINGS_B = 16, LED_STATUS = 14, LED_8 = 7;
constexpr uint8_t SWITCH_8 = 7, PARAMETER_LEVEL_UI = 4;
enum ActiveControl { ACTIVE_CONTROL_FIRST, ACTIVE_CONTROL_LAST };
enum EditMode { EDIT_IDLE, EDIT_STARTED_BY_ENCODER, EDIT_STARTED_BY_POT };
uint8_t lowNibble(uint8_t x) { return x & 15; }
uint8_t highNibble(uint8_t x) { return x >> 4; }
uint8_t highNibbleUnshifted(uint8_t x) { return x & 240; }
char NibbleToAscii(uint8_t x) { return x < 10 ? '0' + x : 'a' + x - 10; }
template<class T> void UnsafeItoa(T x, uint8_t width, char* p) {
  char text[16];
  int n = std::snprintf(text, sizeof(text), "%d", int(x));
  assert(n <= width);
  std::memcpy(p, text, n);
  if (n < width) p[n] = 0;
}
void AlignRight(char* p, uint8_t width) {
  auto n = strnlen(p, width);
  std::memmove(p + width - n, p, n);
  std::memset(p, ' ', width - n);
}

struct PageInfo { uint8_t index, data[8], next_page; };
struct UiPage {
  static inline int8_t active_control_;
  static inline EditMode edit_mode_;
  static inline PageInfo* info_;
  static void OnInit(PageInfo* info) { info_ = info; edit_mode_ = EDIT_IDLE; active_control_ = 0; }
  static uint8_t OnClick() {
    edit_mode_ = edit_mode_ == EDIT_IDLE ? EDIT_STARTED_BY_ENCODER : EDIT_IDLE;
    return 1;
  }
  static void UpdateLeds() {}
};
struct ParameterEditor : UiPage {
  using SnapMask = uint8_t;
  static SnapMask snapped_;
  static uint8_t parameter_index(uint8_t), part_index(uint8_t), instance_index(uint8_t);
  static void OnInit(PageInfo*), SetActiveControl(ActiveControl);
  static uint8_t OnIncrement(int8_t), OnClick(), OnPot(uint8_t, uint8_t);
  static bool OnIncrementAndCycle(int8_t, int8_t);
  static void UpdateScreen(), UpdateLeds();
};
constexpr uint8_t kNumVoices = 6, kNumParts = 6;
inline uint8_t byteInverse(uint8_t v) { return static_cast<uint8_t>(~v); }
inline uint8_t byteOr(uint8_t a, uint8_t b) { return a | b; }
inline uint8_t byteAnd(uint8_t a, uint8_t b) { return a & b; }
constexpr uint8_t kAudioStarved = 0xfe, kAudioHeadroomUnsupported = 0xff;
enum StackContext : uint8_t {
  STACK_CTX_IDLE, STACK_CTX_UI, STACK_CTX_LOAD, STACK_CTX_SAVE,
  STACK_CTX_BACKUP, STACK_CTX_SYSEX, STACK_CTX_SD_TICK, STACK_CTX_MIDI,
  STACK_CTX_LAST
};
uint8_t stack_low_context = STACK_CTX_IDLE;
inline uint8_t U8(int v) { return static_cast<uint8_t>(v); }
// Records which cards were polled, so the test can check the round-robin.
struct VoicecardTx {
  uint8_t polled[kNumVoices] = {};
  uint8_t reply = 40;
  uint8_t GetAudioHeadroom(uint8_t voice_id) {
    assert(voice_id < kNumVoices);
    ++polled[voice_id];
    return reply;
  }
} voicecard_tx;
struct MidiDispatcher {
  uint8_t peak = 0;
  uint8_t out_peak() const { return peak; }
} midi_dispatcher;
struct VoiceAssigner : ParameterEditor {
  static void OnInit(PageInfo*), SetActiveControl(ActiveControl);
  static uint8_t OnIncrement(int8_t), OnClick(), OnPot(uint8_t, uint8_t);
  static uint8_t OnNote(uint8_t, uint8_t);
  static void UpdateScreen();
};
struct OsInfoPage : UiPage {
  static void OnInit(PageInfo*), UpdateScreen(), UpdateLeds();
  static uint8_t OnIncrement(int8_t), OnKey(uint8_t), OnClick();
  static void MemoryOnInit(PageInfo*), MemoryUpdateScreen(), MemoryUpdateLeds();
  static uint8_t MemoryOnIncrement(int8_t), MemoryOnKey(uint8_t);
  // Defined below as no-ops: the firmware-update half is a separate
  // translation unit that these tests do not compile.
  static void FirmwareOnInit(PageInfo*), FirmwareUpdateScreen(), FirmwareUpdateLeds();
  static uint8_t FirmwareOnIncrement(int8_t), FirmwareOnKey(uint8_t);
  static void FindFirmwareFiles(uint8_t);
  static uint8_t audio_headroom_[kNumVoices];
  static uint8_t audio_headroom_index_;
  static uint8_t show_memory_;
};

PageInfo prefs_a = {15, {66,67,71,72,68,69,70,0xf8}, 16};
PageInfo prefs_b = {16, {75,76,77,255,255,255,255,0xf9}, 15};
struct Ui {
  struct State {
    uint8_t values[8] = {};
    uint8_t* bytes() { return values; }
    uint8_t active_env_lfo() { return 0; }
  } state_;
  int previous = 0;
  uint8_t active_part() { return 0; }
  State& state() { return state_; }
  uint8_t last_page = 0;
  void ShowPage(uint8_t page) {
    last_page = page;
    // Only the preferences pages have fixtures here; other pages are just
    // recorded, so a page that navigates away can be observed doing it.
    if (page == 15 || page == 16) {
      ParameterEditor::OnInit(page == 15 ? &prefs_a : &prefs_b);
    }
  }
  void ShowPageRelative(int8_t increment) {
    ShowPage(UiPage::info_->next_page);
    ParameterEditor::SetActiveControl(increment > 0 ? ACTIVE_CONTROL_FIRST : ACTIVE_CONTROL_LAST);
  }
  void ShowPreviousPage() { ++previous; }
} ui;
struct PartMapping { uint8_t voice_allocation = 0; };
struct Multi {
  struct Knob { uint8_t parameter = 0, part = 0, instance = 0; } knobs[8];
  PartMapping mappings[6];
  unsigned assignments = 0;
  Multi& data() { return *this; }
  Knob& knobAssignment(uint8_t i) { assert(i < 8); return knobs[i]; }
  PartMapping& part_mapping(uint8_t i) { assert(i < 6); return mappings[i]; }
  // Mirrors Multi::SolveAllocationConflicts: every voice claimed by another
  // part is unavailable to this one.
  uint8_t SolveAllocationConflicts(uint8_t constraint) {
    uint8_t available = 0xff;
    for (uint8_t i = 0; i < 6; ++i) {
      if (i != constraint) {
        mappings[i].voice_allocation &= available;
        available &= ~mappings[i].voice_allocation;
      }
    }
    return available;
  }
  void AssignVoicesToParts() { ++assignments; }
  Multi& part(uint8_t i) { assert(i < 6); return *this; }
  uint8_t lfo_value(uint8_t) { return 0; }
  bool running() { return false; }
  uint8_t step() { return 0; }
} multi;
struct Settings {
  bool help = false, snapping = false;
  Settings& data() { return *this; }
  bool snap() { return snapping; }
  bool show_help() { return help; }
} system_settings;
constexpr uint8_t kLcdNoCursorValue = 255;
struct Display {
  char memory[83];
  void clear() {
    std::memset(memory, ' ', sizeof(memory));
    memory[0] = 'L'; memory[82] = 'R';
  }
  char* line_buffer(uint8_t line) { assert(line < 2); return memory + 1 + 40 * line; }
  uint8_t cursor = kLcdNoCursorValue;
  void set_cursor_character(char) {}
  void set_cursor_position(uint8_t p) {
    // 80 visible cells, or the no-cursor sentinel. Anything else is a bug.
    assert(p == kLcdNoCursorValue || p < 80);
    cursor = p;
  }
  void check() { assert(memory[0] == 'L' && memory[82] == 'R'); }
} display;
struct Leds { void set_pixel(uint8_t, uint8_t) {} } leds;
constexpr uint8_t kLcdNoCursor = 255;
struct Parameter {
  uint8_t indexed_by = 255, level = 3, max_value = 1;
  void PrintObject(uint8_t, uint8_t, char* b, uint8_t width) const { std::memset(b, 'o', width); }
  void Print(uint8_t, char* b, uint8_t nw, uint8_t vw) const { std::memset(b, 'p', nw + vw + 1); }
};
struct ParameterManager {
  Parameter p;
  unsigned lookups = 0, writes = 0;
  const Parameter& parameter(uint8_t id) { assert(id < kNumParameters); ++lookups; return p; }
  uint8_t GetValue(const Parameter&, uint8_t, uint8_t) { return 0; }
  void SetValue(const Parameter&, uint8_t, uint8_t, uint8_t, uint8_t) { ++writes; }
  void Increment(const Parameter&, uint8_t, uint8_t, int8_t, bool) { ++writes; }
  void Increment(uint8_t id, uint8_t, uint8_t, int8_t, bool) { parameter(id); ++writes; }
  void Scale(const Parameter&, uint8_t, uint8_t, uint8_t) { ++writes; }
  bool is_snapped(const Parameter&, uint8_t, uint8_t, uint8_t) { return true; }
} parameter_manager;
uint16_t free_sram = 270, untouched_sram = 128;
uint16_t FreeSram() { return free_sram; }
uint16_t UntouchedSram() { return untouched_sram; }
uint8_t ResetCause() { return 0xab; }
}  // namespace ambika

#include "controller/ui_pages/parameter_editor.cc"
// The firmware-update half lives in os_info_page.cc and needs the SD card, the
// voice card protocol and EEPROM; the diagnostic view is what these tests are
// about, so only that translation unit is compiled. Its dispatchers reference
// the firmware entry points, which are stubbed here.
namespace ambika {
void OsInfoPage::FirmwareOnInit(PageInfo*) {}
uint8_t OsInfoPage::FirmwareOnIncrement(int8_t) { return 1; }
uint8_t OsInfoPage::FirmwareOnKey(uint8_t) { return 1; }
void OsInfoPage::FirmwareUpdateScreen() {}
void OsInfoPage::FirmwareUpdateLeds() {}
void OsInfoPage::FindFirmwareFiles(uint8_t) {}
}  // namespace ambika
#include "controller/ui_pages/os_info_page_diag.cc"
#include "controller/ui_pages/voice_assigner.cc"

int main() {
  using namespace ambika;
  for (auto* page : {&prefs_a, &prefs_b}) {
    ParameterEditor::OnInit(page);
    display.clear();
    ParameterEditor::UpdateScreen();
    display.check();
    assert(std::memcmp(display.line_buffer(1) + 34,
                       page == &prefs_a ? "more->" : "<-back", 6) == 0);
    for (unsigned i = 8; i < 256; ++i) {
      assert(ParameterEditor::parameter_index(i) == 255);
    }
    for (uint8_t i = 0; i < 8; ++i) {
      auto before = parameter_manager.writes;
      ParameterEditor::OnPot(i, 64);
      bool real = page->data[i] < kNumParameters;
      assert(parameter_manager.writes == before + real);
      if (!real) assert(ParameterEditor::parameter_index(i) == 255);
    }
    ParameterEditor::edit_mode_ = EDIT_IDLE;
    ParameterEditor::active_control_ = 7;
    ParameterEditor::OnClick();
    assert(ParameterEditor::info_->index == page->next_page);
    assert(ParameterEditor::edit_mode_ == EDIT_IDLE);
    ParameterEditor::OnInit(page);
    ParameterEditor::active_control_ = 7;
    ParameterEditor::OnIncrement(1);
    assert(ParameterEditor::info_->index == page->next_page);
    assert(ParameterEditor::active_control_ == 0);
  }
  ParameterEditor::OnInit(&prefs_b);
  // Preferences page B carries three settings in the default build, so the
  // walk to the back control skips four unused cells rather than five.
  for (int i = 0; i < 3; ++i) ParameterEditor::OnIncrement(1);
  assert(ParameterEditor::active_control_ == 7);
  ParameterEditor::OnIncrement(-1);
  assert(ParameterEditor::active_control_ == 2);
  ParameterEditor::OnClick();
  auto before = parameter_manager.writes;
  ParameterEditor::OnIncrement(1);
  assert(parameter_manager.writes == before + 1);
  system_settings.help = true;
  display.clear();
  ParameterEditor::UpdateScreen();
  display.check();

  // Navigation pots bypass snap and editing, and only switch once per sweep.
  // The middle dead band preserves the selected page in both directions.
  for (bool snap : {false, true}) {
    system_settings.snapping = snap;
    ParameterEditor::OnInit(&prefs_a);
    ParameterEditor::edit_mode_ = EDIT_STARTED_BY_POT;
    auto lookups = parameter_manager.lookups;
    auto writes = parameter_manager.writes;
    for (uint8_t value = 0; value < 80; ++value) {
      ParameterEditor::OnPot(7, value);
      assert(ParameterEditor::info_ == &prefs_a);
    }
    for (unsigned value = 80; value <= 127; ++value) {
      ParameterEditor::OnPot(7, value);
      assert(ParameterEditor::info_ == &prefs_b);
    }
    assert(ParameterEditor::edit_mode_ == EDIT_IDLE);
    for (int value = 127; value >= 48; --value) {
      ParameterEditor::OnPot(7, value);
      assert(ParameterEditor::info_ == &prefs_b);
    }
    for (int value = 47; value >= 0; --value) {
      ParameterEditor::OnPot(7, value);
      assert(ParameterEditor::info_ == &prefs_a);
    }
    for (unsigned index = 8; index <= 255; ++index) {
      ParameterEditor::OnPot(index, 127);
      assert(ParameterEditor::info_ == &prefs_a);
    }
    assert(parameter_manager.lookups == lookups);
    assert(parameter_manager.writes == writes);
  }

  PageInfo knobs = {11, {240,241,242,243,244,245,246,247}, 11};
  ParameterEditor::OnInit(&knobs);
  for (unsigned id = 0; id < 256; ++id) {
    multi.knobs[0].parameter = id;
    assert(ParameterEditor::parameter_index(0) == (id < 79 ? id : 255));
  }

  // Exercise the actual diagnostic renderer at boundary values.
  for (uint16_t value : {0, 1, 9, 99, 270, 4096}) {
    free_sram = untouched_sram = value;
    display.clear();
    OsInfoPage::UpdateScreen();
    display.check();
    assert(std::memcmp(display.line_buffer(0), "RAM ", 4) == 0);
    assert(std::memcmp(display.line_buffer(0) + 10, "LOW ", 4) == 0);
    assert(std::memcmp(display.line_buffer(0) + 20, "MID ", 4) == 0);
    assert(std::memcmp(display.line_buffer(0) + 28, "RST ab", 6) == 0);
    assert(std::memcmp(display.line_buffer(1), "AUD ", 4) == 0);
    for (int i = 0; i < 80; ++i) assert(display.memory[i + 1] != 0);
  }
  // Clicking swaps to the firmware-update view and back, so a diagnostic build
  // can still flash voice cards.
  {
    OsInfoPage::OnInit(&prefs_a);
    assert(OsInfoPage::show_memory_ == 1);
    OsInfoPage::OnClick();
    assert(OsInfoPage::show_memory_ == 0);
    OsInfoPage::OnClick();
    assert(OsInfoPage::show_memory_ == 1);
  }

  // The diagnostic page polls one voice card per redraw, round-robin, and
  // renders every card's headroom in a 3-character field.
  {
    for (uint8_t i = 0; i < kNumVoices; ++i) voicecard_tx.polled[i] = 0;
    for (uint8_t reply : {uint8_t(0), uint8_t(40), uint8_t(128), kAudioStarved,
                          kAudioHeadroomUnsupported}) {
      voicecard_tx.reply = reply;
      // The MIDI queue peak shares line 0 and must not collide with RST.
      midi_dispatcher.peak = 64;
      for (int pass = 0; pass < kNumVoices; ++pass) {
        display.clear();
        OsInfoPage::UpdateScreen();
        display.check();
      }
      assert(std::memcmp(display.line_buffer(1), "AUD ", 4) == 0);
      // The click hint must not land on a card's cell.
      assert(std::memcmp(display.line_buffer(1) + 29, "clk:fw", 6) == 0);
      assert(std::memcmp(display.line_buffer(1) + 36, "exit", 4) == 0);
      assert(std::memcmp(display.line_buffer(0) + 24, " 64", 3) == 0);
      assert(std::memcmp(display.line_buffer(0) + 28, "RST ab", 6) == 0);
      // A card with no counter must read as "--", never as a number: 0xff is
      // what an older voice card leaves in SPDR, not a real measurement.
      const char* want = reply == kAudioHeadroomUnsupported ? " --"
                       : reply == kAudioStarved ? "254"
                       : reply == 128 ? "128" : (reply == 40 ? " 40" : "  0");
      for (uint8_t i = 0; i < kNumVoices; ++i) {
        assert(std::memcmp(display.line_buffer(1) + 4 + i * 4, want, 3) == 0);
      }
    }

    for (uint8_t i = 0; i < kNumVoices; ++i) assert(voicecard_tx.polled[i] == 5);

    // The stack context tag must name whichever path set the low watermark,
    // and must fall back to "idl" for an out-of-range value.
    for (uint8_t ctx : {uint8_t(STACK_CTX_IDLE), uint8_t(STACK_CTX_LOAD),
                        uint8_t(STACK_CTX_MIDI), uint8_t(STACK_CTX_LAST),
                        uint8_t(200)}) {
      stack_low_context = ctx;
      display.clear();
      OsInfoPage::UpdateScreen();
      display.check();
      const char* want = ctx == STACK_CTX_LOAD ? "lod"
                       : ctx == STACK_CTX_MIDI ? "mid" : "idl";
      assert(std::memcmp(display.line_buffer(0) + 36, want, 3) == 0);
    }
    stack_low_context = STACK_CTX_IDLE;

    voicecard_tx.reply = 40;
    display.clear();
    OsInfoPage::UpdateScreen();
    display.check();
  }

  for (uint8_t key = 0; key < 7; ++key) OsInfoPage::OnKey(key);
  assert(ui.previous == 0);
  OsInfoPage::OnKey(SWITCH_8);
  assert(ui.previous == 1);
  // Deferred library load: the timer that replaces a full patch load on every
  // encoder detent. Exercised directly because library.cc needs the SD card.
  {
    DeferredLoad timer;
    // Nothing is due until something is armed, whatever the clock says.
    for (uint16_t now : {uint16_t(0), uint16_t(1), uint16_t(40000), uint16_t(65535)}) {
      assert(!timer.armed());
      assert(!timer.Due(now, 0));
      assert(!timer.Due(now, 500));
    }

    timer.Arm(1000);
    assert(timer.armed());
    assert(!timer.Due(1000, 500));
    assert(!timer.Due(1499, 500));
    assert(timer.Due(1500, 500));   // exactly at the delay
    assert(timer.Due(9000, 500));   // and any time after
    assert(timer.Due(1000, 0));     // a zero delay is due at once

    // Re-arming while the user keeps scrolling pushes the deadline out.
    timer.Arm(1400);
    assert(!timer.Due(1500, 500));
    assert(timer.Due(1900, 500));

    timer.Disarm();
    assert(!timer.armed());
    assert(!timer.Due(9000, 500));

    // The 16-bit millisecond snapshot wraps every 65.536 s. An armed load must
    // survive the rollover rather than waiting most of a minute for the clock
    // to catch up, or firing instantly.
    timer.Arm(65500);
    assert(!timer.Due(65535, 500));           // 35 ms elapsed
    assert(!timer.Due(35, 500));              // 71 ms elapsed, past the wrap
    assert(timer.Due(64, 100));               // 100 ms elapsed exactly
    assert(timer.Due(500, 750) == 0);         // 536 ms elapsed, not yet
    assert(timer.Due(750, 750));              // 786 ms elapsed
  }

  // The voice assignment page (PAGE_MULTI). Its controls run 0..3 for the four
  // parameters and 4..9 for the six voice slots -- but PageInfo::data is only
  // eight bytes, so controls 8 and 9 have no entry and must never be used to
  // index it. This page is also the one Carey found showing voices 1/3/5 with
  // an unresponsive encoder.
  {
    PageInfo multi_page = {11, {58, 59, 60, 61, 255, 255, 255, 255}, 11};
    for (uint8_t i = 0; i < 6; ++i) multi.mappings[i].voice_allocation = 0;
    multi.mappings[0].voice_allocation = 0x15;  // the factory default: 1, 3, 5
    multi.mappings[1].voice_allocation = 0x2a;  // and 2, 4, 6

    VoiceAssigner::OnInit(&multi_page);
    assert(VoiceAssigner::active_control_ == 0);

    // Turning the encoder must walk every control, including 8 and 9, and must
    // render each one without running off the display or the page data.
    for (int step = 0; step < 9; ++step) {
      VoiceAssigner::OnIncrement(1);
      assert(VoiceAssigner::active_control_ == step + 1);
      display.clear();
      VoiceAssigner::UpdateScreen();
      display.check();
    }
    assert(VoiceAssigner::active_control_ == 9);

    // Clicking a voice slot toggles that voice for the active part.
    for (uint8_t slot = 0; slot < 6; ++slot) {
      VoiceAssigner::active_control_ = 4 + slot;
      uint8_t before = multi.mappings[0].voice_allocation;
      VoiceAssigner::OnClick();
      uint8_t after = multi.mappings[0].voice_allocation;
      assert(after != before);
      assert(((after ^ before) & (1 << slot)) != 0);
    }

    // And walking back down must leave the page at the bottom, not underflow.
    VoiceAssigner::active_control_ = 0;
    ui.last_page = 0;
    VoiceAssigner::OnIncrement(-1);
    assert(ui.last_page == multi_page.next_page);
  }

  std::puts("PASS: preferences navigation, synthetic/invalid IDs, pots, rendering, "
            "diagnostic screen and deferred load timing");
}
