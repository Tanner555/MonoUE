// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once
#if 0
#include "SNewClassDialog.h"

/**
* A dialog to create a new C# class, specifying parent and name
*/
class SNewMonoClassDialog : public SNewClassDialog
{
protected:
	virtual TSharedRef<SVerticalBox> ConstructPropertiesContainer() override;

	virtual FText GetClassNameDescription() const override;

	virtual FText GetClassNameDetails() const override;

	virtual FString GetNameErrorLabelText() const override;

	virtual bool ValidateInput() override;

	virtual void AddClassToProject() override;

	virtual void SetupFeaturedClasses(TArray<GameProjectUtils::FNewClassInfo> &FeaturedClasses) const override;

	virtual FText GetChooseParentClassDescription() const override;
private:
	FString CalculatedClassFileName;

	FText OnGetClassFileNameText() const;
};
#endif