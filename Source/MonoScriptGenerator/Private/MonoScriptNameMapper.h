// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"
#include "ScriptGenUtil.h"

using namespace ScriptGenUtil;
struct FMonoBindingsModule;

class IMonoModuleFinder
{
public:
	virtual ~IMonoModuleFinder() {}

	virtual const FMonoBindingsModule& FindModule(const UObject& Object) const = 0;
	virtual const FMonoBindingsModule& FindModule(FName ModuleFName) const = 0;
};

class MonoScriptNameMapper : public ScriptNameMapper
{
public:
	MonoScriptNameMapper(IMonoModuleFinder* InMonoModuleFinder) : MonoModuleFinder(*InMonoModuleFinder)
	{
	}

	virtual FString ScriptifyName(const FString& InName, const EScriptNameKind InNameKind) const override;

	FString GetQualifiedName(const UClass& Class) const;
	FString GetQualifiedName(const UScriptStruct& Struct) const;
	FString GetQualifiedName(const UEnum& Enum) const;

private:
	IMonoModuleFinder& MonoModuleFinder;
};