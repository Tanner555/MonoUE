// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SNewMonoClassDialog.h"
#include "MonoEditorCommon.h"

#if 0
#define LOCTEXT_NAMESPACE "MonoEditor"

TSharedRef<SVerticalBox> SNewMonoClassDialog::ConstructPropertiesContainer() 
{
	NewClassPath = FPaths::ConvertRelativePathToFull(SelectedModuleInfo->ModuleSourcePath / FString("..") / (SelectedModuleInfo->ModuleName + FString("Mono")) / "");

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0)
		[
			SNew(SGridPanel)
			.FillColumn(1, 1.0f)

			// Name label
			+ SGridPanel::Slot(0, 0)
			.VAlign(VAlign_Center)
			.Padding(0, 0, 12, 0)
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel")
				.Text(LOCTEXT("NameLabel", "Name"))
			]

			// Name edit box
			+ SGridPanel::Slot(1, 0)
				.Padding(0.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(EditableTextHeight)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							ConstructNameEditBox()
						]
					]
				]

			// Path label
			+ SGridPanel::Slot(0, 1)
				.VAlign(VAlign_Center)
				.Padding(0, 0, 12, 0)
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel")
					.Text(LOCTEXT("PathLabel", "Path").ToString())
				]

			// Path edit box
			+ SGridPanel::Slot(1, 1)
				.Padding(0.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					ConstructPathEditBox()
				]

			// Source output label
			+ SGridPanel::Slot(0, 2)
				.VAlign(VAlign_Center)
				.Padding(0, 0, 12, 0)
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "NewClassDialog.SelectedParentClassLabel")
					.Text(LOCTEXT("SourceFileLabel", "Source File").ToString())
				]

			// Source output text
			+ SGridPanel::Slot(1, 2)
				.Padding(0.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					.HeightOverride(EditableTextHeight)
					[
						SNew(STextBlock)
						.Text(this, &SNewMonoClassDialog::OnGetClassFileNameText)
					]
				]
		];
}

FText SNewMonoClassDialog::GetClassNameDescription() const
{
	return LOCTEXT("MonoClassNameDescription", "Enter a name for your new class. Class names may only contain alphanumeric characters, and may not contain a space.");
}

FText SNewMonoClassDialog::GetClassNameDetails() const
{
	return LOCTEXT("MonoClassNameDetails", "When you click the \"Create\" button below, a source (.cs) file will be made using this name.");
}

FString SNewMonoClassDialog::GetNameErrorLabelText() const
{
	if (!bLastInputValidityCheckSuccessful)
	{
		return LastInputValidityErrorText.ToString();
	}

	return TEXT("");
}

bool SNewMonoClassDialog::ValidateInput()
{
	// Validate the path first since this has the side effect of updating the UI
	CalculatedClassFileName = FPaths::ConvertRelativePathToFull(NewClassPath) / "";
	CalculatedClassFileName /= NewClassName + FString(".cs");

	return GameProjectUtils::IsValidClassNameForCreation(NewClassName, *SelectedModuleInfo, LastInputValidityErrorText);
}

void SNewMonoClassDialog::AddClassToProject()
{
	FString ClassFilePath;

	FText FailReason;

	FMonoEditorPlugin &MonoEditorPlugin = FModuleManager::LoadModuleChecked<FMonoEditorPlugin>("MonoEditor");

	if (MonoEditorUtils::AddCodeToProject(*SelectedModuleInfo, NewClassName, NewClassPath, ParentClassInfo, ClassFilePath, FailReason))
	{
		// Prevent periodic validity checks. This is to prevent a brief error message about the class already existing while you are exiting.
		bPreventPeriodicValidityChecksUntilNextChange = true;

		// Code successfully added, notify the user and ask about opening the IDE now
		const FText Message = FText::Format(LOCTEXT("MonoAddCodeSuccessWithSync", "Successfully added class {0}. Would you like to edit the code now?"), FText::FromString(NewClassName));
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes)
		{
			//Open the file in the editor
			//FSourceCodeNavigation::OpenSourceFile(NewClassPath); //This can't work, Unreal assumes there's only one IDE.
			//This will need changing if/when Visual Studio support for the mono projects are added.
			FString OpenCommand = FString::Printf(TEXT("OpenFile \"%s\""), *ClassFilePath);
			MonoIdeAgent_SendCommand(true, *OpenCommand);
		}


		// Successfully created the code and potentially opened the IDE. Close the dialog.
		CloseContainingWindow();
	}
	else
	{
		// @todo show fail reason in error label
		// Failed to add code
		const FText Message = FText::Format(LOCTEXT("MonoAddCodeFailed", "Failed to add class {0}. {1}"), FText::FromString(NewClassName), FailReason);
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}
}

FText SNewMonoClassDialog::OnGetClassFileNameText() const
{
	return FText::FromString(CalculatedClassFileName);
}

void SNewMonoClassDialog::SetupFeaturedClasses(TArray<GameProjectUtils::FNewClassInfo> &FeaturedClasses) const
{
	// @todo make this ini configurable
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(ACharacter::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(APawn::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AActor::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(APlayerCameraManager::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(APlayerController::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AGameMode::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AWorldSettings::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AHUD::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(APlayerState::StaticClass()));
	FeaturedClasses.Add(GameProjectUtils::FNewClassInfo(AGameState::StaticClass()));
}

FText SNewMonoClassDialog::GetChooseParentClassDescription() const
{
	return LOCTEXT("MonoChooseParentClassDescription", "You are about to add a C# source code file.");
}

#undef LOCTEXT_NAMESPACE
#endif