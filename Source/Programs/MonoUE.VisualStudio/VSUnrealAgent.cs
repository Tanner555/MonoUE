// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using MonoUE.IdeAgent;
using System;

namespace MonoUE.VisualStudio
{
    class VSUnrealAgent : UnrealAgentClient
    {
        public VSUnrealAgent(IUnrealAgentLogger log, string engineRoot, string gameRoot, string config) : base(log, engineRoot, gameRoot, config)
        {
        }

        protected override void OpenClass(string className)
        {
            throw new NotImplementedException();
        }

        protected override void OpenFile(string file)
        {
            throw new NotImplementedException();
        }

        protected override void OpenFunction(string className, string functionName)
        {
            throw new NotImplementedException();
        }

        protected override void OpenProperty(string className, string propertyName)
        {
            throw new NotImplementedException();
        }
    }
}
