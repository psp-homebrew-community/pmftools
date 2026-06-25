cmake_minimum_required(VERSION 3.15)
project(pmftools-wasm LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This CMakeLists builds the WASM target only. Use emcmake cmake.")
endif()

set(LIB_SOURCES
    lib/pmf_demux.c
    lib/h264_parse.c
    lib/mps_mux.c
    lib/pmf_mux.c
)

add_library(pmftools STATIC ${LIB_SOURCES})
target_include_directories(pmftools PUBLIC lib)

set(ATRACDENC_ROOT "${CMAKE_SOURCE_DIR}/external/atracdenc")
set(ATRACDENC_FOUND OFF)

if(EXISTS "${ATRACDENC_ROOT}/src/CMakeLists.txt")
    set(ATRACDENC_PCM_IO_BACKEND "stub" CACHE STRING "" FORCE)
    set(LIBSNDFILE_FOUND TRUE)
    set(LIBSNDFILE_INCLUDE_DIR "${ATRACDENC_ROOT}/src")
    set(SNDFILE_LIBRARIES "")
    set(PCM_IO_LIBRARIES "")

    add_subdirectory(${ATRACDENC_ROOT} ${CMAKE_BINARY_DIR}/atracdenc)

    if(TARGET atracdenc_impl)
        set(ATRACDENC_FOUND ON)
        target_sources(pmftools PRIVATE lib/at3_codec.cpp)
        target_link_libraries(pmftools PUBLIC atracdenc_impl pcm_io oma bitstream fft_impl gha)
        target_include_directories(pmftools PUBLIC
            "${ATRACDENC_ROOT}/src"
            "${ATRACDENC_ROOT}/src/lib"
            "${ATRACDENC_ROOT}/src/lib/liboma/include"
            "${ATRACDENC_ROOT}/src/lib/fft/kissfft_impl"
        )
        target_compile_definitions(pmftools PUBLIC PMFTOOLS_HAS_ATRACDENC=1)
        message(STATUS "atracdenc: ENABLED")
    else()
        message(STATUS "atracdenc: library targets not found")
    endif()
else()
    message(STATUS "atracdenc not found at external/atracdenc")
endif()

set(EXPORTED_FUNCS
    "_wasm_pmf_open"
    "_wasm_pmf_demux"
    "_wasm_pmf_get_video_ptr"
    "_wasm_pmf_get_video_size"
    "_wasm_pmf_get_audio_ptr"
    "_wasm_pmf_get_audio_size"
    "_wasm_pmf_get_audio_count"
    "_wasm_pmf_close"
    "_wasm_pmf_is_mps"
    "_wasm_pmf_has_audio"
    "_wasm_pmf_mps_to_pmf"
    "_wasm_pmf_count_h264_frames"
    "_wasm_pmf_count_sync_frames"
    "_wasm_pmf_get_version"
    "_wasm_at3_encode_pcm"
    "_wasm_at3_get_encoded_size"
    "_wasm_at3_get_encoded_ptr"
    "_wasm_at3_decode"
    "_wasm_at3_get_decoded_samples"
    "_wasm_at3_get_decoded_ptr"
    "_wasm_at3_encoder_create"
    "_wasm_at3_encoder_feed"
    "_wasm_at3_encoder_finalize"
    "_wasm_at3_encoder_free"
    "_wasm_at3_decoder_create"
    "_wasm_at3_decoder_get_samples"
    "_wasm_at3_decoder_get_pcm"
    "_wasm_at3_decoder_free"
    "_malloc"
    "_free"
)
string(REPLACE ";" "," EXPORTED_FUNCS_STR "${EXPORTED_FUNCS}")

add_executable(pmftools_wasm pmftools.cpp)
target_link_libraries(pmftools_wasm pmftools)

if(ATRACDENC_FOUND)
    target_link_libraries(pmftools_wasm pmftools atracdenc_impl pcm_io oma bitstream fft_impl gha)
    target_compile_definitions(pmftools_wasm PRIVATE PMFTOOLS_HAS_ATRACDENC=1)
endif()

set_target_properties(pmftools_wasm PROPERTIES
    LINK_FLAGS "-s WASM=1 -s EXPORTED_FUNCTIONS='[${EXPORTED_FUNCS_STR}]' -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"HEAPU8\",\"HEAPU32\",\"getValue\",\"setValue\"]' -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME=\"PmfTools\""
)
