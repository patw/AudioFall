# Pinned native inference dependency. No server, Python, or runtime downloads.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_CURL OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
# Portable baseline on x86; don't inherit the CI machine's instruction set.
set(GGML_AVX OFF CACHE BOOL "" FORCE)
set(GGML_AVX2 OFF CACHE BOOL "" FORCE)
set(GGML_FMA OFF CACHE BOOL "" FORCE)
set(GGML_F16C OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
if(APPLE)
    set(GGML_METAL ON CACHE BOOL "" FORCE)
    set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
else()
    set(GGML_METAL OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(whisper
    URL https://codeload.github.com/ggml-org/whisper.cpp/tar.gz/d1be6fde11ac6e0407606b4e42fe72d34add8037
    URL_HASH SHA256=645445da884df6dbd52b30f82d7be7c01d1767c938714e477e786b88d529e9f4
)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
FetchContent_MakeAvailable(whisper)
# whisper's directory settings must not change AudioFall's existing output layout.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(AUDIOFALL_MODEL_NAME ggml-large-v3-turbo-q5_0.bin)
set(AUDIOFALL_MODEL_SHA256 394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2)
set(AUDIOFALL_MODEL_FILE "${CMAKE_SOURCE_DIR}/models/${AUDIOFALL_MODEL_NAME}" CACHE FILEPATH "Build-time model cache (not a runtime setting)")
get_filename_component(_model_dir "${AUDIOFALL_MODEL_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_model_dir}")
if(NOT EXISTS "${AUDIOFALL_MODEL_FILE}")
    message(STATUS "Downloading bundled Whisper Turbo Q5 model (~547 MiB)")
    file(DOWNLOAD
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/5359861c739e955e79d9a303bcbc70fb988958b1/${AUDIOFALL_MODEL_NAME}"
        "${AUDIOFALL_MODEL_FILE}.part"
        EXPECTED_HASH "SHA256=${AUDIOFALL_MODEL_SHA256}" TLS_VERIFY ON SHOW_PROGRESS
        STATUS _download_status)
    list(GET _download_status 0 _download_code)
    if(NOT _download_code EQUAL 0)
        file(REMOVE "${AUDIOFALL_MODEL_FILE}.part")
        message(FATAL_ERROR "Whisper model download failed: ${_download_status}")
    endif()
    file(RENAME "${AUDIOFALL_MODEL_FILE}.part" "${AUDIOFALL_MODEL_FILE}")
endif()
file(SHA256 "${AUDIOFALL_MODEL_FILE}" _model_hash)
if(NOT _model_hash STREQUAL AUDIOFALL_MODEL_SHA256)
    message(FATAL_ERROR "Bundled Whisper model checksum mismatch: ${AUDIOFALL_MODEL_FILE}")
endif()

function(audiofall_bundle_transcription target)
    target_link_libraries(${target} PRIVATE whisper)
    if(APPLE)
        set(_assets "$<TARGET_FILE_DIR:${target}>/../Resources")
    else()
        set(_assets "$<TARGET_FILE_DIR:${target}>")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_assets}/models" "${_assets}/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AUDIOFALL_MODEL_FILE}" "${_assets}/models/${AUDIOFALL_MODEL_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${whisper_SOURCE_DIR}/LICENSE" "${_assets}/licenses/whisper.cpp-MIT.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/licenses/whisper-model-MIT.txt" "${_assets}/licenses/whisper-model-MIT.txt"
        VERBATIM)
endfunction()
