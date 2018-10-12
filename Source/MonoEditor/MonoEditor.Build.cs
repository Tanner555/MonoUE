// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

namespace UnrealBuildTool.Rules
{
	public class MonoEditor : ModuleRules
	{
        public MonoEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			// TODO: Add a check to make sure this isn't a full engine build
            {
                PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
                PublicDefinitions.Add("MONOUE_STANDALONE");
            }
			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"LevelEditor",
					"MonoRuntime",
                    "GameProjectGeneration",
                    "MessageLog",
                    "EditorStyle",
                    "MainFrame",
                    "SlateCore",
                    "HotReload",
                    "Slate"
				}
			);
		}
	}
}