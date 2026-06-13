/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <vector>

namespace view {

class RatchetView {
public:
    ~RatchetView();

    void init(lv_obj_t* parent);
    void update(bool leftButtonPressed = false, bool rightButtonPressed = false, bool bothButtonsPressed = false);

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _touch_mask;
    std::vector<uint8_t> _gear_frame_data;
    std::vector<lv_image_dsc_t> _gear_frame_dscs;
    std::vector<uint8_t> _gear_frame_ready;
    std::vector<uint16_t> _gear_radius_cache;
    std::vector<int16_t> _gear_degrees_cache;
    std::unique_ptr<uitk::lvgl_cpp::Image> _gear_image;
    std::vector<uint8_t> _highlight_overlay_data;
    lv_image_dsc_t _highlight_overlay_dsc = {};
    std::unique_ptr<uitk::lvgl_cpp::Image> _highlight_overlay;
    std::vector<uint8_t> _notch_image_data;
    lv_image_dsc_t _notch_image_dsc = {};
    std::unique_ptr<uitk::lvgl_cpp::Image> _notch_image;

    bool _dragging          = false;
    float _display_rotation_deg = 0.0f;
    float _angular_velocity_deg_s = 0.0f;
    float _last_touch_angle = 0.0f;
    int _last_tooth_index   = 0;
    bool _has_feedback      = false;
    uint32_t _last_feedback_tick = 0;
    uint32_t _last_motion_tick = 0;
    uint32_t _last_touch_tick = 0;
    uint32_t _last_drag_motion_tick = 0;
    int _last_frame_index = -1;
    bool _inertia_active = false;
    TaskHandle_t _gear_builder_task = nullptr;
    std::atomic_bool _gear_builder_stop { false };

    void updateTouch(uint32_t now);
    void updateButtonInput(uint32_t now, bool leftButtonPressed, bool rightButtonPressed, bool bothButtonsPressed);
    void updateInertia(uint32_t now);
    void stopMotion(bool stopFeedback = false);
    void applyGearFrame(bool force = false);
    void applyNotchTransform();
    void renderGearFrame(int frame);
    void clearGearBuildCache();
    void startGearBuilderTask();
    void stopGearBuilderTask();
    void runGearBuilderTask();
    int nearestReadyFrameIndex(int requestedFrame) const;
    static void gearBuilderTaskEntry(void* userData);
    bool isTouchOnGear(const lv_point_t& point) const;
    float touchAngleDegrees(const lv_point_t& point) const;
    void updateToothFeedback();
    void triggerFeedback();
    void buildRuntimeGearFrames();
    void buildHighlightOverlayImage();
    void buildNotchImage();
};

}  // namespace view
