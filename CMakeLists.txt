cmake_minimum_required(VERSION 3.20)
project(fmod_fm_synth_dsp VERSION 1.0.0 LANGUAGES CXX)

# Enforce C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# FMOD Core SDK Configuration
# ---------------------------------------------------------------------------
set(FMOD_SDK_DIR "" CACHE PATH "Percorso della cartella API di FMOD Core")

if(NOT FMOD_SDK_DIR AND DEFINED ENV{FMOD_SDK_DIR})
    set(FMOD_SDK_DIR "$ENV{FMOD_SDK_DIR}" CACHE PATH "Percorso della cartella API di FMOD Core" FORCE)
endif()

message(STATUS "FMOD_SDK_DIR impostato a: '${FMOD_SDK_DIR}'")

# Search paths for FMOD headers (supports standard FMOD distributions: inc/ or include/)
set(FMOD_INC_SEARCH_PATHS
    "${FMOD_SDK_DIR}/inc"
    "${FMOD_SDK_DIR}/include"
    "${FMOD_SDK_DIR}/api/core/inc"
    "${FMOD_SDK_DIR}/api/core/include"
    "${FMOD_SDK_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/fmod"
)

find_path(FMOD_INCLUDE_DIR
    NAMES fmod_dsp.h fmod.h
    PATHS ${FMOD_INC_SEARCH_PATHS}
    DOC "Directory contenente gli header di FMOD Core (fmod_dsp.h)"
)

if(NOT FMOD_INCLUDE_DIR)
    message(FATAL_ERROR
        "Header di FMOD non trovati! Assicurati di impostare -DFMOD_SDK_DIR=<percorso_fmod_api> "
        "puntando alla cartella contenente la sottocartella 'inc' o 'include'."
    )
else()
    message(STATUS "FMOD include directory trovata: ${FMOD_INCLUDE_DIR}")
endif()

# ---------------------------------------------------------------------------
# Target Definition: Shared Library
# ---------------------------------------------------------------------------
add_library(fmod_fm_synth_dsp SHARED
    include/FMSynthDSP.h
    src/FMSynthDSP.cpp
)

# Header include paths
target_include_directories(fmod_fm_synth_dsp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${FMOD_INCLUDE_DIR}
)

# Note: FMOD DSP plugins are loaded dynamically at runtime by FMOD Studio or game engines.
# Plugins must NOT link against libfmod directly, as the host application already provides
# the audio engine and passes all runtime functions via FMOD_DSP_STATE pointers.

# Target Properties & Symbol Visibility
set_target_properties(fmod_fm_synth_dsp PROPERTIES
    OUTPUT_NAME "fmod_fm_synth_dsp"
    PREFIX "" # Generates clean filenames: fmod_fm_synth_dsp.dll / fmod_fm_synth_dsp.dylib / fmod_fm_synth_dsp.so
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)

# Optimization Flags & Compiler Warnings
if(MSVC)
    target_compile_options(fmod_fm_synth_dsp PRIVATE
        /W4
        $<$<CONFIG:Release>:/O2 /Oi /Ot /fp:fast>
        $<$<CONFIG:Debug>:/Od /Zi>
    )
else()
    target_compile_options(fmod_fm_synth_dsp PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Release>:-O3 -ffast-math>
        $<$<CONFIG:Debug>:-O0 -g>
    )
endif()

# Installation target
install(TARGETS fmod_fm_synth_dsp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    fmod_studio/FMSynthDSP.plugin.xml
    fmod_studio/FMSynthDSP.plugin.js
    DESTINATION fmod_studio
)
