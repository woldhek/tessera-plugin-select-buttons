#include "select_buttons.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome::select_buttons {

namespace ui = tessera::ui;
using tessera::Font;
using tessera::Tone;

static const char *const TAG = "select_buttons";

// A key the finger chose stays ringed this long while Home Assistant has not answered; then the keys show what Home
// Assistant says again. "Tap again" waits this long for its second tap.
static constexpr uint32_t PENDING_MS = 8000, ARMED_MS = 4000;
// An option of this many bytes or more may have been cut on its way (attributes come in texts of 48 bytes at most):
// its key shows, but it cannot choose it.
static constexpr size_t CUT_AT = 46;

// The icons a person can give a key (the manifest's choices), from Tessera's icon set.
struct NamedIcon {
  const char *name;
  uint32_t codepoint;
};
static constexpr NamedIcon ICONS[] = {
    {"gamepad-variant", 0xF0297}, {"monitor", 0xF0379},      {"laptop", 0xF0322},        {"desk", 0xF1239},
    {"briefcase", 0xF00D6},       {"home", 0xF02DC},         {"television", 0xF0502},    {"projector", 0xF042E},
    {"cast", 0xF0118},            {"audio-video", 0xF093D},  {"remote-tv", 0xF0EC5},     {"speaker", 0xF04C3},
    {"headphones", 0xF02CB},      {"microphone", 0xF036C},   {"music", 0xF075A},         {"radio", 0xF0439},
    {"record-player", 0xF099A},   {"movie-open", 0xF0FCE},   {"account", 0xF0004},       {"account-group", 0xF0849},
    {"router-wireless", 0xF0469}, {"power", 0xF0425},        {"play", 0xF040A},          {"cog", 0xF0493},
};

static uint32_t icon_of(const char *name) {
  if (!name) return 0;
  for (const auto &i : ICONS)
    if (std::strcmp(i.name, name) == 0) return i.codepoint;
  return 0;
}

static void copy(char *to, size_t size, const char *from) { std::snprintf(to, size, "%s", from ? from : ""); }

static int gap() { return ui::px(ui::large() ? 8 : 5); }
static int pad() { return ui::px(ui::large() ? 8 : 5); }

// ---- create: every part once, hidden until the entity says how many keys there are ----

void ButtonsTile::create(const tessera::TileContext &c) {
  width_ = c.width;
  height_ = c.height;
  entity_ = roomy::text(std::string(c.entity ? c.entity : ""));
  tile_name_ = roomy::text(std::string(c.name ? c.name : ""));
  show_name_ = c.options["show_name"] | true;
  show_labels_ = c.options["show_labels"] | true;
  confirm_ = c.options["confirm"] | false;

  title_ = ui::label(c.parent, Font::BODY, theme::MUTED);
  lv_obj_set_width(title_, width_);
  lv_obj_add_flag(title_, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < MAX_KEYS; ++i) {
    Key &k = keys_[i];
    char id[8];
    std::snprintf(id, sizeof(id), "icon_%d", i + 1);
    k.codepoint = icon_of(c.options[id] | "none");
    k.block = ui::block(c.parent, theme::BUTTON);
    lv_obj_set_style_border_color(k.block, ui::color(theme::ACCENT), 0);
    lv_obj_set_style_border_width(k.block, 0, 0);
    if (k.codepoint) {
      k.icon = ui::label(k.block, Font::ICON, theme::INK);
      lv_label_set_long_mode(k.icon, LV_LABEL_LONG_MODE_CLIP);
      lv_obj_set_style_text_align(k.icon, LV_TEXT_ALIGN_CENTER, 0);
      ui::set_text(k.icon, ui::icon(k.codepoint));
    }
    k.label = ui::label(k.block, Font::BODY, theme::INK);
    lv_obj_set_style_text_align(k.label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(k.block, LV_OBJ_FLAG_HIDDEN);
  }

  note_ = ui::text(c.parent, Font::BODY, theme::MUTED);
  lv_obj_set_size(note_, width_, std::min(height_, 2 * ui::line_height(Font::BODY)));
  lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(note_, LV_ALIGN_CENTER, 0, 0);
}

// ---- on_state: the entity as Home Assistant has it, again at every change (also one made elsewhere) ----

void ButtonsTile::on_state(JsonObjectConst data) {
  const char *wait = data["wait"] | "";
  wrong_ = std::strcmp(wait, "wrong_entity") == 0;
  const char *state = data["state"] | "";
  waiting_ = !data["state"].is<const char *>();

  const char *name = data["name"] | "";
  if (*name) entity_name_ = roomy::text(std::string(name));

  const bool changed = std::strcmp(state, state_) != 0;
  copy(state_, sizeof(state_), state);
  available_ = !waiting_ && std::strcmp(state_, "unavailable") != 0;

  const int before = static_cast<int>(choices_.size());
  choices_.clear();
  for (JsonVariantConst o : data["attributes"]["options"].as<JsonArrayConst>()) {
    const char *text = o | "";
    if (!*text) continue;
    if (static_cast<int>(choices_.size()) >= MAX_KEYS) {
      ESP_LOGD(TAG, "%s has more than %d options: the tile shows the first %d", entity_.c_str(), MAX_KEYS, MAX_KEYS);
      break;
    }
    Choice ch;
    copy(ch.text, sizeof(ch.text), text);
    ch.whole = std::strlen(text) < CUT_AT;
    choices_.push_back(ch);
  }

  // What Home Assistant says wins: the option a tap sent has come, or something else changed the entity meanwhile.
  if (pending_ >= 0 && (changed || pending_ >= static_cast<int>(choices_.size()) ||
                        std::strcmp(choices_[pending_].text, state_) == 0))
    pending_ = -1;
  if (armed_ >= 0 && (changed || armed_ >= static_cast<int>(choices_.size()))) armed_ = -1;

  if (static_cast<int>(choices_.size()) != before || shown_ < 0) layout();
  paint();
}

// ---- layout: the grid of keys that fits the tile best ----

void ButtonsTile::layout() {
  const int n = static_cast<int>(choices_.size());
  shown_ = n;
  const int g = gap(), least = ui::mm(7), line = ui::line_height(Font::BODY);

  // The name over the keys, when it is on and leaves every key at least 7 mm.
  const std::string title = roomy::plain(tile_name_.empty() ? entity_name_ : tile_name_);
  int top = 0;
  if (show_name_ && !title.empty() && n > 0) top = line + g;

  // Every grid of `cols` columns; the best has the largest of min(width, 2 x height): keys about twice as wide as high,
  // so a wide tile makes a row, a tall one a stack and a square one a grid.
  int best_cols = 1, best_score = -1;
  bool best_fits = false;
  for (int attempt = 0; attempt < 2 && n > 0; ++attempt) {
    const int room = height_ - top;
    for (int cols = 1; cols <= n; ++cols) {
      const int rows = (n + cols - 1) / cols;
      const int w = (width_ - (cols - 1) * g) / cols, h = (room - (rows - 1) * g) / rows;
      const bool fits = w >= least && h >= least;
      const int score = std::min(w, 2 * h);
      if ((fits && !best_fits) || (fits == best_fits && score > best_score)) {
        best_cols = cols;
        best_score = score;
        best_fits = fits;
      }
    }
    if (best_fits || top == 0) break;
    top = 0;  // the name takes too much: the keys first
    best_score = -1;
  }
  top_ = top;

  if (top > 0) {
    ui::set_text(title_, title);
    lv_obj_set_pos(title_, 0, 0);
    lv_obj_remove_flag(title_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(title_, LV_OBJ_FLAG_HIDDEN);
  }

  const int cols = best_cols, rows = n > 0 ? (n + cols - 1) / cols : 1;
  const int w = (width_ - (cols - 1) * g) / cols, h = (height_ - top - (rows - 1) * g) / rows;
  const int p = w < ui::mm(12) ? ui::px(3) : pad();

  // Every key has the same size, so one look for all of them: the largest words in which the longest option (or "Tap
  // again") fits, and the largest icon that fits over them; an icon beside the words only where over them does not fit.
  int longest_large = ui::text_width(plugin_->text("tap_again"), Font::BODY_LARGE);
  for (const Choice &ch : choices_) longest_large = std::max(longest_large, ui::text_width(ch.text, Font::BODY_LARGE));
  const Font words = ui::large() && longest_large <= w - 2 * p && h >= ui::line_height(Font::BODY_LARGE) + 2 * p
                         ? Font::BODY_LARGE
                         : Font::BODY;
  const int lw = ui::line_height(words);
  bool any_icon = false;
  for (int i = 0; i < n; ++i) any_icon = any_icon || keys_[i].icon != nullptr;
  enum { WORDS, OVER, BESIDE, ICON_ONLY } look = WORDS;
  Font icon_font = Font::ICON;
  if (any_icon) {
    if (!show_labels_) {
      look = ICON_ONLY;
      if (std::min(w, h) < ui::line_height(Font::ICON) + p) icon_font = Font::ICON_SMALL;
    } else if (h >= ui::line_height(Font::ICON) + lw + 2 * p) {
      look = OVER;
    } else if (h >= ui::line_height(Font::ICON_SMALL) + lw + p) {
      look = OVER;
      icon_font = Font::ICON_SMALL;
    } else {
      look = BESIDE;
      icon_font = Font::ICON_SMALL;
    }
  }
  const int li = ui::line_height(icon_font);

  for (int i = 0; i < MAX_KEYS; ++i) {
    Key &k = keys_[i];
    if (i >= n) {
      lv_obj_add_flag(k.block, LV_OBJ_FLAG_HIDDEN);
      k.w = k.h = 0;
      continue;
    }
    // The last row's keys stand in the middle when it is not full.
    const int row = i / cols, col = i % cols;
    const int in_row = row == rows - 1 ? n - row * cols : cols;
    const int shift = (cols - in_row) * (w + g) / 2;
    k.x = shift + col * (w + g);
    k.y = top + row * (h + g);
    k.w = w;
    k.h = h;
    lv_obj_set_pos(k.block, k.x, k.y);
    lv_obj_set_size(k.block, w, h);
    lv_obj_remove_flag(k.block, LV_OBJ_FLAG_HIDDEN);

    // Inside the key: the icon over the words, beside them, alone, or the words alone (a key without an icon always
    // has its words, also with the option names off: an empty key says nothing).
    ui::set_font(k.label, words);
    const bool icon = k.icon != nullptr;
    const auto mode = icon ? look : WORDS;
    if (icon) {
      ui::set_font(k.icon, icon_font);
      lv_obj_set_width(k.icon, std::min(w, li + p));
    }
    const int iw = icon ? std::min(w, li + p) : 0;
    switch (mode) {
      case OVER: {
        const int y0 = (h - li - lw) / 2;
        lv_obj_set_pos(k.icon, (w - iw) / 2, y0);
        lv_obj_set_width(k.label, std::max(1, w - 2 * p));
        lv_obj_set_pos(k.label, p, y0 + li);
        break;
      }
      case BESIDE:
        lv_obj_set_pos(k.icon, p, (h - li) / 2);
        lv_obj_set_width(k.label, std::max(1, w - iw - 2 * p));
        lv_obj_set_pos(k.label, iw + p, (h - lw) / 2);
        break;
      case ICON_ONLY:
        lv_obj_set_pos(k.icon, (w - iw) / 2, (h - li) / 2);
        break;
      case WORDS:
        lv_obj_set_width(k.label, std::max(1, w - 2 * p));
        lv_obj_set_pos(k.label, p, (h - lw) / 2);
        break;
    }
    if (mode == ICON_ONLY) lv_obj_add_flag(k.label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(k.label, LV_OBJ_FLAG_HIDDEN);
  }
}

// ---- paint: what each key shows now; ui::set_* change a property only when it differs ----

void ButtonsTile::paint() {
  const char *note = nullptr;
  if (waiting_ || wrong_ || entity_.empty()) note = "choose_entity";
  else if (choices_.empty()) note = available_ ? "no_options" : "unavailable";
  if (note) {
    for (auto &k : keys_) lv_obj_add_flag(k.block, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(title_, LV_OBJ_FLAG_HIDDEN);
    ui::set_text(note_, plugin_->text(note));
    lv_obj_remove_flag(note_, LV_OBJ_FLAG_HIDDEN);
    shown_ = -1;  // the keys come back with a layout once there is something to show
    return;
  }
  lv_obj_add_flag(note_, LV_OBJ_FLAG_HIDDEN);
  if (shown_ != static_cast<int>(choices_.size())) layout();
  for (int i = 0; i < static_cast<int>(choices_.size()) && i < MAX_KEYS; ++i) paint_key(i);
}

void ButtonsTile::paint_key(int i) {
  Key &k = keys_[i];
  const Choice &ch = choices_[i];
  const bool active = std::strcmp(ch.text, state_) == 0;
  const bool usable = available_ && ch.whole;
  const bool ringed = i == pending_ || i == armed_;

  ui::set_fill(k.block, active ? Tone::ACCENT : Tone::NORMAL, active ? theme::ACCENT : theme::BUTTON);
  const theme::Role ink = active ? theme::ON_ACCENT : usable ? theme::INK : theme::MUTED;
  ui::set_color(k.label, ink);
  if (k.icon) ui::set_color(k.icon, ink);
  ui::set_text(k.label, i == armed_ ? std::string(plugin_->text("tap_again")) : std::string(ch.text));

  const int ring = ringed ? std::max(1, ui::px(3)) : 0;
  if (lv_obj_get_style_border_width(k.block, LV_PART_MAIN) != ring) lv_obj_set_style_border_width(k.block, ring, 0);
}

// ---- on_tick: a ring nobody answered, a "Tap again" nobody tapped, go after a while ----

void ButtonsTile::on_tick(uint32_t epoch) {
  (void) epoch;
  const uint32_t now = millis();
  if (pending_ >= 0 && now - pending_at_ > PENDING_MS) pending_ = -1;
  if (armed_ >= 0 && now - armed_at_ > ARMED_MS) armed_ = -1;
  paint();
}

void ButtonsTile::on_theme() {
  for (auto &k : keys_) lv_obj_set_style_border_color(k.block, ui::color(theme::ACCENT), 0);
  ui::set_color(title_, theme::MUTED);
  ui::set_color(note_, theme::MUTED);
  if (!choices_.empty()) paint();
}

// ---- taps ----

int ButtonsTile::key_at(int x, int y) const {
  if (x < 0 || y < 0) return -1;
  const int half = gap() / 2;  // a tap in the gap between two keys goes to the nearer one
  for (int i = 0; i < static_cast<int>(choices_.size()) && i < MAX_KEYS; ++i) {
    const Key &k = keys_[i];
    if (x >= k.x - half && x < k.x + k.w + half && y >= k.y - half && y < k.y + k.h + half) return i;
  }
  return -1;
}

void ButtonsTile::on_tap() {
  if (!available_ || choices_.empty()) return;
  const tessera::Point p = tessera::tap_point();
  const int i = key_at(p.x, p.y);
  if (i < 0 || !choices_[i].whole) return;
  const bool active = std::strcmp(choices_[i].text, state_) == 0;
  if (active && pending_ < 0) {
    armed_ = -1;  // the current option: nothing to switch
    paint();
    return;
  }
  if (confirm_ && armed_ != i) {
    armed_ = i;
    armed_at_ = millis();
    paint();
    return;
  }
  armed_ = -1;
  choose(i);
  paint();
}

void ButtonsTile::choose(int i) {
  const std::string entity = roomy::plain(entity_);
  const char *service = nullptr;
  if (entity.rfind("input_select.", 0) == 0) service = "input_select.select_option";
  else if (entity.rfind("select.", 0) == 0) service = "select.select_option";
  if (!service) return;
  JsonDocument fields;
  fields["option"] = choices_[i].text;
  if (tessera::action(service, entity, fields.as<JsonVariantConst>())) {
    pending_ = i;
    pending_at_ = millis();
  } else {
    ESP_LOGW(TAG, "%s on %s was not sent", service, entity.c_str());
  }
}

void SelectButtons::setup() {
  // The id is the tile's id in the manifest; the core makes a ButtonsTile for every card that shows one.
  add_tile("buttons", [this]() { return new ButtonsTile(this); });
}

}  // namespace esphome::select_buttons
