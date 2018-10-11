// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#include "MonoRuntimePrivate.h"
#include "MonoRuntimeCommon.h"
#include "IMonoRuntime.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDeviceRedirector.h"
#include "PInvokeSignatures.h"

DEFINE_LOG_CATEGORY(LogMono);

// PInvoke for LogStream class
MONO_PINVOKE_FUNCTION(void) LogTextWriter_Serialize(const UTF16CHAR* String, unsigned int readOffset)
{
#if !NO_LOGGING
	if (UE_LOG_ACTIVE(LogMono, Log))
	{
		GLog->Serialize(StringCast<TCHAR>(String).Get() + readOffset, ELogVerbosity::Log, LogMono.GetCategoryName());
	}
#endif
}

