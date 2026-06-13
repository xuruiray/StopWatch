file(GLOB_RECURSE MY_SRCS
    "apps/*.c"
    "apps/*.cc"
    "apps/*.cpp"
    "assets/*.c"
    "assets/*.cc"
    "assets/*.cpp"
    "hal/*.c"
    "hal/*.cc"
    "hal/*.cpp"
)

set(SCHULTE_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}/../Schulte-StopWatch/hardware/mooncake/main")
if(NOT EXISTS "${SCHULTE_MODULE_DIR}/apps/app_schulte/app_schulte.cpp")
    message(FATAL_ERROR "Schulte-StopWatch submodule is missing. Run: git submodule update --init --recursive")
endif()
list(APPEND MY_SRCS
    "${SCHULTE_MODULE_DIR}/apps/app_schulte/app_schulte.cpp"
    "${SCHULTE_MODULE_DIR}/apps/app_schulte/view/view.cpp"
    "${SCHULTE_MODULE_DIR}/assets/images/icon_schulte.c"
)
list(APPEND MY_EXTRA_INCS
    "${SCHULTE_MODULE_DIR}/apps"
)

set(MY_INCS
    "."
    ${MY_EXTRA_INCS}
)

idf_component_register(
    SRCS 
        "main.cpp"
        ${MY_SRCS}
    INCLUDE_DIRS 
        ${MY_INCS}
    EMBED_FILES
        "assets/sfx/boot_sfx.bin"
    EMBED_TXTFILES
        "hal/utils/config_ap/assets/badge_config_ap.html"
)
