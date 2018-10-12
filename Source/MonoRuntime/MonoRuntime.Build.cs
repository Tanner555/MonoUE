// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool.Rules
{
	public class MonoRuntime : ModuleRules
	{
		public MonoRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
            // TODO: Add a check to make sure this isn't a full engine build
            {
                PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
                Definitions.Add("MONOUE_STANDALONE");
            }

            PrivateIncludePaths.AddRange(
				new string[] {
					"MonoRuntime/Private",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Projects",
					"Json",
					"CoreUObject",
					"Engine",
					"InputCore",
					"Slate",
					"Mono",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("DesktopPlatform");
			}
		}
	}
}