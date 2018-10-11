// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class TP_ThirdPersonMonoTarget : TargetRules
{
	public TP_ThirdPersonMonoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		ExtraModuleNames.Add("TP_ThirdPersonMono");
	}
}
