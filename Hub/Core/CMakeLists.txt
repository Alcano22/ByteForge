add_library(HubCore STATIC
        src/EditorInstalls.cpp
)

target_include_directories(HubCore PUBLIC include)

target_link_libraries(HubCore
    PUBLIC
        Engine
    PRIVATE
        ByteForgeWarnings
)
