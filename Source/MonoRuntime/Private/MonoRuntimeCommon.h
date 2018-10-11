// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.


#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4510) // disable "default constructor could not be generated" - triggered by C style struct defs
#pragma warning(disable:4610) // disable "struct can never be instantiated" - triggered by C style struct defs
#endif

#if PLATFORM_WINDOWS
#pragma pack(push,8)
#endif // PLATFORM_WINDOWS

// Bump this when we upgrade mono, to force a recompile
// MONO_REBUILD_BUMP 3

#if PLATFORM_WINDOWS
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif