using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Serialization;
using simpleFParser = PluginInstaller.SimpleFileParserUtility;

// TODO:
// [ ] Exception handling for various actions which could fail (file handles being held etc)

namespace PluginInstaller
{
    class Program
    {
        private const string ExampleEnginePath = "C:/Epic Games/UE_4.20/";
        private static string msbuildPath;

        public static readonly bool IsLinux;
        public static readonly bool IsMacOS;
        public static readonly bool IsWindows;

        /// <summary>
        /// The directory path of PluginInstaller.exe
        /// </summary>
        public static string AppDirectory;

        static Program()
        {
            switch (Environment.OSVersion.Platform)
            {
                case PlatformID.Unix:
                case PlatformID.MacOSX:
                    if (File.Exists("/usr/lib/libc.dylib") && File.Exists("/System/Library/CoreServices/SystemVersion.plist"))
                    {
                        // This isn't exactly fool proof but msbuild does similar in NativeMethodsShared.cs
                        IsMacOS = true;
                    }
                    else
                    {
                        IsLinux = true;
                    }
                    break;
                default:
                    IsWindows = true;
                    break;
            }
        }

        static void Main(string[] args)
        {
            AppDirectory = Path.GetDirectoryName(new Uri(Assembly.GetExecutingAssembly().CodeBase).LocalPath);

            string enginePath = GetEnginePathFromCurrentFolder();
            if (string.IsNullOrEmpty(enginePath) || !Directory.Exists(enginePath))
            {
                Console.WriteLine("Failed to find the engine folder! Make sure PluginInstaller.exe is under /Engine/Plugins/MonoUE/Binaries/Managed/PluginInstaller/PluginInstaller.exe");
                Console.ReadLine();
                return;
            }
            
            PrintHelp();
            Console.WriteLine();

            Console.WriteLine("Targeting engine version '" + new DirectoryInfo(enginePath).Name + "'");
            Console.WriteLine();

            ProcessArgs(null, args);

            while (true)
            {
                string line = Console.ReadLine();
                string[] lineArgs = ParseArgs(line);
                if (ProcessArgs(line, lineArgs))
                {
                    break;
                }
            }
        }

        private static bool ProcessArgs(string commandLine, string[] args)
        {
            if (args.Length > 0)
            {
                switch (args[0].ToLower())
                {
                    case "exit":
                    case "close":
                    case "quit":
                    case "q":
                        return true;

                    case "clear":
                    case "cls":
                        Console.Clear();
                        break;

                    case "buildcs":
                        CompileCs(args);
                        Console.WriteLine("done");
                        break;

                    case "buildcpp":
                        {
                            Stopwatch cppCompileTime = new Stopwatch();
                            cppCompileTime.Start();
                            CompileCpp(args);
                            Console.WriteLine("done (" + cppCompileTime.Elapsed + ")");
                        }
                        break;

                    case "patchmodules":
                        PatchBuiltInModules(args);
                        Console.WriteLine("done");
                        break;
                    case "help":
                    case "?":
                        PrintHelp();
                        break;
                }
            }

            return false;
        }

        private static void PrintHelp()
        {
            Console.WriteLine("Available commands:");
            //Console.WriteLine("- build       builds C# and C++ projects");
            Console.WriteLine("- buildcs     builds C# projects (Loader, AssemblyRewriter, Runtime)");
            Console.WriteLine("- buildcpp    builds C++ projects");
            Console.WriteLine("- patchmodules    patches the builtinmodules directory to correct c# project / create solution / and compile.");
            //Console.WriteLine("- copyruntime [all] [mono] [coreclr] copies the given runtime(s) locally");
        }

        private static string[] ParseArgs(string commandLine)
        {
            char[] parmChars = commandLine.ToCharArray();
            bool inSingleQuote = false;
            bool inDoubleQuote = false;
            for (var index = 0; index < parmChars.Length; index++)
            {
                if (parmChars[index] == '"' && !inSingleQuote)
                {
                    inDoubleQuote = !inDoubleQuote;
                    parmChars[index] = '\n';
                }
                if (parmChars[index] == '\'' && !inDoubleQuote)
                {
                    inSingleQuote = !inSingleQuote;
                    parmChars[index] = '\n';
                }
                if (!inSingleQuote && !inDoubleQuote && parmChars[index] == ' ')
                {
                    parmChars[index] = '\n';
                }
            }
            return (new string(parmChars)).Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);
        }

        private static Dictionary<string, string> GetKeyValues(string[] args)
        {
            Dictionary<string, string> result = new Dictionary<string, string>();
            if (args != null)
            {
                foreach (string arg in args)
                {
                    int valueIndex = arg.IndexOf('=');
                    if (valueIndex > 0)
                    {
                        string key = arg.Substring(0, valueIndex);
                        string value = arg.Substring(valueIndex + 1);
                        result[arg.ToLower()] = value;
                    }
                    else
                    {
                        result[arg.ToLower()] = null;
                    }
                }
            }
            return result;
        }

        private static string GetCurrentDirectory()
        {
            return AppDirectory;
        }

        internal static void CopyFiles(DirectoryInfo source, DirectoryInfo target, bool overwrite)
        {
            CopyFiles(source, target, overwrite, false);
        }

        internal static void CopyFiles(DirectoryInfo source, DirectoryInfo target, bool overwrite, bool recursive)
        {
            if (!target.Exists)
            {
                target.Create();
            }

            if (recursive)
            {
                foreach (DirectoryInfo dir in source.GetDirectories())
                {
                    CopyFilesRecursive(dir, target.CreateSubdirectory(dir.Name), overwrite);
                }
            }
            foreach (FileInfo file in source.GetFiles())
            {
                CopyFile(file.FullName, Path.Combine(target.FullName, file.Name), overwrite);
            }
        }

        internal static void CopyFilesRecursive(DirectoryInfo source, DirectoryInfo target, bool overwrite)
        {
            CopyFiles(source, target, overwrite, true);
        }

        internal static void CopyFile(string sourceFileName, string destFileName, bool overwrite)
        {
            if ((overwrite || !File.Exists(destFileName)) && File.Exists(sourceFileName))
            {
                try
                {
                    File.Copy(sourceFileName, destFileName, overwrite);
                }
                catch
                {
                    Console.WriteLine("Failed to copy to '{0}'", destFileName);
                }
            }
        }

        static string GetEnginePathFromCurrentFolder()
        {
            // Check upwards for /Epic Games/ENGINE_VERSION/Engine/Plugins/MonoUE/ and extract the path from there
            string[] parentFolders = { "Managed", "Binaries", "MonoUE", "Plugins", "Engine" };
            string currentPath = GetCurrentDirectory();

            DirectoryInfo dir = Directory.GetParent(currentPath);
            for (int i = 0; i < parentFolders.Length; i++)
            {
                if (!dir.Exists || !dir.Name.Equals(parentFolders[i], StringComparison.OrdinalIgnoreCase))
                {
                    return null;
                }
                dir = dir.Parent;
            }

            // Make sure one of these folders exists along side the Engine folder: FeaturePacks, Samples, Templates
            if (dir.Exists && Directory.Exists(Path.Combine(dir.FullName, "Templates")))
            {
                return dir.FullName;
            }

            return null;
        }

        /// <summary>
        /// Returns the main MonoUE plugin directory from the engine path
        /// </summary>
        private static string GetMonoUEPluginDirectory(string enginePath)
        {
            return Path.Combine(enginePath, "Engine", "Plugins", "MonoUE");
        }

        private static void CompileCs(string[] args)
        {
            //Compile C# Projects
            string pluginDir = Path.Combine(GetCurrentDirectory(), "../../../");
            string pluginManagedDir = Path.Combine(pluginDir, "Managed");

            if (!Directory.Exists(pluginManagedDir))
            {
                Console.WriteLine("Failed to find the Plugin Managed directory");
                return;
            }

            string unrealRuntimeSln = Path.Combine(pluginManagedDir, "MonoBindings", "UnrealEngine.Runtime" + ".sln");
            string unrealRuntimeProj = Path.Combine(pluginManagedDir, "MonoBindings", "UnrealEngine.Runtime" + ".csproj");
            string mainDomainSln = Path.Combine(pluginManagedDir, "MonoMainDomain", "UnrealEngine.MainDomain" + ".sln");
            string mainDomainProj = Path.Combine(pluginManagedDir, "MonoMainDomain", "UnrealEngine.MainDomain" + ".csproj");
            
            ///Pair of Solution / Project Paths
            Dictionary<string, string> solutionProjectBuildLibrary = new Dictionary<string, string>
            {
                { unrealRuntimeSln, unrealRuntimeProj},  { mainDomainSln, mainDomainProj}
            };

            Dictionary<string, string> keyValues = GetKeyValues(args);
            bool x86Build = keyValues.ContainsKey("x86");

            foreach (var solutionProjectPaths in solutionProjectBuildLibrary)
            {
                string slnPath = solutionProjectPaths.Key;
                string projPath = solutionProjectPaths.Value;

                if (!File.Exists(projPath))
                {
                    Console.WriteLine("Failed to find C# project '{0}'", projPath);
                    return;
                }

                string targetName = Path.GetFileName(projPath);
                if (!BuildCs(slnPath, projPath, true, x86Build, null))
                {
                    Console.WriteLine("Failed to build (see build.log) - " + targetName);
                    return;
                }
                else
                {
                    Console.WriteLine("Build successful - " + targetName);
                }
            }

            //Move C# Output Files
            //Temporary Path - Only Works On Windows And May Change In the Future
            DirectoryInfo copyFromOutputDir = new DirectoryInfo(Path.Combine(pluginDir, "Binaries", "Win64", "Mono-Debug"));
            DirectoryInfo copyToOutputDir = new DirectoryInfo(Path.Combine(pluginDir, "ThirdParty", "mono", "fx", "MonoUE", "v1.0"));
            if (!copyFromOutputDir.Exists)
            {
                Console.WriteLine("Couldn't Copy Files Because Directory" + copyFromOutputDir.FullName + " Doesn't Exist.");
                return;
            }
            else if (!copyToOutputDir.Exists)
            {
                Console.WriteLine("Couldn't Copy Files Because Directory" + copyToOutputDir.FullName + " Doesn't Exist.");
                return;
            }
            else
            {
                Console.WriteLine("Copying Files From:");
                Console.WriteLine(@"""" + copyFromOutputDir.FullName + @"""");
                Console.WriteLine("To:");
                Console.WriteLine(@"""" + copyToOutputDir.FullName + @"""");
                CopyFilesRecursive(copyFromOutputDir, copyToOutputDir, true);
            }
        }

        private static bool BuildCs(string solutionPath, string projectPath, bool debug, bool x86, string customDefines)
        {
            string buildLogFile = Path.Combine(GetCurrentDirectory(), "build.log");

            if (!string.IsNullOrEmpty(solutionPath) && File.Exists(solutionPath))
            {
                solutionPath = Path.GetFullPath(solutionPath);
            }
            if (!string.IsNullOrEmpty(projectPath) && File.Exists(projectPath))
            {
                projectPath = Path.GetFullPath(projectPath);
            }            

            if (string.IsNullOrEmpty(msbuildPath))
            {
                msbuildPath = NetRuntimeHelper.FindMsBuildPath();
            }

            if (string.IsNullOrEmpty(msbuildPath))
            {
                File.AppendAllText(buildLogFile, "Couldn't find MSBuild path" + Environment.NewLine);
                return false;
            }
            
            string config = debug ? "Debug" : "Release";
            string platform = x86 ? "x86" : "\"Any CPU\"";
            string fileArgs = "\"" + solutionPath + "\"" + " /p:Configuration=" + config + " /p:Platform=" + platform;
            if (!string.IsNullOrEmpty(projectPath))
            {
                // '.' must be replaced with '_' for /t
                string projectName = Path.GetFileNameWithoutExtension(projectPath).Replace(".", "_");

                // Skip project references just in case (this means projects should be built in the correct order though)
                fileArgs += " /t:" + projectName + " /p:BuildProjectReferences=false";
            }
            if (!string.IsNullOrEmpty(customDefines))
            {
                Debug.Assert(!customDefines.Contains(' '));
                fileArgs += " /p:DefineConstants=" + customDefines;
            }

            File.AppendAllText(buildLogFile, "Build: " + msbuildPath + " - " + fileArgs + Environment.NewLine);

            using (Process process = new Process())
            {
                process.StartInfo = new ProcessStartInfo
                {
                    FileName = msbuildPath,
                    Arguments = fileArgs,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };

                StringBuilder output = new StringBuilder();
                StringBuilder error = new StringBuilder();

                using (AutoResetEvent outputWaitHandle = new AutoResetEvent(false))
                using (AutoResetEvent errorWaitHandle = new AutoResetEvent(false))
                {
                    process.OutputDataReceived += (sender, e) =>
                    {
                        if (e.Data == null)
                        {
                            outputWaitHandle.Set();
                        }
                        else
                        {
                            output.AppendLine(e.Data);
                        }
                    };
                    process.ErrorDataReceived += (sender, e) =>
                    {
                        if (e.Data == null)
                        {
                            errorWaitHandle.Set();
                        }
                        else
                        {
                            error.AppendLine(e.Data);
                        }
                    };

                    process.Start();

                    process.BeginOutputReadLine();
                    process.BeginErrorReadLine();

                    int timeout = 60000;
                    bool built = process.WaitForExit(timeout) && outputWaitHandle.WaitOne(timeout) && errorWaitHandle.WaitOne(timeout);

                    File.AppendAllText(buildLogFile, "Build sln '" + solutionPath + "' proj '" + projectPath + "'" + Environment.NewLine);
                    File.AppendAllText(buildLogFile, string.Empty.PadLeft(100, '-') + Environment.NewLine);
                    File.AppendAllText(buildLogFile, output.ToString() + Environment.NewLine);
                    File.AppendAllText(buildLogFile, error.ToString() + Environment.NewLine + Environment.NewLine);

                    if (!built)
                    {
                        Console.WriteLine("Failed to wait for compile.");
                    }

                    return built && process.ExitCode == 0;
                }
            }
        }

        private static void CompileCpp(string[] args)
        {
            Dictionary<string, string> keyValues = GetKeyValues(args);
            bool shippingBuild = keyValues.ContainsKey("shipping");
            bool x86Build = keyValues.ContainsKey("x86");
            bool skipCopy = keyValues.ContainsKey("nocopy");
            bool skipCleanup = keyValues.ContainsKey("noclean");

            string pluginName = "MonoUE";
            string pluginExtension = ".uplugin";
            string pluginExtensionTemp = ".uplugin_temp";

            string enginePath = GetEnginePathFromCurrentFolder();
            string batchFileName = "RunUAT" + (IsWindows ? ".bat" : ".sh");
            string batchFilesDir = Path.Combine(enginePath, "Engine/Build/BatchFiles/");
            string batchPath = Path.Combine(batchFilesDir, batchFileName);
            
            string pluginDir = Path.Combine(enginePath, "Engine/Plugins/", pluginName);
            string pluginPath = Path.Combine(pluginDir, pluginName + pluginExtension);
            //string outputDir = Path.Combine(projectBaseDir, "Build");

            if (!File.Exists(batchPath))
            {
                Console.WriteLine("Failed to compile C++ project. Couldn't find RunUAT at '" + batchPath + "'");
                return;
            }

            try
            {
                if (!File.Exists(pluginPath))
                {
                    // We might have a temp plugin file extension due to a partial previous build
                    string tempPluginPath = Path.ChangeExtension(pluginPath, pluginExtensionTemp);
                    if (File.Exists(tempPluginPath))
                    {
                        File.Move(tempPluginPath, pluginPath);
                    }
                }
            }
            catch
            {
            }

            if (!File.Exists(pluginPath))
            {
                Console.WriteLine("Failed to compile C++ project. Couldn't find the plugin '{0}'", Path.GetFullPath(pluginPath));
                return;
            }

            // Use an appdata folder instead of a local Build folder as we may be building from inside the engine folder
            // which doesn't allow compile output to be within a sub folder of /Engine/
            string monoUEAppData = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "MonoUE");
            if (!Directory.Exists(monoUEAppData))
            {
                Directory.CreateDirectory(monoUEAppData);
            }            
            string outputDir = Path.Combine(monoUEAppData, "Build");

            // In 4.20 if it detects that the plugin already exists our compilation will fail (even if we are compiling in-place!)
            // Therefore we need to rename the existing .uplugin file to have a different extension so that UBT doesn't complain.
            // NOTE: For reference the build error is "Found 'MonoUE' plugin in two locations" ... "Plugin names must be unique."
            string existingPluginFile = Path.Combine(pluginDir, pluginName + pluginExtension);
            string tempExistingPluginFile = null;
            if (File.Exists(existingPluginFile))
            {
                tempExistingPluginFile = Path.ChangeExtension(existingPluginFile, pluginExtensionTemp);
                if (File.Exists(tempExistingPluginFile))
                {
                    File.Delete(tempExistingPluginFile);
                }
                File.Move(existingPluginFile, tempExistingPluginFile);
            }

            // Since we are compiling from within the engine plugin folder make sure to use temp changed .plugin_temp file
            pluginPath = tempExistingPluginFile;

            // Create *another* temp directory where we will copy just the C++ source files over. Otherwise the build tool will copy
            // all of the content files before it builds (which might be a lot of data)
            string tempCopyDir = Path.Combine(monoUEAppData, "BuildTemp2");
            string tempCopyPluginPath = Path.Combine(tempCopyDir, Path.GetFileName(pluginPath));
            if (!Directory.Exists(tempCopyDir))
            {
                Directory.CreateDirectory(tempCopyDir);
            }
            CopyFile(pluginPath, tempCopyPluginPath, true);
            CopyFilesRecursive(new DirectoryInfo(Path.Combine(pluginDir, "Source")), new DirectoryInfo(Path.Combine(tempCopyDir, "Source")), true);
            CopyFilesRecursive(new DirectoryInfo(Path.Combine(pluginDir, "Config")), new DirectoryInfo(Path.Combine(tempCopyDir, "Config")), true);
            // The Intermediate directory doesn't seem to improve compile times so don't bother copying it for now (saves around 100MB of data copying 2x)
            //CopyFilesRecursive(new DirectoryInfo(Path.Combine(pluginDir, "Intermediate")), new DirectoryInfo(Path.Combine(tempCopyDir, "Intermediate")), true);

            try
            {
                using (Process process = new Process())
                {
                    process.StartInfo = new ProcessStartInfo()
                    {
                        FileName = batchPath,
                        UseShellExecute = false,

                        // The -Platform arg is ignored? It instead compiles based on whitelisted/blacklisted? (or for all platforms if no list)
                        Arguments = string.Format("BuildPlugin -Plugin=\"{0}\" -Package=\"{1}\" -Rocket -Platform=Win64", tempCopyPluginPath, outputDir)
                    };
                    process.Start();
                    process.WaitForExit();
                }

                if (!skipCopy)
                {
                    // Copy the entire contents of /Binaries/ and /Intermediate/
                    string[] copyDirs = { "Binaries", "Intermediate" };
                    foreach (string dir in copyDirs)
                    {
                        if (Directory.Exists(Path.Combine(outputDir, dir)))
                        {
                            CopyFilesRecursive(new DirectoryInfo(Path.Combine(outputDir, dir)),
                                new DirectoryInfo(Path.Combine(pluginDir, dir)), true);
                        }
                    }
                }
            }
            finally
            {
                try
                {
                    if (!string.IsNullOrEmpty(tempExistingPluginFile) && File.Exists(tempExistingPluginFile))
                    {
                        if (File.Exists(existingPluginFile))
                        {
                            File.Delete(existingPluginFile);
                        }
                        File.Move(tempExistingPluginFile, existingPluginFile);
                    }
                }
                catch
                {
                }

                if (!skipCleanup)
                {
                    try
                    {
                        if (Directory.Exists(outputDir))
                        {
                            Directory.Delete(outputDir, true);
                        }
                    }
                    catch
                    {
                    }
                }

                try
                {
                    if (Directory.Exists(tempCopyDir))
                    {
                        Directory.Delete(tempCopyDir, true);
                    }
                }
                catch
                {
                }
            }
        }

        private static void PatchBuiltInModules(string[] args)
        {
            string pluginDir = Path.GetFullPath(Path.Combine(GetCurrentDirectory(), "../../../"));
            string builtInModulesDir = Path.GetFullPath(Path.Combine(pluginDir, "Intermediate", "Build", "Win64", "Mono", "BuiltinModules"));
            string builtInModulesProj = Path.GetFullPath(Path.Combine(builtInModulesDir, "UnrealEngine.BuiltinModules" + ".csproj"));
            string builtInModulesSln = Path.GetFullPath(Path.Combine(builtInModulesDir, "UnrealEngine.BuiltinModules" + ".sln"));

            if (!Directory.Exists(builtInModulesDir))
            {
                Console.WriteLine("Built In Modules directory doesn't exist, please generate modules in an Unreal Project.");
                return;
            }
            if (!File.Exists(builtInModulesProj))
            {
                Console.WriteLine("Built In Modules C# Project doesn't exist, please generate modules in an Unreal Project.");
                return;
            }
            Console.WriteLine("Built In Modules Directory Found " + @"""" + builtInModulesDir + @"""");
            Console.WriteLine("Built In Modules C# Project Found " + @"""" + builtInModulesProj + @"""" + "\n");
            string projectGUID;
            Console.WriteLine("Patching BuiltInModules C# Project...");
            PatchBuiltInModulesProject(builtInModulesProj, out projectGUID);
            if (string.IsNullOrEmpty(projectGUID))
            {
                Console.WriteLine("Couldn't Obtain Project GUID from BuildInModules C# Project");
                return;
            }
            Console.WriteLine("Patching BuiltInModules C# Project Complete \n");
            //Create Solution With Project GUID
            Console.WriteLine("Creating Solution At " + @"""" + builtInModulesSln + @"""");
            string projectName = Path.GetFileNameWithoutExtension(builtInModulesProj);
            CreateSolutionFileFromProjectFile(builtInModulesSln, builtInModulesProj, projectName, new Guid(projectGUID));
            Console.WriteLine("Solution Created. \n");
            //Attempt To Compile Project
            Console.WriteLine("Attempting To Compile Built In Modules Project " + @"""" + builtInModulesProj + @"""");
            string targetName = Path.GetFileName(builtInModulesProj);
            if (!BuildCs(builtInModulesSln, builtInModulesProj, true, false, null))
            {
                Console.WriteLine("Failed to build (see build.log) - " + targetName);
                return;
            }
            else
            {
                Console.WriteLine("Build successful - " + targetName);
            }
            Console.WriteLine("\n");
            //Find Dlls In Appropriate Location
            string builtInModulesDLL = "UnrealEngine.BuiltinModules.dll";
            DirectoryInfo platformOutputDir = new DirectoryInfo(Path.Combine(pluginDir, "Binaries", "Win64"));
            string builtInModulesDLLParentDir;
            if (!platformOutputDir.Exists)
            {
                Console.WriteLine("Couldn't find platformOutputDir at " + platformOutputDir);
                return;
            }
            Console.WriteLine("Looking For Generated " + builtInModulesDLL + " file at " + platformOutputDir.FullName);
            if (LookForFileRecursive(platformOutputDir, builtInModulesDLL, true, out builtInModulesDLLParentDir))
            {
                Console.WriteLine(builtInModulesDLL + " found at " + Path.Combine(builtInModulesDLLParentDir, builtInModulesDLL));

            }
            else
            {
                Console.WriteLine("Couldn't Find " + builtInModulesDLL);
            }
        }

        protected static bool CreateSolutionFileFromProjectFile(string slnPath, string projPath, string projName, Guid projectGuid)
        {
            try
            {
                CreateFileDirectoryIfNotExists(slnPath);
                File.WriteAllLines(slnPath, GetSolutionContents(slnPath, projName, projPath, projectGuid));
            }
            catch
            {
                return false;
            }
            return true;
        }

        private static void PatchBuiltInModulesProject(string projPath, out string projectGUID)
        {
            projectGUID = "";
            string monoBindingsPropsName = "MonoUE.EngineBinding.props";
            string monoBindingsPropsRef = @" <Import Project=" + @"""" + @"..\..\..\..\..\MSBuild\MonoUE.EngineBinding.props" + @"""" + @" />";
            string unrealRuntimeCsRef = "UnrealEngine.Runtime.csproj";
            string microsoftCSharpRefInclude = @"<Reference Include=" + @"""" + @"Microsoft.CSharp" + @"""" + @" />";
            string unrealRuntimeDllRef = @"    <Reference Include=" + @"""" + @"UnrealEngine.Runtime" + @"""" + @" />";
            string projectGuidOpeningTag = @"<ProjectGuid>";

            if (!File.Exists(projPath)) return;

            List<string> projectContent = File.ReadAllLines(projPath).ToList();
            if (projectContent == null || projectContent.Count <= 0) return;

            int monoBindingsPropsNameIndex = -1;
            int unrealRuntimeCsRefIndex = -1;
            int microsoftCSharpRefIncludeIndex = -1;
            int unrealRuntimeDllRefIndex = -1;
            int projectGuidOpeningTagIndex = -1;

            //Makes This Functionality Reusable
            Action updateLocalProjectContentIndexes = new Action(() =>
            {
                for (int i = 0; i < projectContent.Count; i++)
                {
                    if (projectContent[i].Contains(monoBindingsPropsName))
                    {
                        monoBindingsPropsNameIndex = i;
                    }
                    if (projectContent[i].Contains(unrealRuntimeCsRef))
                    {
                        unrealRuntimeCsRefIndex = i;
                    }
                    if (projectContent[i].Contains(microsoftCSharpRefInclude))
                    {
                        microsoftCSharpRefIncludeIndex = i;
                    }
                    if (projectContent[i].Contains(unrealRuntimeDllRef))
                    {
                        unrealRuntimeDllRefIndex = i;
                    }
                    if (projectContent[i].Contains(projectGuidOpeningTag))
                    {
                        projectGuidOpeningTagIndex = i;
                    }
                }
            });

            updateLocalProjectContentIndexes();

            if(projectGuidOpeningTagIndex != -1)
            {
                //Attempt To Parse Project GUID
                projectGUID = simpleFParser.ObtainStringFromLine('{', '}', 
                    projectContent[projectGuidOpeningTagIndex],
                    projectContent[projectGuidOpeningTagIndex].IndexOf(projectGuidOpeningTag));
            }

            if (monoBindingsPropsNameIndex != -1)
            {
                //Replace Mono Bindings Reference With Accurate One
                projectContent[monoBindingsPropsNameIndex] = monoBindingsPropsRef;
            }

            if (unrealRuntimeCsRefIndex != -1)
            {
                bool bFoundOpeningItemGroupTag = true;
                bool bFoundClosingItemGroupTag = true;
                string openingItemGroupTag = @"<ItemGroup>";
                string closingItemGroupTag = @"</ItemGroup>";
                int openingItemGroupIndex = -1;
                int closingItemGroupIndex = -1;
                //Try Finding Opening Tag
                if (projectContent[unrealRuntimeCsRefIndex].Contains(openingItemGroupTag))
                {
                    openingItemGroupIndex = unrealRuntimeCsRefIndex;
                }
                else if (projectContent[unrealRuntimeCsRefIndex - 1].Contains(openingItemGroupTag))
                {
                    openingItemGroupIndex = unrealRuntimeCsRefIndex - 1;
                }
                else
                {
                    bFoundOpeningItemGroupTag = false;
                }
                //Try Finding Closing Tag
                if (projectContent[unrealRuntimeCsRefIndex].Contains(closingItemGroupTag))
                {
                    closingItemGroupIndex = unrealRuntimeCsRefIndex;
                }
                else if(projectContent[unrealRuntimeCsRefIndex + 1].Contains(closingItemGroupTag))
                {
                    closingItemGroupIndex = unrealRuntimeCsRefIndex + 1;
                }
                else if(projectContent[unrealRuntimeCsRefIndex + 2].Contains(closingItemGroupTag))
                {
                    closingItemGroupIndex = unrealRuntimeCsRefIndex + 2;
                }
                else
                {
                    bFoundClosingItemGroupTag = false;
                }

                if(bFoundOpeningItemGroupTag && bFoundClosingItemGroupTag &&
                    openingItemGroupIndex != -1 && closingItemGroupIndex != -1)
                {
                    //Remove All Lines Containing UnrealRuntime C# Project Reference
                    int unrealRuntimeCsProjRefLineCount = closingItemGroupIndex - openingItemGroupIndex;
                    projectContent.RemoveRange(openingItemGroupIndex, unrealRuntimeCsProjRefLineCount);
                }
            }

            updateLocalProjectContentIndexes();

            //Only Insert unrealRuntimeDllRef If CSharp Ref Exists and unrealRuntimeDllRefIndex doesn't Exist
            if (microsoftCSharpRefIncludeIndex != -1 && unrealRuntimeDllRefIndex == -1)
            {
                projectContent.Insert(microsoftCSharpRefIncludeIndex + 1, unrealRuntimeDllRef);
            }

            //Write To The Project File
            if (projectContent.Count > 0)
            {
                File.WriteAllLines(projPath, projectContent.ToArray());
            }
        }

        protected static string[] GetSolutionContents(string slnPath, string projName, string projPath, Guid projectGuid)
        {
            string relativeProjPath = NormalizePath(MakePathRelativeTo(projPath, slnPath));

            Guid projectTypeGuid = new Guid(@"FAE04EC0-301F-11D3-BF4B-00C04F79EFBC");// C# project type guid
            Guid solutionGuid = Guid.NewGuid();
            return new string[]
            {
                @"Microsoft Visual Studio Solution File, Format Version 12.00",
                @"# Visual Studio 15",
                @"VisualStudioVersion = 15.0.28010.2041",
                @"MinimumVisualStudioVersion = 10.0.40219.1",
                @"Project(""{" + GuidToString(projectTypeGuid) + @"}"") = """ + projName + @""", """ + relativeProjPath + @""", ""{" + GuidToString(projectGuid) + @"}""",
                @"EndProject",
                @"Global",
                @"	GlobalSection(SolutionConfigurationPlatforms) = preSolution",
                @"		Debug|Any CPU = Debug|Any CPU",
                @"	EndGlobalSection",
                @"	GlobalSection(ProjectConfigurationPlatforms) = postSolution",
                @"		{" + GuidToString(projectGuid) + @"}.Debug|Any CPU.ActiveCfg = Debug|Any CPU",
                @"		{" + GuidToString(projectGuid) + @"}.Debug|Any CPU.Build.0 = Debug|Any CPU",
                @"	EndGlobalSection",
                @"	GlobalSection(SolutionProperties) = preSolution",
                @"		HideSolutionNode = FALSE",
                @"	EndGlobalSection",
                @"	GlobalSection(ExtensibilityGlobals) = postSolution",
                @"		SolutionGuid = {" + GuidToString(solutionGuid) + @"}",
                @"	EndGlobalSection",
                @"EndGlobal"
            };
        }

        /// <summary>
        /// Assuming both paths (or filenames) are relative to the base dir, find the relative path to the InPath.
        /// </summary>
        /// <param name="inPath">Path to make this path relative to.</param>
        /// <param name="inRelativeTo">Path relative to InPath</param>
        /// <returns></returns>
        public static string MakePathRelativeTo(string inPath, string inRelativeTo)
        {
            Uri directory = new Uri(inRelativeTo);
            Uri filePath = new Uri(inPath);
            return directory.MakeRelativeUri(filePath).OriginalString;
        }

        protected static string GuidToString(Guid guid)
        {
            return guid.ToString().ToUpper();
        }

        /// <summary>
        /// Normalizes a file path to be used in a .sln/csproj ('\' must be used instead of '/')
        /// </summary>
        protected static string NormalizePath(string path)
        {
            return path.Replace('/', '\\');
        }

        protected static void CreateFileDirectoryIfNotExists(string path)
        {
            string directory = Path.GetDirectoryName(path);
            try
            {
                if (!Directory.Exists(directory))
                {
                    Directory.CreateDirectory(directory);
                }
            }
            catch
            {
            }
        }

        internal static bool LookForFile(DirectoryInfo source, string fileLookFor, bool recursive, out List<string> foundDirs)
        {
            bool bFileWasFound = false;
            List<string> localFoundDirs = new List<string>();
            foundDirs = new List<string>();

            if (recursive)
            {
                foreach (DirectoryInfo dir in source.GetDirectories())
                {
                    if(LookForFile(dir, fileLookFor, recursive, out foundDirs))
                    {
                        bFileWasFound = true;
                        foreach (string foundDir in foundDirs)
                        {
                            if (!localFoundDirs.Contains(foundDir)) localFoundDirs.Add(foundDir);
                        }
                    }
                }
            }
            foreach (FileInfo file in source.GetFiles())
            {
                if(file.Name == fileLookFor)
                {
                    bFileWasFound = true;
                    foundDirs.Add(file.Directory.FullName);
                    foreach (string foundDir in foundDirs)
                    {
                        if (!localFoundDirs.Contains(foundDir)) localFoundDirs.Add(foundDir);
                    }
                }
            }
            foundDirs = localFoundDirs;
            return bFileWasFound;
        }

        internal static bool LookForFileRecursive(DirectoryInfo source, string fileLookFor, bool recursive, out string foundDir)
        {
            List<string> foundDirs;
            List<FileInfo> foundFileInfos = new List<FileInfo>();
            if(LookForFile(source, fileLookFor, recursive, out foundDirs))
            {
                //Attempt To Find the Most Recent File
                foreach (string foundDirStr in foundDirs)
                {
                    string foundFileInfoStrPath = Path.Combine(foundDirStr, fileLookFor);
                    FileInfo foundFileInfo = new FileInfo(foundFileInfoStrPath);
                    if (foundFileInfo.Exists)
                    {
                        foundFileInfos.Add(foundFileInfo);
                    }
                }
                if (foundFileInfos != null && foundFileInfos.Count > 0)
                {
                    FileInfo mostRecentFileInfo = (from f in foundFileInfos
                                                   orderby f.LastWriteTime descending
                                                   select f).First();
                    foundDir = mostRecentFileInfo.Directory.FullName;
                    return true;
                }
            }
            foundDir = string.Empty;
            return false;
        }
    }
}
