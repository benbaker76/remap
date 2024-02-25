cmake_minimum_required(VERSION 2.6)
project(remap)

include_directories(
    include
    lib/stb/include
)

add_library(remap_library STATIC
    src/color.c
    src/convert.c
    src/diff.c
    src/palette.c
    src/lodepng.c
    src/blur.c
    src/libimagequant.c
    src/mediancut.c
    src/mempool.c
    src/nearest.c
    src/pam.c
    src/viter.c
)
set_target_properties(remap_library
    PROPERTIES OUTPUT_NAME remap
)

add_executable(remap_executable
    src/remap.c
)
target_link_libraries(remap_executable
    remap_library
    m
)
set_target_properties(remap_executable
    PROPERTIES OUTPUT_NAME remap
)

add_subdirectory(test)

install(TARGETS remap_library DESTINATION lib)
install(TARGETS remap_executable DESTINATION bin)
