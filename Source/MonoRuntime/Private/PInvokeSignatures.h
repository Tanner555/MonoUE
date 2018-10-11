// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"
#include "IMonoRuntime.h"
#include "MonoHelpers.h"
#include "UObject/Object.h"

class UAtmosphericFogComponent;
class AController;
class UCharacterMovementComponent;
class APawn;
class AActor;

// UnrealEngine.Runtime.ScriptArrayBase pinvokes, implemented in MonoScriptArrayBase.cpp
MONO_PINVOKE_FUNCTION(void) ScriptArrayBase_EmptyArray(UProperty* ArrayProperty, void* ScriptArray);
MONO_PINVOKE_FUNCTION(void) ScriptArrayBase_AddToArray(UProperty* ArrayProperty, void* ScriptArray);
MONO_PINVOKE_FUNCTION(void) ScriptArrayBase_InsertInArray(UProperty* ArrayProperty, void* ScriptArray, int index);
MONO_PINVOKE_FUNCTION(void) ScriptArrayBase_RemoveFromArray(UProperty* ArrayProperty, void* ScriptArray, int index);

// PInvoke for LogStream class, implemented in MonoLogTextWriter.cpp
MONO_PINVOKE_FUNCTION(void) LogTextWriter_Serialize(const UTF16CHAR* String, unsigned int readOffset);

// MonoUnrealInterop.cpp
MONO_PINVOKE_FUNCTION(void) Bindings_OnUnhandledExceptionNative(const UTF16CHAR* InMessage, const UTF16CHAR* InStackTrace);
MONO_PINVOKE_FUNCTION(UClass*) UnrealInterop_GetNativeClassFromName(const UTF16CHAR* InClassName);
MONO_PINVOKE_FUNCTION(UStruct*) UnrealInterop_GetNativeStructFromName(const UTF16CHAR* InStructName);
MONO_PINVOKE_FUNCTION(int32) UnrealInterop_GetNativeStructSize(UScriptStruct* ScriptStruct);
MONO_PINVOKE_FUNCTION(int) UnrealInterop_GetPropertyOffsetFromName(UStruct* InStruct, const UTF16CHAR* InPropertyName);
MONO_PINVOKE_FUNCTION(UProperty*) UnrealInterop_GetNativePropertyFromName(UStruct* Struct, const UTF16CHAR* PropertyName);
MONO_PINVOKE_FUNCTION(uint16) UnrealInterop_GetPropertyRepIndexFromName(UStruct* Struct, const UTF16CHAR* PropertyName);
MONO_PINVOKE_FUNCTION(int32) UnrealInterop_GetArrayElementSize(UStruct* Struct, const UTF16CHAR* PropertyName);
MONO_PINVOKE_FUNCTION(int32) UnrealInterop_GetPropertyArrayDimFromName(UStruct* InStruct, const UTF16CHAR* InPropertyName);
MONO_PINVOKE_FUNCTION(bool) UnrealInterop_GetBitfieldValueFromProperty(uint8* NativeBuffer, UProperty* Property, int32 Offset);
MONO_PINVOKE_FUNCTION(void) UnrealInterop_SetBitfieldValueForProperty(uint8* NativeObject, UProperty* Property, int32 Offset, bool Value);
MONO_PINVOKE_FUNCTION(void) UnrealInterop_SetStringValueForProperty(UObject* NativeObject, UProperty* Property, int32 Offset, const UTF16CHAR* Value);
MONO_PINVOKE_FUNCTION(void) UnrealInterop_SetStringValue(FString *NativeString, const UTF16CHAR* Value);
MONO_PINVOKE_FUNCTION(void) UnrealInterop_RPC_ResetLastFailedReason();
MONO_PINVOKE_FUNCTION(void) UnrealInterop_RPC_ValidateFailed(const UTF16CHAR* Reason);
MONO_PINVOKE_FUNCTION(const TCHAR*) UnrealInterop_RPC_GetLastFailedReason();
MONO_PINVOKE_FUNCTION(int32) UnrealInterop_RandHelper(int32 Max);

// UnrealObject pinvoke functions from MonoUnrealObject.cpp

// Name access
MONO_PINVOKE_FUNCTION(FMarshalledName) UnrealObject_GetFName(UObject* InObject);

// UFunction exposure
MONO_PINVOKE_FUNCTION(UFunction*) UnrealObject_GetNativeFunctionFromClassAndName(UClass* Class, const UTF16CHAR* FunctionName);
MONO_PINVOKE_FUNCTION(UFunction*) UnrealObject_GetNativeFunctionFromInstanceAndName(UObject* Obj, const UTF16CHAR* FunctionName);
MONO_PINVOKE_FUNCTION(int16) UnrealObject_GetNativeFunctionParamsSize(UFunction* NativeFunction);
MONO_PINVOKE_FUNCTION(void) UnrealObject_InvokeFunction(UObject* NativeObject, UFunction* NativeFunction, void* Arguments, int ArgumentsSize);
MONO_PINVOKE_FUNCTION(void) UnrealObject_InvokeStaticFunction(UClass* NativeClass, UFunction* NativeFunction, void* Arguments, int ArgumentsSize);
MONO_PINVOKE_FUNCTION(void) FName_FromString(FName* Name, UTF16CHAR* Value, EFindName FindType);
MONO_PINVOKE_FUNCTION(void) FName_FromStringAndNumber(FName* Name, UTF16CHAR* Value, int Number, EFindName FindType);

//P/Invoke calling convention doesn't handle FQuat's alignment requirement
extern "C" 
struct FQuatArg
{
	float X, Y, Z, W;
};

MONO_PINVOKE_FUNCTION(void) FRotator_FromQuat(FRotator* OutRotator, FQuatArg QuatArg);
MONO_PINVOKE_FUNCTION(void) FRotator_FromMatrix(FRotator* OutRotator, const FMatrix* RotationMatrixArg);
MONO_PINVOKE_FUNCTION(void) FQuat_FromRotator(FQuat* OutQuat, FRotator Rotator);
MONO_PINVOKE_FUNCTION(void) FMatrix_FromRotator(FMatrix* OutRotationMatrix, FRotator Rotator);
MONO_PINVOKE_FUNCTION(void) FVector_FromRotator(FVector* OutDirection, FRotator Rotator);
MONO_PINVOKE_FUNCTION(void) FVector_SafeNormal(FVector* OutVector, FVector InVector, float tolerance);
MONO_PINVOKE_FUNCTION(void) FVector_SafeNormal2D(FVector* OutVector, FVector InVector, float tolerance);
MONO_PINVOKE_FUNCTION(void) FVector_ToRotator(FRotator* OutRotator, FVector InVector);
MONO_PINVOKE_FUNCTION(void) Actor_GetComponentsBoundingBoxNative(AActor* InActor, FBox* OutBox, bool bNonColliding);
MONO_PINVOKE_FUNCTION(ETickingGroup) Actor_GetTickGroup(AActor* ThisActor);
MONO_PINVOKE_FUNCTION(void) Actor_SetTickGroup(AActor* ThisActor, ETickingGroup TickGroup);
MONO_PINVOKE_FUNCTION(bool) Actor_GetActorTickEnabled(AActor* ThisActor);
MONO_PINVOKE_FUNCTION(void) Actor_SetActorTickEnabled(AActor* ThisActor, bool bEnabled);
MONO_PINVOKE_FUNCTION(void) FQuat_ScaleVector(FVector* OutVector, FQuatArg InQuat, FVector InVector);
MONO_PINVOKE_FUNCTION(void) Actor_TearOff(AActor* ThisActor);
MONO_PINVOKE_FUNCTION(void) Controller_GetPlayerViewPoint(AController* Controller, FVector* OutLocation, FRotator* OutRotation);
MONO_PINVOKE_FUNCTION(ETickingGroup) ActorComponent_GetTickGroup(UActorComponent* ThisComponent);
MONO_PINVOKE_FUNCTION(void) ActorComponent_SetTickGroup(UActorComponent* ThisComponent, ETickingGroup TickGroup);
MONO_PINVOKE_FUNCTION(bool) ActorComponent_GetComponentTickEnabled(UActorComponent* ThisComponent);
MONO_PINVOKE_FUNCTION(void) ActorComponent_SetComponentTickEnabled(UActorComponent* ThisComponent, bool bEnabled);
MONO_PINVOKE_FUNCTION(void) CharacterMovementComponent_ForceReplicationUpdate(UCharacterMovementComponent* ThisComponent);
MONO_PINVOKE_FUNCTION(void) Pawn_GetViewRotation(APawn* NativePawn, FRotator* OutRotator);
MONO_PINVOKE_FUNCTION(void) Pawn_TurnOff(APawn* ThisPawn);
MONO_PINVOKE_FUNCTION(ECollisionChannel) CollisionChannel_FromTraceType(ETraceTypeQuery TraceType);
MONO_PINVOKE_FUNCTION(ECollisionChannel) CollisionChannel_FromObjectType(EObjectTypeQuery ObjectType);
MONO_PINVOKE_FUNCTION(ETraceTypeQuery) TraceType_FromCollisionChannel(ECollisionChannel CollisionChannel);
MONO_PINVOKE_FUNCTION(EObjectTypeQuery) ObjectType_FromCollisionChannel(ECollisionChannel CollisionChannel);
MONO_PINVOKE_FUNCTION(float) FRandomStream_GetFraction(FRandomStream* SelfParameter);
MONO_PINVOKE_FUNCTION(uint32) FRandomStream_GetUnsignedInt(FRandomStream* SelfParameter);
MONO_PINVOKE_FUNCTION(void) FRandomStream_GetUnitVector(FRandomStream* SelfParameter, FVector* OutVector);
MONO_PINVOKE_FUNCTION(int) FRandomStream_RandRange(FRandomStream* SelfParameter, int32 Min, int32 Max);
MONO_PINVOKE_FUNCTION(void) FRandomStream_VRandCone(FRandomStream* SelfParameter, FVector* OutVector, FVector Dir, float ConeHalfAngleRad);
MONO_PINVOKE_FUNCTION(void) FRandomStream_VRandCone2(FRandomStream* SelfParameter, FVector* OutVector, FVector Dir, float HorizontalConeHalfAngleRad, float VerticalConeHalfAngleRad);
MONO_PINVOKE_FUNCTION(void) SceneComponent_SetupAttachment(USceneComponent* Self, USceneComponent* Parent, FName Socket);