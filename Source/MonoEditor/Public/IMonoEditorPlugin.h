// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IMonoEditorPlugin : public IModuleInterface
{

public:
	static inline IMonoEditorPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked<IMonoEditorPlugin>("MonoEditor");
	}
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("MonoEditor");
	}
};

