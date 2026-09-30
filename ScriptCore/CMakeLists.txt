find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

set(SCRIPTCORE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/ScriptCore)
set(SCRIPTCORE_PROJECT ${CMAKE_CURRENT_SOURCE_DIR}/ScriptCore.csproj)

file(GLOB_RECURSE SCRIPTCORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cs)

add_custom_command(
        OUTPUT ${SCRIPTCORE_OUTPUT_DIR}/ScriptCore.dll
        COMMAND ${DOTNET_EXECUTABLE} build ${SCRIPTCORE_PROJECT}
            -c $<IF:$<CONFIG:Debug>,Debug,Release>
            -o ${SCRIPTCORE_OUTPUT_DIR}
            --nologo
        DEPENDS ${SCRIPTCORE_SOURCES} ${SCRIPTCORE_PROJECT}
        COMMENT "Building ScriptCore (C#)"
)

add_custom_target(ScriptCore ALL DEPENDS ${SCRIPTCORE_OUTPUT_DIR}/ScriptCore.dll)
