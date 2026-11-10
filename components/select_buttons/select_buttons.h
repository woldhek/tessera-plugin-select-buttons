#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::select_buttons {

// The plugin: an ESPHome component and a tessera::Plugin. It registers its tile type in setup().
class SelectButtons : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
};

// One option of the entity as the tile keeps it: its words, and whether they came whole (an attribute's text is cut at
// 48 bytes on its way to the screen, and select_option needs the option exactly as Home Assistant has it).
struct Choice {
  char text[52];
  bool whole;
};

// The tile: a key for each option of an input_select or a select, the current one lit. A tap chooses an option; with
// `confirm` the first tap only asks "Tap again". What Home Assistant says is always what the keys show: a change made
// elsewhere (an automation, the HA app, a remote) comes as new data and lights its key.
class ButtonsTile : public tessera::Tile {
 public:
  static constexpr int MAX_KEYS = 6;

  explicit ButtonsTile(const tessera::Plugin *plugin) : plugin_(plugin) {}
  void create(const tessera::TileContext &c) override;
  void on_state(JsonObjectConst data) override;
  void on_tick(uint32_t epoch) override;
  void on_theme() override;
  void on_tap() override;

 private:
  struct Key {
    lv_obj_t *block{};
    lv_obj_t *icon{};   // only when an icon was chosen for it
    lv_obj_t *label{};
    int x = 0, y = 0, w = 0, h = 0;
    uint32_t codepoint = 0;
  };

  void layout();
  void paint();
  void paint_key(int i);
  int key_at(int x, int y) const;
  void choose(int i);

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  bool show_name_ = true, show_labels_ = true, confirm_ = false;
  tessera::Text entity_, tile_name_, entity_name_;

  tessera::List<Choice> choices_;   // what Home Assistant offers, at most MAX_KEYS of them
  char state_[52] = "";             // the current option as Home Assistant has it
  bool available_ = false, waiting_ = true, wrong_ = false;

  int pending_ = -1;          // the key a tap sent, until Home Assistant has it (or PENDING_WAIT passed)
  uint32_t pending_at_ = 0;
  int armed_ = -1;            // with confirm: the key that asks "Tap again"
  uint32_t armed_at_ = 0;

  int shown_ = -1;            // how many keys the layout is for (-1: none yet)
  int top_ = 0;               // where the keys start, under the name
  Key keys_[MAX_KEYS];
  lv_obj_t *title_{}, *note_{};
};

}  // namespace esphome::select_buttons
