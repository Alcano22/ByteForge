add_executable(FontBaker src/FontBaker.cpp)

target_link_libraries(FontBaker PRIVATE Engine ByteForgeWarnings)

set_target_properties(FontBaker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BYTEFORGE_TOOLS_DIR})

byteforge_deploy_runtime(FontBaker)

set(BYTEFORGE_BAKED_FONT_DIR ${CMAKE_BINARY_DIR}/baked/fonts
        CACHE INTERNAL "Prebuilt atlases of the engine's built-in fonts")

set(BYTEFORGE_BAKED_FONT_INPUTS
        ${BYTEFORGE_ENGINE_FONT_DIR}/inter_regular.ttf
        ${CMAKE_SOURCE_DIR}/Engine/include/Engine/Text/FontAtlas.h
        ${CMAKE_SOURCE_DIR}/Engine/src/Text/FontAtlas.cpp
        ${CMAKE_SOURCE_DIR}/Engine/src/Text/FontAtlasFile.cpp
        CACHE INTERNAL "Files the baked font atlases depend on")

set(atlas ${BYTEFORGE_BAKED_FONT_DIR}/inter_regular.bfatlas)

add_custom_command(
        OUTPUT ${atlas}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BYTEFORGE_BAKED_FONT_DIR}
        COMMAND FontBaker ${BYTEFORGE_ENGINE_FONT_DIR}/inter_regular.ttf ${atlas}
        DEPENDS ${BYTEFORGE_BAKED_FONT_INPUTS}
        COMMENT "Baking the atlas of the built-in font"
        VERBATIM
)

add_custom_target(BakeEngineFonts DEPENDS ${atlas})
