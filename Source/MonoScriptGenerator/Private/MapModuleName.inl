// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Some modules are mapped to others in scripts
//this is shared between the generator and the runtime

namespace ScriptGenUtil
{

static const FName ModuleToScriptModuleMappings[][2] = {
	{ TEXT("CoreUObject"), TEXT("Core") },
	{ TEXT("SlateCore"), TEXT("Slate") },
	{ TEXT("UnrealEd"), TEXT("Editor") },
	{ TEXT("PythonScriptPlugin"), TEXT("Python") },
};

FName MapModuleNameToScriptModuleName(const FName InModuleName)
{

	FName MappedModuleName = InModuleName;
	for (const auto& ScriptModuleMapping : ModuleToScriptModuleMappings)
	{
		if (InModuleName == ScriptModuleMapping[0])
		{
			MappedModuleName = ScriptModuleMapping[1];
			break;
		}
	}

	return MappedModuleName;
}

FName MapScriptModuleNameToModuleName(const FName InScriptModuleName)
{

	FName MappedModuleName = InScriptModuleName;
	for (const auto& ScriptModuleMapping : ModuleToScriptModuleMappings)
	{
		if (InScriptModuleName == ScriptModuleMapping[1])
		{
			MappedModuleName = ScriptModuleMapping[0];
			break;
		}
	}

	return MappedModuleName;
}

}