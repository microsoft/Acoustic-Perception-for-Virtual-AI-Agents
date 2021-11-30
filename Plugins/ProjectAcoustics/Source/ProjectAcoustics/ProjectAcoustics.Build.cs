// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

//For debugging purposes only.
//#define ENABLE_DEBUGGING

using UnrealBuildTool;
using System.IO;

public class ProjectAcoustics : ModuleRules
{
    public ProjectAcoustics(ReadOnlyTargetRules Target) : base(Target)
    {

#if (ENABLE_DEBUGGING)
        PCHUsage = ModuleRules.PCHUsageMode.Default;
        OptimizeCode = CodeOptimization.InShippingBuildsOnly;
#else
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
#endif

        PublicIncludePaths.AddRange(
            new string[] {
                // ... add public include paths required here ...
                "Runtime/Engine",
                "Runtime/Core"
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {
                "ProjectAcoustics/Public",
                "../ThirdParty/Include",
                "ProjectAcoustics/Private",
                "../../Wwise/Source/AkAudio/Public"
                // ... add other private include paths required here ...
            }
            );
        
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Projects",
                "AudioMixer"
                // ... add other public dependencies that you statically link with here ...
            }
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "AudioMixer",
                "AkAudio"
                // ... add private dependencies that you statically link with here ...
            }
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
            );

        var thirdPartyDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty"));
		// fix up include path that is needed for TritonWwiseParams.h
		PublicIncludePaths.Add(Path.Combine(thirdPartyDir, "Include"));
        // Go find the right libs to link based on target platform and config
        // Assume release to start
        string configuration = "Release";
        string arch = "";
        string tritonLibName = "Triton.Runtime.lib";
        string tritonCodecLibName = "Triton.Codec.lib";
        string zlibName = "zlibstatic.lib";

        // Override release only if running debug target with debug crt
        // change bDebugBuildsActuallyUseDebugCRT to true in BuildConfiguration.cs to actually link debug binaries
        if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
        {
            configuration = "Debug";
        }

        if (Target.Platform == UnrealTargetPlatform.Win32)
        {
            arch = "Win32";
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            arch = "Win64";
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            arch = "Android_armeabi-v7a";
            tritonLibName = "libTriton.Runtime.a";
            tritonCodecLibName = "libTriton.Codec.a";
            zlibName = "libz.a";
            // Android only has one config
            configuration = "";
        }
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            arch = "XboxOne";
            zlibName = "zlib.lib";
        }
        
        var thirdPartyLibPath = Path.Combine(Path.Combine(thirdPartyDir, arch), configuration);
        PublicAdditionalLibraries.Add(thirdPartyLibPath + "/" + tritonLibName);
        PublicAdditionalLibraries.Add(thirdPartyLibPath + "/" + tritonCodecLibName);
        PublicAdditionalLibraries.Add(thirdPartyLibPath + "/" + zlibName);
    }
}