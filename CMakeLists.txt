cmake_minimum_required(VERSION 4.1)
project(qrcode_network)

if(MSVC)
    add_compile_options(/O2 /arch:AVX2 /DNDEBUG)
elseif(MINGW)
    add_compile_options(-O3 -march=native -DNDEBUG)

    add_link_options(-static
    -static-libgcc
    -static-libstdc++)
else()
    add_compile_options(-O3 -march=native -DNDEBUG)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(qrcode_network
        main.cpp
        stdinclude.hpp
        main.hpp
        networking.hpp
        resender.hpp
        frames.hpp
)

add_executable(receiver
        receiver.cpp
        stdinclude.hpp
        main.hpp
)

add_executable(resender
        resender.cpp
        main.hpp
        stdinclude.hpp
        resender.hpp
        networking.hpp
)

add_custom_target(all_targets ALL
        DEPENDS
        qrcode_network
        receiver
        resender
)

find_package(libdeflate CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(ZXing CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(cryptopp CONFIG REQUIRED)

find_path(QRENCODE_INCLUDE_DIR NAMES qrencode.h REQUIRED)
find_library(QRENCODE_LIBRARY_RELEASE NAMES qrencode REQUIRED)
find_library(QRENCODE_LIBRARY_DEBUG NAMES qrencoded)

if(QRENCODE_LIBRARY_DEBUG)
    set(QRENCODE_LIBRARIES optimized ${QRENCODE_LIBRARY_RELEASE} debug ${QRENCODE_LIBRARY_DEBUG})
else()
    set(QRENCODE_LIBRARIES ${QRENCODE_LIBRARY_RELEASE})
endif()

find_path(PICOSHA2_INCLUDE_DIRS NAMES picosha2.h REQUIRED)
find_path(STB_INCLUDE_DIR NAMES stb_image.h REQUIRED)

target_include_directories(qrcode_network PRIVATE
        ${QRENCODE_INCLUDE_DIR}
        ${PICOSHA2_INCLUDE_DIRS}
        ${STB_INCLUDE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(qrcode_network PRIVATE
        ${QRENCODE_LIBRARIES}
        SDL3::SDL3
        ZXing::ZXing
        ${OpenCV_LIBS}
        cryptopp::cryptopp
        $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_shared>,libdeflate::libdeflate_shared,libdeflate::libdeflate_static>
)

target_include_directories(receiver PRIVATE
        ${QRENCODE_INCLUDE_DIR}
        ${PICOSHA2_INCLUDE_DIRS}
        ${STB_INCLUDE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(receiver PRIVATE
        ${QRENCODE_LIBRARIES}
        SDL3::SDL3
        ZXing::ZXing
        ${OpenCV_LIBS}
        cryptopp::cryptopp
        $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_shared>,libdeflate::libdeflate_shared,libdeflate::libdeflate_static>
)

target_include_directories(resender PRIVATE
        ${QRENCODE_INCLUDE_DIR}
        ${PICOSHA2_INCLUDE_DIRS}
        ${STB_INCLUDE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(resender PRIVATE
        ${QRENCODE_LIBRARIES}
        SDL3::SDL3
        ZXing::ZXing
        ${OpenCV_LIBS}
        cryptopp::cryptopp
        $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_shared>,libdeflate::libdeflate_shared,libdeflate::libdeflate_static>
)