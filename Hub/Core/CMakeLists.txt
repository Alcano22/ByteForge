find_package(LibArchive REQUIRED)

add_library(HubCore STATIC
        src/EditorInstalls.cpp
        src/EditorReleases.cpp
        src/EditorManifest.cpp
        src/Archive.cpp
        src/EditorChoice.cpp
)

target_include_directories(HubCore PUBLIC include)

target_link_libraries(HubCore
    PUBLIC
        ByteForgeCore
    PRIVATE
        LibArchive::LibArchive
        ByteForgeWarnings
)
