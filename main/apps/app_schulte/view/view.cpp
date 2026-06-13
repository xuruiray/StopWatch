/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <tools/random/random.hpp>

using namespace view;
using namespace uitk;
using namespace uitk::lvgl_cpp;
using smooth_ui_toolkit::Random;

namespace {

void schulte_board_draw_event_cb(lv_event_t* e);
void schulte_flash_draw_event_cb(lv_event_t* e);
void schulte_transition_draw_event_cb(lv_event_t* e);

constexpr int _panel_size = 466;
constexpr int _center     = 233;
constexpr int _outer_radius = 233;
constexpr float _radius_scale = static_cast<float>(_outer_radius) / 215.0f;

constexpr uint32_t _screen_bg_color       = 0xFFFFF9;
constexpr uint32_t _board_backdrop_color  = 0xFFFFF8;
constexpr uint32_t _sector_fill_color     = 0xFFFFF9;
constexpr uint32_t _grid_color            = 0xB9CABE;
constexpr uint32_t _grid_strong_color     = 0x9FB6AA;
constexpr uint32_t _hit_fill_color        = 0xD2EADF;
constexpr uint32_t _hit_stroke_color      = 0x0F8577;
constexpr uint32_t _miss_fill_color       = 0xFBE0DA;
constexpr uint32_t _miss_stroke_color     = 0xDF4E44;
constexpr uint32_t _result_bg_color       = 0xFFFFF7;
constexpr uint32_t _result_border_color   = 0x91AB9A;
constexpr uint32_t _result_text_color     = 0x17201B;
constexpr uint32_t _number_stroke_color   = 0xFFFFF9;
constexpr uint32_t _density_hint_color    = 0x58605B;
constexpr lv_opa_t _density_hint_opa      = 107;
constexpr uint16_t _hit_vibrate_duration_ms  = 18;
constexpr uint16_t _miss_vibrate_duration_ms = 35;
constexpr uint8_t _hit_vibrate_strength      = 60;
constexpr uint8_t _miss_vibrate_strength     = 88;
constexpr uint32_t _transition_duration_ms   = 180;
constexpr int _transition_steps              = 12;
constexpr int _transition_cover_radius       = 330;
constexpr int _transition_ring_start_radius  = 18;

constexpr std::array<int, 3> _density_cycle = {25, 18, 30};

constexpr std::array<SchulteView::Ring, 3> _layout_18 = {{
    {0.0f, 72.0f * _radius_scale, 4, -135.0f},
    {72.0f * _radius_scale, 144.0f * _radius_scale, 6, -120.0f},
    {144.0f * _radius_scale, static_cast<float>(_outer_radius), 8, -112.5f},
}};

constexpr std::array<SchulteView::Ring, 3> _layout_25 = {{
    {0.0f, 78.0f * _radius_scale, 6, -120.0f},
    {78.0f * _radius_scale, 145.0f * _radius_scale, 8, -112.5f},
    {145.0f * _radius_scale, static_cast<float>(_outer_radius), 11, -106.36f},
}};

constexpr std::array<SchulteView::Ring, 3> _layout_30 = {{
    {0.0f, 72.0f * _radius_scale, 6, -120.0f},
    {72.0f * _radius_scale, 144.0f * _radius_scale, 10, -108.0f},
    {144.0f * _radius_scale, static_cast<float>(_outer_radius), 14, -102.857142f},
}};

constexpr std::array<uint32_t, 7> _number_color_bases = {
    0xD64036,
    0x1F64B5,
    0x00856F,
    0x9B4D16,
    0x5C4BB2,
    0x14191B,
    0xB07800,
};

constexpr float _pi = 3.14159265358979323846f;

uint8_t color_channel(uint32_t color, int shift)
{
    return static_cast<uint8_t>((color >> shift) & 0xFF);
}

uint32_t make_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

int clamp_channel(int value)
{
    return std::min(220, std::max(0, value));
}

uint32_t flash_fill_color(SchulteView::FlashType type)
{
    if (type == SchulteView::FlashType::Hit) {
        return _hit_fill_color;
    }
    if (type == SchulteView::FlashType::Miss) {
        return _miss_fill_color;
    }
    return _sector_fill_color;
}

uint32_t flash_stroke_color(SchulteView::FlashType type)
{
    if (type == SchulteView::FlashType::Hit) {
        return _hit_stroke_color;
    }
    if (type == SchulteView::FlashType::Miss) {
        return _miss_stroke_color;
    }
    return _grid_color;
}

bool tick_reached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

float sector_centroid_radius(float inner, float outer, float stepDegrees)
{
    const float theta = stepDegrees * _pi / 180.0f;
    if (theta <= 0.0f || outer <= 0.0f) {
        return (inner + outer) * 0.5f;
    }

    const float angular_factor = std::sin(theta * 0.5f) / (theta * 0.5f);
    if (inner <= 0.0f) {
        return (2.0f * outer / 3.0f) * angular_factor;
    }

    const float denominator = outer * outer - inner * inner;
    if (denominator <= 0.0f) {
        return (inner + outer) * 0.5f;
    }

    return (2.0f / 3.0f) * ((outer * outer * outer - inner * inner * inner) / denominator) * angular_factor;
}

std::vector<int16_t>& schulte_hit_buffer()
{
    static std::vector<int16_t> buffer;
    return buffer;
}

std::vector<int16_t>& schulte_miss_buffer()
{
    static std::vector<int16_t> buffer;
    return buffer;
}

int& schulte_hit_sample_rate()
{
    static int sample_rate = 0;
    return sample_rate;
}

int& schulte_miss_sample_rate()
{
    static int sample_rate = 0;
    return sample_rate;
}

void prepare_schulte_feedback_clip(SchulteView::FlashType type)
{
    const int sample_rate = GetHAL().getAudioSampleRate();
    auto& buffer = type == SchulteView::FlashType::Hit ? schulte_hit_buffer() : schulte_miss_buffer();
    auto& cached_rate = type == SchulteView::FlashType::Hit ? schulte_hit_sample_rate() : schulte_miss_sample_rate();

    if (!buffer.empty() && cached_rate == sample_rate) {
        return;
    }

    const bool is_hit = type == SchulteView::FlashType::Hit;
    const float duration_sec = is_hit ? 0.030f : 0.048f;
    const int sample_count = std::max(1, static_cast<int>(static_cast<float>(sample_rate) * duration_sec));
    const int fade_samples = std::max(1, static_cast<int>(static_cast<float>(sample_rate) * 0.0025f));
    const float target_peak_amp = is_hit ? 15500.0f : 18500.0f;

    buffer.assign(sample_count, 0);
    cached_rate = sample_rate;

    auto render_pass = [&](std::vector<int16_t>* out, float scale) {
        uint32_t noise_state = is_hit ? 0x4d2a17cbu : 0xb5319e75u;
        float previous_noise = 0.0f;
        float filtered_noise = 0.0f;
        float max_abs = 0.0f;

        for (int i = 0; i < sample_count; ++i) {
            noise_state = noise_state * 1664525u + 1013904223u;
            const float noise = (static_cast<float>((noise_state >> 8) & 0xffffu) / 32767.5f) - 1.0f;
            const float highpass = 0.70f * (filtered_noise + noise - previous_noise);
            const float lowpass = filtered_noise * 0.82f + noise * 0.18f;
            previous_noise = noise;
            filtered_noise = is_hit ? highpass : lowpass;

            const float t = static_cast<float>(i) / static_cast<float>(sample_rate);
            const float progress = static_cast<float>(i) / static_cast<float>(std::max(1, sample_count - 1));
            const float attack = std::min(1.0f, t / (is_hit ? 0.00035f : 0.0012f));
            float sample = 0.0f;

            if (is_hit) {
                const float envelope = attack * std::exp(-progress * 18.0f);
                sample += std::sin(2.0f * _pi * 1850.0f * t) * 0.62f * envelope;
                sample += std::sin(2.0f * _pi * 3550.0f * t + 0.7f) * 0.24f * envelope;
                sample += filtered_noise * 0.35f * envelope;
            } else {
                sample += std::sin(2.0f * _pi * 170.0f * t) * 0.82f * attack * std::exp(-progress * 8.0f);
                sample += std::sin(2.0f * _pi * 92.0f * t + 0.4f) * 0.40f * attack * std::exp(-progress * 5.4f);
                sample += filtered_noise * 0.22f * attack * std::exp(-progress * 11.0f);
            }

            if (i >= sample_count - fade_samples) {
                sample *= static_cast<float>(sample_count - i - 1) / static_cast<float>(fade_samples);
            }

            max_abs = std::max(max_abs, std::fabs(sample));
            if (out != nullptr) {
                (*out)[i] = static_cast<int16_t>(std::clamp(sample * scale, -32767.0f, 32767.0f));
            }
        }

        return max_abs;
    };

    const float max_abs = render_pass(nullptr, 1.0f);
    const float scale = max_abs > 0.0f ? target_peak_amp / max_abs : 1.0f;
    render_pass(&buffer, scale);
}

void play_schulte_feedback_clip(SchulteView::FlashType type)
{
    if (GetHAL().getSpeakerVolume() <= 0) {
        return;
    }

    prepare_schulte_feedback_clip(type);
    auto& buffer = type == SchulteView::FlashType::Hit ? schulte_hit_buffer() : schulte_miss_buffer();
    GetHAL().audioPlay(buffer, true);
}

}  // namespace

void SchulteView::init(lv_obj_t* parent)
{
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(_screen_bg_color), LV_PART_MAIN);

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_hex(_screen_bg_color));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _board = std::make_unique<Container>(_panel->get());
    _board->align(LV_ALIGN_CENTER, 0, 0);
    _board->setSize(_panel_size, _panel_size);
    _board->setRadius(LV_RADIUS_CIRCLE);
    _board->setBorderWidth(0);
    _board->setPaddingAll(0);
    _board->setBgOpa(LV_OPA_TRANSP);
    _board->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _board->removeFlag(LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_board->get(), schulte_board_draw_event_cb, LV_EVENT_ALL, this);
    lv_obj_refresh_ext_draw_size(_board->get());

    _flash_overlay = std::make_unique<Container>(_panel->get());
    _flash_overlay->setPos(0, 0);
    _flash_overlay->setSize(1, 1);
    _flash_overlay->setRadius(0);
    _flash_overlay->setBorderWidth(0);
    _flash_overlay->setPaddingAll(0);
    _flash_overlay->setBgOpa(LV_OPA_TRANSP);
    _flash_overlay->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _flash_overlay->removeFlag(LV_OBJ_FLAG_CLICKABLE);
    _flash_overlay->addFlag(LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(_flash_overlay->get(), schulte_flash_draw_event_cb, LV_EVENT_ALL, this);
    lv_obj_refresh_ext_draw_size(_flash_overlay->get());

    createLabelPool();

    _density_hint_label = std::make_unique<Label>(_panel->get());
    _density_hint_label->setSize(240, 98);
    _density_hint_label->align(LV_ALIGN_CENTER, 0, 3);
    _density_hint_label->setText("");
    _density_hint_label->setTextFont(&CommissionerMedium108);
    _density_hint_label->setTextColor(lv_color_hex(_density_hint_color));
    _density_hint_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(_density_hint_label->get(), LV_LABEL_LONG_CLIP);
    lv_obj_set_style_bg_opa(_density_hint_label->get(), LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_opa(_density_hint_label->get(), _density_hint_opa, LV_PART_MAIN);
    lv_obj_set_style_text_outline_stroke_color(_density_hint_label->get(), lv_color_hex(_density_hint_color), LV_PART_MAIN);
    lv_obj_set_style_text_outline_stroke_width(_density_hint_label->get(), 1, LV_PART_MAIN);
    lv_obj_set_style_text_outline_stroke_opa(_density_hint_label->get(), _density_hint_opa, LV_PART_MAIN);
    lv_obj_remove_flag(_density_hint_label->get(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_density_hint_label->get(), LV_OBJ_FLAG_HIDDEN);

    _result_panel = std::make_unique<Container>(_panel->get());
    _result_panel->align(LV_ALIGN_CENTER, 0, 0);
    _result_panel->setSize(230, 72);
    _result_panel->setRadius(14);
    _result_panel->setBorderWidth(1);
    _result_panel->setBorderColor(lv_color_hex(_result_border_color));
    _result_panel->setPaddingAll(0);
    _result_panel->setBgColor(lv_color_hex(_result_bg_color));
    _result_panel->setBgOpa(LV_OPA_COVER);
    _result_panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _result_panel->removeFlag(LV_OBJ_FLAG_CLICKABLE);

    _result_label = std::make_unique<Label>(_result_panel->get());
    _result_label->setSize(224, 58);
    _result_label->align(LV_ALIGN_CENTER, 0, 0);
    _result_label->setText("");
    _result_label->setTextFont(&lv_font_maple_mono_medium_48);
    _result_label->setTextColor(lv_color_hex(_result_text_color));
    _result_label->setTextAlign(LV_TEXT_ALIGN_CENTER);

    _transition_overlay = std::make_unique<Container>(_panel->get());
    _transition_overlay->setPos(0, 0);
    _transition_overlay->setSize(_panel_size, _panel_size);
    _transition_overlay->setRadius(0);
    _transition_overlay->setBorderWidth(0);
    _transition_overlay->setPaddingAll(0);
    _transition_overlay->setBgOpa(LV_OPA_TRANSP);
    _transition_overlay->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _transition_overlay->removeFlag(LV_OBJ_FLAG_CLICKABLE);
    _transition_overlay->addFlag(LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(_transition_overlay->get(), schulte_transition_draw_event_cb, LV_EVENT_ALL, this);
    lv_obj_refresh_ext_draw_size(_transition_overlay->get());

    showIdleBoard(false);
    prepare_schulte_feedback_clip(FlashType::Hit);
    prepare_schulte_feedback_clip(FlashType::Miss);
}

void SchulteView::update()
{
    const uint32_t now = GetHAL().millis();
    updateTouch(now);
    updateFlash(now);
    updateTransition(now);
}

void SchulteView::handleLeftKey()
{
    if (_state == State::Idle) {
        startGame();
    } else if (_state == State::Running || _state == State::Result) {
        showIdleBoard(true);
    }
}

void SchulteView::handleRightKey()
{
    switchDensity();
}

void SchulteView::showIdleBoard(bool animate)
{
    _state = State::Idle;
    _target = 1;
    _stop_time = 0;
    _game_colors.clear();
    _ring_phase_offsets = {0.0f, 0.0f, 0.0f};
    clearFlash();
    renderBoard(false);
    updateResultVisibility();
    if (animate) {
        startTransition();
    }
}

void SchulteView::switchDensity()
{
    if (_state != State::Idle) {
        return;
    }

    _density_index = (_density_index + 1) % static_cast<int>(_density_cycle.size());
    renderBoard(false);
    updateResultVisibility();
    startTransition();
}

void SchulteView::startGame()
{
    if (_state != State::Idle) {
        return;
    }

    _target = 1;
    _stop_time = 0;
    _state = State::Running;
    createGameColors();
    createRingPhaseOffsets();
    clearFlash();
    renderBoard(true);
    updateResultVisibility();
    _start_time = GetHAL().millis();
    startTransition();
}

void SchulteView::finishGame()
{
    _stop_time = GetHAL().millis() - _start_time;
    _state = State::Result;
    if (_result_label) {
        _result_label->setText(formatTime(_stop_time));
    }
    stopTransition();
    updateResultVisibility();
}

void SchulteView::renderBoard(bool showNumbers)
{
    _cells.clear();

    std::vector<int> numbers;
    if (showNumbers) {
        numbers.reserve(currentDensity());
        for (int value = 1; value <= currentDensity(); ++value) {
            numbers.push_back(value);
        }
        shuffleNumbers(numbers);
    }

    int index = 0;
    const auto& layout = currentLayout();
    for (int ring_index = 0; ring_index < static_cast<int>(layout.size()); ++ring_index) {
        const Ring& ring = layout[ring_index];
        for (int local_index = 0; local_index < ring.count; ++local_index) {
            Cell cell;
            cell.ring_index = ring_index;
            cell.local_index = local_index;
            cell.value = showNumbers ? numbers[index] : 0;
            cell.color = showNumbers && index < static_cast<int>(_game_colors.size())
                             ? _game_colors[index]
                             : _number_color_bases[index % static_cast<int>(_number_color_bases.size())];
            cell.bounds = cellBounds(cell);
            cell.done = false;
            _cells.push_back(cell);
            index++;
        }
    }

    createNumberLabels();
    invalidateBoard();
}

void SchulteView::createLabelPool()
{
    _number_labels.reserve(30);
    for (int i = 0; i < 30; ++i) {
        LabelSlot slot;
        slot.label = std::make_unique<Label>(_panel->get());
        slot.cell_index = -1;
        slot.label->setSize(74, 32);
        slot.label->setText("");
        slot.label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        lv_label_set_long_mode(slot.label->get(), LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_letter_space(slot.label->get(), 0, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.label->get(), LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_outline_stroke_color(slot.label->get(), lv_color_hex(_number_stroke_color), LV_PART_MAIN);
        lv_obj_set_style_text_outline_stroke_width(slot.label->get(), 3, LV_PART_MAIN);
        lv_obj_set_style_text_outline_stroke_opa(slot.label->get(), 189, LV_PART_MAIN);
        lv_obj_remove_flag(slot.label->get(), LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(slot.label->get(), LV_OBJ_FLAG_HIDDEN);
        _number_labels.push_back(std::move(slot));
    }
}

void SchulteView::createNumberLabels()
{
    for (int label_index = 0; label_index < static_cast<int>(_number_labels.size()); ++label_index) {
        LabelSlot& slot = _number_labels[label_index];
        if (!slot.label) {
            continue;
        }

        if (label_index >= static_cast<int>(_cells.size()) || _state == State::Idle) {
            slot.cell_index = -1;
            slot.label->setText("");
            lv_obj_add_flag(slot.label->get(), LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const int cell_index = label_index;
        const Cell& cell = _cells[cell_index];
        const lv_font_t* font = fontForSize(numberFontSize(cell.ring_index));
        const int line_height = lv_font_get_line_height(font);
        const int label_width = 76;
        const int label_height = line_height + 4;
        const lv_point_t position = cellLabelPoint(cell);

        slot.cell_index = cell_index;
        slot.label->setSize(label_width, label_height);
        slot.label->setPos(position.x - label_width / 2, position.y - line_height / 2 - 1);
        slot.label->setText(std::to_string(cell.value));
        slot.label->setTextFont(font);
        slot.label->setTextColor(lv_color_hex(cell.color));
        lv_obj_remove_flag(slot.label->get(), LV_OBJ_FLAG_HIDDEN);
    }

    if (_result_panel) {
        _result_panel->moveForeground();
    }
}

void SchulteView::updateResultVisibility()
{
    if (_density_hint_label) {
        if (_state == State::Idle) {
            _density_hint_label->setText(std::to_string(currentDensity()));
            lv_obj_remove_flag(_density_hint_label->get(), LV_OBJ_FLAG_HIDDEN);
            _density_hint_label->moveForeground();
        } else {
            _density_hint_label->setText("");
            lv_obj_add_flag(_density_hint_label->get(), LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (_result_panel == nullptr) {
        return;
    }

    if (_state == State::Result) {
        lv_obj_remove_flag(_result_panel->get(), LV_OBJ_FLAG_HIDDEN);
        _result_panel->moveForeground();
    } else {
        lv_obj_add_flag(_result_panel->get(), LV_OBJ_FLAG_HIDDEN);
        if (_result_label) {
            _result_label->setText("");
        }
    }
}

void SchulteView::updateTouch(uint32_t now)
{
    lv_indev_t* indev = GetHAL().lvTouchpad;
    if (indev == nullptr) {
        _touch_was_pressed = false;
        return;
    }

    const bool is_pressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;
    if (!is_pressed) {
        _touch_was_pressed = false;
        return;
    }

    if (_transition_active) {
        _touch_was_pressed = true;
        return;
    }

    if (_touch_was_pressed) {
        return;
    }
    _touch_was_pressed = true;

    if (_state != State::Running) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    const int cell_index = cellIndexAtPoint(point);
    if (cell_index >= 0) {
        handleCellPress(cell_index, now);
    }
}

void SchulteView::updateFlash(uint32_t now)
{
    if (_flash_type == FlashType::None || _flash_until_tick == 0) {
        return;
    }

    if (tick_reached(now, _flash_until_tick)) {
        clearFlash();
    }
}

void SchulteView::updateTransition(uint32_t now)
{
    if (!_transition_active || _transition_overlay == nullptr) {
        return;
    }

    const uint32_t elapsed = now - _transition_start_tick;
    if (elapsed >= _transition_duration_ms) {
        stopTransition();
        return;
    }

    const int step = std::min<int>(_transition_steps - 1,
                                   static_cast<int>((elapsed * _transition_steps) / _transition_duration_ms));
    if (step == _transition_step) {
        return;
    }

    _transition_step = step;
    lv_obj_invalidate(_transition_overlay->get());
}

void SchulteView::handleCellPress(int cellIndex, uint32_t now)
{
    if (_state != State::Running || cellIndex < 0 || cellIndex >= static_cast<int>(_cells.size())) {
        return;
    }

    if (_cells[cellIndex].done || _cells[cellIndex].value != _target) {
        triggerCellFeedback(FlashType::Miss);
        startFlash(cellIndex, FlashType::Miss, 130, now);
        return;
    }

    _cells[cellIndex].done = true;
    triggerCellFeedback(FlashType::Hit);
    startFlash(cellIndex, FlashType::Hit, 100, now);
    _target += 1;

    if (_target > currentDensity()) {
        finishGame();
    }
}

void SchulteView::startFlash(int cellIndex, FlashType type, uint32_t durationMs, uint32_t now)
{
    clearFlash();
    _flash_cell_index = cellIndex;
    _flash_type = type;
    _flash_until_tick = now + durationMs;
    fitFlashOverlayToCell(cellIndex);

    if (type == FlashType::Miss) {
        for (auto& slot : _number_labels) {
            if (slot.cell_index == cellIndex && slot.label) {
                slot.label->setTextColor(lv_color_hex(_miss_stroke_color));
                break;
            }
        }
    }

    if (_flash_overlay) {
        lv_obj_remove_flag(_flash_overlay->get(), LV_OBJ_FLAG_HIDDEN);
    }
}

void SchulteView::clearFlash()
{
    if (_flash_type == FlashType::None) {
        return;
    }

    const int old_flash_cell_index = _flash_cell_index;
    const FlashType old_flash_type = _flash_type;

    if (_flash_overlay) {
        lv_obj_add_flag(_flash_overlay->get(), LV_OBJ_FLAG_HIDDEN);
    }

    if (old_flash_type == FlashType::Miss) {
        for (auto& slot : _number_labels) {
            if (slot.label && slot.cell_index == old_flash_cell_index && old_flash_cell_index >= 0 &&
                old_flash_cell_index < static_cast<int>(_cells.size())) {
                slot.label->setTextColor(lv_color_hex(_cells[old_flash_cell_index].color));
                break;
            }
        }
    }

    _flash_cell_index = -1;
    _flash_type = FlashType::None;
    _flash_until_tick = 0;
}

void SchulteView::triggerCellFeedback(FlashType type)
{
    if (type == FlashType::Hit) {
        GetHAL().vibrate(_hit_vibrate_duration_ms, _hit_vibrate_strength);
        play_schulte_feedback_clip(type);
    } else if (type == FlashType::Miss) {
        GetHAL().vibrate(_miss_vibrate_duration_ms, _miss_vibrate_strength);
        play_schulte_feedback_clip(type);
    }
}

void SchulteView::startTransition()
{
    stopTransition();
}

void SchulteView::stopTransition()
{
    if (_transition_overlay == nullptr) {
        _transition_active = false;
        _transition_step = -1;
        return;
    }

    if (_transition_active || !lv_obj_has_flag(_transition_overlay->get(), LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(_transition_overlay->get(), LV_OBJ_FLAG_HIDDEN);
    }
    _transition_active = false;
    _transition_start_tick = 0;
    _transition_step = -1;
}

void SchulteView::fitFlashOverlayToCell(int cellIndex)
{
    if (_flash_overlay == nullptr || cellIndex < 0 || cellIndex >= static_cast<int>(_cells.size())) {
        return;
    }

    const lv_area_t& bounds = _cells[cellIndex].bounds;
    const int width = bounds.x2 - bounds.x1 + 1;
    const int height = bounds.y2 - bounds.y1 + 1;

    _flash_overlay->setPos(bounds.x1, bounds.y1);
    _flash_overlay->setSize(width, height);
}

void SchulteView::invalidateBoard()
{
    if (_board) {
        lv_obj_invalidate(_board->get());
    }
}

const std::array<SchulteView::Ring, 3>& SchulteView::currentLayout() const
{
    const int density = currentDensity();
    if (density == 18) {
        return _layout_18;
    }
    if (density == 30) {
        return _layout_30;
    }
    return _layout_25;
}

const SchulteView::Ring& SchulteView::ringForCell(const Cell& cell) const
{
    return currentLayout()[cell.ring_index];
}

const lv_font_t* SchulteView::fontForSize(int fontSize) const
{
    if (fontSize == 20) {
        return &lv_font_montserrat_20;
    }
    if (fontSize == 22) {
        return &lv_font_montserrat_22;
    }
    if (fontSize == 24) {
        return &lv_font_montserrat_24;
    }
    return &lv_font_montserrat_26;
}

int SchulteView::currentDensity() const
{
    return _density_cycle[_density_index];
}

int SchulteView::numberFontSize(int ringIndex) const
{
    const int density = currentDensity();
    if (density == 30) {
        return ringIndex == 0 ? 20 : 22;
    }
    if (density == 25) {
        return ringIndex == 0 ? 25 : 26;
    }
    return ringIndex == 0 ? 24 : 25;
}

int SchulteView::cellIndexAtPoint(lv_point_t point) const
{
    if (_board == nullptr) {
        return -1;
    }

    lv_area_t coords;
    lv_obj_get_coords(_board->get(), &coords);
    const float local_x = static_cast<float>(point.x - coords.x1);
    const float local_y = static_cast<float>(point.y - coords.y1);
    const float dx = local_x - static_cast<float>(_center);
    const float dy = local_y - static_cast<float>(_center);
    const float radius = std::sqrt(dx * dx + dy * dy);
    if (radius > static_cast<float>(_outer_radius)) {
        return -1;
    }

    const float angle = normalizeDegrees(std::atan2(dy, dx) * 180.0f / _pi);
    const auto& layout = currentLayout();
    for (int ring_index = 0; ring_index < static_cast<int>(layout.size()); ++ring_index) {
        const Ring& ring = layout[ring_index];
        if (radius < ring.inner || radius > ring.outer) {
            continue;
        }

        const float step = 360.0f / static_cast<float>(ring.count);
        const float relative = normalizeDegrees(angle - (ring.phase + ringPhaseOffset(ring_index)));
        int local_index = static_cast<int>(std::floor(relative / step));
        if (local_index >= ring.count) {
            local_index = ring.count - 1;
        }
        return ringStartIndex(ring_index) + local_index;
    }

    return -1;
}

int SchulteView::ringStartIndex(int ringIndex) const
{
    int start = 0;
    const auto& layout = currentLayout();
    for (int i = 0; i < ringIndex; ++i) {
        start += layout[i].count;
    }
    return start;
}

float SchulteView::ringPhaseOffset(int ringIndex) const
{
    return _state == State::Running ? _ring_phase_offsets[ringIndex] : 0.0f;
}

float SchulteView::normalizeDegrees(float degrees) const
{
    float normalized = std::fmod(degrees, 360.0f);
    if (normalized < 0.0f) {
        normalized += 360.0f;
    }
    return normalized;
}

lv_point_t SchulteView::polarToPoint(float radius, float degrees) const
{
    const float radians = degrees * _pi / 180.0f;
    return {
        static_cast<lv_coord_t>(std::lround(static_cast<float>(_center) + std::cos(radians) * radius)),
        static_cast<lv_coord_t>(std::lround(static_cast<float>(_center) + std::sin(radians) * radius)),
    };
}

lv_point_t SchulteView::cellLabelPoint(const Cell& cell) const
{
    const Ring& ring = ringForCell(cell);
    const float step = 360.0f / static_cast<float>(ring.count);
    const float start_deg = ring.phase + ringPhaseOffset(cell.ring_index) + step * cell.local_index;
    const float end_deg = start_deg + step;
    const float mid_deg = (start_deg + end_deg) * 0.5f;
    return polarToPoint(sector_centroid_radius(ring.inner, ring.outer, step), mid_deg);
}

lv_area_t SchulteView::cellBounds(const Cell& cell) const
{
    const Ring& ring = ringForCell(cell);
    const float step = 360.0f / static_cast<float>(ring.count);
    const float start_deg = ring.phase + ringPhaseOffset(cell.ring_index) + step * cell.local_index;
    const float end_deg = start_deg + step;

    std::vector<lv_point_t> points;
    points.reserve(10);
    points.push_back(polarToPoint(ring.outer, start_deg));
    points.push_back(polarToPoint(ring.outer, end_deg));
    if (ring.inner <= 0.0f) {
        points.push_back({_center, _center});
    } else {
        points.push_back(polarToPoint(ring.inner, start_deg));
        points.push_back(polarToPoint(ring.inner, end_deg));
    }

    for (float cardinal : {0.0f, 90.0f, 180.0f, 270.0f}) {
        const float relative = normalizeDegrees(cardinal - start_deg);
        if (relative > 0.0f && relative < step) {
            points.push_back(polarToPoint(ring.outer, cardinal));
            if (ring.inner > 0.0f) {
                points.push_back(polarToPoint(ring.inner, cardinal));
            }
        }
    }

    int min_x = _panel_size;
    int min_y = _panel_size;
    int max_x = 0;
    int max_y = 0;
    for (const auto& point : points) {
        min_x = std::min<int>(min_x, point.x);
        min_y = std::min<int>(min_y, point.y);
        max_x = std::max<int>(max_x, point.x);
        max_y = std::max<int>(max_y, point.y);
    }

    constexpr int padding = 8;
    return {
        static_cast<lv_coord_t>(std::max(0, min_x - padding)),
        static_cast<lv_coord_t>(std::max(0, min_y - padding)),
        static_cast<lv_coord_t>(std::min(_panel_size - 1, max_x + padding)),
        static_cast<lv_coord_t>(std::min(_panel_size - 1, max_y + padding)),
    };
}

uint32_t SchulteView::createColorVariant(uint32_t baseColor)
{
    auto& random = Random::getInstance();
    const int jitter = baseColor == 0x14191B ? 14 : 24;
    const int r = clamp_channel(static_cast<int>(color_channel(baseColor, 16)) + random.getInt(-jitter, jitter));
    const int g = clamp_channel(static_cast<int>(color_channel(baseColor, 8)) + random.getInt(-jitter, jitter));
    const int b = clamp_channel(static_cast<int>(color_channel(baseColor, 0)) + random.getInt(-jitter, jitter));
    return make_rgb(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
}

uint32_t SchulteView::pickColorBase(const std::vector<uint32_t>& excludedColors)
{
    std::vector<uint32_t> choices;
    choices.reserve(_number_color_bases.size());
    for (uint32_t color : _number_color_bases) {
        if (std::find(excludedColors.begin(), excludedColors.end(), color) == excludedColors.end()) {
            choices.push_back(color);
        }
    }

    auto& random = Random::getInstance();
    if (choices.empty()) {
        return _number_color_bases[random.getInt(0, static_cast<int>(_number_color_bases.size()) - 1)];
    }
    return choices[random.getInt(0, static_cast<int>(choices.size()) - 1)];
}

void SchulteView::createGameColors()
{
    _game_colors.clear();
    _game_colors.reserve(currentDensity());

    const auto& layout = currentLayout();
    for (const Ring& ring : layout) {
        std::vector<uint32_t> ring_color_bases;
        ring_color_bases.reserve(ring.count);
        for (int i = 0; i < ring.count; ++i) {
            std::vector<uint32_t> excluded_colors;
            if (i > 0) {
                excluded_colors.push_back(ring_color_bases[i - 1]);
            }
            if (i == ring.count - 1 && !ring_color_bases.empty()) {
                excluded_colors.push_back(ring_color_bases[0]);
            }
            const uint32_t color_base = pickColorBase(excluded_colors);
            ring_color_bases.push_back(color_base);
            _game_colors.push_back(createColorVariant(color_base));
        }
    }
}

void SchulteView::createRingPhaseOffsets()
{
    const auto& layout = currentLayout();
    for (int i = 0; i < static_cast<int>(layout.size()); ++i) {
        _ring_phase_offsets[i] = Random::getInstance().getFloat(0.0f, 360.0f / static_cast<float>(layout[i].count));
    }
}

void SchulteView::shuffleNumbers(std::vector<int>& values)
{
    auto& random = Random::getInstance();
    for (int i = static_cast<int>(values.size()) - 1; i > 0; --i) {
        const int j = random.getInt(0, i);
        std::swap(values[i], values[j]);
    }
}

std::string SchulteView::formatTime(uint32_t ms) const
{
    char buffer[16] = {};
    const double seconds = static_cast<double>(ms) / 1000.0;
    std::snprintf(buffer, sizeof(buffer), "%.2f", seconds);

    std::string result(buffer);
    while (result.size() < 5) {
        result.insert(result.begin(), '0');
    }
    result += "s";
    return result;
}

void SchulteView::drawBoard(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, 4);
        return;
    }

    if (code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_COVER);
        return;
    }

    if (code != LV_EVENT_DRAW_MAIN_BEGIN) {
        return;
    }

    lv_obj_t* obj = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    lv_point_t center = {
        static_cast<lv_coord_t>(coords.x1 + _center),
        static_cast<lv_coord_t>(coords.y1 + _center),
    };

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.center = center;
    arc_dsc.radius = _outer_radius;
    arc_dsc.width = _outer_radius * 2;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    arc_dsc.rounded = 0;
    arc_dsc.opa = LV_OPA_COVER;
    arc_dsc.color = lv_color_hex(_board_backdrop_color);
    lv_draw_arc(layer, &arc_dsc);

    const auto& layout = currentLayout();
    for (int ring_index = 0; ring_index < static_cast<int>(layout.size()); ++ring_index) {
        const Ring& ring = layout[ring_index];
        const float step = 360.0f / static_cast<float>(ring.count);
        const float phase = ring.phase + ringPhaseOffset(ring_index);
        for (int local_index = 0; local_index < ring.count; ++local_index) {
            const float start = normalizeDegrees(phase + step * local_index);
            const float end = start + step;

            lv_draw_arc_dsc_init(&arc_dsc);
            arc_dsc.center = center;
            arc_dsc.radius = static_cast<uint16_t>(std::lround(ring.outer));
            arc_dsc.width = std::max<int32_t>(1, static_cast<int32_t>(std::lround(ring.outer - ring.inner)));
            arc_dsc.start_angle = start;
            arc_dsc.end_angle = end;
            arc_dsc.rounded = 0;
            arc_dsc.opa = 214;
            arc_dsc.color = lv_color_hex(_sector_fill_color);
            lv_draw_arc(layer, &arc_dsc);
        }
    }

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(_grid_color);
    line_dsc.width = 2;
    line_dsc.opa = LV_OPA_COVER;

    for (int ring_index = 0; ring_index < static_cast<int>(layout.size()); ++ring_index) {
        const Ring& ring = layout[ring_index];
        const float step = 360.0f / static_cast<float>(ring.count);
        const float phase = ring.phase + ringPhaseOffset(ring_index);
        for (int local_index = 0; local_index < ring.count; ++local_index) {
            const lv_point_t inner = polarToPoint(ring.inner, phase + step * local_index);
            const lv_point_t outer = polarToPoint(ring.outer, phase + step * local_index);
            line_dsc.p1.x = coords.x1 + inner.x;
            line_dsc.p1.y = coords.y1 + inner.y;
            line_dsc.p2.x = coords.x1 + outer.x;
            line_dsc.p2.y = coords.y1 + outer.y;
            lv_draw_line(layer, &line_dsc);
        }
    }

    for (const Ring& ring : layout) {
        lv_draw_arc_dsc_init(&arc_dsc);
        arc_dsc.center = center;
        arc_dsc.radius = static_cast<uint16_t>(std::lround(ring.outer));
        arc_dsc.width = 2;
        arc_dsc.start_angle = 0;
        arc_dsc.end_angle = 360;
        arc_dsc.rounded = 0;
        arc_dsc.opa = LV_OPA_COVER;
        arc_dsc.color = lv_color_hex(_grid_strong_color);
        lv_draw_arc(layer, &arc_dsc);
    }

}

void SchulteView::drawFlashOverlay(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, 4);
        return;
    }

    if (code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_NOT_COVER);
        return;
    }

    if (code != LV_EVENT_DRAW_MAIN_BEGIN || _flash_type == FlashType::None || _flash_cell_index < 0 ||
        _flash_cell_index >= static_cast<int>(_cells.size())) {
        return;
    }

    lv_obj_t* obj = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    const Cell& cell = _cells[_flash_cell_index];
    const Ring& ring = ringForCell(cell);
    const float step = 360.0f / static_cast<float>(ring.count);
    const float start = normalizeDegrees(ring.phase + ringPhaseOffset(cell.ring_index) + step * cell.local_index);
    const float end = start + step;
    const lv_point_t center = {
        static_cast<lv_coord_t>(coords.x1 - cell.bounds.x1 + _center),
        static_cast<lv_coord_t>(coords.y1 - cell.bounds.y1 + _center),
    };

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.center = center;
    arc_dsc.radius = static_cast<uint16_t>(std::lround(ring.outer));
    arc_dsc.width = std::max<int32_t>(1, static_cast<int32_t>(std::lround(ring.outer - ring.inner)));
    arc_dsc.start_angle = start;
    arc_dsc.end_angle = end;
    arc_dsc.rounded = 0;
    arc_dsc.opa = LV_OPA_COVER;
    arc_dsc.color = lv_color_hex(flash_fill_color(_flash_type));
    lv_draw_arc(layer, &arc_dsc);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(flash_stroke_color(_flash_type));
    line_dsc.width = 2;
    line_dsc.opa = LV_OPA_COVER;

    for (float deg : {start, end}) {
        const lv_point_t inner = polarToPoint(ring.inner, deg);
        const lv_point_t outer = polarToPoint(ring.outer, deg);
        line_dsc.p1.x = coords.x1 - cell.bounds.x1 + inner.x;
        line_dsc.p1.y = coords.y1 - cell.bounds.y1 + inner.y;
        line_dsc.p2.x = coords.x1 - cell.bounds.x1 + outer.x;
        line_dsc.p2.y = coords.y1 - cell.bounds.y1 + outer.y;
        lv_draw_line(layer, &line_dsc);
    }

    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.center = center;
    arc_dsc.radius = static_cast<uint16_t>(std::lround(ring.outer));
    arc_dsc.width = 2;
    arc_dsc.start_angle = start;
    arc_dsc.end_angle = end;
    arc_dsc.rounded = 0;
    arc_dsc.opa = LV_OPA_COVER;
    arc_dsc.color = lv_color_hex(flash_stroke_color(_flash_type));
    lv_draw_arc(layer, &arc_dsc);

    if (ring.inner > 0.0f) {
        arc_dsc.radius = static_cast<uint16_t>(std::lround(ring.inner));
        lv_draw_arc(layer, &arc_dsc);
    }
}

void SchulteView::drawTransitionOverlay(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, 0);
        return;
    }

    if (code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_NOT_COVER);
        return;
    }

    if (code != LV_EVENT_DRAW_MAIN_BEGIN || !_transition_active || _transition_step < 0) {
        return;
    }

    const float linear =
        static_cast<float>(_transition_step) / static_cast<float>(std::max(1, _transition_steps - 1));
    const float eased = 1.0f - std::pow(1.0f - linear, 3.0f);
    const int ring_radius = static_cast<int>(std::lround(
        static_cast<float>(_transition_ring_start_radius) +
        eased * static_cast<float>(_transition_cover_radius - _transition_ring_start_radius)));
    if (ring_radius >= _transition_cover_radius) {
        return;
    }

    lv_obj_t* obj = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.center = {
        static_cast<lv_coord_t>(coords.x1 + _center),
        static_cast<lv_coord_t>(coords.y1 + _center),
    };
    arc_dsc.radius = ring_radius;
    arc_dsc.width = 3;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    arc_dsc.rounded = 0;
    arc_dsc.opa = static_cast<lv_opa_t>(std::lround((1.0f - linear) * 172.0f));
    arc_dsc.color = lv_color_hex(_grid_strong_color);
    lv_draw_arc(layer, &arc_dsc);

    if (ring_radius > 18) {
        arc_dsc.radius = std::max(1, ring_radius - 14);
        arc_dsc.width = 2;
        arc_dsc.opa = static_cast<lv_opa_t>(std::lround((1.0f - linear) * 82.0f));
        arc_dsc.color = lv_color_hex(_grid_color);
        lv_draw_arc(layer, &arc_dsc);
    }
}

namespace {

void schulte_board_draw_event_cb(lv_event_t* e)
{
    auto* view = static_cast<SchulteView*>(lv_event_get_user_data(e));
    if (view == nullptr) {
        return;
    }

    view->drawBoard(e);
}

void schulte_flash_draw_event_cb(lv_event_t* e)
{
    auto* view = static_cast<SchulteView*>(lv_event_get_user_data(e));
    if (view == nullptr) {
        return;
    }

    view->drawFlashOverlay(e);
}

void schulte_transition_draw_event_cb(lv_event_t* e)
{
    auto* view = static_cast<SchulteView*>(lv_event_get_user_data(e));
    if (view == nullptr) {
        return;
    }

    view->drawTransitionOverlay(e);
}

}  // namespace
