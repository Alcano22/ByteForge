project "Engine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files 
    {
        "src/**.h",
        "src/**.cpp"
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
