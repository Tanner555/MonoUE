// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

#if AGENT_CLIENT
namespace MonoUE.IdeAgent
#else
namespace UnrealEngine.MainDomain
#endif
{
#if AGENT_CLIENT
    public
#endif
    abstract class UnrealAgentClient : UnrealAgent
    {
        readonly FileSystemWatcher fsw;

        protected UnrealAgentClient(IUnrealAgentLogger log, string engineRoot, string gameRoot, string config) : base(log, engineRoot, gameRoot)
        {
            Configuration = config;

            fsw = new FileSystemWatcher(Path.GetDirectoryName(AgentFile), Path.GetFileName(AgentFile));
            //ignore create, the file will be empty until changed
            fsw.Changed += CheckAgent;
            fsw.Deleted += CheckAgent;
            fsw.EnableRaisingEvents = true;

            CheckAgent();
        }

        public string Configuration { get; }

        void CheckAgent(object sender, FileSystemEventArgs e)
        {
            Log.Log("Agent file change: {0}", e.ChangeType);
            CheckAgent();
        }

        void CheckAgent()
        {
            if (IsDisposed)
                return;
            lock (ConnectionCreationLock)
            {
                if (IsDisposed)
                    return;
                if (File.Exists(AgentFile))
                {
                    if (!IsConnected)
                        Connect();
                }
                else
                {
                    if (IsConnected)
                        CloseConnection();
                }
            }
        }

        //must be called from inside connectionAssignLock lock
        void Connect()
        {
            Log.Log("Found .xamarin-ide file");
            int port;
            try
            {
                port = int.Parse(File.ReadAllText(AgentFile));
            }
            catch (Exception ex)
            {
                Log.Error(ex, "Failed to read port from .xamarin-ide file");
                return;
            }

            var client = new TcpClient();
            var c = OnConnecting(client);

            Log.Log("Connecting on port {0}", port);
            client.BeginConnect(IPAddress.Loopback, port, ar =>
            {
                try
                {
                    client.EndConnect(ar);
                    var t = new Thread(c.Run)
                    {
                        IsBackground = true,
                        Name = "Unreal Agent Client"
                    };
                    t.Start();
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Failed to connect to port {0}", port);
                    c.Close();
                }
            }, null);
        }

        protected override Process StartTarget()
        {
            string unrealEd = UnrealPath.GetUnrealEdBinaryPath(EngineRoot, Configuration);
            string uproject = UnrealPath.GetUProject(GameRoot);

            var psi = new ProcessStartInfo { UseShellExecute = false };

            if (UnrealAgentHelper.IsMac)
            {
                psi.FileName = "/usr/bin/open";
                psi.Arguments = string.Format(
                    "{0} -a {1}",
                    ProcessArgumentBuilder.Quote(uproject),
                    ProcessArgumentBuilder.Quote(unrealEd)
                );
            }
            else
            {
                psi.FileName = unrealEd;
                psi.Arguments = ProcessArgumentBuilder.Quote(uproject);
            }

            return Process.Start(psi);
        }

        protected override bool HandleCommand(string name, string[] args)
        {
            switch (name)
            {
                case "OnBeginPie":
                    {
                        var e = OnBeginPie;
                        if (e != null)
                        {
                            bool simulating = args.Length == 1 && bool.Parse(args[0]);
                            e(simulating);
                        }
                        return true;
                    }
                case "OnEndPie":
                    {
                        var e = OnEndPie;
                        if (e != null)
                        {
                            bool simulating = args.Length == 1 && bool.Parse(args[0]);
                            e(simulating);
                        }
                        return true;
                    }
                case "OnHotReload":
                    {
                        var e = OnHotReload;
                        if (e != null)
                        {
                            bool success = args.Length == 1 && bool.Parse(args[0]);
                            e(success);
                        }
                        return true;
                    }
                case "OnBeginLocalPlay":
                    {
                        var e = OnBeginLocalPlay;
                        if (e != null)
                        {
                            var pid = int.Parse(args[0]);
                            e(pid);
                        }
                        return true;
                    }
                case "OpenClass":
                    {
                        OpenClass(args[0]);
                        return true;
                    }
                case "OpenFunction":
                    {
                        OpenFunction(args[0], args[1]);
                        return true;
                    }
                case "OpenProperty":
                    {
                        OpenProperty(args[0], args[1]);
                        return true;
                    }
                case "OpenFile":
                    {
                        OpenFile(args[0]);
                        return true;
                    }
                case "NoOp":
                    {
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

            fsw.Dispose();
        }

        public void HotReload()
        {
            Send("HotReload");
        }

        public Task BeginLocalPlay(CancellationToken token, bool mobile = false, string args = null)
        {
            return ConnectAndSend(token, false, "BeginLocalPlay", mobile, args);
        }

        public void EndPie()
        {
            Send("EndPie");
        }

        public event Action<bool> OnBeginPie;
        public event Action<bool> OnEndPie;
        public event Action<bool> OnHotReload;
        public event Action<int> OnBeginLocalPlay;

        protected abstract void OpenClass(string className);
        protected abstract void OpenFunction(string className, string functionName);
        protected abstract void OpenProperty(string className, string propertyName);
        protected abstract void OpenFile(string file);

        public Task FocusUnrealEditor(CancellationToken token)
        {
            return ConnectAndSend(token, null);
        }
    }
}
