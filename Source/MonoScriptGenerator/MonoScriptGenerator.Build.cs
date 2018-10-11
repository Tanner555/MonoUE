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