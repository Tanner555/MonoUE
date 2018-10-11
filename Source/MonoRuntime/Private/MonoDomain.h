// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "CoreMinimal.h"

#include <mono/metadata/appdomain.h>

namespace Mono
{
	enum class MONORUNTIME_API InvokeExceptionBehavior : uint8
	{
		OutputToLog,
		OutputToMessageLog
	};
}

class FMonoDomain
{
public:
	FMonoDomain(MonoDomain* InDomain, Mono::InvokeExceptionBehavior InExceptionBehavior);
	virtual ~FMonoDomain();

	MonoDomain* GetDomain() const { return Domain; }

	Mono::InvokeExceptionBehavior GetExceptionBehavior() const { return ExceptionBehavior;  }
protected:
	explicit FMonoDomain(Mono::InvokeExceptionBehavior InExceptionBehavior);

	void SetDomain(MonoDomain* NewDomain);

private:
	MonoDomain* Domain;
	Mono::InvokeExceptionBehavior ExceptionBehavior;
};