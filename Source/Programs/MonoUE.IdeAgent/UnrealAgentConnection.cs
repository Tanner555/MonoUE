// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Diagnostics;

namespace MonoUE.IdeAgent
{
#if AGENT_CLIENT
    public
#endif
    class UnrealAgentConnection : IDisposable
    {
        const string protocolVersion = "1.0";

#if AGENT_CLIENT
        const string sendHandshake = "MONOUE AGENT " + protocolVersion + " CLIENT";
        const string recvHandshake = "MONOUE AGENT " + protocolVersion + " SERVER";
#else
		const string sendHandshake = "MONOUE AGENT " + protocolVersion + " SERVER";
		const string recvHandshake = "MONOUE AGENT " + protocolVersion + " CLIENT";
#endif

        bool disposed;

        TcpClient client;
        TextWriter writer;
        TextReader reader;
        int remotePid;

        readonly Func<string, string[], bool> commandHandler;
        readonly Action<UnrealAgentConnection> disposedHandler;

        public UnrealAgentConnection(
            IUnrealAgentLogger log,
            TcpClient client,
            Func<string, string[], bool> commandHandler,
            Action<UnrealAgentConnection> disposedHandler)
        {
            this.log = log;
            this.client = client;
            this.commandHandler = commandHandler;
            this.disposedHandler = disposedHandler;
        }

        IUnrealAgentLogger log;

        public int RemotePid { get { return remotePid; } }

        public void Run()
        {
            try
            {
                if (!Connect())
                    return;

                try
                {
                    commandHandler("Connected", null);
                }
                catch (Exception ex)
                {
                    if (!disposed)
                        log.Error(ex, "Error handling command Connected");
                }

                string line;
                while ((line = Read()) != null)
                {
                    ParseCommand(line, out string command, out string[] args);

                    //TODO: this should eventually be less verbose
                    log.Log("Command {0}", line);

                    if (command == "CLOSE")
                    {
                        log.Log("Disconnecting.");
                        return;
                    }

                    try
                    {
                        if (commandHandler(command, args))
                            continue;
                    }
                    catch (Exception ex)
                    {
                        if (!disposed)
                            log.Error(ex, "Error handling command " + command);
                    }

                    log.Error(null, "Unknown command " + command);
                }
            }
            catch (Exception ex)
            {
                if (!disposed)
                    log.Error(ex, "Unhandled error in agent thread");
            }
            finally
            {
                Dispose();
            }
        }

        bool Connect()
        {
            var stream = client.GetStream();

            //don't wait forever on writes if other end is unresponsive
            stream.WriteTimeout = 10000;

            writer = new StreamWriter(stream, Encoding.UTF8);
            reader = new StreamReader(stream, Encoding.UTF8);

            var pid = Process.GetCurrentProcess().Id;

            if (!Write(sendHandshake + " " + pid))
                return false;

            var line = Read();
            if (line == null)
                return false;

            if (!line.StartsWith(recvHandshake, StringComparison.Ordinal)
                || !int.TryParse(line.Substring(recvHandshake.Length).Trim(), out remotePid))
            {
                log.Error(null, "Bad handshake.");
                return false;
            }

            log.Log("Handshake succeeded, connected to {0}", remotePid);
            return true;
        }

        bool Write(string line)
        {
            var w = writer;
            if (w == null)
                return false;
            try
            {
                lock (w)
                {
                    w.WriteLine(line);
                    w.Flush();
                }
                return true;
            }
            catch (Exception ex)
            {
                if (!disposed)
                {
                    log.Error(ex, "Unhandled error in agent");
                    Dispose();
                }
            }
            return false;
        }

        string Read()
        {
            var r = reader;
            if (r == null)
                return null;
            try
            {
                return r.ReadLine();
            }
            catch
            {
                if (disposed)
                    return null;
                throw;
            }
        }

        static void ParseCommand(string line, out string command, out string[] args)
        {
            int idx = line.IndexOf(' ');
            if (idx < 0)
            {
                command = line;
                args = new string[0];
                return;
            }
            command = line.Substring(0, idx);

            args = ProcessArgumentBuilder.Parse(line.Substring(idx + 1));
        }

        public bool Send(string name, params object[] args)
        {
            if (args.Length == 0)
                return Write(name);

            var pb = new ProcessArgumentBuilder();
            pb.Add(name);
            foreach (object o in args)
            {
                pb.AddQuoted(o.ToString());
            }

            return Write(pb.ToString());
        }

        public void Close()
        {
            log.Log("Disconnecting");
            try
            {
                Write("CLOSE");
                // Analysis disable once EmptyGeneralCatchClause
            }
            catch
            {
            }

            Dispose();
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;

            var w = writer;
            if (w != null)
            {
                writer = null;
                w.Dispose();
            }

            var r = reader;
            if (r != null)
            {
                reader = null;
                r.Dispose();
            }

            var c = client;
            if (c != null)
            {
                client = null;
                try
                {
                    c.Close();
                }
                catch (Exception ex)
                {
                    log.Error(ex, "Unhandled error closing connection");
                }
            }

            disposedHandler(this);
        }
    }
}
