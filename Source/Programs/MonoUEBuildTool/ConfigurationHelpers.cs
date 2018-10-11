// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;

namespace MonoUEBuildTool
{
    enum HostPlatform
    {
        Win64,
        Mac,
    }

    enum TargetConfiguration
    {
        Debug,
        DebugGame,
        Development,
        Test,
        Shipping
    }

    enum TargetType
    {
        Game,
        Editor,
        Client,
        Server,
        Program
    }

    enum TargetPlatform
    {
        Win32,
        Win64,
        Mac,
        Linux
    }

    static class ConfigurationHelpers
    {
        public static string GetSolutionConfigurationName(TargetConfiguration targetConfiguration, TargetType targetType)
        {
            return targetConfiguration.ToString() + (targetType == TargetType.Game ? "" : (" " + targetType.ToString()));
        }

        //MUST BE IN SYNC : MonoUE.Core.props, ConfigurationHelpers.cs, MonoMainDomain.cpp, MonoRuntimeStagingRules.cs, MonoScriptCodeGenerator.cpp, and IDE extensions
        public static string GetAssemblyDirectory(TargetConfiguration targetConfiguration, TargetPlatform targetPlatform, TargetType targetType, string rootDirectory = null)
        {
            string ConfigSuffix = null;
            switch (targetConfiguration)
            {
                case TargetConfiguration.Debug:
                    ConfigSuffix = "-Debug";
                    break;
                case TargetConfiguration.DebugGame:
                    ConfigSuffix = "-DebugGame";
                    break;
                case TargetConfiguration.Development:
                    ConfigSuffix = null;
                    break;
                case TargetConfiguration.Test:
                    ConfigSuffix = "-Test";
                    break;
                case TargetConfiguration.Shipping:
                    ConfigSuffix = "-Shipping";
                    break;
            }

            string Name = "Mono";
            switch (targetType)
            {
                case TargetType.Editor:
                    Name = Name + "Editor";
                    break;
                case TargetType.Client:
                    Name = Name + "Client";
                    break;
                case TargetType.Server:
                    Name = Name + "Server";
                    break;
                case TargetType.Program:
                    throw new ArgumentException(nameof(targetType));
            }

            var dir = Path.Combine("Binaries", targetPlatform.ToString(), Name + ConfigSuffix);
            return rootDirectory == null ? dir : Path.Combine(rootDirectory, dir);
        }
    }
}
