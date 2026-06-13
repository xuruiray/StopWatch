/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"
#include <hal/hal.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size                 = 466;
constexpr int _panel_center               = _panel_size / 2;
constexpr int _gear_size                  = 330;
constexpr int _gear_pivot                 = _gear_size / 2;
constexpr int _gear_teeth                 = 9;
constexpr int _gear_touch_radius_min      = 32;
constexpr int _gear_touch_radius_max      = 186;
constexpr int _gear_outer_radius          = 153;
constexpr int _gear_root_radius           = 126;
constexpr int _gear_face_radius           = 112;
constexpr int _gear_hub_outer_radius      = 51;
constexpr int _gear_hub_mid_radius        = 39;
constexpr int _gear_hub_hole_radius       = 27;
constexpr float _tooth_angle_deg          = 360.0f / static_cast<float>(_gear_teeth);
constexpr uint32_t _feedback_interval_ms  = 50;
constexpr uint16_t _vibrate_duration_ms   = 20;
constexpr uint8_t _vibrate_strength       = 90;
constexpr uint32_t _bg_color              = 0x000000;
constexpr float _pi                       = 3.14159265358979323846f;
constexpr float _max_rotation_speed_deg_s = 720.0f;
constexpr float _max_pending_rotation_deg = _tooth_angle_deg;
constexpr uint32_t _rotation_apply_ms     = 10;

float normalize_delta_degrees(float delta)
{
    while (delta > 180.0f) {
        delta -= 360.0f;
    }
    while (delta < -180.0f) {
        delta += 360.0f;
    }
    return delta;
}

int tooth_index_for_rotation(float rotationDeg)
{
    return static_cast<int>(std::floor(rotationDeg / _tooth_angle_deg));
}

int32_t normalize_rotation_tenth(int32_t rotation_tenth)
{
    rotation_tenth %= 3600;
    if (rotation_tenth < 0) {
        rotation_tenth += 3600;
    }
    return rotation_tenth;
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float smooth_band(float distance, float half_width, float feather)
{
    return clamp01((half_width + feather - std::fabs(distance)) / std::max(0.001f, feather));
}

float tooth_boundary_radius(float degrees)
{
    float phase = std::fmod(degrees + 90.0f, _tooth_angle_deg);
    if (phase < 0.0f) {
        phase += _tooth_angle_deg;
    }
    phase /= _tooth_angle_deg;

    if (phase < 0.02f) {
        return static_cast<float>(_gear_root_radius);
    }
    if (phase < 0.16f) {
        const float t = (phase - 0.02f) / 0.14f;
        return static_cast<float>(_gear_root_radius) +
               (static_cast<float>(_gear_outer_radius - _gear_root_radius) * t);
    }
    if (phase < 0.53f) {
        return static_cast<float>(_gear_outer_radius);
    }
    if (phase < 0.80f) {
        const float t = (phase - 0.53f) / 0.27f;
        return static_cast<float>(_gear_outer_radius) -
               (static_cast<float>(_gear_outer_radius - _gear_root_radius) * t);
    }
    return static_cast<float>(_gear_root_radius);
}

float tooth_boundary_phase(float degrees)
{
    float phase = std::fmod(degrees + 90.0f, _tooth_angle_deg);
    if (phase < 0.0f) {
        phase += _tooth_angle_deg;
    }
    return std::min(phase, _tooth_angle_deg - phase);
}

float angular_distance_degrees(float a, float b)
{
    float delta = std::fmod(a - b + 180.0f, 360.0f);
    if (delta < 0.0f) {
        delta += 360.0f;
    }
    return std::fabs(delta - 180.0f);
}

uint32_t mix_color(uint32_t a, uint32_t b, float t)
{
    t              = clamp01(t);
    const uint8_t ar = (a >> 16) & 0xff;
    const uint8_t ag = (a >> 8) & 0xff;
    const uint8_t ab = a & 0xff;
    const uint8_t br = (b >> 16) & 0xff;
    const uint8_t bg = (b >> 8) & 0xff;
    const uint8_t bb = b & 0xff;
    const uint8_t rr = static_cast<uint8_t>(std::lround(static_cast<float>(ar) + (static_cast<float>(br - ar) * t)));
    const uint8_t rg = static_cast<uint8_t>(std::lround(static_cast<float>(ag) + (static_cast<float>(bg - ag) * t)));
    const uint8_t rb = static_cast<uint8_t>(std::lround(static_cast<float>(ab) + (static_cast<float>(bb - ab) * t)));
    return (static_cast<uint32_t>(rr) << 16) | (static_cast<uint32_t>(rg) << 8) | rb;
}

uint32_t compose_color(float x, float y)
{
    const float radius  = std::sqrt(x * x + y * y);
    const float degrees = std::atan2(y, x) * 180.0f / _pi;
    const float boundary_radius = tooth_boundary_radius(degrees);

    uint32_t color = radius > _gear_root_radius ? 0x11161b : 0x090c10;
    if (radius <= _gear_face_radius) {
        color = 0x0b1116;
    }

    const float outer_edge = boundary_radius - radius;
    if (outer_edge >= 0.0f) {
        color = mix_color(color, 0x53606a, smooth_band(outer_edge - 3.5f, 4.0f, 1.2f));
        color = mix_color(color, 0xe4e8ea, smooth_band(outer_edge - 1.2f, 1.3f, 0.8f));
    }

    color = mix_color(color, 0x1f2830, smooth_band(radius - (_gear_root_radius - 5.0f), 2.2f, 0.8f));
    color = mix_color(color, 0xd7dcdf, smooth_band(radius - (_gear_root_radius - 13.0f), 1.7f, 0.7f));
    color = mix_color(color, 0x303941, smooth_band(radius - (_gear_face_radius - 11.0f), 1.0f, 0.7f) * 0.8f);

    for (int ring = 66; ring <= 104; ring += 7) {
        const uint32_t tone = (ring % 14 == 0) ? 0x20282f : 0x151d24;
        color = mix_color(color, tone, smooth_band(radius - static_cast<float>(ring), 0.45f, 0.5f) * 0.7f);
    }

    if (radius >= _gear_face_radius - 10 && radius <= _gear_root_radius - 16) {
        color = mix_color(color, 0x444e57, smooth_band(tooth_boundary_phase(degrees), 0.42f, 0.4f) * 0.75f);
    }

    if (radius <= _gear_hub_outer_radius) {
        color = 0x050607;
    }
    color = mix_color(color, 0xe5e8ea, smooth_band(radius - _gear_hub_outer_radius, 2.8f, 0.9f));
    color = mix_color(color, 0x6a747c, smooth_band(radius - _gear_hub_mid_radius, 2.7f, 0.9f));
    color = mix_color(color, 0xdfe2e4, smooth_band(radius - (_gear_hub_mid_radius - 9.0f), 1.0f, 0.7f));

    if (radius >= _gear_hub_hole_radius + 6 && radius <= _gear_hub_outer_radius - 7) {
        const float index_slot = smooth_band(angular_distance_degrees(degrees, -70.0f), 2.0f, 1.0f);
        color                  = mix_color(color, 0xeaf0f2, index_slot * 0.85f);
    }

    color = mix_color(color, 0xeaf0f2, smooth_band(radius - _gear_hub_hole_radius, 1.8f, 0.8f));

    return color;
}

uint8_t coverage_alpha(float x, float y)
{
    constexpr int sample_count = 2;
    int covered                = 0;

    for (int sy = 0; sy < sample_count; ++sy) {
        for (int sx = 0; sx < sample_count; ++sx) {
            const float ox = (static_cast<float>(sx) + 0.5f) / static_cast<float>(sample_count) - 0.5f;
            const float oy = (static_cast<float>(sy) + 0.5f) / static_cast<float>(sample_count) - 0.5f;
            const float px = x + ox;
            const float py = y + oy;
            const float radius  = std::sqrt(px * px + py * py);
            const float degrees = std::atan2(py, px) * 180.0f / _pi;
            const bool in_gear  = radius <= tooth_boundary_radius(degrees);
            const bool in_hole  = radius < static_cast<float>(_gear_hub_hole_radius - 2);
            if (in_gear && !in_hole) {
                ++covered;
            }
        }
    }

    return static_cast<uint8_t>((covered * 255) / (sample_count * sample_count));
}

std::vector<int16_t>& ratchet_click_buffer()
{
    static std::vector<int16_t> _buffer;
    return _buffer;
}

int& ratchet_click_sample_rate()
{
    static int _sample_rate = 0;
    return _sample_rate;
}

void prepare_ratchet_click()
{
    const int sample_rate = GetHAL().getAudioSampleRate();
    auto& buffer          = ratchet_click_buffer();
    auto& cached_rate     = ratchet_click_sample_rate();

    if (!buffer.empty() && cached_rate == sample_rate) {
        return;
    }

    constexpr float duration_sec    = 0.024f;
    constexpr float attack_sec      = 0.00025f;
    constexpr float highpass_coef   = 0.78f;
    constexpr float target_peak_amp = 21000.0f;
    const int sample_count          = static_cast<int>(sample_rate * duration_sec);
    const int fade_samples          = std::max(1, static_cast<int>(sample_rate * 0.002f));
    buffer.resize(sample_count);
    cached_rate = sample_rate;

    auto render_sample = [&](int i, float highpass_noise) {
        const float t        = static_cast<float>(i) / static_cast<float>(sample_rate);
        const float progress = static_cast<float>(i) / static_cast<float>(std::max(1, sample_count - 1));
        const float attack   = std::min(1.0f, t / attack_sec);

        auto resonance = [&](float frequency, float amplitude, float decay, float phase, float delay_sec) {
            if (t < delay_sec) {
                return 0.0f;
            }
            const int delay_samples     = static_cast<int>(sample_rate * delay_sec);
            const float local_t         = t - delay_sec;
            const float local_progress  = static_cast<float>(i - delay_samples) /
                                          static_cast<float>(std::max(1, sample_count - delay_samples - 1));
            const float local_envelope  = std::exp(-local_progress * decay);
            return std::sin(2.0f * _pi * frequency * local_t + phase) * amplitude * local_envelope;
        };

        float sample = highpass_noise * 0.85f * attack * std::exp(-progress * 15.0f);
        sample += resonance(2600.0f, 0.30f, 14.0f, 0.3f, 0.0f);
        sample += resonance(5100.0f, 0.22f, 17.0f, 1.1f, 0.0002f);
        sample += resonance(7600.0f, 0.12f, 19.0f, 0.0f, 0.0004f);

        if (i >= sample_count - fade_samples) {
            sample *= static_cast<float>(sample_count - i - 1) / static_cast<float>(fade_samples);
        }

        return sample;
    };

    auto render_pass = [&](std::vector<int16_t>* out, float scale) {
        uint32_t noise_state = 0x8f21e35bu;
        float previous_noise = 0.0f;
        float highpass_noise = 0.0f;
        float max_abs        = 0.0f;

        for (int i = 0; i < sample_count; ++i) {
            noise_state = noise_state * 1664525u + 1013904223u;
            const float noise =
                (static_cast<float>((noise_state >> 8) & 0xffffu) / 32767.5f) - 1.0f;
            highpass_noise = highpass_coef * (highpass_noise + noise - previous_noise);
            previous_noise = noise;

            const float sample = render_sample(i, highpass_noise);
            max_abs            = std::max(max_abs, std::fabs(sample));

            if (out != nullptr) {
                (*out)[i] = static_cast<int16_t>(std::clamp(sample * scale, -32767.0f, 32767.0f));
            }
        }

        return max_abs;
    };

    const float max_abs = render_pass(nullptr, 1.0f);
    const float scale   = max_abs > 0.0f ? target_peak_amp / max_abs : 1.0f;
    render_pass(&buffer, scale);
}

void play_ratchet_click()
{
    auto& buffer = ratchet_click_buffer();
    if (buffer.empty()) {
        prepare_ratchet_click();
    }
    GetHAL().audioPlay(buffer, true);
}

}  // namespace

void RatchetView::buildRuntimeGearImage()
{
    constexpr std::size_t pixel_count = static_cast<std::size_t>(_gear_size) * static_cast<std::size_t>(_gear_size);
    _gear_image_data.assign(pixel_count * 3, 0);

    uint8_t* color_plane = _gear_image_data.data();
    uint8_t* alpha_plane = color_plane + pixel_count * 2;
    const float center   = static_cast<float>(_gear_size) * 0.5f;

    for (int y = 0; y < _gear_size; ++y) {
        for (int x = 0; x < _gear_size; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(_gear_size) +
                                      static_cast<std::size_t>(x);
            const float local_x = static_cast<float>(x) + 0.5f - center;
            const float local_y = static_cast<float>(y) + 0.5f - center;
            const uint8_t alpha = coverage_alpha(local_x, local_y);
            uint32_t color      = alpha == 0 ? 0x000000 : compose_color(local_x, local_y);

            const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xff);
            const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xff);
            const uint8_t b = static_cast<uint8_t>(color & 0xff);
            const uint16_t rgb565 =
                static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));

            color_plane[index * 2]     = static_cast<uint8_t>(rgb565 & 0xff);
            color_plane[index * 2 + 1] = static_cast<uint8_t>((rgb565 >> 8) & 0xff);
            alpha_plane[index]         = alpha;
        }
    }

    _gear_image_dsc = {};
    _gear_image_dsc.header.cf    = LV_COLOR_FORMAT_RGB565A8;
    _gear_image_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    _gear_image_dsc.header.w     = _gear_size;
    _gear_image_dsc.header.h     = _gear_size;
    _gear_image_dsc.data_size    = pixel_count * 3;
    _gear_image_dsc.data         = _gear_image_data.data();
}

void RatchetView::init(lv_obj_t* parent)
{
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(_bg_color), LV_PART_MAIN);

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_hex(_bg_color));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    buildRuntimeGearImage();

    _gear_image = std::make_unique<Image>(_panel->get());
    _gear_image->setSrc(&_gear_image_dsc);
    _gear_image->setPivot(_gear_pivot, _gear_pivot);
    _gear_image->setPos(_panel_center - _gear_pivot, _panel_center - _gear_pivot);

    prepare_ratchet_click();

    _touch_mask = std::make_unique<Container>(_panel->get());
    _touch_mask->align(LV_ALIGN_CENTER, 0, 0);
    _touch_mask->setSize(_panel_size, _panel_size);
    _touch_mask->setBgOpa(LV_OPA_TRANSP);
    _touch_mask->setBorderWidth(0);
    _touch_mask->setPaddingAll(0);
    _touch_mask->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _touch_mask->moveForeground();

    _last_tooth_index = tooth_index_for_rotation(_display_rotation_deg);
    _last_motion_tick = GetHAL().millis();
    applyGearRotation(_last_motion_tick, true);
}

void RatchetView::update()
{
    const uint32_t now = GetHAL().millis();
    updateTouch();
    updateMotion(now);
    applyGearRotation(now);
}

void RatchetView::updateTouch()
{
    lv_indev_t* indev = GetHAL().lvTouchpad;
    if (indev == nullptr) {
        _dragging = false;
        return;
    }

    const bool is_pressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    if (!is_pressed) {
        _dragging = false;
        _target_rotation_deg = _display_rotation_deg;
        return;
    }

    if (!_dragging) {
        if (!isTouchOnGear(point)) {
            return;
        }
        _dragging             = true;
        _target_rotation_deg  = _display_rotation_deg;
        _last_touch_angle     = touchAngleDegrees(point);
        _last_tooth_index     = tooth_index_for_rotation(_display_rotation_deg);
        return;
    }

    const float current_angle = touchAngleDegrees(point);
    const float delta         = normalize_delta_degrees(current_angle - _last_touch_angle);
    _last_touch_angle         = current_angle;

    if (std::fabs(delta) < 0.01f) {
        return;
    }

    _target_rotation_deg += delta;

    const float pending = _target_rotation_deg - _display_rotation_deg;
    if (pending > _max_pending_rotation_deg) {
        _target_rotation_deg = _display_rotation_deg + _max_pending_rotation_deg;
    } else if (pending < -_max_pending_rotation_deg) {
        _target_rotation_deg = _display_rotation_deg - _max_pending_rotation_deg;
    }
}

void RatchetView::updateMotion(uint32_t now)
{
    if (_last_motion_tick == 0) {
        _last_motion_tick = now;
        return;
    }

    const uint32_t elapsed_ms = now - _last_motion_tick;
    if (elapsed_ms == 0) {
        return;
    }

    _last_motion_tick = now;

    const float delta = _target_rotation_deg - _display_rotation_deg;
    if (std::fabs(delta) < 0.01f) {
        return;
    }

    const float elapsed_sec = std::min(static_cast<float>(elapsed_ms), static_cast<float>(_feedback_interval_ms)) /
                              1000.0f;
    const float max_step = _max_rotation_speed_deg_s * elapsed_sec;
    if (std::fabs(delta) <= max_step) {
        _display_rotation_deg = _target_rotation_deg;
    } else {
        _display_rotation_deg += (delta > 0.0f ? max_step : -max_step);
    }

    updateToothFeedback();
}

void RatchetView::applyGearRotation(uint32_t now, bool force)
{
    if (_gear_image == nullptr) {
        return;
    }

    const int32_t rotation_tenth =
        normalize_rotation_tenth(static_cast<int32_t>(std::lround(_display_rotation_deg * 10.0f)));
    if (!force && _has_applied_rotation && rotation_tenth == _last_applied_rotation_tenth) {
        return;
    }
    if (!force && _has_applied_rotation && now - _last_rotation_apply_tick < _rotation_apply_ms) {
        return;
    }

    _gear_image->setRotation(rotation_tenth);
    _last_applied_rotation_tenth = rotation_tenth;
    _last_rotation_apply_tick    = now;
    _has_applied_rotation        = true;
}

bool RatchetView::isTouchOnGear(const lv_point_t& point) const
{
    const int dx = static_cast<int>(point.x) - _panel_center;
    const int dy = static_cast<int>(point.y) - _panel_center;
    const int distance_sq = dx * dx + dy * dy;
    return distance_sq >= _gear_touch_radius_min * _gear_touch_radius_min &&
           distance_sq <= _gear_touch_radius_max * _gear_touch_radius_max;
}

float RatchetView::touchAngleDegrees(const lv_point_t& point) const
{
    const float dx = static_cast<float>(point.x - _panel_center);
    const float dy = static_cast<float>(point.y - _panel_center);
    return std::atan2(dy, dx) * 180.0f / _pi;
}

void RatchetView::updateToothFeedback()
{
    const int tooth_index = tooth_index_for_rotation(_display_rotation_deg);
    if (tooth_index == _last_tooth_index) {
        return;
    }

    _last_tooth_index = tooth_index;
    triggerFeedback();
}

void RatchetView::triggerFeedback()
{
    const uint32_t now = GetHAL().millis();
    if (_has_feedback && now - _last_feedback_tick < _feedback_interval_ms) {
        return;
    }

    _has_feedback        = true;
    _last_feedback_tick  = now;
    const bool should_play_click = GetHAL().getSpeakerVolume() > 0;
    GetHAL().vibrate(_vibrate_duration_ms, _vibrate_strength);
    if (should_play_click) {
        play_ratchet_click();
    }
}
