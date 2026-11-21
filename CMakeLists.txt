cmake_minimum_required(VERSION 3.20)
project(War_Strategy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

find_package(Threads REQUIRED)

if(NOT GIT_EXECUTABLE)
    find_program(GIT_EXECUTABLE NAMES git git.exe PATHS "C:/Users/quangnguyen/AppData/Local/Programs/Git/cmd" "C:/Program Files/Git/cmd" "C:/Program Files/Git/bin")
    if(GIT_EXECUTABLE)
        set(GIT_EXECUTABLE "${GIT_EXECUTABLE}" CACHE FILEPATH "Git executable" FORCE)
    endif()
endif()

include(FetchContent)

FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.8
)

set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
FetchContent_Declare(
        SDL2_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-2.22.0
)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL2 SDL2_ttf nlohmann_json)

# ==============================================================================
# Core Simulation Static Library (Pure C++20, No SDL2 dependency)
# ==============================================================================
set(CORE_SOURCES
    src/mapgen/FastRng.hpp
    src/mapgen/MapGenConfig.hpp
    src/mapgen/MapGenerator.hpp
    src/mapgen/MapGenerator.cpp
    src/mapgen/SimplexNoise.hpp
    src/mapgen/SimplexNoise.cpp

    src/sim/WorldSim.hpp
    src/sim/WorldSim.cpp
    src/sim/combat/CombatSystem.hpp
    src/sim/combat/CombatSystem.cpp
    src/sim/commands/Command.hpp
    src/sim/config/SimConfig.hpp
    src/sim/config/SimConfig.cpp
    src/sim/map/WorldMap.hpp
    src/sim/map/WorldMap.cpp
    src/sim/pathfinding/Pathfinder.hpp
    src/sim/pathfinding/Pathfinder.cpp
    src/sim/units/Unit.hpp
    src/sim/units/Unit.cpp
    src/sim/vision/VisionSystem.hpp
    src/sim/vision/VisionSystem.cpp

    src/ai/AiParams.hpp
    src/ai/AiTelemetry.hpp
    src/ai/TacticalAI.hpp
    src/ai/TacticalAI.cpp
    src/ai/AiTrainer.hpp
    src/ai/AiTrainer.cpp

    src/replay/Replay.hpp
    src/replay/Replay.cpp
)

add_library(war_sim_core STATIC ${CORE_SOURCES})
target_include_directories(war_sim_core PUBLIC src)
target_link_libraries(war_sim_core PUBLIC nlohmann_json::nlohmann_json Threads::Threads)

# ==============================================================================
# Graphical Application (War_Strategy: SDL2 + SDL2_ttf)
# ==============================================================================
set(GUI_SOURCES
    src/render/Camera.hpp
    src/render/Camera.cpp
    src/render/MapRenderer.hpp
    src/render/MapRenderer.cpp
    src/render/TextRenderer.hpp
    src/render/TextRenderer.cpp
    src/render/UnitRenderer.hpp
    src/render/UnitRenderer.cpp
    src/render/Window.hpp
    src/render/Window.cpp
    src/app/GameApp.hpp
    src/app/GameApp.cpp
    src/app/main.cpp
)

add_executable(War_Strategy ${GUI_SOURCES})
target_include_directories(War_Strategy PRIVATE src)
target_link_libraries(War_Strategy PRIVATE war_sim_core SDL2::SDL2main SDL2::SDL2 SDL2_ttf::SDL2_ttf)

if(WIN32)
    add_custom_command(TARGET War_Strategy POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
            $<TARGET_FILE_DIR:War_Strategy>
    )
endif()

# ==============================================================================
# Headless Runner Executable (War_Strategy_Headless: Pure C++20, NO SDL2)
# ==============================================================================
add_executable(War_Strategy_Headless src/headless/main_headless.cpp)
target_include_directories(War_Strategy_Headless PRIVATE src)
target_link_libraries(War_Strategy_Headless PRIVATE war_sim_core)

# ==============================================================================
# AI Self-Play & Trainer Executable (War_Strategy_Train: Pure C++20, NO SDL2)
# ==============================================================================
add_executable(War_Strategy_Train src/train/main_train.cpp)
target_include_directories(War_Strategy_Train PRIVATE src)
target_link_libraries(War_Strategy_Train PRIVATE war_sim_core Threads::Threads)

# ==============================================================================
# Testing
# ==============================================================================
enable_testing()

add_executable(test_mapgen tests/test_mapgen.cpp)
target_include_directories(test_mapgen PRIVATE src)
target_link_libraries(test_mapgen PRIVATE war_sim_core)
add_test(NAME test_mapgen COMMAND test_mapgen)

add_executable(test_units tests/test_units.cpp)
target_include_directories(test_units PRIVATE src)
target_link_libraries(test_units PRIVATE war_sim_core)
add_test(NAME test_units COMMAND test_units)

add_executable(test_pathfinding tests/test_pathfinding.cpp)
target_include_directories(test_pathfinding PRIVATE src)
target_link_libraries(test_pathfinding PRIVATE war_sim_core)
add_test(NAME test_pathfinding COMMAND test_pathfinding)

add_executable(test_combat tests/test_combat.cpp)
target_include_directories(test_combat PRIVATE src)
target_link_libraries(test_combat PRIVATE war_sim_core)
add_test(NAME test_combat COMMAND test_combat)

add_executable(test_vision tests/test_vision.cpp)
target_include_directories(test_vision PRIVATE src)
target_link_libraries(test_vision PRIVATE war_sim_core)
add_test(NAME test_vision COMMAND test_vision)

add_executable(test_ai tests/test_ai.cpp)
target_include_directories(test_ai PRIVATE src)
target_link_libraries(test_ai PRIVATE war_sim_core)
add_test(NAME test_ai COMMAND test_ai)

add_executable(test_replay tests/test_replay.cpp)
target_include_directories(test_replay PRIVATE src)
target_link_libraries(test_replay PRIVATE war_sim_core)
add_test(NAME test_replay COMMAND test_replay)

add_executable(test_train tests/test_train.cpp)
target_include_directories(test_train PRIVATE src)
target_link_libraries(test_train PRIVATE war_sim_core Threads::Threads)
add_test(NAME test_train COMMAND test_train)

# ==============================================================================
# All C Test Runner Target
# ==============================================================================
add_custom_target(all_c_test
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_mapgen test_units test_pathfinding test_combat test_vision test_ai test_replay test_train
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all C tests via CTest"
)

