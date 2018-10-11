// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool.Rules
{
	public class Mono : ModuleRules
	{
		public Mono(ReadOnlyTargetRules Target) : base(Target)
		{
			Type = ModuleType.External;

			string MonoUEPluginDirectory = ModuleDirectory + "/../../..";

			PublicSystemIncludePaths.AddRange(
				new string[] {
					MonoUEPluginDirectory + "/ThirdParty/mono/include/mono-2.0",
				}
			);

			if (GetGeneratingProjects ())
			{
				RunBootstrapper(MonoUEPluginDirectory);
			}

			AddMonoRuntime(Target, MonoUEPluginDirectory);
		}

		bool GetGeneratingProjects()
		{
			var GenType = typeof(ReadOnlyTargetRules).Assembly.GetType("UnrealBuildTool.ProjectFileGenerator");
			if (GenType != null) {
				var BF = System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic;
				var GenProjectsField = GenType.GetField ("bGenerateProjectFiles", BF);
				if (GenProjectsField != null)
				{
					return (bool) GenProjectsField.GetValue(GenType);
				}
			}
			return true;
		}

		void AddMonoRuntime(ReadOnlyTargetRules Target, string MonoUEPluginDirectory)
		{
			string MonoLibPath = MonoUEPluginDirectory + "/ThirdParty/mono/lib/" + Target.Platform;
			PublicLibraryPaths.Add(MonoLibPath);

			if (Target.Platform == UnrealTargetPlatform.Win64
				|| Target.Platform == UnrealTargetPlatform.Win32)
			{
				string LibraryName = "mono-2.0-sgen";
				PublicAdditionalLibraries.Add(LibraryName + ".lib");
				PublicDelayLoadDLLs.Add(LibraryName + ".dll");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				string LibraryName = "libmonosgen-2.0";
				PublicAdditionalLibraries.Add("iconv");
				PublicDelayLoadDLLs.Add(Path.Combine(MonoLibPath, LibraryName + ".dylib"));
			}
			else
			{
				throw new BuildException("Mono not supported on platform '{0}'", Target.Platform);
			}

			PublicDefinitions.Add("MONO_IS_DYNAMIC_LIB=1");
		}

		void RunBootstrapper(string MonoUEPluginDirectory)
		{
			BuildProject(MonoUEPluginDirectory, Path.Combine(MonoUEPluginDirectory, "Source", "Programs", "MonoUEBootstrapper", "MonoUEBootstrapper.csproj"), false);

			var BootstrapperExe = Path.Combine(MonoUEPluginDirectory, "Binaries", "DotNET", "MonoUEBootstrapper.exe");
			var Bootstrapper = Process.Start(new ProcessStartInfo(BootstrapperExe, Log.bIsVerbose ? "--verbose" : "") { UseShellExecute = false });
			Bootstrapper.WaitForExit();

			if (Bootstrapper.ExitCode > 0)
			{
				Console.Error.WriteLine("Failed to bootstrap MonoUE dependencies");
				Environment.Exit(100);
			}

			//MonoUEBuildTool depends on build functionality (MSBuild Restore) that may not be present until the bootstrapper has run
			BuildProject(MonoUEPluginDirectory, Path.Combine(MonoUEPluginDirectory, "Source", "Programs", "MonoUEBuildTool", "MonoUEBuildTool.csproj"), true);
		}

		void BuildProject(string MonoUEPluginDirectory, string ProjectPath, bool Restore)
		{
			var MSBuildExe = GetMSBuildPath(MonoUEPluginDirectory);
			string args = "";

			if (Restore)
			{
				args += "/r";
			}

			if (!Log.bIsVerbose)
			{
				args += " /v:quiet /nologo";
			}

			var MSBuildProcess = Process.Start(
				new ProcessStartInfo(MSBuildExe, args) { UseShellExecute = false, WorkingDirectory = Path.GetDirectoryName(ProjectPath) }
			);
			MSBuildProcess.WaitForExit();

			if (MSBuildProcess.ExitCode > 0)
			{
				Console.Error.WriteLine("Failed to build MonoUEBuildTool");
				Environment.Exit(101);
			}
		}

		string GetMSBuildPath(string MonoUEPluginDirectory)
		{
			switch (BuildHostPlatform.Current.Platform)
			{
				case UnrealTargetPlatform.Win64:
					return GetMSBuildPathWindows();
				case UnrealTargetPlatform.Mac:
					return Path.Combine(MonoUEPluginDirectory, "MSBuild", "mac-msbuild.sh");
				default:
					throw new BuildException("TODO");
			}
		}

		static string GetMSBuildPathWindows()
		{
			string[] locations = {
				@"HKEY_CURRENT_USER\SOFTWARE\",
				@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\",
			};

			foreach (var loc in locations)
			{
				var vsroot = Microsoft.Win32.Registry.GetValue(loc + @"Microsoft\VisualStudio\SxS\VS7", "15.0", null) as string;
				if (string.IsNullOrEmpty(vsroot))
				{
					continue;
				}
				string msbuildPath = Path.Combine(vsroot, "MSBuild", "15.0", "Bin", "MSBuild.exe");
				if (File.Exists(msbuildPath))
				{
					return msbuildPath;
				}
			}

			throw new BuildException("MonoUE requires Visual Studio 2017");
		}
	}
}