using AutomationTool;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace MonoUE.Automation
{
	/*
	public class MonoRuntimeStagingRules : Project.PluginStagingRules
	{
		public override void StagePluginFiles(ProjectParams Params, DeploymentContext SC)
		{
			string MonoPath = CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Plugins/MonoUE/ThirdParty/mono");

			foreach (var Config in SC.StageTargetConfigurations)
			{
				StageMonoRuntime(Params, SC, Config, MonoPath);

				var Targets = new List<TargetRules.TargetType>();
				foreach (var ExeName in SC.StageExecutables)
				{
					switch(ExeName)
					{
						case "UE4Game":
							Targets.Add (TargetRules.TargetType.Game);
							continue;
						case "UE4Server":
							Targets.Add (TargetRules.TargetType.Server);
							continue;
						case "UE4Client":
							Targets.Add(TargetRules.TargetType.Client);
							continue;
						default:
							throw new Exception("Unknown executable '" + ExeName + "'");
					}
				}

				foreach (var Target in Targets)
				{
					StageMonoAssemblies(Params, SC, Config, Target, MonoPath);
				}
			}
		}

		void StageMonoRuntime(ProjectParams Params, DeploymentContext SC, UnrealTargetConfiguration config, string MonoPath)
		{
			string PlatformIntermediateDir = CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Intermediate/Build/" + SC.PlatformDir + "/Mono");
			string BinDir = "Engine/Binaries/ThirdParty/EMono/" + SC.PlatformDir;

			var runtimeInfo = GetRuntimeInfo(PlatformIntermediateDir, MonoPath, SC.PlatformDir);

			// copy mono runtime binaries
			foreach (var lib in runtimeInfo.DynamicLibs)
			{
				var dir = Path.GetDirectoryName(lib);
				SC.StageFiles(StagedFileType.NonUFS, dir, Path.GetFileName(lib), NewPath: BinDir);
				var prefix = Path.GetFileNameWithoutExtension(lib) + "*";
				foreach (var ext in SC.StageTargetPlatform.GetDebugFileExtentions())
				{
					SC.StageFiles(StagedFileType.DebugNonUFS, dir, prefix + ext, bAllowNone: true, NewPath: BinDir);
				}
			}
		}

		void StageMonoAssemblies(ProjectParams Params, DeploymentContext SC, UnrealTargetConfiguration Config, TargetRules.TargetType TargetType, string MonoPath)
		{
			string RuntimeAssemblyDir = GetAssemblyDirectory (Config, SC.StageTargetPlatform.PlatformType, TargetType, "Engine");
			string GameAssemblyDir = GetAssemblyDirectory(Config, SC.StageTargetPlatform.PlatformType, TargetType);

			var GameAssemblies = System.IO.Directory.GetFiles(CommandUtils.CombinePaths(SC.ProjectRoot, GameAssemblyDir), "*.dll");
			var SearchDirs = new[] { CommandUtils.CombinePaths(SC.LocalRoot, RuntimeAssemblyDir) };
			var CoreAssemblies = new[] { CommandUtils.CombinePaths(SC.LocalRoot, RuntimeAssemblyDir, "UnrealEngine.MainDomain.dll") };

			var AsmInfo = PackageAssemblies(MonoPath, SC.StageTargetPlatform.PlatformType.ToString(), GameAssemblies.Concat (CoreAssemblies), SearchDirs, !Params.NoDebugInfo, false, null);

			//TODO: add a option to control whether assemblies are staged in UFS
			const StagedFileType stageType = StagedFileType.UFS;

			string GameAsmOutputDir = CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, GameAssemblyDir);

			var GameAssemblyFileNames = new HashSet<string>(GameAssemblies.Select(a => Path.GetFileName (a)));

			foreach (var f in AsmInfo.Assemblies)
			{
				string dir = Path.GetDirectoryName(f);
				string name = Path.GetFileName(f);

				string OutputDir = GameAssemblyFileNames.Contains(name) ? GameAsmOutputDir : RuntimeAssemblyDir;

				SC.StageFiles(stageType, dir, name, false, NewPath: OutputDir);
				SC.StageFiles(stageType, dir, Path.ChangeExtension(name, ".json"), false, bAllowNone: true, NewPath: OutputDir);
				if (!Params.NoDebugInfo)
				{
					SC.StageFiles(stageType, dir, Path.ChangeExtension(name, ".pdb"), false, bAllowNone: true, NewPath: OutputDir);
				}
			}
		}

		//MUST BE IN SYNC : MonoUE.Core.props, MonoRuntime.Plugin.cs, MonoMainDomain.cpp, MonoRuntimeStagingRules.cs, MonoScriptCodeGenerator.cpp, and IDE extensions
		static RuntimeInfo GetRuntimeInfo(string PlatformIntermediateDir, string MonoPath, string Platform)
		{
			string RuntimeInfoCache = Path.Combine(PlatformIntermediateDir, "MonoRuntimeInfo.json");
			string RuntimeInfoString;

			//TODO: refresh the cache?
			if (File.Exists(RuntimeInfoCache))
			{
				RuntimeInfoString = File.ReadAllText(RuntimeInfoCache);
			}
			else
			{
				var MonoTool = GetMonoEmbeddingTool (MonoPath, Platform);
				var psi = new ProcessStartInfo(MonoTool, "info");

				var writer = new StringWriter();
				var task = StartProcess(psi, writer, null, CancellationToken.None);

				if (task.Result > 0)
				{
					throw new BuildException("Mono embedding tool did not return info for platform '{0}'", Platform);
				}

				RuntimeInfoString = writer.ToString();
				Directory.CreateDirectory(PlatformIntermediateDir);
				File.WriteAllText(RuntimeInfoCache, RuntimeInfoString);
			}

			var RuntimeInfo = new RuntimeInfo();
			fastJSON.JSON.Instance.FillObject(RuntimeInfo, RuntimeInfoString);

			return RuntimeInfo;
		}

		static string GetMonoEmbeddingTool (string Directory, string Platform)
		{
			var MonoTool = File.ReadAllLines (Path.Combine (Directory, "MonoLocation.txt"))
				.Select (l => l.TrimStart())
				.Where (l => l.StartsWith (Platform + " "))
				.Select (l => l.Substring (Platform.Length + 1).Trim())
				.First();
			return Path.GetFullPath (Path.Combine (Directory, MonoTool));
		}

		class RuntimeInfo
		{
	#pragma warning disable 649
			public string FrameworkId;
			public string FrameworkVersion;
			public string RuntimeAssemblyDir;
			public string[] IncludePaths;
			public string[] StaticLibs;
			public string[] DynamicLibs;
#pragma warning restore 649
		}

		static PackagedAssemblyInfo PackageAssemblies(string MonoPath, string Platform, IEnumerable<string> Assemblies, IEnumerable<string> SearchDirectories, bool Debug, bool Link, string OutputDir)
		{
			var sb = new StringBuilder();
			sb.Append ("package");

			foreach (var dir in SearchDirectories)
			{
				sb.Append(" --search=" + Quote(dir));
			}

			if (Debug)
			{
				sb.Append (" --debug");
			}

			if (Link)
			{
				sb.Append (" --link");
			}

			if (OutputDir != null)
			{
				sb.Append(" --output=" + Quote(OutputDir));
			}

			foreach (var asm in Assemblies)
			{
				sb.Append(" " + Quote(asm));
			}

			string MonoTool = GetMonoEmbeddingTool (MonoPath, Platform);

			var psi = new ProcessStartInfo(MonoTool, sb.ToString ());

			var writer = new StringWriter();
			var task = StartProcess(psi, writer, null, CancellationToken.None);

			if (task.Result > 0)
			{
				throw new BuildException("Mono embedding tool failed to package assemblies for platform '{0}'", Platform);
			}

			var	PackagedAssemblyInfoString = writer.ToString();

			Console.WriteLine(PackagedAssemblyInfoString);

			var PackagedAssemblyInfo = new PackagedAssemblyInfo();
			fastJSON.JSON.Instance.FillObject(PackagedAssemblyInfo, PackagedAssemblyInfoString);

			return PackagedAssemblyInfo;
		}

		static string Quote (string s)
		{
			return "\"" + s.Replace("\\", "\\\\").Replace ("\"", "\\\"") + "\"";
		}

		class PackagedAssemblyInfo
		{
#pragma warning disable 649
			public string[] Assemblies;
			public string[] StaticLibs;
			public string[] DynamicLibs;
#pragma warning restore 649
		}

		//MIT license, from MonoDevelop ProcessUtils.cs
		static Task<int> StartProcess(ProcessStartInfo psi, TextWriter stdout, TextWriter stderr, CancellationToken cancellationToken)
		{
			var tcs = new TaskCompletionSource<int>();
			if (cancellationToken.CanBeCanceled && cancellationToken.IsCancellationRequested)
			{
				tcs.TrySetCanceled();
				return tcs.Task;
			}

			psi.UseShellExecute = false;
			psi.RedirectStandardOutput |= stdout != null;
			psi.RedirectStandardError |= stderr != null;

			var p = Process.Start(psi);

			if (cancellationToken.CanBeCanceled)
			{
				cancellationToken.Register(() =>
				{
					try
					{
						if (!p.HasExited)
						{
							p.Kill();
						}
					}
					catch (InvalidOperationException ex)
					{
						if (ex.Message.IndexOf("already exited", StringComparison.Ordinal) < 0)
							throw;
					}
				});
			}

			bool outputDone = false;
			bool errorDone = false;
			bool exitDone = false;

			p.EnableRaisingEvents = true;
			if (psi.RedirectStandardOutput)
			{
				bool stdOutInitialized = false;
				p.OutputDataReceived += (sender, e) =>
				{
					try
					{
						if (e.Data == null)
						{
							outputDone = true;
							if (exitDone && errorDone)
								tcs.TrySetResult(p.ExitCode);
							return;
						}

						if (stdOutInitialized)
							stdout.WriteLine();
						stdout.Write(e.Data);
						stdOutInitialized = true;
					}
					catch (Exception ex)
					{
						tcs.TrySetException(ex);
					}
				};
				p.BeginOutputReadLine();
			}
			else
			{
				outputDone = true;
			}

			if (psi.RedirectStandardError)
			{
				bool stdErrInitialized = false;
				p.ErrorDataReceived += (sender, e) =>
				{
					try
					{
						if (e.Data == null)
						{
							errorDone = true;
							if (exitDone && outputDone)
								tcs.TrySetResult(p.ExitCode);
							return;
						}

						if (stdErrInitialized)
							stderr.WriteLine();
						stderr.Write(e.Data);
						stdErrInitialized = true;
					}
					catch (Exception ex)
					{
						tcs.TrySetException(ex);
					}
				};
				p.BeginErrorReadLine();
			}
			else
			{
				errorDone = true;
			}

			p.Exited += (sender, e) =>
			{
				exitDone = true;
				if (errorDone && outputDone)
					tcs.TrySetResult(p.ExitCode);
			};

			return tcs.Task;
		}

		//MUST BE IN SYNC : MonoUE.Core.props, MonoRuntime.Plugin.cs, MonoMainDomain.cpp, MonoRuntimeStagingRules.cs, MonoScriptCodeGenerator.cpp, and IDE extensions
		string GetAssemblyDirectory(UnrealTargetConfiguration Configuration, UnrealTargetPlatform Platform, TargetRules.TargetType TargetType, string RootDirectory = null)
		{
			string ConfigSuffix = null;
			switch (Configuration)
			{
				case UnrealTargetConfiguration.Debug:
					ConfigSuffix = "-Debug";
					break;
				case UnrealTargetConfiguration.DebugGame:
					ConfigSuffix = "-DebugGame";
					break;
				case UnrealTargetConfiguration.Development:
					ConfigSuffix = null;
					break;
				case UnrealTargetConfiguration.Test:
					ConfigSuffix = "-Test";
					break;
				case UnrealTargetConfiguration.Shipping:
					ConfigSuffix = "-Shipping";
					break;
			}

			string Name = "Mono";
			switch (TargetType)
			{
				case TargetRules.TargetType.Editor:
					Name += "Editor";
					break;
				case TargetRules.TargetType.Client:
					Name += "Client";
					break;
				case TargetRules.TargetType.Server:
					Name += "Server";
					break;
			}

			var dir = Path.Combine("Binaries", Platform.ToString(), Name + ConfigSuffix);
			return RootDirectory == null ? dir : Path.Combine(RootDirectory, dir);
		}
	}
*/
}