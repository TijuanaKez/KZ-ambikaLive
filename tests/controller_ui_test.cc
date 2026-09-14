// Compile the actual page implementations against checked host substitutes.
// This validates navigation/rendering, not AVR peripherals or interrupt timing.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#define PSTR(s) (s)
#define strncpy_P std::strncpy
#define memcpy_P std::memcpy
#define IGNORE_UNUSED(x) (void)(x)
#define DIAGNOSTIC_BUILD

namespace ambika {
constexpr uint8_t kNumParameters = 77, kNumParametersPerPage = 8;
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
struct OsInfoPage : UiPage {
  static void OnInit(PageInfo*), UpdateScreen(), UpdateLeds();
  static uint8_t OnIncrement(int8_t), OnKey(uint8_t);
};

PageInfo prefs_a = {15, {66,67,71,72,68,69,70,0xf8}, 16};
PageInfo prefs_b = {16, {75,76,255,255,255,255,255,0xf9}, 15};
struct Ui {
  struct State {
    uint8_t values[8] = {};
    uint8_t* bytes() { return values; }
    uint8_t active_env_lfo() { return 0; }
  } state_;
  int previous = 0;
  uint8_t active_part() { return 0; }
  State& state() { return state_; }
  void ShowPage(uint8_t page) {
    assert(page == 15 || page == 16);
    ParameterEditor::OnInit(page == 15 ? &prefs_a : &prefs_b);
  }
  void ShowPageRelative(int8_t increment) {
    ShowPage(UiPage::info_->next_page);
    ParameterEditor::SetActiveControl(increment > 0 ? ACTIVE_CONTROL_FIRST : ACTIVE_CONTROL_LAST);
  }
  void ShowPreviousPage() { ++previous; }
} ui;
struct Multi {
  struct Knob { uint8_t parameter = 0, part = 0, instance = 0; } knobs[8];
  Multi& data() { return *this; }
  Knob& knobAssignment(uint8_t i) { assert(i < 8); return knobs[i]; }
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
struct Display {
  char memory[83];
  void clear() {
    std::memset(memory, ' ', sizeof(memory));
    memory[0] = 'L'; memory[82] = 'R';
  }
  char* line_buffer(uint8_t line) { assert(line < 2); return memory + 1 + 40 * line; }
  void check() { assert(memory[0] == 'L' && memory[82] == 'R'); }
} display;
struct Leds { void set_pixel(uint8_t, uint8_t) {} } leds;
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
#include "controller/ui_pages/os_info_page.cc"

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
  ParameterEditor::OnIncrement(1);
  ParameterEditor::OnIncrement(1);
  assert(ParameterEditor::active_control_ == 7);  // Skip five unused cells.
  ParameterEditor::OnIncrement(-1);
  assert(ParameterEditor::active_control_ == 1);
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
    assert(ParameterEditor::parameter_index(0) == (id < 77 ? id : 255));
  }

  // Exercise the actual diagnostic renderer at boundary values.
  for (uint16_t value : {0, 1, 9, 99, 270, 4096}) {
    free_sram = untouched_sram = value;
    display.clear();
    OsInfoPage::UpdateScreen();
    display.check();
    assert(std::memcmp(display.line_buffer(0), "KZ DIAG3", 8) == 0);
    assert(std::memcmp(display.line_buffer(1), "RST ab", 6) == 0);
    for (int i = 0; i < 80; ++i) assert(display.memory[i + 1] != 0);
  }
  for (uint8_t key = 0; key < 7; ++key) OsInfoPage::OnKey(key);
  assert(ui.previous == 0);
  OsInfoPage::OnKey(SWITCH_8);
  assert(ui.previous == 1);
  std::puts("PASS: preferences navigation, synthetic/invalid IDs, pots, rendering and diagnostic screen");
}
