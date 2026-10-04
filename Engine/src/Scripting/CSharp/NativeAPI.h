#pragma once

#include <cstdint>

namespace ByteForge
{
    struct NativeAPI
    {
        int Size;

        void (*Log)(int level, const char* message);
        void (*ReportException)(const char* message);

        uint32_t (*Api_FindFunction)(const char* id, const char* signature);
        int (*Api_Invoke)(uint32_t function, const void* arguments, int argumentCount, void* result);
        const char* (*Api_GetLastError)();
    };
}
