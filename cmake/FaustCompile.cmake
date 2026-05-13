# ─── FaustCompile.cmake ─────────────────────────────────────────────
# Provides faust_compile() for converting .dsp files to C++ headers.
#
# Usage:
#   faust_compile(
#       DSP_FILE    faust/factory/overdrive.dsp
#       OUTPUT_DIR  faust/generated
#       ARCH_FILE   faust/architecture/pedalforge.arch
#       TARGET      PedalForge
#   )

find_program(FAUST_COMPILER faust)
if(NOT FAUST_COMPILER)
    message(FATAL_ERROR
        "FAUST compiler not found. Install it with: brew install faust")
endif()

function(faust_compile)
    cmake_parse_arguments(FC "" "DSP_FILE;OUTPUT_DIR;ARCH_FILE;TARGET" "" ${ARGN})

    get_filename_component(DSP_NAME "${FC_DSP_FILE}" NAME_WE)
    set(OUTPUT_FILE "${FC_OUTPUT_DIR}/${DSP_NAME}.h")

    # Convert to absolute paths
    get_filename_component(ABS_DSP "${FC_DSP_FILE}" ABSOLUTE)
    get_filename_component(ABS_OUT "${OUTPUT_FILE}" ABSOLUTE)
    get_filename_component(ABS_ARCH "${FC_ARCH_FILE}" ABSOLUTE)

    add_custom_command(
        OUTPUT "${ABS_OUT}"
        COMMAND "${FAUST_COMPILER}"
                -i
                -a "${ABS_ARCH}"
                -cn "${DSP_NAME}"
                "${ABS_DSP}"
                -o "${ABS_OUT}"
        DEPENDS "${ABS_DSP}" "${ABS_ARCH}"
        COMMENT "FAUST: Compiling ${DSP_NAME}.dsp → ${DSP_NAME}.h"
        VERBATIM
    )

    # Add generated file to the target's sources
    target_sources(${FC_TARGET} PRIVATE "${ABS_OUT}")

    # Make sure the output directory exists
    get_filename_component(OUT_DIR "${ABS_OUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${OUT_DIR}")
endfunction()
