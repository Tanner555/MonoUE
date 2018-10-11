// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "IScriptGeneratorPluginInterface.h"
#include "Modules/ModuleManager.h"

class IMonoScriptGenerator : public IScriptGeneratorPluginInterface
{
public:
	static inline IMonoScriptGenerator& Get()
	{
		return FModuleManager::LoadModuleChecked< IMonoScriptGenerator >( "MonoScriptGenerator" );
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "MonoScriptGenerator" );
	}
};

