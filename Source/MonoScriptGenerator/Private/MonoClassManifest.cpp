// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoClassManifest.h"
#include "MonoGeneratedFileManager.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

void FMonoClassManifest::Initialize(const FString& InOutputFilePath)
{
	OutputFilePath = InOutputFilePath;
}


void FMonoClassManifest::AddClass(UClass& InClass)
{
	AllUnrealClasses.Add(&InClass);
		
	for (TFieldIterator<UScriptStruct> StructIt(&InClass, EFieldIteratorFlags::ExcludeSuper); StructIt; ++StructIt)
	{
		UScriptStruct* Struct = *StructIt;
		AllUnrealStructs.Add(Struct);
	}

	for (TFieldIterator<UEnum> EnumIt(&InClass, EFieldIteratorFlags::ExcludeSuper); EnumIt; ++EnumIt)
	{
		UEnum* Enum = *EnumIt;
		AllUnrealEnums.Add(Enum);
	}
}

void FMonoClassManifest::FinishExport()
{
	check(OutputFilePath.Len());

	TArray<TSharedPtr<FJsonValue>> Classes;

	for (UClass* Class : AllUnrealClasses)
	{
		Classes.Add(MakeShareable(new FJsonValueString(Class->GetName())));
	}

	TArray<TSharedPtr<FJsonValue>> Structs;

	for (UScriptStruct* Struct : AllUnrealStructs)
	{
		Structs.Add(MakeShareable(new FJsonValueString(Struct->GetName())));
	}

	TArray<TSharedPtr<FJsonValue>> Enums;

	for (UEnum* Enum : AllUnrealEnums)
	{
		Enums.Add(MakeShareable(new FJsonValueString(Enum->GetName())));
	}

	TSharedPtr<FJsonObject> Manifest = MakeShareable(new FJsonObject);
	Manifest->SetArrayField(TEXT("Classes"), Classes);
	Manifest->SetArrayField(TEXT("Structs"), Structs);
	Manifest->SetArrayField(TEXT("Enums"), Enums);

	FString OutputString;
	auto JsonWriter = TJsonWriterFactory<>::Create(&OutputString);
	// I can't think of a valid case where this could fail and it's not a programming error
	verify(FJsonSerializer::Serialize(Manifest.ToSharedRef(), JsonWriter));

	FMonoGeneratedFileManager GeneratedFileManager;
	GeneratedFileManager.SaveFileIfChanged(OutputFilePath, OutputString);
	GeneratedFileManager.RenameTempFiles();
}
