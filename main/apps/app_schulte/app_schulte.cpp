/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_schulte.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

AppSchulte::AppSchulte()
{
    setAppInfo().name = "Schulte";
    setAppInfo().icon = (void*)&icon_schulte;
}

void AppSchulte::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppSchulte::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    GetHAL().setCenterOutFlushEnabled(true);
    _key_manager = std::make_unique<input::KeyManager>();

    LvglLockGuard lock;

    _view = std::make_unique<view::SchulteView>();
    _view->init(lv_screen_active());
}

void AppSchulte::onRunning()
{
    GetHAL().updateButtonStates();

    input::KeyEvent key_event = input::KeyEvent::None;
    if (_key_manager) {
        key_event = _key_manager->update(false);
    }

    if (key_event == input::KeyEvent::GoHome) {
        close();
        return;
    }

    LvglLockGuard lock;

    if (_view) {
        if (key_event == input::KeyEvent::GoPrevious) {
            _view->handleLeftKey();
        } else if (key_event == input::KeyEvent::GoNext) {
            _view->handleRightKey();
        }
        _view->update();
    }
}

void AppSchulte::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    GetHAL().setCenterOutFlushEnabled(false);
    _key_manager.reset();

    LvglLockGuard lock;

    _view.reset();
}
