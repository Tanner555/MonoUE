// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using Mono.Options;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace MonoUEBuildTool
{
    class GenerateProjectsArguments
    {
        public string SolutionName { get; private set; }
        public string SolutionDir { get; private set; }
        public List<string> Projects { get; private set; }

        public bool Parse(List<string> args)
        {
            bool showHelp = false;
            var options = new OptionSet()
            {
                { "SolutionDir=", "Solution directory", d => SolutionDir = d },
                { "SolutionName=", "Solution name", p => SolutionName = p },
                { "Help", "show this message and exit", h => showHelp = h != null },
            };

            //TODO: validate we have all the required args
            if (args.Count == 0)
            {
                ShowHelp();
                return false;
            }

            Projects = options.Parse(args);

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
