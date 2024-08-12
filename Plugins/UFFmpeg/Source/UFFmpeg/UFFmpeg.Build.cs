// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

using UnrealBuildTool;

public class UFFmpeg : ModuleRules
{
    private string ModulePath { get { return ModuleDirectory; } }
    private string ProjectPath { get { return Directory.GetParent(ModulePath).Parent.FullName; } }
    private string ThirdPartyPath { get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/ffmpeg-6.2.r113110")); } }

    public UFFmpeg(ReadOnlyTargetRules Target) : base(Target)
    {
        //bEnableUndefinedIdentifierWarnings = false;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        OptimizeCode = CodeOptimization.InShippingBuildsOnly;

        PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "include"));
        PublicIncludePaths.Add(Path.Combine(Directory.GetCurrentDirectory(), "Runtime","AudioMixer","Private"));

        PublicDependencyModuleNames .AddRange(new string[] { "Core", "AudioMixer" });
        PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Core", "Engine", "Slate", "SlateCore", "Projects", "Engine", "RHI", "RHICore", "RenderCore" });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string[] libraries = { "avcodec.lib", "avdevice.lib", "avfilter.lib", "avformat.lib", "avutil.lib", "swresample.lib", "swscale.lib" };
            string LibrariesPath = Path.Combine(ThirdPartyPath, "lib");
            foreach (string library in libraries)
            {
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, library));
            }

            string[] dlls = { "avcodec.dll", "avdevice.dll", "avfilter.dll", "avformat.dll", "avutil.dll", "swresample.dll", "swscale.dll", "postproc.dll" };
            string BinariesPath = Path.Combine(ThirdPartyPath, "bin");
            foreach (string dll in dlls)
            {
                PublicDelayLoadDLLs.Add(dll);
                RuntimeDependencies.Add(Path.Combine(BinariesPath, dll), StagedFileType.NonUFS);
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            //TODO: Not compiled mac library
            string LibrariesPath = Path.Combine(Path.Combine(ThirdPartyPath, "ffmpeg", "lib"), "osx");
            System.Console.WriteLine("... LibrariesPath -> " + LibrariesPath);

            string[] libs = { "libavcodec.58.dylib", "libavdevice.58.dylib", "libavfilter.7.dylib", "libavformat.58.dylib", "libavutil.56.dylib", "libswresample.3.dylib", "libswscale.5.dylib", "libpostproc.55.dylib" };
            foreach (string lib in libs)
            {
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, lib));
                RuntimeDependencies.Add(Path.Combine(LibrariesPath, lib), StagedFileType.NonUFS);
            }

        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            //TODO: Not compiled linux library
            string LibrariesPath = Path.Combine(Path.Combine(ThirdPartyPath, "ffmpeg", "lib"), "amd64");
            System.Console.WriteLine("... LibrariesPath -> " + LibrariesPath);

            string[] libs = { "libavcodec.a", "libavdevice.a", "libavfilter.a", "libavformat.a", "libavutil.a", "libswresample.a", "libswscale.a", "libpostproc.a" };
            foreach (string lib in libs)
            {
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, lib));
                RuntimeDependencies.Add(Path.Combine(LibrariesPath, lib), StagedFileType.NonUFS);
            }

            PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "ffmpeg", "include", "amd64"));
        }
    }
}
