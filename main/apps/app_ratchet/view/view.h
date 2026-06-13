/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <memory>
#include <vector>

namespace view {

class RatchetView {
public:
    void init(lv_obj_t* parent);
    void update();

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _touch_mask;
    std::vector<uint8_t> _gear_image_data;
    lv_image_dsc_t _gear_image_dsc = {};
    std::unique_ptr<uitk::lvgl_cpp::Image> _gear_image;

    bool _dragging          = false;
    float _target_rotation_deg  = 0.0f;
    float _display_rotation_deg = 0.0f;
    float _last_touch_angle = 0.0f;
    int _last_tooth_index   = 0;
    bool _has_feedback      = false;
    uint32_t _last_feedback_tick = 0;
    uint32_t _last_motion_tick   = 0;
    uint32_t _last_rotation_apply_tick = 0;
    int32_t _last_applied_rotation_tenth = 0;
    bool _has_applied_rotation = false;

    void updateTouch();
    void updateMotion(uint32_t now);
    void applyGearRotation(uint32_t now, bool force = false);
    bool isTouchOnGear(const lv_point_t& point) const;
    float touchAngleDegrees(const lv_point_t& point) const;
    void updateToothFeedback();
    void triggerFeedback();
    void buildRuntimeGearImage();
};

}  // namespace view
