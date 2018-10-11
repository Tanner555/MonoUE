// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "Tests/MonoTestSubObject.h"
#include "MonoRuntimeCommon.h"

UMonoTestSubObject::UMonoTestSubObject(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TestReadableInt32 = 42;
}