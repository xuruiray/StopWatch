/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <mooncake.h>
#include <memory>

/**
 * @brief Ratchet gear app.
 *
 */
class AppRatchet : public mooncake::AppAbility {
public:
    AppRatchet();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<view::RatchetView> _view;
    std::unique_ptr<input::KeyManager> _key_manager;
};
