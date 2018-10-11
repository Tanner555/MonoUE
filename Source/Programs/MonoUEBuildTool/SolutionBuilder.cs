// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.IO;
using System.Xml.Linq;

namespace MonoUEBuildTool
{
    [Serializable]
    class InvalidProjectException : Exception
    {
        public InvalidProjectException(string message)
            : base(message)
        {

        }
    }

    struct ProjectInfo
    {
        //the new CPS-based C# project system that can handle sdk-style projects]
        static readonly Guid cpsCSharpProjectSystemGuid = Guid.ParseExact("{9A19103F-16F7-4668-BE54-9A1E7A4F7556}", "B");

        //the old native C# project system
        static readonly Guid mpsCSharpProjectSystemGuid = Guid.ParseExact("{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}", "B");

        public string ProjectName;
        public Uri ProjectUri;
        public Guid ProjectGuid;
        public Guid ProjectType;

        public ProjectInfo(string path)
        {
            ProjectName = Path.GetFileNameWithoutExtension(path);
            ProjectUri = new Uri(path);
            ProjectGuid = Guid.Empty;

            var Doc = XDocument.Load(path);

            bool isSdkStyle = Doc.Root.Attribute("Sdk") != null;

            // need to use the correct project system or things will break
            ProjectType = isSdkStyle? cpsCSharpProjectSystemGuid : mpsCSharpProjectSystemGuid;

            // if the project specifies a GUID, use it, else generate a new one to represent it in the SLN
            // it doesn't really matter what it is as long as it's consistent
            var ProjectGuidElement = Doc.Descendants("ProjectGuid").LastOrDefault();
            if (ProjectGuidElement != null)
            {
                ProjectGuid = Guid.ParseExact(ProjectGuidElement.Value.Trim("{}".ToCharArray()), "D");
            }
            else
            {
                ProjectGuid = Guid.NewGuid();
            }
        }
    }

    [Serializable]
    class MismatchedUnindentException : Exception
    {
        public MismatchedUnindentException()
            : base("Unmatched Unindent call")
        {

        }
    }

    class SolutionBuilder    
    {
        StringBuilder Builder = new StringBuilder();
        int IndentCount = 0;
        string IndentString = "";
        Uri SolutionDirectoryUri;
        Dictionary<string, Guid> SolutionFolderGuids = new Dictionary<string, Guid>();

        public SolutionBuilder(string solutionDirectory, List<GameProjectInfo> gameProjects, List<Tuple<string,string>> configs)
        {
            var lastChar = solutionDirectory[solutionDirectory.Length-1];
            if (lastChar != '\\' && lastChar != '/')
            {
                solutionDirectory = solutionDirectory + Path.DirectorySeparatorChar;
            }
            SolutionDirectoryUri = new Uri(solutionDirectory);

            var projectsByGameProject = new Dictionary<GameProjectInfo, ProjectInfo[]>();
 
            foreach(var gameProject in gameProjects)
            {
                // TODO: support F#
                // find all csprojs
                var csprojs = Directory.GetFiles(gameProject.SourceDirectory, "*.csproj", SearchOption.AllDirectories);

                var projectInfos = (from proj in csprojs
                                    where File.Exists(proj)
                                    select new ProjectInfo(proj)).ToArray();

                projectsByGameProject[gameProject] = projectInfos;
            }

            Builder.AppendLine();
            AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
            AppendLine("# Visual Studio 15");
            AppendLine("VisualStudioVersion = 15.0.27004.2010");
            AppendLine("MinimumVisualStudioVersion = 10.0.40219.1");

            bool useFolders = projectsByGameProject.Count > 1;
            
            foreach(var gameProject in projectsByGameProject)
            {
                if (useFolders)
                {
                    AddSolutionFolder(gameProject.Key.Name);
                }

                foreach (var project in gameProject.Value)
                {
                    AddProject(project);
                }
            }

            AppendLine("Global");
            Indent();

            AppendLine("GlobalSection(SolutionConfigurationPlatforms) = preSolution");
            Indent();

            foreach (var pair in configs)
            {
                AppendLine("{0}|{1} = {0}|{1}", pair.Item1, pair.Item2);
            }

            Unindent();
            AppendLine("EndGlobalSection");

            // project config platforms
            AppendLine("GlobalSection(ProjectConfigurationPlatforms) = postSolution");
            Indent();

            foreach (var gameProject in projectsByGameProject)
            {
                foreach (var project in gameProject.Value)
                {
                    AddProjectConfigs(project, configs);
                }
            }

            Unindent();
            AppendLine("EndGlobalSection");

            AppendLine("GlobalSection(SolutionProperties) = preSolution");
            Indent();
            AppendLine("HideSolutionNode = FALSE");

            Unindent();
            AppendLine("EndGlobalSection");

            if(useFolders)
            {
                AppendLine("GlobalSection(\"NestedProjects\") = preSolution");
                Indent();

                foreach(var gameProject in projectsByGameProject )
                {

                    foreach(var project in gameProject.Value)
                    {
                        AddProjectToSolutionFolder(gameProject.Key.Name, project);
                    }
                }

                Unindent();
                AppendLine("EndGlobalSection");
            }

            Unindent();
            AppendLine("EndGlobal");

        }

        void Indent()
        {
            IndentString = IndentString + "\t";
            IndentCount++;
        }

        void Unindent()
        {
            if(IndentCount <= 0)
            {
                throw new MismatchedUnindentException();
            }
            IndentCount--;
            IndentString = IndentString.Substring(0, IndentString.Length -1);
        }

        void AppendLine(string format, params object[] parms)
        {
            Builder.AppendLine(IndentString + String.Format(format, parms));
        }

       
        void AddProject(ProjectInfo project)
        {
            var relativePath = SolutionDirectoryUri.MakeRelativeUri(project.ProjectUri).ToString().Replace('/', Path.DirectorySeparatorChar);

            AppendLine("Project(\"{0}\") = \"{1}\", \"{2}\", \"{3}\"", project.ProjectType.ToString("B").ToUpper(), project.ProjectName, relativePath, project.ProjectGuid.ToString("B").ToUpper());
            AppendLine("EndProject");
        }

        void AddProjectConfigs(ProjectInfo project, List<Tuple<string,string>> configs)
        {
            // TODO: parse configs and try to match?
            foreach (var pair in configs)
            {
                AddProjectConfig(project, pair.Item1, pair.Item2);
            }
        }

        void AddProjectConfig(ProjectInfo project, string config, string platform)
        {
            AppendLine("{0}.{1}|{2}.ActiveCfg = {1}|{2}", project.ProjectGuid.ToString("B").ToUpper(), config, platform);
            AppendLine("{0}.{1}|{2}.Build.0 = {1}|{2}", project.ProjectGuid.ToString("B").ToUpper(), config, platform);
        }

        void AddSolutionFolder(string name)        
        {
            // TODO: parse guids out of any existing solution so we're not constantly overwriting
            Guid folderGuid = Guid.NewGuid();
            AppendLine("Project(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"{0}\", \"{1}\", \"{3}\"", name, name, folderGuid.ToString("B").ToUpper());
            AppendLine("EndProject");

            SolutionFolderGuids[name] = folderGuid;
        }

        void AddProjectToSolutionFolder(string folderName, ProjectInfo project)
        {
            AppendLine("{0} = {1}", project.ProjectGuid.ToString("B").ToUpper(), SolutionFolderGuids[folderName].ToString("B").ToUpper());
        }

        public override string ToString()
        {
            return Builder.ToString();
        }


    }
}
