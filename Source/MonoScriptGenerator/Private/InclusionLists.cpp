// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "InclusionLists.h"
#include "MonoScriptCodeGeneratorUtils.h"
#include "UObject/UnrealType.h"

void FInclusionLists::AddEnum(FName EnumName)
{
	Enumerations.Add(EnumName);
}

bool FInclusionLists::HasEnum(const UEnum* Enum) const
{
	return Enumerations.Contains(Enum->GetFName());
}

void FInclusionLists::AddClass(FName ClassName)
{
	Classes.Add(ClassName);
}

bool FInclusionLists::HasClass(const UClass* Class) const
{
	return Classes.Contains(Class->GetFName());
}

void FInclusionLists::AddStruct(FName StructName)
{
	Structs.Add(StructName);
}

bool FInclusionLists::HasStruct(const UStruct* Struct) const
{
	return Structs.Contains(Struct->GetFName());
}

void FInclusionLists::AddAllFunctions(FName StructName)
{
	AllFunctions.Add(StructName);
}

void FInclusionLists::AddFunction(FName StructName, FName FunctionName)
{
	Functions.FindOrAdd(StructName).Add(FunctionName);
}

void FInclusionLists::AddFunctionCategory(FName StructName, const FString& Category)
{
	FunctionCategories.FindOrAdd(StructName).Add(Category);
}

bool FInclusionLists::HasFunction(const UStruct* Struct, const UFunction* Function) const
{
	if (AllFunctions.Contains(Struct->GetFName()))
	{
		return true;
	}
	const TSet<FName>* List = Functions.Find(Struct->GetFName());
	if (List && List->Contains(Function->GetFName()))
	{
		return true;
	}
	const TSet<FString>* CategoryList = FunctionCategories.Find(Struct->GetFName());
	if (CategoryList && Function->HasMetaData(MD_FunctionCategory))
	{
		const FString& Category = Function->GetMetaData(MD_FunctionCategory);

		return CategoryList->Contains(Category);
	}
	return false;
}

void FInclusionLists::AddOverridableFunction(FName StructName, FName OverridableFunctionName)
{
	OverridableFunctions.FindOrAdd(StructName).Add(OverridableFunctionName);
}

bool FInclusionLists::HasOverridableFunction(const UStruct* Struct, const UFunction* OverridableFunction) const
{
	const TSet<FName>* List = OverridableFunctions.Find(Struct->GetFName());
	return List && List->Contains(OverridableFunction->GetFName());
}

void FInclusionLists::AddProperty(FName StructName, FName PropertyName)
{
	Properties.FindOrAdd(StructName).Add(PropertyName);
}

bool FInclusionLists::HasProperty(const UStruct* Struct, const UProperty* Property) const
{
	const TSet<FName>* List = Properties.Find(Struct->GetFName());
	return List && List->Contains(Property->GetFName());
}
