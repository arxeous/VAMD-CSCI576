workspace "VideoSearch"
	architecture "x64"
	startproject "VideoSearch"

	configurations
	{
		"Debug",
		"Release"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "VideoSearch"
	location "VideoSearch"
	kind "ConsoleApp"
	language "C++"

	targetdir("exe/" .. outputdir ..  "/%{prj.name}")
	objdir("exe-int/" .. outputdir ..  "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/ImGui/**.cpp",
		"%{prj.name}/vendor/ImGui/**.h",
		"%{prj.name}/vendor/AudioFile/**.h",
		"%{prj.name}/vendor/nativefiledialog/**.cpp"
	}

	includedirs
	{
		"%{prj.name}/vendor/ffmpeg/include",
		"%{prj.name}/vendor/SDL2/include",
		"%{prj.name}/vendor/ImGui",
		"%{prj.name}/vendor/fftw3/include",
		"%{prj.name}/vendor/AudioFile"
		"%{prj.name}/vendor/nativefiledialog/include"

	}

	libdirs {"%{prj.name}/vendor/SDL2/lib", "%{prj.name}/vendor/ffmpeg/lib", "%{prj.name}/vendor/fftw3/lib", "%{prj.name}/vendor/nativefiledialog/lib" }


	links
	{
		"SDL2",
		"SDL2main",
		"avcodec",
		"avdevice",
		"avfilter",
		"avformat",
		"avutil",
		"postproc",
		"swresample",
		"swscale",
<<<<<<< HEAD
		"libfftw3-3"
=======
		"nfd_d",
		"comctl32"
>>>>>>> 8821f387004a597778f4bc17336579f0efe6da11
	}

	filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "10.0"

		postbuildcommands
		{
			"xcopy /Q /Y /I %{prj.location}\\vendor\\ffmpeg\\lib\\*.dll %{cfg.buildtarget.directory}",
			"xcopy /Q /Y /I %{prj.location}\\vendor\\SDL2\\lib\\*.dll %{cfg.buildtarget.directory}",
			"xcopy /Q /Y /I %{prj.location}\\vendor\\fftw3\\lib\\*.dll %{cfg.buildtarget.directory}"
		}
