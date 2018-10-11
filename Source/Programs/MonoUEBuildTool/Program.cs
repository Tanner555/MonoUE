// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace MonoUEBuildTool
{
    class Program
    {
        enum BuildAction
        {
            Build,
            Clean,
            GenerateProjects,
        }

        public static int Main(string[] args)
        {
            if (System.Diagnostics.Debugger.IsAttached)
            {
                return Run(args);
            }
            else
            {
                try
                {
                    return Run(args);
                }
                catch (Exception e)
                {
                    Console.Error.WriteLine(e.Message);
                    return 3;
                }
            }
        }

        static int Run (string[] args)
        {
            if (args.Length == 0)
            {
                Console.Error.WriteLine($"No action specified");
                return 1;
            }

            if (!Enum.TryParse(args[0], out BuildAction action))
            {
                Console.Error.WriteLine($"Unknown action '{args[0]}'");
                return 2;
            }

            var actionArgs = new List<string>(args.Skip(1));

            switch (action)
            {
                case BuildAction.Build:
                case BuildAction.Clean:
                    var ba = new BuildArguments();
                    if (!ba.Parse(actionArgs))
                    {
                        return 1;
                    }
                    if (ba.TargetType == TargetType.Program)
                    {
                        return 0;
                    }
                    foreach (var sln in GetSolutions(ba))
                    {
                        if (!MSBuildHelpers.BuildSolution(sln, action.ToString(), action == BuildAction.Build, ba))
                        {
                            return 1;
                        }
                    }
                    return 0;
                case BuildAction.GenerateProjects:
                    var ga = new GenerateProjectsArguments();
                    if (!ga.Parse(actionArgs))
                    {
                        return 1;
                    }
                    return GenerateProjects.Run(ga);
                default:
                    Console.Error.WriteLine($"Unsupported action '{action}'");
                    return 1;
            }
        }

        static IEnumerable<string> GetSolutions (BuildArguments args)
        {
            string generatedSolutionFile;
            string appName;

            if (!string.IsNullOrEmpty (args.ProjectFile))
            {
                appName = Path.GetFileNameWithoutExtension (args.ProjectFile);
                generatedSolutionFile = Path.Combine(args.ProjectDir, args.PlatformIntermediateDir, appName, "Mono", appName + "_Bindings.sln");
            }
            else
            {
                appName = "UE4";

                // non-game, UE4 target
                if (!args.AppName.StartsWith("UE4", StringComparison.Ordinal))
                {
                    throw new InvalidOperationException("Expected UE4 target because no uproject file was specified");
                }

                generatedSolutionFile = Path.Combine(args.PluginDir, args.PlatformIntermediateDir, "Mono", "UE4_Bindings.sln");
            }

            if (File.Exists(generatedSolutionFile))
            {
                yield return generatedSolutionFile;
            }

            // see if we're building a managed game
            if (string.IsNullOrEmpty (args.ProjectFile) || !args.AppName.StartsWith(appName, StringComparison.Ordinal))
            {
                yield break;
            }

            // See if there is a managed solution to build
            var managedSolutionPath = Path.Combine(args.ProjectDir, appName + "_Managed.sln");

            if (File.Exists(managedSolutionPath))
            {
                yield return managedSolutionPath;
            }

        }
    }
}
