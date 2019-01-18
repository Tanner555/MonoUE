// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using Microsoft.Build.Framework;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Linq;

namespace MonoUE.MSBuildSdkResolver
{
    static class SdkResolverExtensions
    {
        public static SdkResult IndicateFailureAndLog(this SdkResultFactory factory, IEnumerable<string> errors, IEnumerable<string> warnings = null)
        {
            System.Windows.Forms.MessageBox.Show("Mono.UE4.Sdk failed. Reason: " + string.Join(Environment.NewLine, errors));
            return factory.IndicateFailure(errors, warnings);
        }
    }

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
					return factory.IndicateFailureAndLog(new[] { $"Could not find a uproject file" });
				}

				var engineAssociation = ReadEngineAssociationFromUProject(uproject);
				engineDir = GetEngineFromID(engineAssociation);

				if (string.IsNullOrEmpty(engineDir))
				{
                    string installedLocationsInfo = ". Found: " + string.Join(", ", EnumerateEngineInstallations(engineAssociation, string.Empty).ToList().Select(x => x.ID));
                    return factory.IndicateFailureAndLog(new[] { $"Could not find UE4 engine matching '{engineAssociation}' {installedLocationsInfo}"  });
				}
			}

			if (!Directory.Exists(engineDir))
			{
				return factory.IndicateFailureAndLog(new[] { $"UE4 engine directory '{engineDir}' does not exist" });
			}

			if (!IsValidEngineDirectory(engineDir))
			{
				return factory.IndicateFailureAndLog(new[] { $"Engine '{engineDir}' is not a valid installation" });
			}

			if (!IsMonoUEEngineDirectory(engineDir))
			{
				return factory.IndicateFailureAndLog(new[] { $"Engine '{engineDir}' does not contain MonoUE plugin" });
			}

			var sdkDir = Path.Combine(engineDir, "Engine", "Plugins", "MonoUE", "MSBuild", "Sdks", sdkReference.Name, "Sdk");

			if (Directory.Exists(sdkDir))
			{
				string engineVersion = GetEngineVersion(engineDir);
				return factory.IndicateSuccess(sdkDir, "1.0");
			}

			return factory.IndicateFailureAndLog(new[] { $"Did not find SDK '{sdkReference.Name}'" });
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
                foreach (var installation in EnumerateEngineInstallations(engineID, engineGuid.ToString()))
				{
					if (Guid.TryParse(installation.ID, out Guid installationGuid) && installationGuid == engineGuid)
					{
						return installation.Path;
					}
				}
			}
			else
			{
				foreach (var installation in EnumerateEngineInstallations(engineID, string.Empty))
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

		static IEnumerable<EngineInstallation> EnumerateEngineInstallations(string engineID, string engineIDFromGUID)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return EnumerateEngineInstallationsWindows(engineID, engineIDFromGUID);
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

        static IEnumerable<EngineInstallation> EnumerateEngineInstallationsWindows(string engineID, string engineIDFromGUID)
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
                        if (!string.IsNullOrEmpty(path) && Directory.Exists(path))
                        {
                            yield return new EngineInstallation { ID = valueID, Path = path };
                        }
					}
				}
			}

            string localSoftwareUEKeyString = @"SOFTWARE\EpicGames\Unreal Engine\";
            using (var localkey64 = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
            using (var localSoftwareUEKey = localkey64.OpenSubKey(localSoftwareUEKeyString))
            {
                if(localSoftwareUEKey != null)
                {
                    string[] subkeyNames = localSoftwareUEKey.GetSubKeyNames();
                    //Used to get the UE4 installation path from the Local Machine Registry
                    string ue4MajorVersion = string.Empty;
                    foreach (string subKeyName in subkeyNames)
                    {
                        //If SubKey Is Equal To Engine ID, Or If ID From GUID Isn't Empty And SubKey Is Equal To It
                        if(subKeyName == engineID || (!string.IsNullOrEmpty(engineIDFromGUID) && subKeyName == engineIDFromGUID))
                        {
                            ue4MajorVersion = subKeyName;
                            break;
                        }
                    }
                    //If Major UE4 Version is Found, Then Retrieve InstalledDirectory Key Value
                    if (!string.IsNullOrEmpty(ue4MajorVersion))
                    {
                        using (var localSoftwareUEMajorInstallKey = localkey64.OpenSubKey(localSoftwareUEKeyString + ue4MajorVersion))
                        {
                            string valueName = "InstalledDirectory";
                            var path = (string)localSoftwareUEMajorInstallKey.GetValue(valueName);
                            if (!string.IsNullOrEmpty(path) && Directory.Exists(path))
                            {
                                yield return new EngineInstallation { ID = ue4MajorVersion, Path = path };
                            }
                        }
                    }
                }
            }

            using (var hkcu64 = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
            using (var key = hkcu64.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"))
            {
                foreach (string subkeyName in key.GetSubKeyNames())
                {
                    using (RegistryKey subkey = key.OpenSubKey(subkeyName))
                    {
                        string displayName = subkey.GetValue("DisplayName") as string;
                        if (displayName == "Unreal Engine")
                        {
                            string installLocation = subkey.GetValue("InstallLocation") as string;
                            if (!string.IsNullOrEmpty(installLocation) && Directory.Exists(installLocation))
                            {
                                DirectoryInfo directory = new DirectoryInfo(installLocation);
                                foreach (DirectoryInfo engineVersion in directory.GetDirectories())
                                {
                                    if (Directory.Exists(Path.Combine(engineVersion.FullName, "Engine")) &&
                                       Directory.Exists(Path.Combine(engineVersion.FullName, "Templates")))
                                    {
                                        yield return new EngineInstallation { ID = engineVersion.Name, Path = engineVersion.FullName };

                                        if (engineVersion.Name.StartsWith("UE_") && engineVersion.Name.Length > 3)
                                        {
                                            yield return new EngineInstallation { ID = engineVersion.Name.Substring(3), Path = engineVersion.FullName };
                                        }
                                    }
                                }
                            }
                        }
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
