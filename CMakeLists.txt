cmake_minimum_required(VERSION 3.21)
project(AudioFall VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)

set(AUDIOFALL_CORE
    src/config.cpp src/wav.cpp src/pipeline.cpp
)

add_executable(audiofall
    src/main.cpp src/mainwindow.cpp src/recorder.cpp resources.qrc
    ${AUDIOFALL_CORE}
)
target_link_libraries(audiofall PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Multimedia
)
target_include_directories(audiofall PRIVATE src)
set_target_properties(audiofall PROPERTIES MACOSX_BUNDLE TRUE)

find_package(Qt6 COMPONENTS Test QUIET)
if (Qt6Test_FOUND)
    enable_testing()
    add_executable(audiofall_tests
        tests/tests.cpp
        src/mainwindow.cpp
        src/recorder.cpp
        ${AUDIOFALL_CORE}
    )
    target_link_libraries(audiofall_tests PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::Test
    )
    target_include_directories(audiofall_tests PRIVATE src)
    add_test(NAME AudioFallTests COMMAND audiofall_tests)
endif()
