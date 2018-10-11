// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

static FString SanitizeScriptPackageName(const FString& InPackageName)
{
	// copy and paste of FPackageTools::SanitizePackageName
	FString SanitizedName;
	FString InvalidChars = INVALID_LONGPACKAGE_CHARACTERS;

	// See if the name contains invalid characters.
	FString Char;
	for (int32 CharIdx = 0; CharIdx < InPackageName.Len(); ++CharIdx)
	{
		Char = InPackageName.Mid(CharIdx, 1);

		if (InvalidChars.Contains(*Char))
		{
			SanitizedName += TEXT("_");
		}
		else
		{
			SanitizedName += Char;
		}
	}

	return SanitizedName;
}