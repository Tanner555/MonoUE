// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;

#if AGENT_CLIENT
namespace MonoUE.IdeAgent
#else
namespace UnrealEngine.MainDomain
#endif
{
#if AGENT_CLIENT
    public
#endif
    static class UnrealPath
    {
        public static bool IsGame(string gameRoot)
        {
            return File.Exists(GetUProject(gameRoot));
        }

        public static string GetUProject(string gameRoot)
        {
            var name = Path.GetFileName(gameRoot);
            return Path.Combine(gameRoot, name + ".uproject");
        }

        public static bool ValidateEngine(string engineRoot, string config, out string error)
        {
            if (string.IsNullOrEmpty(engineRoot))
            {
                error = "Unreal Engine location has not been set.";
            }
            else if (!IsEngine(engineRoot))
            {
                error = "Unreal Engine location is not valid.";
            }
            else if (!IsBuilt(engineRoot, config))
            {
                error = "Unreal Engine has not been built.";
            }
            else if (!EngineIncludesMono(engineRoot, config))
            {
                error = "Unreal Engine does not include Mono support.";
            }
            else
            {
                error = null;
            }
            return error == null;
        }

        static bool IsEngine(string engineRoot)
        {
            return File.Exists(Path.Combine(engineRoot, "Engine", "Source", "Runtime", "Engine", "Public", "Engine.h"));
        }

        static bool IsBuilt(string engineRoot, string config)
        {
            var unrealEd = GetUnrealEdBinaryPath(engineRoot, config);
            if (UnrealAgentHelper.IsMac)
                unrealEd = Path.Combine(unrealEd, "Contents", "Info.plist");
            return File.Exists(unrealEd);
        }

        static bool EngineIncludesMono(string engineRoot, string config)
        {
            return File.Exists(GetMonoPluginBinaryPath(engineRoot, config));
        }

        static string GetMonoPluginBinaryPath(string engineRoot, string config)
        {
            var platform = CurrentPlatform();
            return Path.Combine(
                engineRoot, "Engine", "Plugins", "XamarinUE4", "MonoRuntime", "Binaries", platform,
                ExpandConfig("UE4Editor-MonoEditor.dll", config, platform)
            );
        }

        public static string GetUnrealEdBinaryPath(string engineRoot, string config)
        {
            var platform = CurrentPlatform();
            return Path.Combine(
                engineRoot, "Engine", "Binaries", platform,
                ExpandConfig("UE4Editor.exe", config, platform)
            );
        }

        static string ExpandConfig(string file, string config, string platform)
        {
            var extension = Path.GetExtension(file);
            if (platform == "Mac")
            {
                if (extension == ".exe")
                    extension = ".app";
                else if (extension == ".dll")
                    extension = ".dylib";
            }
            if (config == null)
                return Path.ChangeExtension(file, extension);
            return Path.GetFileNameWithoutExtension(file) + "-" + platform + "-" + config + extension;
        }

        static string CurrentPlatform()
        {
            if (UnrealAgentHelper.IsWindows)
                return "Win64";
            if (UnrealAgentHelper.IsMac)
                return "Mac";
            throw new NotImplementedException();
        }
    }
}
