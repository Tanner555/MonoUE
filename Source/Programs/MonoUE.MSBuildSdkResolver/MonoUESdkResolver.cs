// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using Microsoft.Build.Framework;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

namespace MonoUE.MSBuildSdkResolver
{
	//
	// NOTE
	//
	// This project targets net461 instead of netstandard, because for some reason the Microsoft.Win32.Registry
	// netstandard assembly would not load in msbuild. this means it can't load in vs code. however, we don't
	// have a good way to install sdk resolvers into vs code anyway.
	//
	public class MonoUESdkResolver : SdkResolver
	{
		public override string Name => "MonoUE.MSBuildSdkResolver";

		// got after the default one so we don't cause MonoUE resolver errors to be emitted for non-UE4 projects
		// this is annoying, as it means we can't provide useful errors to the user when it *is* a UE4 project
		public override int Priority => 15000;

		public override SdkResult Resolve(SdkReference sdkReference, SdkResolverContext resolverContext, SdkResultFactory factory)
		{
			if (sdkReference.Name != "Mono.UE4.Sdk")
			{
				return factory.IndicateFailure(new[] { $"Not a UE4 SDK reference" });
			}

			//provide a way to force a specific directory
			string engineDir = Environment.GetEnvironmentVariable("MONOUE_SDKRESOLVER_OVERRIDE_ENGINE_DIR");

			if (string.IsNullOrEmpty(engineDir))
			{
				var uproject = GetUProjectFromMSBuildProject(resolverContext.SolutionFilePath, resolverContext.ProjectFilePath);
				if (uproject == null)
				{
					return factory.IndicateFailure(new[] { $"Could not find a uproject file" });
				}

				var engineAssociation = ReadEngineAssociationFromUProject(uproject);
				engineDir = GetEngineFromID(engineAssociation);

				if (string.IsNullOrEmpty(engineDir))
				{
					return factory.IndicateFailure(new[] { $"Could not find UE4 engine matching '{engineAssociation}'" });
				}
			}

			if (!Directory.Exists(engineDir))
			{
				return factory.IndicateFailure(new[] { $"UE4 engine directory '{engineDir}' does not exist" });
			}

			if (!IsValidEngineDirectory(engineDir))
			{
				return factory.IndicateFailure(new[] { $"Engine '{engineDir}' is not a valid installation" });
			}

			if (!IsMonoUEEngineDirectory(engineDir))
			{
				return factory.IndicateFailure(new[] { $"Engine '{engineDir}' does not contain MonoUE plugin" });
			}

			var sdkDir = Path.Combine(engineDir, "Engine", "Plugins", "MonoUE", "MSBuild", "Sdks", sdkReference.Name, "Sdk");

			if (Directory.Exists(sdkDir))
			{
				string engineVersion = GetEngineVersion(engineDir);
				return factory.IndicateSuccess(sdkDir, "1.0");
			}

			return factory.IndicateFailure(new[] { $"Did not find SDK '{sdkReference.Name}'" });
		}

		static string GetUProjectFromMSBuildProject(string msbuildSolutionFile, string msbuildProjectFile)
		{
			//check for a uproject matching the sln
			if (msbuildSolutionFile != null)
			{
				var possibleUProject = Path.ChangeExtension(msbuildSolutionFile, "uproject");
				if (File.Exists(possibleUProject))
				{
					return possibleUProject;
				}

				const string managedSlnSuffix = "_Managed.sln";
				if (msbuildSolutionFile.EndsWith(managedSlnSuffix, StringComparison.OrdinalIgnoreCase))
				{
					possibleUProject = msbuildSolutionFile.Substring(0, msbuildSolutionFile.Length - managedSlnSuffix.Length) + ".uproject";
					if (File.Exists(possibleUProject))
					{
						return possibleUProject;
					}
				}
			}

			//fallback: search parent directories for a uproject
			var parentDir = Path.GetDirectoryName(msbuildProjectFile);
			while(!string.IsNullOrEmpty(parentDir))
			{
				var uprojects = Directory.GetFiles(parentDir, "*.uproject");
				if (uprojects.Length > 0)
				{
					return uprojects[0];
				}
				parentDir = Path.GetDirectoryName(parentDir);
			}

			return null;
		}

		static string ReadEngineAssociationFromUProject(string uProjectFile)
		{
			using (var file = File.OpenText(uProjectFile))
			{
				var uProject = (System.Json.JsonObject)System.Json.JsonValue.Load(file);
				return uProject["EngineAssociation"];
			}
		}

		static string GetEngineFromID(string engineID)
		{
			if (Guid.TryParse(engineID, out Guid engineGuid))
			{
				foreach (var installation in EnumerateEngineInstallations())
				{
					if (Guid.TryParse(installation.ID, out Guid installationGuid) && installationGuid == engineGuid)
					{
						return installation.Path;
					}
				}
			}
			else
			{
				foreach (var installation in EnumerateEngineInstallations())
				{
					if (string.Equals(installation.ID, engineID, StringComparison.OrdinalIgnoreCase))
					{
						return installation.Path;
					}
				}
			}
			return null;
		}

		struct EngineInstallation
		{
			public string ID, Path;
		}

		static IEnumerable<EngineInstallation> EnumerateEngineInstallations()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return EnumerateEngineInstallationsWindows();
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				return EnumerateEngineInstallationsMac();
			}
			else
			{
				throw new PlatformNotSupportedException();
			}
		}

		static IEnumerable<EngineInstallation> EnumerateEngineInstallationsWindows()
		{
			// TODO: read launcher installations using LauncherInstalled.dat
			// doesn't really matter until we support unpatched engines

			using (var hkcu64 = RegistryKey.OpenBaseKey(RegistryHive.CurrentUser, RegistryView.Registry64))
			using (var buildsKey = hkcu64.OpenSubKey(@"Software\Epic Games\Unreal Engine\Builds"))
			{
				if (buildsKey != null)
				{
					foreach (var valueID in buildsKey.GetValueNames())
					{
						var path = (string)buildsKey.GetValue(valueID);
						yield return new EngineInstallation { ID = valueID, Path = path };
					}
				}
			}
		}

		static IEnumerable<EngineInstallation> EnumerateEngineInstallationsMac()
		{
			// TODO: find launcher installations using LSCopyApplicationURLsForURL
			// doesn't really matter until we support unpatched engines

			var homeDir = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
			var iniFile = Path.Combine(homeDir, "Library", "Application Support", "Epic", "UnrealEngine", "Install.ini");
			using (var reader = File.OpenText(iniFile))
			{
				bool foundSection = false;
				while(!reader.EndOfStream)
				{
					var line = reader.ReadLine();
					if (line == null)
					{
						yield break;
					}
					if (string.IsNullOrWhiteSpace(line))
					{
						continue;
					}

					if (!foundSection)
					{
						foundSection = line.StartsWith("[Installations]", StringComparison.Ordinal);
						continue;
					}

					var idx = line.IndexOf('=');
					if (idx >= 0)
					{
						yield return new EngineInstallation
						{
							ID = line.Substring(0, idx),
							Path = line.Substring(idx + 1).Trim()
						};
					}
				}
			}
		}

		static string GetEngineVersion(string enginePath)
		{
			var versionFilePath = Path.Combine(enginePath, "Engine", "Build", "Build.version");
			using (var versionFile = File.OpenText(versionFilePath))
			{
				var versionObj = (System.Json.JsonObject)System.Json.JsonValue.Load(versionFile);
				int majorVersion = versionObj["MajorVersion"];
				int minorVersion = versionObj["MinorVersion"];
				int patchVersion = versionObj["PatchVersion"];
				return $"{majorVersion}.{minorVersion}.{patchVersion}";
			}

			// we could also parse Engine/Source/Runtime/Launch/Resources/Version.h as a fallback
		}

		static bool IsValidEngineDirectory(string engineRoot)
		{
			//this is the logic from FDesktopPlatformBase::IsValidRootDirectory
			return Directory.Exists(Path.Combine(engineRoot, "Engine", "Binaries")) && Directory.Exists(Path.Combine(engineRoot, "Engine", "Build"));
		}

		static bool IsMonoUEEngineDirectory(string engineRoot)
		{
			//this is the logic from FDesktopPlatformBase::IsValidRootDirectory
			return Directory.Exists(Path.Combine(engineRoot, "Engine", "Plugins", "MonoUE", "MSBuild"));
		}

	}
}
