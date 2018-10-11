// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using UnrealEngine.MainDomain;

namespace MonoUE.IdeAgent
{
    class UnrealAgentServer : UnrealAgent
    {
        static UnrealAgentServer instance;

        //called by MonoRuntime.cpp
        public static void Start(string engineRoot, string gameRoot)
        {
            if (instance != null)
                throw new InvalidOperationException();

            instance = new UnrealAgentServer(engineRoot, gameRoot);
        }

        //called by MonoRuntime.cpp
        public static void Stop()
        {
            if (instance == null)
                throw new InvalidOperationException();

            instance.Dispose();
            instance = null;
        }

        //Unreal appends file separator to directory paths
        static string CleanDirectoryPath (string dir)
        {
            return Path.GetFullPath(dir.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
        }

        TcpListener listener;
        FileStream lockFile;

        UnrealAgentServer(string engineRoot, string gameRoot)
            : base (new UnrealEditorLog (), CleanDirectoryPath(engineRoot), CleanDirectoryPath(gameRoot))
        {
            try {
                //file may still be open if the previous process crashed, and hidden files not writable
                TrySetAgentFileHidden(false);
                lockFile = File.Open(AgentFile, FileMode.Create, FileAccess.Write, FileShare.Read);
                TrySetAgentFileHidden(true);
            } catch (Exception ex) {
                Log.Error(ex,"Could not acquire RPC lock file '{0}'.", AgentFile);
                lockFile = null;
                return;
            }

            FEditorDelegates.BeginLocalPlay += OnBeginLocalPlay;

            SetCommandCallback(commandCallbackDelegate = HandleCommandCallback);

            try {
                listener = new TcpListener (new IPEndPoint (IPAddress.Loopback, 0));
                listener.Start ();

                var port = ((IPEndPoint)listener.Server.LocalEndPoint).Port;
                Log.Log("Listening on port {0}", port);
                using (var tw = new StreamWriter (lockFile,Encoding.UTF8))
                {
                    tw.WriteLine(port);
                }
            } catch (Exception ex) {
                Log.Error(ex, "Failed to start listening '{0}'.", AgentFile);
                return;
            }


            //this is intentionally not a background thread as a sanity check
            //it will end automatically when the sockets are shut down
            var serverThread = new Thread(ServerMain);
            serverThread.Start();
        }

        void TrySetAgentFileHidden(bool hidden)
        {
            if (!UnrealAgentHelper.IsWindows)
                return;
            try {
                var fi = new FileInfo(AgentFile);
                if (fi.Exists)
                    fi.Attributes = hidden? FileAttributes.Hidden : FileAttributes.Normal;
            }
            // Analysis disable once EmptyGeneralCatchClause
            catch {
            }
        }

        void ServerMain()
        {
            try
            {
                while (!IsDisposed)
                {
                    var client = listener.AcceptTcpClient();
                    Log.Log("Accepted connection.");

                    var c = OnConnecting (client);
                    c.Run ();
                }
            }
            catch (Exception ex)
            {
                var s = ex as SocketException;
                if (!IsDisposed && (s == null || s.SocketErrorCode != SocketError.Interrupted))
                    Log.Error(ex, "Unhandled exception.");
            }
            Log.Log("Server thread ended");
        }

        protected override Process StartTarget()
        {
            var sln = Path.Combine(GameRoot,Path.GetFileName(GameRoot) + "_Managed.sln");
            var psi = new ProcessStartInfo { UseShellExecute = false };

            if (UnrealAgentHelper.IsWindows)
            {
                psi.FileName = VisualStudioHelper.GetVisualStudioExecutableFullQualifiedPath();
                psi.Arguments = ProcessArgumentBuilder.Quote(sln);
            }
            else if (UnrealAgentHelper.IsMac)
            {
                string xs = "/Applications/Visual Studio.app";
                psi.FileName = "/usr/bin/open";
                psi.Arguments = string.Format (
                    "{0} -n -a {1}",
                    ProcessArgumentBuilder.Quote (sln),
                    ProcessArgumentBuilder.Quote (xs)
                );
            }
            else
            {
                throw new NotImplementedException();
            }

            return Process.Start (psi);
        }

        protected override bool HandleCommand(string name, string[] args)
        {
            switch (name)
            {
                case "BeginLocalPlay":
                    {
                        bool mobile = args.Length > 0 && bool.Parse(args[0]);
                        string additionalArgs = args.Length < 2? null : args[1];
                        DispatchToGameThread(() => RequestPlaySession(mobile, additionalArgs));
                        return true;
                    }
                case "HotReload":
                    {
                        DispatchToGameThread(() => {
                            var success = HotReload ();
                            Send("OnHotReload", success);
                        });
                        return true;
                    }
                default:
                    return false;
            }
        }

        protected override void Dispose(bool disposing)
        {
            base.Dispose(disposing);
            if (!disposing)
                return;

            if (listener != null)
            {
                try
                {
                    listener.Stop();
                    listener = null;
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Failed to shut down TCP listener.");
                }
            }

            if (lockFile != null)
            {
                lockFile.Close();
                lockFile = null;

                try
                {
                    TrySetAgentFileHidden(false);
                    File.Delete(AgentFile);
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Failed to clean up lock file.");
                }

                FEditorDelegates.BeginLocalPlay -= OnBeginLocalPlay;

                SetCommandCallback(null);
            }

        }

        void OnBeginLocalPlay (uint pid)
        {
            Send("OnBeginLocalPlay", pid);
        }

        [return:MarshalAs(UnmanagedType.I1)]
        delegate bool CommandCallback ([MarshalAs(UnmanagedType.I1)] bool launch, [MarshalAs(UnmanagedType.LPWStr)] string value);

        [DllImportAttribute("MonoEditor", EntryPoint="MonoIdeAgent_SetCommandCallback")]
        static extern void SetCommandCallback (CommandCallback callback);

        bool HandleCommandCallback(bool launch, string command)
        {
            try {
                if (launch)
                    ConnectAndSend(CancellationToken.None, command);
                else
                    Send(command);
            } catch (Exception ex) {
                Log.Error(ex, "Unhandled exception");
            }
            return IsConnected;
        }

        //hang onto the delegate so it doesn't get collected
        CommandCallback commandCallbackDelegate;

        [DllImportAttribute("MonoEditor")]
        static extern void MonoIdeAgent_DispatchToGameThread(Action<IntPtr> action, IntPtr data);

        static void GameThreadDispatchCallback(IntPtr h)
        {
            var handle = GCHandle.FromIntPtr(h);
            try {
                ((Action)handle.Target)();
            } catch (Exception ex) {
                UnrealEditorLog.Error(ex, "Unhandled error in game thread dispatch");
            }
            handle.Free();
        }

        static readonly Action<IntPtr> GameThreadDispatchCallbackInstance = GameThreadDispatchCallback;

        static void DispatchToGameThread (Action action)
        {
            //TODO: maybe a ConcurrentQueue would be more efficient than GCHandles?
            //the callback would simply dequeue, should preserve ordering
            var handle = GCHandle.Alloc(action);
            MonoIdeAgent_DispatchToGameThread(GameThreadDispatchCallbackInstance, GCHandle.ToIntPtr(handle));
        }

        [DllImportAttribute("MonoEditor", EntryPoint="MonoIdeAgent_UEditorEngine_RequestPlaySession")]
        [return:MarshalAs(UnmanagedType.I1)]
        static extern bool RequestPlaySession(
            [MarshalAs(UnmanagedType.I1)] bool mobile,
            [MarshalAs(UnmanagedType.LPWStr)] string args);

        [DllImportAttribute("MonoEditor", EntryPoint="MonoIdeAgent_UEditorEngine_RequestEndPlayMap")]
        static extern void RequestEndPlayMap();

        [DllImportAttribute("MonoEditor", EntryPoint="MonoIdeAgent_HotReload")]
        [return:MarshalAs(UnmanagedType.I1)]
        static extern bool HotReload();
    }
}
