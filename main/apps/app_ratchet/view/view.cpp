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
constexpr int _gear_size                  = 300;
constexpr int _gear_pivot                 = _gear_size / 2;
constexpr int _gear_teeth                 = 9;
constexpr int _gear_frame_count           = 32;
constexpr int _gear_touch_radius_min      = 32;
constexpr int _gear_touch_radius_max      = 178;
constexpr int _gear_outer_radius          = 139;
constexpr int _gear_root_radius           = 115;
constexpr int _gear_face_radius           = 102;
constexpr int _gear_hub_outer_radius      = 47;
constexpr int _gear_hub_mid_radius        = 36;
constexpr int _gear_hub_hole_radius       = 25;
constexpr int _notch_width                = 52;
constexpr int _notch_height               = 18;
constexpr int _notch_pivot_x              = _notch_width / 2;
constexpr int _notch_pivot_y              = _notch_height / 2;
constexpr int _notch_orbit_radius         = 36;
constexpr float _tooth_angle_deg          = 360.0f / static_cast<float>(_gear_teeth);
constexpr uint32_t _feedback_interval_ms  = 50;
constexpr uint16_t _vibrate_duration_ms   = 20;
constexpr uint8_t _vibrate_strength       = 90;
constexpr uint32_t _bg_color              = 0x000000;
constexpr float _pi                       = 3.14159265358979323846f;
constexpr int _gear_builder_stack_size    = 6144;
constexpr UBaseType_t _gear_builder_priority = tskIDLE_PRIORITY + 1;
constexpr int _gear_builder_yield_rows    = 8;
constexpr int _gear_frame_build_order[]   = {
    16, 8,  24, 4,  12, 20, 28, 2,  6,  10, 14, 18, 22, 26, 30, 1,
    3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,
};

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

float normalized_degrees(float degrees)
{
    degrees = std::fmod(degrees, 360.0f);
    if (degrees < 0.0f) {
        degrees += 360.0f;
    }
    return degrees;
}

float tooth_phase_degrees(float rotationDeg)
{
    float phase = std::fmod(rotationDeg, _tooth_angle_deg);
    if (phase < 0.0f) {
        phase += _tooth_angle_deg;
    }
    return phase;
}

int frame_index_for_rotation(float rotationDeg)
{
    const float phase = tooth_phase_degrees(rotationDeg);
    const auto frame =
        static_cast<int>(std::floor((phase / _tooth_angle_deg) * static_cast<float>(_gear_frame_count) + 0.5f));
    return frame % _gear_frame_count;
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

uint16_t rgb565_from_color(uint32_t color)
{
    const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xff);
    const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xff);
    const uint8_t b = static_cast<uint8_t>(color & 0xff);
    return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

uint32_t blend_over_black(uint32_t color, uint8_t alpha)
{
    const uint32_t r = (((color >> 16) & 0xff) * alpha) / 255;
    const uint32_t g = (((color >> 8) & 0xff) * alpha) / 255;
    const uint32_t b = ((color & 0xff) * alpha) / 255;
    return (r << 16) | (g << 8) | b;
}

void write_rgb565(uint8_t* data, std::size_t index, uint16_t color)
{
    data[index * 2]     = static_cast<uint8_t>(color & 0xff);
    data[index * 2 + 1] = static_cast<uint8_t>((color >> 8) & 0xff);
}

uint32_t compose_body_color(float radius, float degrees)
{
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

    for (int ring = 60; ring <= 96; ring += 6) {
        const uint32_t tone = (ring % 12 == 0) ? 0x20282f : 0x151d24;
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

    color = mix_color(color, 0xeaf0f2, smooth_band(radius - _gear_hub_hole_radius, 1.8f, 0.8f));

    return color;
}

uint8_t gear_alpha(float radius, float degrees)
{
    const float outer   = tooth_boundary_radius(degrees) - radius;
    const float hole    = radius - static_cast<float>(_gear_hub_hole_radius - 2);
    const float alpha   = clamp01((outer + 1.25f) / 2.5f) * clamp01((hole + 1.25f) / 2.5f);
    return static_cast<uint8_t>(std::lround(alpha * 255.0f));
}

uint8_t overlay_alpha_for_radius(float radius, float inner, float outer, float feather)
{
    const float inner_alpha = clamp01((radius - inner + feather) / feather);
    const float outer_alpha = clamp01((outer - radius + feather) / feather);
    return static_cast<uint8_t>(std::lround(inner_alpha * outer_alpha * 255.0f));
}

uint32_t compose_highlight_overlay_color(float x, float y, uint8_t& alpha)
{
    const float radius = std::sqrt(x * x + y * y);
    alpha              = 0;

    const float face_mask =
        static_cast<float>(overlay_alpha_for_radius(radius, _gear_hub_outer_radius + 7.0f, _gear_face_radius - 4.0f, 7.0f)) /
        255.0f;
    const float hub_mask =
        static_cast<float>(overlay_alpha_for_radius(radius, _gear_hub_hole_radius + 4.0f, _gear_hub_outer_radius - 4.0f, 5.0f)) /
        255.0f;

    const float top_left = clamp01(((-x - y) / static_cast<float>(_gear_face_radius) + 0.78f) * 0.58f);
    const float bottom_right = clamp01(((x + y) / static_cast<float>(_gear_face_radius) + 0.62f) * 0.42f);
    const float diagonal_band = smooth_band((x + y) * 0.707f + 22.0f, 13.0f, 22.0f);
    const float ring_lip =
        std::max(smooth_band(radius - (_gear_face_radius - 10.0f), 1.5f, 0.8f),
                 smooth_band(radius - (_gear_hub_outer_radius - 1.0f), 2.0f, 0.9f));
    const float hub_lip = smooth_band(radius - (_gear_hub_hole_radius + 1.5f), 1.4f, 0.8f);

    float shine = face_mask * (top_left * 0.16f + diagonal_band * 0.14f) +
                  hub_mask * (top_left * 0.18f + ring_lip * 0.24f + hub_lip * 0.20f);
    float shade = face_mask * bottom_right * 0.10f;

    if (shine >= shade) {
        alpha = static_cast<uint8_t>(std::lround(clamp01(shine) * 255.0f));
        return 0xf4fbff;
    }

    alpha = static_cast<uint8_t>(std::lround(clamp01(shade) * 255.0f));
    return 0x000000;
}

uint32_t compose_notch_color(float x, float y, uint8_t& alpha)
{
    const float half_len   = 19.0f;
    const float half_width = 3.4f;
    const float dx         = std::fabs(x) - half_len;
    const float dy         = std::fabs(y) - half_width;
    const float outside    = std::max(dx, dy);
    const float coverage   = clamp01((2.0f - outside) / 2.0f);
    const float end_round  = clamp01((half_len + 4.5f - std::sqrt(x * x + y * y)) / 3.8f);
    const float body       = std::min(coverage, std::max(0.0f, end_round));

    alpha = static_cast<uint8_t>(std::lround(body * 230.0f));
    if (alpha == 0) {
        return 0x000000;
    }

    const float highlight = clamp01((-y + 2.5f) / 7.5f);
    return mix_color(0x4a535a, 0xf1f8fb, highlight);
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

RatchetView::~RatchetView()
{
    stopGearBuilderTask();
}

void RatchetView::buildRuntimeGearFrames()
{
    constexpr std::size_t pixel_count = static_cast<std::size_t>(_gear_size) * static_cast<std::size_t>(_gear_size);
    constexpr std::size_t frame_bytes = pixel_count * 2;
    _gear_frame_data.assign(frame_bytes * _gear_frame_count, 0);
    _gear_frame_dscs.assign(_gear_frame_count, {});
    _gear_frame_ready.assign(_gear_frame_count, 0);
    _gear_radius_cache.assign(pixel_count, 0);
    _gear_degrees_cache.assign(pixel_count, 0);

    const float center = static_cast<float>(_gear_size) * 0.5f;

    for (int frame = 0; frame < _gear_frame_count; ++frame) {
        uint8_t* frame_data = _gear_frame_data.data() + (static_cast<std::size_t>(frame) * frame_bytes);
        auto& dsc            = _gear_frame_dscs[frame];
        dsc                  = {};
        dsc.header.cf        = LV_COLOR_FORMAT_RGB565;
        dsc.header.magic     = LV_IMAGE_HEADER_MAGIC;
        dsc.header.w         = _gear_size;
        dsc.header.h         = _gear_size;
        dsc.data_size        = frame_bytes;
        dsc.data             = frame_data;
    }

    for (int y = 0; y < _gear_size; ++y) {
        for (int x = 0; x < _gear_size; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(_gear_size) +
                                      static_cast<std::size_t>(x);
            const float local_x = static_cast<float>(x) + 0.5f - center;
            const float local_y = static_cast<float>(y) + 0.5f - center;
            _gear_radius_cache[index] =
                static_cast<uint16_t>(std::lround(std::sqrt(local_x * local_x + local_y * local_y) * 16.0f));
            _gear_degrees_cache[index] =
                static_cast<int16_t>(std::lround((std::atan2(local_y, local_x) * 180.0f / _pi) * 10.0f));
        }
    }

    renderGearFrame(0);
}

void RatchetView::renderGearFrame(int frame)
{
    if (frame < 0 || frame >= _gear_frame_count || _gear_frame_ready.empty()) {
        return;
    }
    if (_gear_frame_ready[frame] != 0) {
        return;
    }

    constexpr std::size_t pixel_count = static_cast<std::size_t>(_gear_size) * static_cast<std::size_t>(_gear_size);
    constexpr std::size_t frame_bytes = pixel_count * 2;
    uint8_t* frame_data = _gear_frame_data.data() + (static_cast<std::size_t>(frame) * frame_bytes);
    const float phase   = (static_cast<float>(frame) / static_cast<float>(_gear_frame_count)) * _tooth_angle_deg;

    const bool background_build = _gear_builder_task != nullptr && xTaskGetCurrentTaskHandle() == _gear_builder_task;

    for (int y = 0; y < _gear_size; ++y) {
        if (background_build && _gear_builder_stop.load()) {
            return;
        }

        for (int x = 0; x < _gear_size; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(_gear_size) +
                                      static_cast<std::size_t>(x);
            const float radius  = static_cast<float>(_gear_radius_cache[index]) / 16.0f;
            const float degrees = (static_cast<float>(_gear_degrees_cache[index]) / 10.0f) - phase;
            const uint8_t alpha = gear_alpha(radius, degrees);
            const uint32_t color =
                alpha == 0 ? 0x000000 : blend_over_black(compose_body_color(radius, degrees), alpha);
            write_rgb565(frame_data, index, rgb565_from_color(color));
        }

        if (background_build && (y % _gear_builder_yield_rows) == 0) {
            vTaskDelay(1);
        }
    }

    _gear_frame_ready[frame] = 1;
}

void RatchetView::startGearBuilderTask()
{
    if (_gear_builder_task != nullptr) {
        return;
    }

    _gear_builder_stop.store(false);
    const BaseType_t result = xTaskCreate(
        gearBuilderTaskEntry,
        "ratchet_frames",
        _gear_builder_stack_size,
        this,
        _gear_builder_priority,
        &_gear_builder_task);
    if (result != pdPASS) {
        _gear_builder_task = nullptr;
        return;
    }
}

void RatchetView::stopGearBuilderTask()
{
    _gear_builder_stop.store(true);
    while (_gear_builder_task != nullptr) {
        vTaskDelay(1);
    }
}

void RatchetView::runGearBuilderTask()
{
    for (int frame : _gear_frame_build_order) {
        if (_gear_builder_stop.load()) {
            break;
        }
        renderGearFrame(frame);
        vTaskDelay(1);
    }

    clearGearBuildCache();
    _gear_builder_task = nullptr;
}

void RatchetView::gearBuilderTaskEntry(void* userData)
{
    auto* self = static_cast<RatchetView*>(userData);
    if (self != nullptr) {
        self->runGearBuilderTask();
    }
    vTaskDelete(nullptr);
}

int RatchetView::nearestReadyFrameIndex(int requestedFrame) const
{
    if (_gear_frame_ready.empty()) {
        return 0;
    }
    if (_gear_frame_ready[requestedFrame] != 0) {
        return requestedFrame;
    }

    for (int offset = 1; offset <= _gear_frame_count / 2; ++offset) {
        const int forward = (requestedFrame + offset) % _gear_frame_count;
        if (_gear_frame_ready[forward] != 0) {
            return forward;
        }

        const int backward = (requestedFrame - offset + _gear_frame_count) % _gear_frame_count;
        if (_gear_frame_ready[backward] != 0) {
            return backward;
        }
    }

    return 0;
}

void RatchetView::clearGearBuildCache()
{
    _gear_radius_cache.clear();
    _gear_radius_cache.shrink_to_fit();
    _gear_degrees_cache.clear();
    _gear_degrees_cache.shrink_to_fit();
}

void RatchetView::buildHighlightOverlayImage()
{
    constexpr std::size_t pixel_count = static_cast<std::size_t>(_gear_size) * static_cast<std::size_t>(_gear_size);
    _highlight_overlay_data.assign(pixel_count * 3, 0);

    uint8_t* color_plane = _highlight_overlay_data.data();
    uint8_t* alpha_plane = color_plane + pixel_count * 2;
    const float center   = static_cast<float>(_gear_size) * 0.5f;

    for (int y = 0; y < _gear_size; ++y) {
        for (int x = 0; x < _gear_size; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(_gear_size) +
                                      static_cast<std::size_t>(x);
            const float local_x = static_cast<float>(x) + 0.5f - center;
            const float local_y = static_cast<float>(y) + 0.5f - center;

            uint8_t alpha       = 0;
            const uint32_t color = compose_highlight_overlay_color(local_x, local_y, alpha);
            write_rgb565(color_plane, index, rgb565_from_color(color));
            alpha_plane[index] = alpha;
        }
    }

    _highlight_overlay_dsc              = {};
    _highlight_overlay_dsc.header.cf    = LV_COLOR_FORMAT_RGB565A8;
    _highlight_overlay_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    _highlight_overlay_dsc.header.w     = _gear_size;
    _highlight_overlay_dsc.header.h     = _gear_size;
    _highlight_overlay_dsc.data_size    = pixel_count * 3;
    _highlight_overlay_dsc.data         = _highlight_overlay_data.data();
}

void RatchetView::buildNotchImage()
{
    constexpr std::size_t pixel_count = static_cast<std::size_t>(_notch_width) * static_cast<std::size_t>(_notch_height);
    _notch_image_data.assign(pixel_count * 3, 0);

    uint8_t* color_plane = _notch_image_data.data();
    uint8_t* alpha_plane = color_plane + pixel_count * 2;

    for (int y = 0; y < _notch_height; ++y) {
        for (int x = 0; x < _notch_width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(_notch_width) +
                                      static_cast<std::size_t>(x);
            const float local_x = static_cast<float>(x) + 0.5f - static_cast<float>(_notch_pivot_x);
            const float local_y = static_cast<float>(y) + 0.5f - static_cast<float>(_notch_pivot_y);

            uint8_t alpha       = 0;
            const uint32_t color = compose_notch_color(local_x, local_y, alpha);
            write_rgb565(color_plane, index, rgb565_from_color(color));
            alpha_plane[index] = alpha;
        }
    }

    _notch_image_dsc              = {};
    _notch_image_dsc.header.cf    = LV_COLOR_FORMAT_RGB565A8;
    _notch_image_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    _notch_image_dsc.header.w     = _notch_width;
    _notch_image_dsc.header.h     = _notch_height;
    _notch_image_dsc.data_size    = pixel_count * 3;
    _notch_image_dsc.data         = _notch_image_data.data();
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
    buildRuntimeGearFrames();
    buildHighlightOverlayImage();
    buildNotchImage();

    _gear_image = std::make_unique<Image>(_panel->get());
    _gear_image->setSrc(&_gear_frame_dscs[0]);
    _gear_image->setPos(_panel_center - _gear_pivot, _panel_center - _gear_pivot);

    _highlight_overlay = std::make_unique<Image>(_panel->get());
    _highlight_overlay->setSrc(&_highlight_overlay_dsc);
    _highlight_overlay->setPos(_panel_center - _gear_pivot, _panel_center - _gear_pivot);

    _notch_image = std::make_unique<Image>(_panel->get());
    _notch_image->setSrc(&_notch_image_dsc);
    _notch_image->setPivot(_notch_pivot_x, _notch_pivot_y);

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
    applyGearFrame(true);
    applyNotchTransform();
    startGearBuilderTask();
}

void RatchetView::update()
{
    updateTouch();
    applyGearFrame();
    applyNotchTransform();
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
        return;
    }

    if (!_dragging) {
        if (!isTouchOnGear(point)) {
            return;
        }
        _dragging             = true;
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

    _display_rotation_deg += delta;
    if (std::fabs(_display_rotation_deg) > 36000.0f) {
        _display_rotation_deg = std::fmod(_display_rotation_deg, 360.0f);
    }
    updateToothFeedback();
}

void RatchetView::applyGearFrame(bool force)
{
    if (_gear_image == nullptr || _gear_frame_dscs.empty()) {
        return;
    }

    const int frame_index = nearestReadyFrameIndex(frame_index_for_rotation(_display_rotation_deg));
    if (!force && frame_index == _last_frame_index) {
        return;
    }

    _gear_image->setSrc(&_gear_frame_dscs[frame_index]);
    _last_frame_index = frame_index;
}

void RatchetView::applyNotchTransform()
{
    if (_notch_image == nullptr) {
        return;
    }

    const float notch_angle = normalized_degrees(_display_rotation_deg - 70.0f);
    const float rad         = notch_angle * _pi / 180.0f;
    const int x             = static_cast<int>(std::lround(_panel_center + (std::cos(rad) * _notch_orbit_radius))) -
                  _notch_pivot_x;
    const int y = static_cast<int>(std::lround(_panel_center + (std::sin(rad) * _notch_orbit_radius))) -
                  _notch_pivot_y;

    _notch_image->setPos(x, y);
    _notch_image->setRotation(
        normalize_rotation_tenth(static_cast<int32_t>(std::lround(notch_angle * 10.0f))));
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
