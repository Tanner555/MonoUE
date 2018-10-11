// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoDomain.h"
#include "MonoRuntimeCommon.h"

FMonoDomain::FMonoDomain(MonoDomain* InDomain, Mono::InvokeExceptionBehavior InExceptionBehavior)
	: Domain(InDomain)
	, ExceptionBehavior(InExceptionBehavior)
{

}

FMonoDomain::FMonoDomain(Mono::InvokeExceptionBehavior InExceptionBehavior)
	: Domain(nullptr)
	, ExceptionBehavior(InExceptionBehavior)
{
}

FMonoDomain::~FMonoDomain()
{
}

void FMonoDomain::SetDomain(MonoDomain* NewDomain)
{
	Domain = NewDomain;
}
