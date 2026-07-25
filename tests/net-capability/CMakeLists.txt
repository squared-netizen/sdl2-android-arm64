cmake_minimum_required(VERSION 3.20)

project(sdl_rogue LANGUAGES CXX)

if(NOT DEFINED SDL2_ROOT)
    message(FATAL_ERROR "SDL2_ROOT was not provided")
endif()

set(SDL2_INCLUDE_DIR
    "${SDL2_ROOT}/include/SDL2"
)

set(SDL2_LIBRARY
    "${SDL2_ROOT}/lib/arm64-v8a/libSDL2.so"
)

set(SDL2_TTF_LIBRARY
    "${SDL2_ROOT}/lib/arm64-v8a/libSDL2_ttf.so"
)

set(SDL2_MIXER_LIBRARY
    "${SDL2_ROOT}/lib/arm64-v8a/libSDL2_mixer.so"
)

set(SDL2_IMAGE_LIBRARY
    "${SDL2_ROOT}/lib/arm64-v8a/libSDL2_image.so"
)

set(SDL2_NET_LIBRARY
    "${SDL2_ROOT}/lib/arm64-v8a/libSDL2_net.so"
)

foreach(REQUIRED_HEADER
    SDL.h
    SDL_ttf.h
    SDL_mixer.h
    SDL_image.h
    SDL_net.h
)
    if(NOT EXISTS "${SDL2_INCLUDE_DIR}/${REQUIRED_HEADER}")
        message(FATAL_ERROR
            "Required SDL2 header not found: "
            "${SDL2_INCLUDE_DIR}/${REQUIRED_HEADER}"
        )
    endif()
endforeach()

foreach(REQUIRED_LIBRARY
    "${SDL2_LIBRARY}"
    "${SDL2_TTF_LIBRARY}"
    "${SDL2_MIXER_LIBRARY}"
    "${SDL2_IMAGE_LIBRARY}"
    "${SDL2_NET_LIBRARY}"
)
    if(NOT EXISTS "${REQUIRED_LIBRARY}")
        message(FATAL_ERROR
            "Required SDL2 library not found: ${REQUIRED_LIBRARY}"
        )
    endif()
endforeach()

add_library(SDL2 SHARED IMPORTED GLOBAL)

set_target_properties(SDL2 PROPERTIES
    IMPORTED_LOCATION "${SDL2_LIBRARY}"
)

add_library(SDL2_ttf SHARED IMPORTED GLOBAL)

set_target_properties(SDL2_ttf PROPERTIES
    IMPORTED_LOCATION "${SDL2_TTF_LIBRARY}"
)

add_library(SDL2_mixer SHARED IMPORTED GLOBAL)

set_target_properties(SDL2_mixer PROPERTIES
    IMPORTED_LOCATION "${SDL2_MIXER_LIBRARY}"
)

add_library(SDL2_image SHARED IMPORTED GLOBAL)

set_target_properties(SDL2_image PROPERTIES
    IMPORTED_LOCATION "${SDL2_IMAGE_LIBRARY}"
)

add_library(SDL2_net SHARED IMPORTED GLOBAL)

set_target_properties(SDL2_net PROPERTIES
    IMPORTED_LOCATION "${SDL2_NET_LIBRARY}"
)

add_library(main SHARED
    main.cpp
)

target_compile_features(main PRIVATE
    cxx_std_20
)

target_compile_options(main PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_include_directories(main PRIVATE
    "${SDL2_INCLUDE_DIR}"
)

target_link_libraries(main PRIVATE
    SDL2_image
    SDL2_mixer
    SDL2_ttf
    SDL2_net
    SDL2
)

set_target_properties(main PROPERTIES
    OUTPUT_NAME "main"
)
