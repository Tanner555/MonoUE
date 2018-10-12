// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class MonoScriptGenerator : ModuleRules
	{
        public MonoScriptGenerator(ReadOnlyTargetRules Target) : base(Target)
		{
            // TODO: Add a check to make sure this isn't a full engine build
            {
                PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
                Definitions.Add("MONOUE_STANDALONE");
            }

            PublicIncludePaths.AddRange(
				new string[] {					
					"Programs/UnrealHeaderTool/Public",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "Projects",
                    "Json"
                }
			);
		}
	}
}