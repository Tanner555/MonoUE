// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;

namespace MonoUE.IdeAgent
{
#if AGENT_CLIENT
    public
#endif
    interface IUnrealAgentLogger
    {
        void Log(string messageFormat, params object[] args);
        void Warn(string messageFormat, params object[] args);
        void Error(Exception ex, string messageFormat, params object[] args);
    }
}
