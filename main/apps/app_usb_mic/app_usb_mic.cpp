/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_usb_mic.h"
#include <assets/assets.h>
#include <mooncake_log.h>
#include <smooth_lvgl.hpp>

using namespace mooncake;

namespace {

Hal::AudioSpectrumFrame muted_spectrum()
{
    return {};
}

}  // namespace

AppUsbMic::AppUsbMic()
{
    setAppInfo().name = "USB Mic";
    setAppInfo().icon = (void*)&icon_usb_mic;
}

void AppUsbMic::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppUsbMic::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    _saved_button_config     = GetHAL().getButtonConfig();
    _has_saved_button_config = true;
    auto button_config       = _saved_button_config;
    button_config.sfxEnabled = false;
    GetHAL().setButtonConfig(button_config, false);

    _usb_ready   = GetHAL().startUsbMic();
    _mic_enabled = _usb_ready;
    applyMicState(_mic_enabled);

    LvglLockGuard lock;
    _view = std::make_unique<view::FftView>();
    _view->init(lv_screen_active());
    _view->setCenterText(_usb_ready ? "ON" : "ERR", _usb_ready ? "USB" : "UAC");
}

void AppUsbMic::onRunning()
{
    if (_key_manager) {
        const auto& key_event = _key_manager->update();
        if (key_event == input::KeyEvent::GoHome) {
            close();
            return;
        }
        if (key_event == input::KeyEvent::GoNext && _usb_ready) {
            applyMicState(!_mic_enabled);
        }
    }

    auto spectrum = (_usb_ready && _mic_enabled) ? GetHAL().getUsbMicSpectrum() : muted_spectrum();

    LvglLockGuard lock;
    if (_view) {
        _view->setSpectrum(spectrum.bands);
        _view->setCenterText(_usb_ready ? (_mic_enabled ? "ON" : "MUTE") : "ERR", _usb_ready ? "USB" : "UAC");
        _view->update();
    }
}

void AppUsbMic::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    applyMicState(false);
    restoreButtonConfig();
    _key_manager.reset();

    LvglLockGuard lock;
    _view.reset();
}

void AppUsbMic::applyMicState(bool enabled)
{
    _mic_enabled = enabled && _usb_ready;
    GetHAL().setUsbMicMuted(!_mic_enabled);
}

void AppUsbMic::restoreButtonConfig()
{
    if (!_has_saved_button_config) {
        return;
    }

    GetHAL().setButtonConfig(_saved_button_config, false);
    _has_saved_button_config = false;
}
