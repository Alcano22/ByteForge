project "Engine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "bfpch.h"
    pchsource "src/bfpch.cpp"

    files 
    {
        "src/**.h",
        "src/**.cpp",

        "vendor/glm/glm/**.hpp",
        "vendor/glm/glm/**.inl"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS",
        "GLFW_INCLUDE_NONE"
    }

    includedirs
    {
        "src",
        "vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.glm}"
    }

    links 
    {
        "GLFW",
        "Glad",
        "opengl32.lib"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines "BF_DEBUG"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines "BF_RELEASE"
        runtime "Release"
        optimize "On"

    filter "configurations:Dist"
        defines "BF_DIST"
        runtime "Release"
        optimize "On"
