cmake_minimum_required(VERSION 3.25)

project(il2_vr_qol VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

find_path(OPENVR_INCLUDE_DIR NAMES openvr.h REQUIRED)
find_library(OPENVR_LIBRARY NAMES openvr_api REQUIRED)

add_library(il2vr_core STATIC
    src/core/LocaleManager.cpp
    src/core/MathTypes.cpp
    src/vr/OpenVrBackend.cpp
    src/seat/SeatManager.cpp
    src/output/VJoyClient.cpp
    src/output/KeyboardInjector.cpp
    src/telemetry/TelemetryListener.cpp
    src/telemetry/Il2TelemetryParser.cpp
    src/haptics/HapticsMixer.cpp
)

target_include_directories(il2vr_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${OPENVR_INCLUDE_DIR}
)

target_link_libraries(il2vr_core
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        glm::glm
    PRIVATE
        ${OPENVR_LIBRARY}
        ws2_32
        user32
)

target_compile_definitions(il2vr_core
    PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(il2vr_core PRIVATE /W4 /utf-8)
endif()

add_executable(il2vr_companion
    src/app/main.cpp
)

target_link_libraries(il2vr_companion
    PRIVATE
        il2vr_core
        imgui::imgui
        d3d11
        dxgi
)

if(MSVC)
    target_compile_options(il2vr_companion PRIVATE /W4 /utf-8)
endif()

function(add_il2vr_spike target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} PRIVATE il2vr_core)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /utf-8)
    endif()
endfunction()

add_il2vr_spike(spike_s2_seated_pose spikes/s2_seated_pose/main.cpp)
add_il2vr_spike(spike_s3_vr_input    spikes/s3_vr_input/main.cpp)
add_il2vr_spike(spike_s4_vjoy        spikes/s4_vjoy/main.cpp)
add_il2vr_spike(spike_s5_telemetry   spikes/s5_telemetry/main.cpp)
add_il2vr_spike(spike_s6_keyboard    spikes/s6_keyboard/main.cpp)

include(CTest)
if(BUILD_TESTING)
    add_executable(il2vr_tests
        tests/test_main.cpp
    )
    target_link_libraries(il2vr_tests
        PRIVATE
            il2vr_core
            Catch2::Catch2WithMain
    )
    if(MSVC)
        target_compile_options(il2vr_tests PRIVATE /W4 /utf-8)
    endif()

    add_test(NAME il2vr_tests COMMAND il2vr_tests)
endif()
