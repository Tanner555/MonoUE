// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace MonoUEBuildTool
{
    class MSBuildHelpers
    {
        public static bool BuildSolution(string sln, string target, bool restore, BuildArguments args)
        {
            var sb = new StringBuilder();

            sb.AppendFormat("/t:{0}", target);

            sb.AppendFormat(
                " /p:Configuration=\"{0}\" /p:Platform=\"{1}\"",
                ConfigurationHelpers.GetSolutionConfigurationName(args.TargetConfiguration, args.TargetType),
                args.TargetPlatform
            );

            //parallel
            sb.Append(" /m");

            if (restore)
            {
                sb.Append(" /r");
            }

            //less verbose
            sb.Append(" /nologo /v:minimal");

            //output binary log
            if (!string.IsNullOrEmpty (Environment.GetEnvironmentVariable ("MONOUE_BINLOG")))
            {
                sb.Append(" /bl");
            }

            sb.AppendFormat(" \"{0}\"", Path.GetFullPath(sln));

            var msbuildExe = GetMSBuildPath(args.PluginDir);

            var process = System.Diagnostics.Process.Start(
                new System.Diagnostics.ProcessStartInfo(msbuildExe, sb.ToString ())
                {
                    UseShellExecute = false,
                    WorkingDirectory = Path.GetDirectoryName(sln)
                });

            process.WaitForExit();

            if (process.ExitCode > 0)
            {
                Console.Error.WriteLine($"Solution '{sln} failed with code {process.ExitCode}'");
                return false;
            }

            return true;
        }

        static string GetMSBuildPath(string monoUEDirectory)
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                return GetMSBuildPathWindows();
            }

            if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                return Path.Combine(monoUEDirectory, "MSBuild", "mac-msbuild.sh");
            }

            throw new InvalidOperationException("Unsupported host platform");
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

            throw new NotSupportedException("MonoUE requires Visual Studio 2017");
        }
    }
}
