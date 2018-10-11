using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Diagnostics;

class MonoUEBootstrapper
{
	const string DependenciesUrl = "https://dl.xamarin.com/uploads/cromwvi2iag/MonoUEDependencies-c8df6088-withvsix.zip";
	const string DependenciesSha1 = "23df8f521c219f05bfd376296f2b0cf740c0bcc5";

	public static int Main(string[] args)
	{
		var instance = new MonoUEBootstrapper()
		{
			Verbose = Environment.GetEnvironmentVariable("MONOUE_BUILD_VERBOSE") != null || args.Any(a => a == "-v" || a == "--verbose")
		};

		try
		{
			instance.EnsureDependencies();
			return 0;
		}
		catch (Exception ex)
		{
			instance.PrintDeferredMessages();
			Console.WriteLine(ex.ToString());
			return 1;
		}
	}

	bool Verbose;
	List<string> LogMessages = new List<string>();

	void EnsureDependencies()
	{
		string thisDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
		string monoUEDir = Path.GetFullPath(Path.Combine(thisDir, "..", ".."));

		string dependenciesDir = Path.Combine(monoUEDir, "Intermediate", "MonoUEDependencies");
		string dependenciesFileName = Path.GetFileName(new Uri(DependenciesUrl).LocalPath);
		string dependenciesFilePath = Path.Combine(dependenciesDir, dependenciesFileName);
		string flagFilePath = Path.Combine(dependenciesDir, "extracted.txt");

		bool isWindows = System.Environment.OSVersion.Platform == PlatformID.Win32NT;
		bool isMac = Directory.Exists("/System/Library/Frameworks/CoreFoundation.framework");

		if (!isMac && !isWindows) {
			System.Console.Error.WriteLine("Only Windows and Mac are currently supported");
			Environment.Exit(101);
		}

		Console.WriteLine("");
		Console.Write("Checking MonoUE dependencies...");

		if (File.Exists(flagFilePath) && File.ReadAllText(flagFilePath).Trim() == DependenciesUrl)
		{
			Console.WriteLine("Up to date");
			return;
		}
		Console.WriteLine("Downloading and unpacking");

		Log($"Checking for usable existing archive: {dependenciesFilePath}");
		if (File.Exists(dependenciesFilePath))
		{
			var sha1 = GetFileSha1(dependenciesFilePath);
			if (sha1 != DependenciesSha1)
			{
				Log($"Hash mismatch! Expected {DependenciesSha1}, got{sha1}. Deleting file.");
				File.Delete(dependenciesFilePath);
			}
		}

		if (!File.Exists(dependenciesFilePath))
		{
			Log("Ensuring dependencies directory exists");
			Directory.CreateDirectory(dependenciesDir);

			//TODO system-wide cache?
			Log($"Downloading dependencies file: {DependenciesUrl}");
			var client = new WebClient();
			int percentage = 0;
			client.DownloadProgressChanged += (s, e) =>
			{
				int p = percentage;
				if (e.ProgressPercentage >= (p + 10) && Interlocked.CompareExchange(ref percentage, e.ProgressPercentage, p) == p)
				{
					Log($"{e.ProgressPercentage}% complete");
				}
			};

			Console.CancelKeyPress += (s, e) => client.CancelAsync();
			var task = client.DownloadFileTaskAsync(DependenciesUrl, dependenciesFilePath);
			task.Wait();

			Log("Download completed, checking hash.");

			var sha1 = GetFileSha1(dependenciesFilePath);
			if (sha1 != DependenciesSha1)
			{
				Log($"Hash mismatch! Expected {DependenciesSha1}, got {sha1}. Deleting file.");
				File.Delete(dependenciesFilePath);
				throw new Exception("Hash mismatch");
			}
		}

		Log("Extracting files");
		var zip = ZipFile.OpenRead(dependenciesFilePath);
		foreach (var entry in zip.Entries)
		{
			var dest = Path.GetFullPath(Path.Combine(monoUEDir, entry.FullName));
			if (Path.GetFileName(dest).Length == 0)
			{
				Log($"Creating {dest}");
				Directory.CreateDirectory(dest);
			}
			else
			{
				Log($"Extracting {dest}");
				Directory.CreateDirectory(Path.GetDirectoryName(dest));
				entry.ExtractToFile(dest, true);
			}
		}

		Log("Installing MSBuild SDK Resolver");
		if (isWindows) {
			InstallVsix(thisDir);
		} else {
			InstallMacResolver(monoUEDir);
		}

		Log("Writing flag file");
		File.WriteAllText(flagFilePath, DependenciesUrl);

		Console.WriteLine("MonoUE dependencies updated to " + dependenciesFileName);
	}

	string GetFileSha1(string filename)
	{
		using (FileStream fileStream = new FileStream(filename, FileMode.Open, FileAccess.Read, FileShare.Read))
		using (var provider = SHA1.Create())
		{
			byte[] hash = provider.ComputeHash(fileStream);
			var sb = new StringBuilder(hash.Length);
			foreach (var b in hash)
			{
				sb.Append(string.Format("{0:x2}", b));
			}
			return sb.ToString();
		}
	}

	void Log(string message)
	{
		if (Verbose)
		{
			Console.WriteLine(message);
		}
		else
		{
			LogMessages.Add(message);
		}
	}

	void PrintDeferredMessages()
	{
		if (!Verbose)
		{
			foreach (var message in LogMessages)
			{
				Console.WriteLine(message);
			}
		}
	}

	//the vsix is shipped as a binary so we don't depend on the VS SDK
	void InstallVsix(string thisDir)
	{
        const string VSIX_NAME = "MonoUE.MSBuildResolver.vsix";
		const int ALREADY_INSTALLED = 1001;
        var vsix = Path.Combine(thisDir, VSIX_NAME);

		int exitCode = RunVsixInstaller (vsix, true);
		if (exitCode <= 0 || exitCode == ALREADY_INSTALLED)
		{
			return;
		}

		Log($"Failed to silently install {VSIX_NAME}, code {exitCode}. Attempting interactive install.");

		exitCode = RunVsixInstaller (vsix, false);
		if (exitCode <= 0 || exitCode == ALREADY_INSTALLED)
		{
			return;
		}

		Log($"Failed to install {VSIX_NAME}, code {exitCode}");
		Log("\nPlease ensure you have Visual Studio 2017 installed and fully updated\n");
		Environment.Exit(101);
	}

	int RunVsixInstaller(string vsix, bool noRepair)
	{
		var vsixInstaller = Path.Combine(GetVisualStudioRoot(), "Common7", "IDE", "VSIXInstaller.exe");

		string args = $"\"{vsix}\"";
		if (noRepair)
		{
			args = "/quiet /norepair " + args;
		}

		var MSBuildProcess = System.Diagnostics.Process.Start(
			new System.Diagnostics.ProcessStartInfo(vsixInstaller, args)
			{
				UseShellExecute = false
			});
		MSBuildProcess.WaitForExit();
		return MSBuildProcess.ExitCode;
	}

	void InstallMacResolver(string monoUEPath)
	{
		//HACK: this is temporary until Mono.framework/External/SdkResolvers exists and we can install a pkg
		//FIXME: how do we install newer versions?
		var msbuildDir = Path.Combine(monoUEPath, "MSBuild", "mac-msbuild.sh");
		var projectDir = Path.Combine(monoUEPath, "Source", "Programs", "MonoUE.MSBuildSdkResolver");
		var resolverDir = "/Library/Frameworks/Mono.framework/Versions/Current/lib/mono/msbuild/15.0/bin/SdkResolvers/MonoUE.MSBuildSdkResolver";
		if (File.Exists(Path.Combine(resolverDir, "MonoUE.MSBuildSdkResolver.dll"))) {
			return;
		}

		if (!RestoreAndBuild(msbuildDir, projectDir))
		{
			Console.Error.WriteLine("Failed to restore and build SDK resolver");
			Environment.Exit(101);
		}

		var binDir = Path.Combine(projectDir, "bin", "Debug", "net461");
		var resolverDll = Path.Combine(binDir, "MonoUE.MSBuildSdkResolver.dll");
		var jsonDll = Path.Combine(binDir, "System.Json.dll");

		Console.WriteLine("Installing MonoUE MSBuild SDK resolver. Please enter your password.");
		var command = $"sh -c 'mkdir -p \\'{resolverDir}\\' && cp \\'MonoUE.MSBuildSdkResolver.dll\\' \\'System.Json.dll\\' \\'{resolverDir}\\''";
		Console.WriteLine($"sudo {command}");
		var installProcess = Process.Start(
			new ProcessStartInfo("sudo", command) {
				UseShellExecute = true,
				WorkingDirectory = binDir
			});
		installProcess.WaitForExit();
	}

	bool RestoreAndBuild(string msbuildExe, string projectDir)
	{
		string args = "/nologo";
		if (!Verbose)
		{
			args += " /v:minimal";
		}

		var restoreProcess = Process.Start(
			new ProcessStartInfo(msbuildExe, args + " /r") {
				UseShellExecute = false,
				WorkingDirectory = projectDir
			});
		restoreProcess.WaitForExit();

		if (restoreProcess.ExitCode > 0)
		{
			return false;
		}

		var buildProcess = Process.Start(
			new ProcessStartInfo(msbuildExe, args) {
				UseShellExecute = false,
				WorkingDirectory = projectDir
			});
		buildProcess.WaitForExit();

		return buildProcess.ExitCode <= 0;
	}

	static string GetVisualStudioRoot()
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
			if (!string.IsNullOrEmpty(vsroot))
			{
				return vsroot;
			}
		}

		throw new Exception("MonoUE requires Visual Studio 2017");
	}
}
