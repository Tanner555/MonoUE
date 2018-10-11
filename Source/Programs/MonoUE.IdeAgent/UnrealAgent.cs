// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using System.Net.Sockets;
using System.Runtime.InteropServices;

namespace MonoUE.IdeAgent
{
#if AGENT_CLIENT
    public
#endif
    abstract class UnrealAgent : IDisposable
    {
        string gameRoot, engineRoot, agentFile;
        bool disposed;

        protected readonly object ConnectionCreationLock = new object();
        UnrealAgentConnection connection;

        protected UnrealAgent(IUnrealAgentLogger log, string engineRoot, string gameRoot)
        {
            this.engineRoot = engineRoot;
            this.gameRoot = gameRoot;
            this.agentFile = Path.Combine(gameRoot, ".monoue-ide");
            this.Log = log;
        }

        public string GameRoot
        {
            get { return gameRoot; }
        }

        public string EngineRoot
        {
            get { return engineRoot; }
        }

        public string AgentFile
        {
            get { return agentFile; }
        }

        protected bool IsDisposed
        {
            get { return disposed; }
        }

        public bool IsConnected
        {
            get { return connection != null; }
        }

        protected IUnrealAgentLogger Log { get; }

        public Task Connect(CancellationToken token)
        {
            if (IsConnected)
                return TaskFromResult((object)null);

            return new ConnectTask(this, StartTarget(), token).Task
                //HACK: small delay before sending commands if launching the editor, or it can crash
                .ContinueWith(t =>
                {
                    t.Wait();
                    if (t.IsCompleted)
                        Thread.Sleep(5000);
                });
        }

        protected abstract Process StartTarget();

        class ConnectTask
        {
            TaskCompletionSource<object> tcs = new TaskCompletionSource<object>();
            readonly UnrealAgent agent;
            readonly Process target;

            public ConnectTask(UnrealAgent agent, Process target, CancellationToken token)
            {
                this.agent = agent;
                this.target = target;

                if (token.CanBeCanceled)
                    token.Register(OnCancelled);

                agent.Connected += OnConnected;
                agent.Disconnected += OnDisconnected;
                target.Exited += OnProcessExited;
                target.EnableRaisingEvents = true;

                if (agent.IsConnected)
                    OnConnected();
                else if (target.HasExited)
                    OnDisconnected();
            }

            public Task Task
            {
                get { return tcs.Task; }
            }

            void OnProcessExited(object sender, EventArgs e)
            {
                if (target.ExitCode != 0 && tcs.TrySetException(new Exception("Target exited unexpectedly")))
                    Dispose();
            }

            void OnCancelled()
            {
                if (tcs.TrySetCanceled())
                    Dispose();
            }

            void OnConnected()
            {
                if (tcs.TrySetResult(null))
                    Dispose();
            }

            void OnDisconnected()
            {
                if (tcs.TrySetException(new Exception("Unable to connect to target")))
                    Dispose();
            }

            void Dispose()
            {
                agent.Connected -= OnConnected;
                agent.Disconnected -= OnDisconnected;
                target.Exited -= OnProcessExited;
                target.EnableRaisingEvents = false;
            }
        }


        /// <summary>
        /// Subclass should call this when a connection is starting.
        /// </summary>
        protected UnrealAgentConnection OnConnecting(TcpClient client)
        {
            lock (ConnectionCreationLock)
            {
                if (connection != null)
                    connection.Dispose();
                connection = new UnrealAgentConnection(Log, client, HandleCommandBase, HandleDisposed);
            }
            return connection;
        }

        /// <summary>
        /// Subclass should call this to close the connection.
        /// </summary>
        protected void CloseConnection()
        {
            lock (ConnectionCreationLock)
            {
                if (connection != null)
                    connection.Close();
                connection = null;
            }
        }

        void HandleDisposed(UnrealAgentConnection c)
        {
            lock (ConnectionCreationLock)
            {
                if (c == connection)
                    connection = null;
            }

            var evt = Disconnected;
            if (evt == null)
                return;
            try
            {
                evt();
            }
            catch (Exception ex)
            {
                Log.Error(ex, "Unhandled exception");
            }
        }

        bool HandleCommandBase(string name, string[] args)
        {
            if (name == "Connected")
            {
                Connected?.Invoke();
                return true;
            }

            return HandleCommand(name, args);
        }

        protected abstract bool HandleCommand(string name, string[] args);

        public virtual void Dispose()
        {
            if (disposed)
                return;

            lock (ConnectionCreationLock)
            {
                if (disposed)
                    return;
                disposed = true;
            }

            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing && connection != null)
            {
                connection.Close();
                connection = null;
            }
        }

        ~UnrealAgent()
        {
            Dispose(false);
        }

        public event Action Connected;
        public event Action Disconnected;

        bool SendSync(string name, params object[] args)
        {
            var c = connection;
            if (c != null)
                return c.Send(name, args);
            return false;
        }

        protected Task<bool> Send(string name, params object[] args)
        {
            if (connection == null)
                return TaskFromResult(false);

            return LogExceptions(Log, Task.Factory.StartNew(() => SendSync(name, args)));
        }

        protected Task<bool> ConnectAndSend(CancellationToken token, string name, params object[] args)
        {
            return ConnectAndSend(token, true, name, args);
        }

        /// <summary>
        /// Sends a command to the remote process, launching it if necessary, and giving it focus.
        /// </summary>
        /// <returns>Task that completes when the connection is established or fails, or the remote process exits.</returns>
        /// <param name="token">Cancellation token.</param>
        /// <param name="focus">Whether to focus the remote process.</param>
        /// <param name="name">Command. May be null.</param>
        /// <param name="args">Format arguments for the command.</param>
        protected Task<bool> ConnectAndSend(CancellationToken token, bool focus, string name, params object[] args)
        {
            if (IsConnected)
            {
                bool success = !focus || GiveFocusToRemoteProcess();
                if (name == null)
                    return TaskFromResult(success);

                //don't try to reconnect on send failures since the reconnect would likely break too
                //ignore the tiny race that could happen if the remote process quits during the send
                return Send(name, args);
            }

            return LogExceptions(Log, Connect(token).ContinueWith(t =>
            {
                t.Wait();
                if (name == null)
                    return true;
                return SendSync(name, args);
            }));
        }

        static Task<T> TaskFromResult<T>(T result)
        {
            var tcs = new TaskCompletionSource<T>();
            tcs.SetResult(result);
            return tcs.Task;
        }

        //ensures exceptions are observed
        static Task<T> LogExceptions<T>(IUnrealAgentLogger log, Task<T> task)
        {
            task.ContinueWith(t =>
            {
                log.Error(t.Exception.Flatten().InnerException, "Error in task");
            }, TaskContinuationOptions.OnlyOnFaulted | TaskContinuationOptions.ExecuteSynchronously);

            return task;
        }

        bool GiveFocusToRemoteProcess()
        {
            var c = connection;
            if (c == null)
                return false;

            return GiveFocusToProcess(c.RemotePid);
        }

        public static bool GiveFocusToProcess(int pid)
        {
            if (UnrealAgentHelper.IsWindows)
                return GiveFocusToRemoteWindowsProcess(pid);
            if (UnrealAgentHelper.IsMac)
                return GiveFocusToRemoteMacProcess(pid);
            throw new NotImplementedException();
        }

        static bool GiveFocusToRemoteWindowsProcess(int pid)
        {
            if (GetMainWindowHandle(pid, out IntPtr handle) && SetForegroundWindow(handle))
            {
                SetFocus(handle);
                return true;
            }
            return false;
        }

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        static extern IntPtr SetFocus(IntPtr hWnd);

        //Process.MainWindowHandle doesn't work on Mono
        static bool GetMainWindowHandle(int pid, out IntPtr handle)
        {
            var result = IntPtr.Zero;

            EnumWindows((hWnd, lParam) =>
            {
                if (GetWindowThreadProcessId(hWnd, out uint winPid) != 0 && pid == winPid && IsWindowVisible(hWnd))
                {
                    result = hWnd;
                    return false;
                }
                //return true means continue enumerating
                return true;
            }, IntPtr.Zero);

            handle = result;
            return result != IntPtr.Zero;
        }

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

        delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [DllImport("user32.dll", SetLastError = true)]
        static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

        static bool GiveFocusToRemoteMacProcess(int pid)
        {
            var args = string.Format(
                "-e \"tell application \\\"System Events\\\" to set frontmost of the first process whose unix id is {0} to true\"",
                pid
            );
            Process.Start(new ProcessStartInfo("/usr/bin/osascript", args) { UseShellExecute = false });
            return true;
        }
    }
}
