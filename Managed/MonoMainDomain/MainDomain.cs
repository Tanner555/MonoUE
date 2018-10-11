// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnrealEngine.Runtime;
using MonoUE.IdeAgent;

namespace UnrealEngine.MainDomain
{
    public class MainDomain
    {
        public static void Initialize(bool withAppDomains)
        {
            if(withAppDomains)
            {
                // if we're using app domains, set up logging for this domain
                Console.SetOut(LogTextWriter.Create());
                Console.SetError(LogTextWriter.Create());
                // hook unhandled exception handler
                AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
            }
            else
            {
                // we're not using app domains, so game initialization code will set up logging
            }
        }

        static void OnUnhandledException (object sender, UnhandledExceptionEventArgs e)
        {
            try
            {
                Exception ex = (Exception)e.ExceptionObject;

                OnUnhandledExceptionNative(ex.Message, ex.StackTrace);
            }
            catch(Exception)
            {
                OnUnhandledExceptionNative("Unknown fatal error", "No stack trace");
            }
        }

        [DllImport("__MonoRuntime", EntryPoint = "Bindings_OnUnhandledExceptionNative")]
        extern private static void OnUnhandledExceptionNative([MarshalAs(UnmanagedType.LPWStr)]string message, [MarshalAs(UnmanagedType.LPWStr)]string stackTrace);
    }
}
