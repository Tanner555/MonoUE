// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using Microsoft.VisualStudio.ProjectSystem;
using System.ComponentModel.Composition;
using System.Threading.Tasks;

namespace MonoUE.VisualStudio
{
    [Export(ExportContractNames.Scopes.UnconfiguredProject, typeof(IProjectDynamicLoadComponent))]
    [AppliesTo(Constants.Capabilities.MonoUE)]
    class MonoUEProjectAgent : IProjectDynamicLoadComponent
    {
        readonly MonoUEAgentManager agentManager;
        readonly UnconfiguredProject project;
        readonly IProjectThreadingService threadingService;

        [ImportingConstructor]
        public MonoUEProjectAgent(
            [Import] MonoUEAgentManager agentManager,
            [Import] IProjectThreadingService threadingService,
            [Import] UnconfiguredProject project
            )
        {
            this.project = project;
            this.agentManager = agentManager;
            this.threadingService = threadingService;
        }

        public Task LoadAsync()
        {
            var active = project.Services.ActiveConfiguredProjectProvider.ActiveConfiguredProject;
            var properties = active.Services.ProjectPropertiesProvider.GetCommonProperties();
            return agentManager.AddProject(active, properties);
        }

        public Task UnloadAsync()
        {
            var active = project.Services.ActiveConfiguredProjectProvider.ActiveConfiguredProject;
            return agentManager.RemoveProject(active);
        }
    }
}
