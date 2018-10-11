// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include <mono/metadata/reflection.h>
#include <mono/metadata/image.h>
#include <mono/metadata/appdomain.h>

struct FMonoTypeReferenceMetadata;

struct MONORUNTIME_API FCachedAssembly
{
	MonoReflectionAssembly* ReflectionAssembly;
private:
	MonoImage* Image;
public:

	FCachedAssembly()
		: ReflectionAssembly(nullptr)
		, Image(nullptr)
	{
	}

	FCachedAssembly(MonoReflectionAssembly* InReflectionAssembly, MonoImage* InImage);

	bool Open(MonoDomain* Domain, const FString& AssemblyPath);

	void Reset();

	MonoClass* GetClass(const FString& Namespace, const FString& ClassName) const;
	MonoClass* GetClass(const ANSICHAR* Namespace, const ANSICHAR* ClassName) const;
	MonoMethod* LookupMethod(const ANSICHAR* FullyQualifiedMethodName) const;
	MonoException* CreateExceptionByName(const ANSICHAR* Namespace, const ANSICHAR* ClassName, const FString& Message) const;

	MonoType* ResolveType(const FMonoTypeReferenceMetadata& InTypeReference) const;
};
