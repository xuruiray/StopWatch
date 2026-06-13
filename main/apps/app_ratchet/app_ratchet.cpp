/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_ratchet.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <cstdint>
#include <vector>

using namespace mooncake;

AppRatchet::AppRatchet()
{
    setAppInfo().name = "Ratchet";
    setAppInfo().icon = (void*)&icon_ratchet;
}

void AppRatchet::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppRatchet::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    LvglLockGuard lock;

    _view = std::make_unique<view::RatchetView>();
    _view->init(lv_screen_active());
}

void AppRatchet::onRunning()
{
    GetHAL().updateButtonStates();
    if (_key_manager && _key_manager->update(false) == input::KeyEvent::GoHome) {
        close();
        return;
    }

    const bool both_buttons_pressed = GetHAL().btnA.isPressed() && GetHAL().btnB.isPressed();
    const bool left_button_pressed  = !both_buttons_pressed && GetHAL().btnA.wasPressed();
    const bool right_button_pressed = !both_buttons_pressed && GetHAL().btnB.wasPressed();

    LvglLockGuard lock;

    if (_view) {
        _view->update(left_button_pressed, right_button_pressed, both_buttons_pressed);
    }
}

void AppRatchet::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();
    GetHAL().stopVibrate();
    std::vector<int16_t> empty_audio;
    GetHAL().audioPlay(empty_audio, true);

    LvglLockGuard lock;

    _view.reset();
}
