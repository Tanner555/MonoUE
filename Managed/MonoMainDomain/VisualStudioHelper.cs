using System;
using System.IO;
using System.Runtime.InteropServices;

namespace MonoUE.IdeAgent
{
    public static class VisualStudioHelper
    {
        public static string GetVisualStudioInstalledPath()
        {
            var visualStudioInstalledPath = GetVisualStudioCommonToolsPath();
            if (!string.IsNullOrWhiteSpace(visualStudioInstalledPath))
            {
                visualStudioInstalledPath = Path.GetFullPath(Path.Combine(visualStudioInstalledPath, "..", "IDE"));
            }
    
            return visualStudioInstalledPath; 
        }

        public static string GetVisualStudioExecutableName()
        {
            return "devenv.exe";
        }

        public static string GetVisualStudioExecutableFullQualifiedPath()
        {
            var vsDirectory = GetVisualStudioInstalledPath();
            var vsExecName = GetVisualStudioExecutableName();

            if (string.IsNullOrEmpty(vsExecName))
            {
                return null;
            }

            return Path.Combine(vsDirectory, vsExecName);
        }

        [DllImport("MonoEditor", EntryPoint = "MonoIdeAgent_GetVisualStudioCommonToolsPath", CharSet = CharSet.Auto)]
        static extern string GetVisualStudioCommonToolsPath();
    }
}
