// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using MonoUE.IdeAgent;
using System;

namespace MonoUE.VisualStudio
{
    class VSUnrealLog : IUnrealAgentLogger
    {
        public void Error(Exception ex, string messageFormat, params object[] args)
        {
            var m = string.Format(messageFormat, args);
            if (ex != null)
                m += "\n" + ex;
            Console.WriteLine(m);
        }

        public void Log(string messageFormat, params object[] args)
        {
            Console.WriteLine(messageFormat, args);
        }

        public void Warn(string messageFormat, params object[] args)
        {
            Console.WriteLine(messageFormat, args);
        }
    }
}
