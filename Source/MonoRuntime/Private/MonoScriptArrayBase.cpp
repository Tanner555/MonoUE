// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoRuntimeCommon.h"

#include "CoreMinimal.h"
#include "IMonoRuntime.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"

// UnrealEngine.Runtime.ScriptArrayBase pinvokes
MONO_PINVOKE_FUNCTION(void) ScriptArrayBase_EmptyArray(UProperty* ArrayProperty, void* ScriptArray)
{
	check(ArrayProperty);
	check(ScriptArray);
	FScriptArrayHelper Helper(CastChecked<UArrayProperty>(ArrayProperty), ScriptArray);
	Helper.EmptyValues();
}

MONO_PINVOKE_FUNCTION(void) ScriptArrayBase_AddToArray(UProperty* ArrayProperty, void* ScriptArray)
{
	check(ArrayProperty);
	check(ScriptArray);
	FScriptArrayHelper Helper(CastChecked<UArrayProperty>(ArrayProperty), ScriptArray);
	Helper.AddValue();
}

MONO_PINVOKE_FUNCTION(void) ScriptArrayBase_InsertInArray(UProperty* ArrayProperty, void* ScriptArray, int index)
{
	check(ArrayProperty);
	check(ScriptArray);
	FScriptArrayHelper Helper(CastChecked<UArrayProperty>(ArrayProperty), ScriptArray);
	Helper.InsertValues(index);
}

MONO_PINVOKE_FUNCTION(void) ScriptArrayBase_RemoveFromArray(UProperty* ArrayProperty, void* ScriptArray, int index)
{
	check(ArrayProperty);
	check(ScriptArray);
	FScriptArrayHelper Helper(CastChecked<UArrayProperty>(ArrayProperty), ScriptArray);
	Helper.RemoveValues(index);
}
