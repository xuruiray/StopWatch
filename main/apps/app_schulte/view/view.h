/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace view {

class SchulteView {
public:
    enum class FlashType {
        None,
        Hit,
        Miss,
    };

    struct Ring {
        float inner;
        float outer;
        int count;
        float phase;
    };

    void init(lv_obj_t* parent);
    void update();
    void handleLeftKey();
    void handleRightKey();
    void drawBoard(lv_event_t* e);
    void drawFlashOverlay(lv_event_t* e);
    void drawTransitionOverlay(lv_event_t* e);

private:
    enum class State {
        Idle,
        Running,
        Result,
    };

    struct Cell {
        int ring_index;
        int local_index;
        int value;
        uint32_t color;
        lv_area_t bounds;
        bool done;
    };

    struct LabelSlot {
        std::unique_ptr<uitk::lvgl_cpp::Label> label;
        int cell_index = -1;
    };

    void showIdleBoard(bool animate = false);
    void switchDensity();
    void startGame();
    void finishGame();
    void renderBoard(bool showNumbers);
    void createLabelPool();
    void createNumberLabels();
    void updateResultVisibility();
    void updateTouch(uint32_t now);
    void updateFlash(uint32_t now);
    void updateTransition(uint32_t now);
    void handleCellPress(int cellIndex, uint32_t now);
    void startFlash(int cellIndex, FlashType type, uint32_t durationMs, uint32_t now);
    void clearFlash();
    void fitFlashOverlayToCell(int cellIndex);
    void triggerCellFeedback(FlashType type);
    void startTransition();
    void stopTransition();
    void invalidateBoard();

    const std::array<Ring, 3>& currentLayout() const;
    const Ring& ringForCell(const Cell& cell) const;
    const lv_font_t* fontForSize(int fontSize) const;
    int currentDensity() const;
    int numberFontSize(int ringIndex) const;
    int cellIndexAtPoint(lv_point_t point) const;
    int ringStartIndex(int ringIndex) const;
    float ringPhaseOffset(int ringIndex) const;
    float normalizeDegrees(float degrees) const;
    lv_point_t polarToPoint(float radius, float degrees) const;
    lv_point_t cellLabelPoint(const Cell& cell) const;
    lv_area_t cellBounds(const Cell& cell) const;
    uint32_t createColorVariant(uint32_t baseColor);
    uint32_t pickColorBase(const std::vector<uint32_t>& excludedColors);
    void createGameColors();
    void createRingPhaseOffsets();
    void shuffleNumbers(std::vector<int>& values);
    std::string formatTime(uint32_t ms) const;

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _board;
    std::unique_ptr<uitk::lvgl_cpp::Container> _flash_overlay;
    std::vector<LabelSlot> _number_labels;
    std::unique_ptr<uitk::lvgl_cpp::Label> _density_hint_label;
    std::unique_ptr<uitk::lvgl_cpp::Container> _result_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _result_label;
    std::unique_ptr<uitk::lvgl_cpp::Container> _transition_overlay;

    State _state = State::Idle;
    int _density_index = 0;
    std::vector<Cell> _cells;
    std::vector<uint32_t> _game_colors;
    std::array<float, 3> _ring_phase_offsets = {0.0f, 0.0f, 0.0f};
    int _target = 1;
    uint32_t _start_time = 0;
    uint32_t _stop_time = 0;
    bool _touch_was_pressed = false;
    int _flash_cell_index = -1;
    FlashType _flash_type = FlashType::None;
    uint32_t _flash_until_tick = 0;
    bool _transition_active = false;
    uint32_t _transition_start_tick = 0;
    int _transition_step = -1;
};

}  // namespace view
