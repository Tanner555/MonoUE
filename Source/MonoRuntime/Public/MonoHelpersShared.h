#pragma once

namespace Mono
{
	// Allocate memory to be freed by managed code, or vice versa.
	MONORUNTIME_API void* CoTaskMemAlloc(int32 Bytes);
	MONORUNTIME_API void* CoTaskMemRealloc(void* Ptr, int32 Bytes);
	MONORUNTIME_API void CoTaskMemFree(void* Ptr);
}
