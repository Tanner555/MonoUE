// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoSourceCodeNavigation.h"
#include "MonoEditorCommon.h"
#include "MonoIdeAgent.h"

bool FMonoSourceCodeNavigationHandler::CanNavigateToClass(const UClass* InClass)
{
	FString MonoClassName = IMonoRuntime::Get().GetMonoQualifiedClassName(InClass,true);
	return MonoClassName.Len() > 0;
}

bool FMonoSourceCodeNavigationHandler::NavigateToClass(const UClass* InClass)
{
	FString MonoClassName = IMonoRuntime::Get().GetMonoQualifiedClassName(InClass, true);
	if (MonoClassName.Len() == 0)
	{
		return false;
	}

	FString command = FString::Printf(TEXT("OpenClass %s"), *MonoClassName);
	MonoIdeAgent_SendCommand(true, *command);

	return true;
}

bool FMonoSourceCodeNavigationHandler::CanNavigateToFunction(const UFunction* InFunction)
{
	return CanNavigateToClass(InFunction->GetOwnerClass());
}

bool FMonoSourceCodeNavigationHandler::NavigateToFunction(const UFunction* InFunction)
{
	FString MonoClassName = IMonoRuntime::Get().GetMonoQualifiedClassName(InFunction->GetOwnerClass(), true);
	if (MonoClassName.Len() == 0)
	{
		return false;
	}

	FString command = FString::Printf(TEXT("OpenFunction %s %s"), *MonoClassName, *InFunction->GetName());
	MonoIdeAgent_SendCommand(true, *command);

	return true;
}

bool FMonoSourceCodeNavigationHandler::CanNavigateToProperty(const UProperty* InProperty)
{
	return CanNavigateToClass(InProperty->GetOwnerClass());
}

bool FMonoSourceCodeNavigationHandler::NavigateToProperty(const UProperty* InProperty)
{
	FString MonoClassName = IMonoRuntime::Get().GetMonoQualifiedClassName(InProperty->GetOwnerClass(), true);
	if (MonoClassName.Len() == 0)
	{
		return false;
	}

	FString command = FString::Printf(TEXT("OpenProperty %s %s"), *MonoClassName, *InProperty->GetName());
	MonoIdeAgent_SendCommand(true, *command);

	return true;
}
