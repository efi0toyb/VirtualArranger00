cmake_minimum_required(VERSION 3.24)

project(VirtualArranger VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. JUCE local path
if(NOT TARGET juce::juce_core)
    add_subdirectory("C:/JUCE" build/_deps/juce-build)
endif()

# 2. FluidSynth Search Paths
# Detect if there are nested subdirectories inside C:/fluidsynth
file(GLOB FLUIDSYNTH_CANDIDATE_DIRS "C:/fluidsynth/*")

set(POSSIBLE_INCLUDE_PATHS
    "C:/fluidsynth/include"
    "C:/fluidsynth"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/fluidsynth/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/fluidsynth"
)

set(POSSIBLE_LIB_PATHS
    "C:/fluidsynth/lib"
    "C:/fluidsynth/bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/fluidsynth/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/fluidsynth/bin"
)

foreach(DIR ${FLUIDSYNTH_CANDIDATE_DIRS})
    if(IS_DIRECTORY "${DIR}")
        list(APPEND POSSIBLE_INCLUDE_PATHS "${DIR}/include" "${DIR}")
        list(APPEND POSSIBLE_LIB_PATHS "${DIR}/lib" "${DIR}/bin")
    endif()
endforeach()

# Force CMake to re-search if previously NOTFOUND
if(FLUIDSYNTH_INCLUDE_DIR MATCHES "NOTFOUND")
    unset(FLUIDSYNTH_INCLUDE_DIR CACHE)
endif()
if(FLUIDSYNTH_LIBRARY MATCHES "NOTFOUND")
    unset(FLUIDSYNTH_LIBRARY CACHE)
endif()

find_path(FLUIDSYNTH_INCLUDE_DIR
    NAMES fluidsynth.h
    PATHS ${POSSIBLE_INCLUDE_PATHS}
)

find_library(FLUIDSYNTH_LIBRARY
    NAMES fluidsynth libfluidsynth libfluidsynth-3
    PATHS ${POSSIBLE_LIB_PATHS}
)

message(STATUS "--> Found FluidSynth Include: ${FLUIDSYNTH_INCLUDE_DIR}")
message(STATUS "--> Found FluidSynth Library: ${FLUIDSYNTH_LIBRARY}")

if(NOT FLUIDSYNTH_INCLUDE_DIR OR NOT FLUIDSYNTH_LIBRARY)
    message(FATAL_ERROR 
        "\nFluidSynth still not found by CMake.\n"
        "Please check: Where is 'fluidsynth.h' located on your C: drive?\n"
    )
endif()

# 3. Application Target
juce_add_gui_app(VirtualArranger
    PRODUCT_NAME "Virtual Arranger"
    VERSION "1.0.0"
)

juce_generate_juce_header(VirtualArranger)

target_sources(VirtualArranger PRIVATE
    Source/Main.cpp
    Source/MainComponent.h       # <--- ADD
    Source/MainComponent.cpp     # <--- ADD
    Source/MainAudioSource.h
    Source/MainAudioSource.cpp
    Source/SynthManager.h
    Source/SynthManager.cpp
    Source/ChordDetector.h
    Source/ChordDetector.cpp
    Source/StyleParser.h
    Source/StyleParser.cpp
    Source/StyleEngine.h
    Source/StyleEngine.cpp
)

target_include_directories(VirtualArranger PRIVATE
    Source
    ${FLUIDSYNTH_INCLUDE_DIR}
)

target_link_libraries(VirtualArranger PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    ${FLUIDSYNTH_LIBRARY}
)

# 4. Copy runtime DLLs next to .exe
get_filename_component(FLUIDSYNTH_ROOT "${FLUIDSYNTH_INCLUDE_DIR}/.." ABSOLUTE)
if(EXISTS "${FLUIDSYNTH_ROOT}/bin")
    add_custom_command(TARGET VirtualArranger POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FLUIDSYNTH_ROOT}/bin"
        "$<TARGET_FILE_DIR:VirtualArranger>"
        COMMENT "Copying FluidSynth DLLs..."
    )
endif()