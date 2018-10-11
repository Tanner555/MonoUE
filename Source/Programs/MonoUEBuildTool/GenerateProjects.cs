// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Linq;

namespace MonoUEBuildTool
{
    struct GameProjectInfo
    {
        public string Directory;
        public string Name;
        public string SourceDirectory;
    }

    class GenerateProjects
    {
        public static int Run(GenerateProjectsArguments args)
        {
            //no-op on the editor
            if (args.SolutionName == "UE4Editor")
            {
                return 0;
            }

            if(args.Projects.Count < 1 )
            {
                Console.Error.WriteLine("Need at least one game project to process!");
                Console.Error.WriteLine($"Try '{Path.GetFileNameWithoutExtension (System.Reflection.Assembly.GetEntryAssembly().Location)} --help' for more information");
                return 1;
            }

            string solutionDirectory = args.SolutionDir;
            string solutionName = args.SolutionName;

            var gameProjects = new List<GameProjectInfo>();

            foreach(var project in args.Projects)
            {
                string gameProjectDirectory = Path.GetFullPath(Path.GetDirectoryName(project));
                string gameProjectName = Path.GetFileNameWithoutExtension(gameProjectDirectory);

                string sourcePath = Path.Combine(gameProjectDirectory, "Source");
                gameProjects.Add(new GameProjectInfo { Directory = gameProjectDirectory, Name = gameProjectName, SourceDirectory = sourcePath });
            }

            if (System.Diagnostics.Debugger.IsAttached)
            {
                GenerateFiles(solutionDirectory, solutionName, gameProjects);
            }
            else
            {
                try
                {
                    GenerateFiles(solutionDirectory, solutionName, gameProjects);
                }
                catch (Exception e)
                {
                    Console.Error.WriteLine(e.Message);
                    return 3;
                }
            }
            return 0;
        }

        static void GenerateFiles(string solutionDirectory, string solutionName, List<GameProjectInfo> gameProjects)
        {
            //TODO: if the sln file exists it we should update it, not regenerate it entirely
            GenerateSolutionFile(solutionDirectory, solutionName, gameProjects, GetValidConfigurations().ToList ());
        }

        static XmlWriterSettings GetDefaultWriterSettings()
        {
            XmlWriterSettings writerSettings = new XmlWriterSettings();
            writerSettings.CloseOutput = true;
            writerSettings.Indent = true;
            writerSettings.IndentChars = "\t";
            writerSettings.OmitXmlDeclaration = true;

            return writerSettings;
        }

        static void WriteConditionalElement(XmlWriter xmlStream, string elementName, string condition, string val)
        {
            xmlStream.WriteStartElement(elementName);
            xmlStream.WriteAttributeString("Condition", condition);
            xmlStream.WriteValue(val);
            xmlStream.WriteEndElement();
        }

        static void WriteConditionalPropertyElement(XmlWriter xmlStream, string elementName, string val)
        {
            WriteConditionalElement(xmlStream, elementName, String.Format("$({0}) == ''", elementName), val);
        }
        
        static void GenerateSolutionFile(string solutionDirectory, string solutionName, List<GameProjectInfo> gameProjects, List<Tuple<string,string>> configs)
        {
            var builder = new SolutionBuilder(solutionDirectory, gameProjects, configs);
            var builderString = builder.ToString();

            var solutionFileName = Path.Combine(solutionDirectory, solutionName + "_Managed.sln");

            if(!File.Exists(solutionFileName) || File.ReadAllText(solutionFileName) != builderString)
            {
                File.WriteAllText(solutionFileName, builderString);
            }
        }

        static string GetSolutionConfigurationName(TargetConfiguration configuration, TargetType target)
        {
            return configuration + (target == TargetType.Game ? "" : (" " + target));
        }

        static bool PlatformSupportsTarget (TargetPlatform platform, TargetType target)
        {
            //Editor only supported on Win64/Mac/Linux
            if (target == TargetType.Editor)
                return platform == TargetPlatform.Win64 || platform == TargetPlatform.Mac || platform == TargetPlatform.Linux;

            return true;
        }

        static bool ConfigSupportsTarget (TargetConfiguration config, TargetType target)
        {
            //Editor not supported for Test/Shipping
            if (target == TargetType.Editor)
                return config != TargetConfiguration.Shipping && config != TargetConfiguration.Test;

            return true;
        }

        //FIXME: this is only an approximation, get the real values from the UBT project generator somehow
        static IEnumerable<Tuple<string, string>> GetValidConfigurations ()
        {
            foreach (TargetConfiguration config in Enum.GetValues (typeof (TargetConfiguration)))
            {
                foreach (TargetType target in Enum.GetValues (typeof (TargetType)))
                {
                    if (!ConfigSupportsTarget(config, target))
                        continue;
                    string name = GetSolutionConfigurationName (config, target);
                    foreach (TargetPlatform platform in Enum.GetValues (typeof (TargetPlatform)))
                    {
                        if (PlatformSupportsTarget(platform, target))
                        {
                            yield return Tuple.Create(name, platform.ToString ());
                        }
                    }
                }
            }
        }
    }
}
