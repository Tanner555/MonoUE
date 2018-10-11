// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "MonoHelpers.h"

class FMonoDelegateHandle : public TSharedFromThis<FMonoDelegateHandle>
{
public: 
	FMonoDelegateHandle(FMonoBindings& InBindings, MonoObject* Delegate, UObject* OptionalTargetObject);
	~FMonoDelegateHandle();

	template <class ReturnValue>
	ReturnValue Invoke();
	template <class ReturnValue, class Arg1Type>
	ReturnValue Invoke(Arg1Type argOne);
	template <class ReturnValue, class Arg1Type, class Arg2Type>
	ReturnValue Invoke(Arg1Type argOne, Arg2Type argTwo);

	FMonoDelegateHandle(const FMonoDelegateHandle&) = delete;
	FMonoDelegateHandle& operator=(const FMonoDelegateHandle&) = delete;

private:
	FMonoBindings&	 Bindings;
	TWeakObjectPtr<> TargetObject;
	uint32_t		 DelegateGCHandle;
	bool			 bTargetObjectBound;
};