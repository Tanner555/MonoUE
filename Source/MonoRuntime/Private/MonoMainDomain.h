// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "MonoDomain.h"
#include "MonoCachedAssembly.h"

class FMonoMainDomain : public FMonoDomain
{
public:
	virtual ~FMonoMainDomain();

	static FMonoMainDomain* CreateMonoJIT(const FString& MonoRuntimeDirectory, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory);
	
	MonoDomain* CreateGameDomain();

	const FCachedAssembly& GetMainAssembly() const { return MainDomainAssembly; }

	static FString GetConfigurationSpecificSubdirectory(const FString &ParentDirectory);

#if MONOUE_STANDALONE
	bool Loaded;
#endif

private:
	FMonoMainDomain(MonoDomain* InDomain, const FString& InEngineAssemblyDirectory, const FString& InGameAssemblyDirectory);

	FCachedAssembly MainDomainAssembly;
	FString			EngineAssemblyDirectory;
	FString			GameAssemblyDirectory;
	MonoClass*		AppDomainClass;
	MonoClassField* AppDomainMonoAppDomainField;
};