// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "SourceCodeNavigation.h"

class FMonoSourceCodeNavigationHandler : ISourceCodeNavigationHandler
{
public:
	virtual ~FMonoSourceCodeNavigationHandler() {};

	virtual bool CanNavigateToClass(const UClass* InClass) override;

	virtual bool NavigateToClass(const UClass* InClass) override;

	virtual bool NavigateToFunction(const UFunction* InFunction) override;

	virtual bool CanNavigateToFunction(const UFunction* InFunction) override;

	virtual bool NavigateToProperty(const UProperty* InProperty) override;

	virtual bool CanNavigateToProperty(const UProperty* InProperty) override;
};
