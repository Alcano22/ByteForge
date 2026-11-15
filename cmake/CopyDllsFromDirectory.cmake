# Copies every DLL from SOURCE_DIR into DESTINATION.
# Used for the editor's copy of the player: next to the built Player.exe, vcpkg has already placed every
# DLL it needs, including indirect ones ($<TARGET_RUNTIME_DLLS> only knows direct dependencies).

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED DESTINATION)
    message(FATAL_ERROR "CopyDllsFromDirectory.cmake needs SOURCE_DIR and DESTINATION")
endif()

file(GLOB dlls "${SOURCE_DIR}/*.dll")
if(dlls)
    file(COPY ${dlls} DESTINATION "${DESTINATION}")
endif()
