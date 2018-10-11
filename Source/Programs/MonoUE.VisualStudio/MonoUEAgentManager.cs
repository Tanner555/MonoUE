// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using Microsoft.VisualStudio.ProjectSystem;
using Microsoft.VisualStudio.ProjectSystem.Properties;
using System.ComponentModel.Composition;
using System.Threading;
using System.Threading.Tasks;

namespace MonoUE.VisualStudio
{
    [Export]
    class MonoUEAgentManager
    {
        SemaphoreSlim lockObj = new SemaphoreSlim(1, 1);
        int refcount;
        VSUnrealAgent agent;

        public MonoUEAgentManager()
        {
        }

        internal async Task AddProject(ConfiguredProject project, IProjectProperties properties)
        {
            var gameRoot = await properties.GetEvaluatedPropertyValueAsync("UE4GameLocation");
            var engineRoot = await properties.GetEvaluatedPropertyValueAsync("UERootDir");
            var configName = project.ProjectConfiguration.Name;

            //remove the platform component
            var pipeIdx = configName.IndexOf('|');
            if (pipeIdx > -1)
            {
                configName = configName.Substring(0, pipeIdx);
            }

            await lockObj.WaitAsync();
            try {
                //if we have an agent and it doesn't match the settings, recreate just in case
                //on the assumption that the lastest project is more valid
                if (agent == null || gameRoot != agent.GameRoot || engineRoot != agent.EngineRoot || configName != agent.Configuration)
                {
                    //TODO warn if refcount isn't zero, as this means we have projects with conflicting settings
                    agent = new VSUnrealAgent(new VSUnrealLog(), engineRoot, gameRoot, configName);
                }
                refcount++;
            } finally
            {
                lockObj.Release();
            }
        }

        internal async Task RemoveProject(ConfiguredProject project)
        {
            await lockObj.WaitAsync();
            try
            {
                if (--refcount == 0)
                {
                    agent.Dispose();
                    agent = null;
                }
            }
            finally
            {
                lockObj.Release();
            }
        }
    }
}
