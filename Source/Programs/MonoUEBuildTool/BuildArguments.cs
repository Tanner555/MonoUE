// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using Mono.Options;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace MonoUEBuildTool
{
    class BuildArguments
    {
        public string EngineDir { get; private set; }
        public string ProjectDir { get; private set; }
        public string TargetName { get; private set; }
        public TargetPlatform TargetPlatform { get; private set; }
        public TargetConfiguration TargetConfiguration { get; private set; }
        public TargetType TargetType { get; private set; }
        public string ProjectFile { get; private set; }
        public string PluginDir { get; private set; }
        public string AppName { get; private set; }
        public string PlatformIntermediateDir { get; private set; }

        public bool Parse (List<string> args)
        {
            bool showHelp = false;
            var options = new OptionSet()
            {
                { "EngineDir=", "Engine directory", d => EngineDir = d },
                { "ProjectDir=", "Project directory", d => ProjectDir = d },
                { "TargetName=", "Target name", n => TargetName = n },
                { "TargetPlatform=", "Target platform", (TargetPlatform p) => TargetPlatform = p },
                { "TargetConfiguration=", "Target configuration", (TargetConfiguration c) => TargetConfiguration = c },
                { "TargetType=", "Target type", (TargetType t) => TargetType = t },
                { "ProjectFile=", "Project file", p => ProjectFile = p },
                { "PluginDir=", "MonoUE plugin directory", p => PluginDir = p },
                { "AppName=", "App name", p => AppName = p },
                { "PlatformIntermediateDir=", "Platform intermediate directory name", p => PlatformIntermediateDir = p },
                { "Help", "show this message and exit", h => showHelp = h != null },
            };

            //TODO: validate we have all the required args and no unknown ones
            if (args.Count == 0)
            {
                ShowHelp();
                return false;
            }

            options.Parse(args);

            if (showHelp)
            {
                ShowHelp();
                return false;
            }

            return true;

            void ShowHelp()
            {
                string name = Path.GetFileNameWithoutExtension(System.Reflection.Assembly.GetEntryAssembly().Location); ;
                Console.Error.WriteLine($"Usage: {name} (build|clean) ARGS PROJECT1 [PROJECT2...]");
                Console.Error.WriteLine($"Args: ");
                options.WriteOptionDescriptions(Console.Error);
            }
        }
    }
}
