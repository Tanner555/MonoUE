// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Runtime.InteropServices;

namespace MonoUE.IdeAgent
{
    class UnrealEditorLog : IUnrealAgentLogger
    {
        void IUnrealAgentLogger.Log (string messageFormat, params object[] args) => Log(messageFormat, args);
        void IUnrealAgentLogger.Warn (string messageFormat, params object[] args) => Warn(messageFormat, args);
        void IUnrealAgentLogger.Error (Exception ex, string messageFormat, params object[] args) => Error(ex, messageFormat, args);

        public static void Log (string messageFormat, params object[] args)
        {
            var m = string.Format(messageFormat, args);
            MonoIdeAgent_Log(LogVerbosity.Log, m);
        }

        public static void Warn (string messageFormat, params object[] args)
        {
            var m = string.Format(messageFormat, args);
            MonoIdeAgent_Log(LogVerbosity.Warning, m);
        }

        public static void Error (Exception ex, string messageFormat, params object[] args)
        {
            var m = string.Format(messageFormat, args);
            if (ex != null)
                m += "\n" + ex;
            MonoIdeAgent_Log(LogVerbosity.Error, m);
        }

        [DllImportAttribute("MonoEditor")]
        static extern void MonoIdeAgent_Log(
            LogVerbosity verbosity,
            [MarshalAs(UnmanagedType.LPWStr)]string message);

        enum LogVerbosity
        {
            NoLogging = 0,
            Fatal,
            Error,
            Warning,
            Display,
            Log,
            Verbose,
            VeryVerbose,
            All = VeryVerbose,
        }
    }
}