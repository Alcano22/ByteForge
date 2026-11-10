#pragma once

#include <cstdint>

namespace ByteForge
{
    struct NativeAPI
    {
        i32 Size;

        void (*Log)(i32 level, const char* message);
        void (*ReportException)(const char* message);

        u32 (*Api_FindFunction)(const char* id, const char* signature);
        i32 (*Api_Invoke)(u32 function, const void* arguments, i32 argumentCount, void* result);
        const char* (*Api_GetLastError)();
    };
}
