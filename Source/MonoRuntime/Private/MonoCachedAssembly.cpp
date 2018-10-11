// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoCachedAssembly.h"
#include "MonoRuntimeCommon.h"
#include "MonoRuntimePrivate.h"
#include "MonoHelpers.h"
#include "MonoAssemblyMetadata.h"

#include <mono/metadata/assembly.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/exception.h>

FCachedAssembly::FCachedAssembly(MonoReflectionAssembly* InReflectionAssembly, MonoImage* InImage)
	: ReflectionAssembly(InReflectionAssembly)
	, Image(InImage)
{
#if DO_CHECK
	// do some basic sanity checking that reflection assembly and image match
	check(ReflectionAssembly);
	check(Image);
	MonoAssembly* Assembly = mono_image_get_assembly(Image);
	check(Assembly);
	MonoDomain* Domain = mono_object_get_domain((MonoObject*)ReflectionAssembly);
	check(Domain);
	MonoReflectionAssembly* ImageReflectionAssembly = mono_assembly_get_object(Domain, Assembly);
	check(ImageReflectionAssembly == ReflectionAssembly);
#endif // DO_CHECK

}
bool FCachedAssembly::Open(MonoDomain* Domain, const FString& AssemblyName)
{
	check(Image == nullptr);
	check(ReflectionAssembly == nullptr);

	MonoDomain *PreviousDomain = mono_domain_get();
	if (PreviousDomain != Domain) {
		mono_domain_set(Domain, false);
	}
	else {
		PreviousDomain = NULL;
	}

	MonoImageOpenStatus status;
	MonoAssembly* Assembly = mono_assembly_load_with_partial_name(TCHAR_TO_ANSI(*AssemblyName), &status);

	if (PreviousDomain)
	{
		mono_domain_set(PreviousDomain, false);
	}
	
	if (nullptr == Assembly)
	{
		UE_LOG(LogMono, Error, TEXT("Cannot load assembly %s"), *AssemblyName);
		return false;
	}

	Image = mono_assembly_get_image(Assembly);
	ReflectionAssembly = mono_assembly_get_object(Domain, Assembly);

	return nullptr != Image;
}

void FCachedAssembly::Reset()
{
	Image = nullptr;
	ReflectionAssembly = nullptr;
}

MonoClass* FCachedAssembly::GetClass(const FString& Namespace, const FString& ClassName) const
{
	return GetClass(TCHAR_TO_ANSI(*Namespace), TCHAR_TO_ANSI(*ClassName));
}

MonoClass* FCachedAssembly::GetClass(const ANSICHAR* Namespace, const ANSICHAR* ClassName) const
{
	return mono_class_from_name(Image, Namespace, ClassName);
}

MonoMethod* FCachedAssembly::LookupMethod(const ANSICHAR* FullyQualifiedMethodName) const
{
	return Mono::LookupMethod(Image, FullyQualifiedMethodName);
}

MonoException* FCachedAssembly::CreateExceptionByName(const ANSICHAR* Namespace, const ANSICHAR* ClassName, const FString& Message) const
{
	return mono_exception_from_name_msg(Image, Namespace, ClassName, TCHAR_TO_ANSI(*Message));
}

MonoType* FCachedAssembly::ResolveType(const FMonoTypeReferenceMetadata& InTypeReference) const
{
	MonoClass* Class = GetClass(InTypeReference.Namespace, InTypeReference.Name);
	if (nullptr == Class)
	{
		return nullptr;
	}
	return mono_class_get_type(Class);
}
