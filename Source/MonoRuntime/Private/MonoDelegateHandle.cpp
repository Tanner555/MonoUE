// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoDelegateHandle.h"
#include "MonoRuntimeCommon.h"

FMonoDelegateHandle::FMonoDelegateHandle(FMonoBindings& InBindings, MonoObject* Delegate, UObject* OptionalTargetObject)
	: Bindings(InBindings)
	, TargetObject(OptionalTargetObject)
	, bTargetObjectBound(nullptr != OptionalTargetObject)
{
	check(Delegate);
	check(mono_class_is_delegate(mono_object_get_class(Delegate)));
	DelegateGCHandle = mono_gchandle_new(Delegate, false);
}

FMonoDelegateHandle::~FMonoDelegateHandle()
{
	mono_gchandle_free(DelegateGCHandle);
}
