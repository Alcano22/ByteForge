add_executable(Sandbox
        src/SandboxApp.cpp
)

target_link_libraries(Sandbox PRIVATE Engine ByteForgeWarnings)

byteforge_add_engine_resources(Sandbox)
