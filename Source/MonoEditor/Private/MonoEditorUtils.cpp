// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoEditorUtils.h"
#include "MonoEditorCommon.h"
#include "Misc/Paths.h"
#include "IMonoRuntime.h"

#define LOCTEXT_NAMESPACE "MonoEditor"

bool MonoEditorUtils::StartedIdeIntegration = false;
FMonoSourceCodeNavigationHandler  MonoEditorUtils::NavigationHandler;

bool MonoEditorUtils::HasManagedSolutionFile()
{
	return !GetManagedSolutionFilepath().IsEmpty();
}

FString MonoEditorUtils::GetManagedSolutionFilepath()
{
	FString SolutionFilepath;

	//managed code only supported from game projects, not from the engine
	if (FPaths::IsProjectFilePathSet())
	{
		//logic from FDesktopPlatformBase::GetSolutionPath
		SolutionFilepath = FPaths::ProjectDir() / FPaths::GetBaseFilename(FPaths::GetProjectFilePath()) + TEXT("_Managed.sln");
		if (!FPaths::FileExists(SolutionFilepath))
		{
			SolutionFilepath = TEXT("");
		}
	}

	return SolutionFilepath;
}

void MonoEditorUtils::EnableIdeIntegration()
{
	if (!StartedIdeIntegration)
	{
		IMonoRuntime::Get().StartIdeAgent();
		FSourceCodeNavigation::AddNavigationHandler((ISourceCodeNavigationHandler*)&NavigationHandler);
		StartedIdeIntegration = true;
	}
}

void MonoEditorUtils::DisableIdeIntegration()
{
	if (StartedIdeIntegration)
	{
		IMonoRuntime::Get().StopIdeAgent();
		FSourceCodeNavigation::RemoveNavigationHandler((ISourceCodeNavigationHandler*)&NavigationHandler);
		StartedIdeIntegration = false;
	}
}
/*
bool MonoEditorUtils::GenerateEmptyProject(const GameProjectUtils::FModuleContextInfo &ModuleInfo, const FString &ProjectFolder, FText& OutFailReason)
{
	FString Template;
	if (!ReadTemplateFile("EmptyProject.csproj.template", Template, OutFailReason))
	{
		return false;
	}

	const FString ProjectName = ModuleInfo.ModuleName + FString("Mono");

	FGuid ProjectGuid = FGuid::NewGuid();
	FString FinalOutput = Template.Replace(TEXT("%PROJECT_NAME%"), *ProjectName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PROJECT_GUID%"), *ProjectGuid.ToString(EGuidFormats::DigitsWithHyphens), ESearchCase::CaseSensitive);

	//Write project file
	if (!GameProjectUtils::WriteOutputFile(ProjectFolder / ProjectName + FString(".csproj"), FinalOutput, OutFailReason))
	{
		return false;
	}

	if (!ReadTemplateFile("AssemblyInfo.cs.template", Template, OutFailReason))
	{
		return false;
	}

	FinalOutput = Template.Replace(TEXT("%PROJECT_NAME%"), *ProjectName, ESearchCase::CaseSensitive);

	//Write AssemblyInfo.cs
	if (!GameProjectUtils::WriteOutputFile(ProjectFolder / FString("Properties") / FString("AssemblyInfo.cs"), FinalOutput, OutFailReason))
	{
		return false;
	}

	return true;
}

bool MonoEditorUtils::AddCodeToProject(const GameProjectUtils::FModuleContextInfo &ModuleInfo, const FString& NewClassName, const FString& NewClassPath, const GameProjectUtils::FNewClassInfo ParentClassInfo, FString& OutClassFilePath, FText& OutFailReason)
{
	if (!ParentClassInfo.IsSet())
	{
		OutFailReason = LOCTEXT("NoParentClass", "You must specify a parent class");
		return false;
	}

	const FString CleanClassName = ParentClassInfo.GetCleanClassName(NewClassName);
	const FString FinalClassName = ParentClassInfo.GetFinalClassName(NewClassName);

	if (!GameProjectUtils::IsValidClassNameForCreation(FinalClassName, ModuleInfo, OutFailReason))
	{
		OutFailReason = FText::FromString("Not a valid name for creation.");
		return false;
	}

	const FString ProjectFolder = FPaths::ConvertRelativePathToFull(ModuleInfo.ModuleSourcePath / FString("..") / (ModuleInfo.ModuleName + FString("Mono")) / "");
	const FString ProjectFile = ProjectFolder / ModuleInfo.ModuleName + FString("Mono.csproj");

	if (!FApp::HasGameName())
	{
		OutFailReason = LOCTEXT("AddCodeToProject_NoGameName", "You can not add code because you have not loaded a project.");
		return false;
	}

	FString ModuleName;

	const bool bAllowNewSlowTask = true;
	FScopedSlowTask SlowTaskMessage(LOCTEXT("AddingCodeToProject", "Adding code to project..."), bAllowNewSlowTask);

	// If the project does not already contain code, add the primary game module
	TArray<FString> CreatedFiles;

	if (!HasManagedSolutionFile())
	{
		//Generate a project!
		if (!GenerateEmptyProject(ModuleInfo, ProjectFolder, OutFailReason))
			return false;

		EnableIdeIntegration();
	}

	// Class CS file
	const FString NewClassFilename = NewClassPath / NewClassName + FString(".cs");
	{
		if (GenerateClassFile(NewClassFilename, CleanClassName, ParentClassInfo, TArray<FString>(), TEXT(""), ModuleInfo, OutFailReason))
		{
			CreatedFiles.Add(NewClassFilename);
		}
		else
		{
			GameProjectUtils::DeleteCreatedFiles(NewClassPath, CreatedFiles);
			return false;
		}
	}

	//Generate Solution and Build now that we've added some files to the project. Is ModuleInfo.ModuleName correct here?
	if (!IMonoRuntime::Get().GenerateProjectsAndBuildGameAssemblies(OutFailReason, *GWarn))
	{
		//returning false here will destroy the files being created. If we do nothing then the user
		//has an opportunity to fix the issue.
	}

	//TODO
	// Mark the files for add in SCC
	//ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	//if (ISourceControlModule::Get().IsEnabled() && SourceControlProvider.IsAvailable())
	//{
	//	TArray<FString> FilesToCheckOut;
	//	for (auto FileIt = CreatedFiles.CreateConstIterator(); FileIt; ++FileIt)
	//	{
	//		FilesToCheckOut.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(**FileIt));
	//	}
	//
	//	SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToCheckOut);
	//}

	OutClassFilePath = NewClassFilename;


	return true;
}


bool MonoEditorUtils::ReadTemplateFile(const FString& TemplateFileName, FString& OutFileContents, FText& OutFailReason)
{
	const FString FullFileName = FPaths::EnginePluginsDir() / TEXT("MonoUE") / TEXT("Resources") / TEXT("Templates") / TemplateFileName;
	if (FFileHelper::LoadFileToString(OutFileContents, *FullFileName))
	{
		return true;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("FullFileName"), FText::FromString(FullFileName));
	OutFailReason = FText::Format(LOCTEXT("FailedToReadTemplateFile", "Failed to read template file \"{FullFileName}\""), Args);
	return false;
}

bool MonoEditorUtils::GenerateClassFile(const FString& NewClassFileName, const FString UnPrefixedClassName, const GameProjectUtils::FNewClassInfo ParentClassInfo, const TArray<FString>& PropertyOverrides, const FString& AdditionalMemberDefinitions, const GameProjectUtils::FModuleContextInfo& ModuleInfo, FText& OutFailReason)
{
	FString Template;
	if (!ReadTemplateFile(ParentClassInfo.GetHeaderTemplateFilename().Replace(TEXT(".h.template"), TEXT(".cs.template")), Template, OutFailReason))
	{
		return false;
	}

	FString MonoBaseClassName;
	FString MonoProjectNamespace = ModuleInfo.ModuleName + FString("Mono");
	FString BaseClassUsingStatement;

	if (ParentClassInfo.GetClassName() != "None")
	{
		MonoBaseClassName = IMonoRuntime::Get().GetMonoClassName(ParentClassInfo.BaseClass);
		if (MonoBaseClassName.Len() == 0)
		{
			//Base class doesn't exist in Mono
			OutFailReason = FText::FromString(FString("Unable to locate Mono base class: ") + ParentClassInfo.GetClassName());
			return false;
		}

		BaseClassUsingStatement = IMonoRuntime::Get().GetMonoClassNamespace(ParentClassInfo.BaseClass);
		if (BaseClassUsingStatement.Len() != 0)
		{
			//Base class has a namespace
			BaseClassUsingStatement = FString("using ") + BaseClassUsingStatement + FString(";");

			if (Template.Contains(BaseClassUsingStatement))
			{
				//Don't duplicate using statements.
				BaseClassUsingStatement = TEXT("");
			}
		}

		//Handle name conflicts with System.Object and Core.Object
		if (MonoBaseClassName == TEXT("Object"))
		{
			MonoBaseClassName = IMonoRuntime::Get().GetMonoClassNamespace(ParentClassInfo.BaseClass) + "." + MonoBaseClassName;
			BaseClassUsingStatement = TEXT("");
		}
	}



	// Not all of these will exist in every class template
	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *GameProjectUtils::MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UNPREFIXED_CLASS_NAME%"), *UnPrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UCLASS_SPECIFIER_LIST%"), *FString(""), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MONO_BASE_CLASS_NAME%"), *MonoBaseClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MONO_BASE_CLASS_USINGSTATEMENT%"), *BaseClassUsingStatement, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MONO_PROJECT_NAMESPACE%"), *MonoProjectNamespace, ESearchCase::CaseSensitive);
	//Todo
	//FinalOutput = FinalOutput.Replace(TEXT("%CLASS_PROPERTIES%"), *ClassProperties, ESearchCase::CaseSensitive);
	//FinalOutput = FinalOutput.Replace(TEXT("%CLASS_FUNCTION_DECLARATIONS%"), *ClassFunctionDeclarations, ESearchCase::CaseSensitive);

	//// Determine the cursor focus location if this file will by synced after creation
	//TArray<FString> Lines;
	//FinalOutput.ParseIntoArray(&Lines, TEXT("\n"), false);
	//for (int32 LineIdx = 0; LineIdx < Lines.Num(); ++LineIdx)
	//{
	//	const FString& Line = Lines[LineIdx];
	//	int32 CharLoc = Line.Find(TEXT("%CURSORFOCUSLOCATION%"));
	//	if (CharLoc != INDEX_NONE)
	//	{
	//		// Found the sync marker
	//		OutSyncLocation = FString::Printf(TEXT("%d:%d"), LineIdx + 1, CharLoc + 1);
	//		break;
	//	}
	//}
	//
	//// If we did not find the sync location, just sync to the top of the file
	//if (OutSyncLocation.IsEmpty())
	//{
	//	OutSyncLocation = TEXT("1:1");
	//}

	// Now remove the cursor focus marker
	FinalOutput = FinalOutput.Replace(TEXT("%CURSORFOCUSLOCATION%"), TEXT(""), ESearchCase::CaseSensitive);

	return GameProjectUtils::WriteOutputFile(NewClassFileName, FinalOutput, OutFailReason);
}
*/

#undef LOCTEXT_NAMESPACE