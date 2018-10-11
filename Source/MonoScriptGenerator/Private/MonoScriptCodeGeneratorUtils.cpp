// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MonoScriptCodeGeneratorUtils.h"
#include "MonoScriptGeneratorLog.h"

#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

// mirrored from EdGraphSchema_K2.cpp (we can't bring in Kismet into a program plugin)
const FName MD_IsBlueprintBase(TEXT("IsBlueprintBase"));
const FName MD_BlueprintFunctionLibrary(TEXT("BlueprintFunctionLibrary"));
const FName MD_AllowableBlueprintVariableType(TEXT("BlueprintType"));
const FName MD_NotAllowableBlueprintVariableType(TEXT("NotBlueprintType"));
const FName MD_BlueprintInternalUseOnly(TEXT("BlueprintInternalUseOnly"));
const FName MD_BlueprintSpawnableComponent(TEXT("BlueprintSpawnableComponent"));
const FName MD_FunctionCategory(TEXT("Category"));
const FName MD_DefaultToSelf(TEXT("DefaultToSelf"));
const FName MD_Latent(TEXT("Latent"));

static const FName NAME_ToolTip(TEXT("ToolTip"));

void FMonoTextBuilder::AppendDocCommentFromMetadata(const UField& InField)
{
	AppendDocCommentSummary(MonoScriptCodeGeneratorUtils::GetFieldToolTip(InField));
}

void FMonoTextBuilder::AppendDocCommentSummary(const FString& SummaryText)
{
	if (SummaryText.Len() > 0)
	{
		FString NewSummaryText = SummaryText.Replace(TEXT("&&"), TEXT("&amp;&amp;"));
		NewSummaryText = NewSummaryText.Replace(TEXT("& "), TEXT("&amp; "));
		NewSummaryText = NewSummaryText.Replace(TEXT("<"), TEXT("&lt;"));
		int32 DummyIndex;
		if (NewSummaryText.FindChar('\n', DummyIndex) || NewSummaryText.FindChar('\r', DummyIndex))
		{
			AppendLine(TEXT("/// <summary>"));

			NewSummaryText = NewSummaryText.Replace(TEXT("\r"), TEXT(""));

			TArray<FString> Lines;
			NewSummaryText.ParseIntoArray(Lines, TEXT("\n"), true);

			for (auto&& Line : Lines)
			{
				AppendLine(FString::Printf(TEXT("/// %s"), *Line));
			}

			AppendLine(TEXT("/// </summary>"));
		}
		else
		{
			AppendLine(FString::Printf(TEXT("/// <summary>%s</summary>"), *NewSummaryText));
		}

	}

}

// copy and paste from FTextLocalizationManager
// We can't use FTextLocalizationManager because the tooltip localization files are not loaded in
// the script generator. We could force them to load, but that requires a dubious UE4 mod, and I wasn't sure if it
// affected the output of the generated C++ files
//
// The localization file format is fairly stable (none of this serialization code in FTextLocalizationManager has changed)
// so for now we copy and paste, and load the tooltips localization ourselves.
namespace LocalizationHack
{
	const FGuid LocResMagic = FGuid(0x7574140E, 0xFC034A67, 0x9D90154A, 0x1B7F37C3);

	enum class ELocResVersion : uint8
	{
		/** Legacy format file - will be missing the magic number. */
		Legacy = 0,
		/** Compact format file - strings are stored in a LUT to avoid duplication. */
		Compact,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};

	struct FLocalizationEntryTracker
	{
		struct FEntry
		{
			FString LocResID;
			uint32 SourceStringHash;
			FString LocalizedString;
		};

		typedef TArray<FEntry> FEntryArray;
		typedef TMap<FString, FEntryArray, FDefaultSetAllocator, FLocKeyMapFuncs<FEntryArray>> FKeysTable;
		typedef TMap<FString, FKeysTable, FDefaultSetAllocator, FLocKeyMapFuncs<FKeysTable>> FNamespacesTable;

		FNamespacesTable Namespaces;

		void LoadFromDirectory(const FString& DirectoryPath);
		bool LoadFromFile(const FString& FilePath);
		bool LoadFromArchive(FArchive& Archive, const FString& Identifier);
	};

	void FLocalizationEntryTracker::LoadFromDirectory(const FString& DirectoryPath)
	{
		// Find resources in the specified folder.
		TArray<FString> ResourceFileNames;
		IFileManager::Get().FindFiles(ResourceFileNames, *(DirectoryPath / TEXT("*.locres")), true, false);

		for (const FString& ResourceFileName : ResourceFileNames)
		{
			LoadFromFile(FPaths::ConvertRelativePathToFull(DirectoryPath / ResourceFileName));
		}
	}

	bool FLocalizationEntryTracker::LoadFromFile(const FString& FilePath)
	{
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath));
		if (!Reader)
		{
			UE_LOG(LogMonoScriptGenerator, Warning, TEXT("LocRes '%s' could not be opened for reading!"), *FilePath);
			return false;
		}

		bool Success = LoadFromArchive(*Reader, FilePath);
		Success &= Reader->Close();
		return Success;
	}

	bool FLocalizationEntryTracker::LoadFromArchive(FArchive& Archive, const FString& LocalizationResourceIdentifier)
	{
		Archive.SetForceUnicode(true);

		// Read magic number
		FGuid MagicNumber;
		
		if (Archive.TotalSize() >= sizeof(FGuid))
		{
			Archive << MagicNumber;
		}

		ELocResVersion VersionNumber = ELocResVersion::Legacy;
		if (MagicNumber == LocResMagic)
		{
			Archive << VersionNumber;
		}
		else
		{
			// Legacy LocRes files lack the magic number, assume that's what we're dealing with, and seek back to the start of the file
			Archive.Seek(0);
			//UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' failed the magic number check! Assuming this is a legacy resource (please re-generate your localization resources!)"), *LocalizationResourceIdentifier);
			UE_LOG(LogMonoScriptGenerator, Log, TEXT("LocRes '%s' failed the magic number check! Assuming this is a legacy resource (please re-generate your localization resources!)"), *LocalizationResourceIdentifier);
		}

		// Read the localized string array
		TArray<FString> LocalizedStringArray;
		if (VersionNumber >= ELocResVersion::Compact)
		{
			int64 LocalizedStringArrayOffset = INDEX_NONE;
			Archive << LocalizedStringArrayOffset;

			if (LocalizedStringArrayOffset != INDEX_NONE)
			{
				const int64 CurrentFileOffset = Archive.Tell();
				Archive.Seek(LocalizedStringArrayOffset);
				Archive << LocalizedStringArray;
				Archive.Seek(CurrentFileOffset);
			}
		}

		// Read namespace count
		uint32 NamespaceCount;
		Archive << NamespaceCount;

		for (uint32 i = 0; i < NamespaceCount; ++i)
		{
			// Read namespace
			FString Namespace;
			Archive << Namespace;

			// Read key count
			uint32 KeyCount;
			Archive << KeyCount;

			FKeysTable& KeyTable = Namespaces.FindOrAdd(*Namespace);

			for (uint32 j = 0; j < KeyCount; ++j)
			{
				// Read key
				FString Key;
				Archive << Key;

				FEntryArray& EntryArray = KeyTable.FindOrAdd(*Key);

				FEntry NewEntry;
				NewEntry.LocResID = LocalizationResourceIdentifier;

				// Read string entry.
				Archive << NewEntry.SourceStringHash;

				if (VersionNumber >= ELocResVersion::Compact)
				{
					int32 LocalizedStringIndex = INDEX_NONE;
					Archive << LocalizedStringIndex;

					if (LocalizedStringArray.IsValidIndex(LocalizedStringIndex))
					{
						NewEntry.LocalizedString = LocalizedStringArray[LocalizedStringIndex];
					}
					else
					{
						UE_LOG(LogMonoScriptGenerator, Warning, TEXT("LocRes '%s' has an invalid localized string index for namespace '%s' and key '%s'. This entry will have no translation."), *LocalizationResourceIdentifier, *Namespace, *Key);
					}
				}
				else
				{
					Archive << NewEntry.LocalizedString;
				}

				EntryArray.Add(NewEntry);
			}
		}

		return true;
	}

	static FLocalizationEntryTracker ToolTipLocalization;
	static bool ToolTipLocalizationInitialized = false;

	bool FindToolTip(const FString& Namespace, const FString& Key, FString& OutText)
	{
		check(ToolTipLocalizationInitialized);
		FLocalizationEntryTracker::FKeysTable* Table = ToolTipLocalization.Namespaces.Find(Namespace);

		if (nullptr != Table)
		{
			FLocalizationEntryTracker::FEntryArray* Entries = Table->Find(Key);

			if (nullptr != Entries && Entries->Num() > 0)
			{
				OutText = (*Entries)[0].LocalizedString;
				return true;
			}
		}

		return false;
	}
}



void MonoScriptCodeGeneratorUtils::InitializeToolTipLocalization()
{
	if (!LocalizationHack::ToolTipLocalizationInitialized)
	{
		TArray<FString> ToolTipPaths;
		// FPaths::GetToolTipPaths doesn't work in this context unfortunately, because the config file is not the game's config file
		// TODO: perhaps we should load the engine/game config file and use its paths
		ToolTipPaths.Add(TEXT("../../../Engine/Content/Localization/ToolTips"));

		for (int32 PathIndex = 0; PathIndex < ToolTipPaths.Num(); ++PathIndex)
		{
			const FString& LocalizationPath = ToolTipPaths[PathIndex];
			// for code documentation, we always want english
			const FString CulturePath = LocalizationPath / TEXT("en");

			LocalizationHack::ToolTipLocalization.LoadFromDirectory(CulturePath);
		}

		LocalizationHack::ToolTipLocalizationInitialized = true;
	}
}

FString MonoScriptCodeGeneratorUtils::GetEnumValueMetaData(const UEnum& InEnum, const TCHAR* MetadataKey, int32 ValueIndex)
{
	FString EnumName = InEnum.GetNameStringByIndex(ValueIndex);
	FString EnumValueMetaDataKey(*FString::Printf(TEXT("%s.%s"), *EnumName, *EnumName));

	if (InEnum.HasMetaData(*EnumValueMetaDataKey, ValueIndex))
	{
		return InEnum.GetMetaData(*EnumValueMetaDataKey, ValueIndex);
	}
	return FString();
}

FString MonoScriptCodeGeneratorUtils::GetEnumValueToolTip(const UEnum& InEnum, int32 ValueIndex)
{
	// Mimic behavior of UEnum::GetToolTipText, which unfortunately is not available since script generator is not actually WITH_EDITOR
	FString LocalizedToolTip;
	const FString NativeToolTip = GetEnumValueMetaData(InEnum, *NAME_ToolTip.ToString(), ValueIndex);

	FString Namespace = TEXT("UObjectToolTips");
	FString Key = ValueIndex == INDEX_NONE
		? InEnum.GetFullGroupName(true) + TEXT(".") + InEnum.GetName()
		: InEnum.GetFullGroupName(true) + TEXT(".") + InEnum.GetName() + TEXT(".") + InEnum.GetNameStringByIndex(ValueIndex);

	if (!LocalizationHack::FindToolTip(Namespace, Key, LocalizedToolTip))
	{
		LocalizedToolTip = NativeToolTip;
	}

	return LocalizedToolTip;
}

FString MonoScriptCodeGeneratorUtils::GetFieldToolTip(const UField& InField)
{
	if (InField.HasMetaData(NAME_ToolTip))
	{
		// mimic behavior of UField::GetToolTipText, which we unfortunately can not use directly because script generator is not actually WITH_EDITOR
		FString LocalizedToolTip;
		const FString NativeToolTip = InField.GetMetaData(NAME_ToolTip);

		static const FString Namespace = TEXT("UObjectToolTips");
		const FString Key = InField.GetFullGroupName(true) + TEXT(".") + InField.GetName();

		if (!LocalizationHack::FindToolTip(Namespace, Key, LocalizedToolTip))
		{
			LocalizedToolTip = NativeToolTip;
		}

		return LocalizedToolTip;
	}
	return FString();
}

UProperty* MonoScriptCodeGeneratorUtils::GetFirstParam(UFunction* Function)
{
	for (TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		if (0 == (It->PropertyFlags & CPF_ReturnParm))
		{
			return *It;
		}
	}
	return nullptr;
}

bool MonoScriptCodeGeneratorUtils::GetBoolMetaDataHeirarchical(const UClass* TestClass, FName KeyName, BoolHierarchicalMetaDataMode Mode)
{
	// can't use GetBoolMetaDataHierarchical because its WITH_EDITOR and program plugins don't define that
	bool bResult = false;
	while (TestClass)
	{
		if (TestClass->HasMetaData(KeyName))
		{
			bResult = TestClass->GetBoolMetaData(KeyName);
			if (Mode == BoolHierarchicalMetaDataMode::SearchStopAtAnyValue || bResult)
			{
				break;
			}
		}

		TestClass = TestClass->GetSuperClass();
	}

	return bResult;
}

bool MonoScriptCodeGeneratorUtils::IsBlueprintFunctionLibrary(const UClass* InClass)
{
	UClass* SuperClass = InClass->GetSuperClass();

	while (nullptr != SuperClass)
	{
		if (SuperClass->GetName() == TEXT("BlueprintFunctionLibrary"))
		{
			return true;
		}

		SuperClass = SuperClass->GetSuperClass();
	}

	return false;
}

// helper to extract a project guid from a csproj file
bool MonoScriptCodeGeneratorUtils::ParseGuidFromProjectFile(FGuid& ResultGuid, const FString& ProjectPath)
{
	FString ProjectFileContents;
	if (!FFileHelper::LoadFileToString(ProjectFileContents, *ProjectPath))
	{
		return false;
	}

	const FString StartAnchor(TEXT("<ProjectGuid>")), EndAnchor (TEXT("</ProjectGuid>"));

	const int32 MatchStart = ProjectFileContents.Find(StartAnchor, ESearchCase::CaseSensitive) + StartAnchor.Len();
	if (MatchStart <  StartAnchor.Len())
	{
		return false;
	}

	const int32 MatchEnd = ProjectFileContents.Find(EndAnchor, ESearchCase::CaseSensitive, ESearchDir::FromStart, MatchStart);
	if (MatchEnd <= MatchStart)
	{
		return false;
	}

	const FString GuidString = ProjectFileContents.Mid(MatchStart, MatchEnd - MatchStart);

	return FGuid::ParseExact(GuidString, EGuidFormats::DigitsWithHyphensInBraces, ResultGuid);
}
