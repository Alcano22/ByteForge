include "Dependencies.lua"

workspace "ByteForge"
    architecture "x86_64"
    startproject "Engine"

    configurations 
    { 
        "Debug", 
        "Release", 
        "Dist" 
    }

    flags 
    {
        "MultiProcessorCompile"
    }

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    group "Core"
        include "Engine"
    group ""

    group "Dependencies"
        include "Engine/vendor/glfw"
        include "Engine/vendor/Glad"
    group ""
