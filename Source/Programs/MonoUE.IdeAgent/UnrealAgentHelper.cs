// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;
using System.IO;

namespace MonoUE.IdeAgent
{
#if AGENT_CLIENT
    public
#endif
    static class UnrealAgentHelper
    {
        public readonly static bool IsWindows;
        public readonly static bool IsMac;

        static UnrealAgentHelper ()
        {
            IsWindows = Path.DirectorySeparatorChar == '\\';
            IsMac = !IsWindows && IsRunningOnMac ();
        }

        //From Managed.Windows.Forms/XplatUI
        static bool IsRunningOnMac ()
        {
            IntPtr buf = IntPtr.Zero;
            try {
                buf = Marshal.AllocHGlobal (8192);
                // This is a hacktastic way of getting sysname from uname ()
                if (uname (buf) == 0) {
                    string os = Marshal.PtrToStringAnsi (buf);
                    if (os == "Darwin")
                        return true;
                }
            }
            // Analysis disable once EmptyGeneralCatchClause
            catch {
            } finally {
                if (buf != IntPtr.Zero)
                    Marshal.FreeHGlobal (buf);
            }
            return false;
        }

        [DllImport ("libc")]
        static extern int uname (IntPtr buf);
    }
}
