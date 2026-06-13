/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <apps/app_fft/view/view.h>
#include <apps/common/key_manager/key_manager.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <memory>

class AppUsbMic : public mooncake::AppAbility {
public:
    AppUsbMic();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::FftView> _view;
    Hal::ButtonConfig _saved_button_config;
    bool _has_saved_button_config = false;
    bool _usb_ready               = false;
    bool _mic_enabled             = false;

    void applyMicState(bool enabled);
    void restoreButtonConfig();
};
