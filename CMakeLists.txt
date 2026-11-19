cmake_minimum_required(VERSION 3.20)
project(War_Strategy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

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

FetchContent_MakeAvailable(SDL2 SDL2_ttf)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(War_Strategy ${GAME_SOURCES})
target_include_directories(War_Strategy PRIVATE src)
target_link_libraries(War_Strategy PRIVATE SDL2::SDL2main SDL2::SDL2 SDL2_ttf::SDL2_ttf)

if(WIN32)
    add_custom_command(TARGET War_Strategy POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
            $<TARGET_FILE_DIR:War_Strategy>
    )
endif()

# ==============================================================================
# Testing (M2: Kiem thu map generator tat dinh va ti le 5 dat : 1 bien)
# ==============================================================================
enable_testing()

add_executable(test_mapgen
        tests/test_mapgen.cpp
        src/mapgen/MapGenerator.cpp
        src/mapgen/SimplexNoise.cpp
        src/sim/map/WorldMap.cpp
)
target_include_directories(test_mapgen PRIVATE src)
add_test(NAME test_mapgen COMMAND test_mapgen)
