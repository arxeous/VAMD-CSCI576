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
		"%{prj.name}/vendor/opencv2",
		"%{prj.name}/vendor/Eigen",
		"%{prj.name}/vendor/fftw3/include",
		"%{prj.name}/vendor/AudioFile",
		"%{prj.name}/vendor/nativefiledialog/include"
	}

	libdirs {"%{prj.name}/vendor/SDL2/lib", "%{prj.name}/vendor/ffmpeg/lib", "%{prj.name}/vendor/fftw3/lib", "%{prj.name}/vendor/nativefiledialog/lib", "%{prj.name}/vendor/opencv2/opencv2/x64/vc16/lib" }


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
		"libfftw3-3",
		"nfd_d",
		"comctl32",
		"opencv_world480d"
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
			"xcopy /Q /Y /I %{prj.location}\\vendor\\fftw3\\lib\\*.dll %{cfg.buildtarget.directory}",
			"xcopy /Q /Y /I %{prj.location}\\vendor\\opencv2\\opencv2\\x64\\vc16\\lib\\*.dll %{cfg.buildtarget.directory}"
		}
