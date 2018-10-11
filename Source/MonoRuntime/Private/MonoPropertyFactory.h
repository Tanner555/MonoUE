// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "UObject/Object.h"
#include "MonoBindings.h"
#include "MonoRuntimePrivate.h"

class  FMonoPropertyFactory
{
public:
	static const FMonoPropertyFactory& Get()
	{
		if (!GInstance)
		{
			GInstance = new FMonoPropertyFactory();
		}

		return *GInstance;
	}

	UProperty* Create(UObject& InOuter, FMonoBindings& InBindings, const FMonoPropertyMetadata& InMetadata) const;

private:
	FMonoPropertyFactory();

	typedef UProperty* (*PropertyFactoryFunc)(UObject&, FMonoBindings&, const FMonoPropertyMetadata&);

	struct PropertyFactoryFunctor
	{
		PropertyFactoryFunc FactoryFunc;

		PropertyFactoryFunctor(PropertyFactoryFunc InFunc)
		: FactoryFunc(InFunc)
		{

		}

		UProperty* operator()(UObject& InObject, FMonoBindings& InBindings, const FMonoPropertyMetadata& InMetadata) const
		{
			check(FactoryFunc);
			return (*FactoryFunc)(InObject, InBindings, InMetadata);
		}
	};

	static FMonoPropertyFactory* GInstance;

	TMap<FName, PropertyFactoryFunctor> PropertyFactoryMap;
};

