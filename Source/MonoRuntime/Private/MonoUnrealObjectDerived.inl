// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

static UObject* FindNativeObjectInternal(UClass* UnrealClass, FString SearchString)
{
	// mirror ConstructorHelpers::FObjectFinderOptional
	UMonoUnrealClass::CheckIfObjectFindIsInConstructor(*SearchString);
	ConstructorHelpers::StripObjectClass(SearchString);

	FString PathName = SearchString;

	UObject* FoundObject = nullptr;
	if (UnrealClass == UPackage::StaticClass())
	{
		FoundObject = ConstructorHelpersInternal::FindOrLoadObject<UPackage>(PathName);
	}
	else
	{
		// slightly modified version of ConstructorHelpersInternal::FindOrLoadObject

		// If there is no dot, add a dot and repeat the object name.
		int32 PackageDelimPos = INDEX_NONE;
		PathName.FindChar(TCHAR('.'), PackageDelimPos);
		if (PackageDelimPos == INDEX_NONE)
		{
			int32 ObjectNameStart = INDEX_NONE;
			PathName.FindLastChar(TCHAR('/'), ObjectNameStart);
			if (ObjectNameStart != INDEX_NONE)
			{
				const FString ObjectName = PathName.Mid(ObjectNameStart + 1);
				PathName += TCHAR('.');
				PathName += ObjectName;
			}
		}

		// force CDO creation if its not already created
		UnrealClass->GetDefaultObject();
		FoundObject = StaticLoadObject(UnrealClass, NULL, *PathName);

		if (nullptr != FoundObject && !FoundObject->IsA(UnrealClass))
		{
			FoundObject = nullptr;
		}
	}

	if (nullptr == FoundObject)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Managed CDO Constructor: Failed to find %s\n"), *SearchString);
		UClass::GetDefaultPropertiesFeedbackContext().Logf(ELogVerbosity::Error, TEXT("Managed CDO Constructor: Failed to find %s"), *SearchString);
	}
	else
	{
#if UE_BUILD_DEBUG
		UObjectRedirector* Redir = FindObject<UObjectRedirector>(ANY_PACKAGE, *PathName);
		if (Redir && Redir->DestinationObject == FoundObject)
		{
			FString NewString = FoundObject->GetFullName();
			NewString.ReplaceInline(TEXT(" "), TEXT("'"));
			NewString += TEXT("'");
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Managed CDO Constructor: Followed redirector (%s), change code to new path (%s)\n"), *SearchString, *NewString);
			UClass::GetDefaultPropertiesFeedbackContext().Logf(ELogVerbosity::Warning, TEXT("Managed CDO Warning: Followed redirector (%s), change code to new path (%s)\n"), *SearchString, *NewString);
		}
#endif
	}

	return FoundObject;
}
