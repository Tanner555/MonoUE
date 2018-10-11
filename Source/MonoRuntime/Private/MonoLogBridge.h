// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "HAL/ThreadSingleton.h"
#include "Containers/StaticArray.h"

#if !NO_LOGGING

// Bridge between Mono style logging (which may make an arbitrary number of log calls between newlines) 
// and UE4 style logging (which makes one call per log statement and automatically inserts a newline)
// This class is used to buffer up log statements and emit them when newlines are encountered or a max line length is reached.
// It is a thread-local singleton so each threads output gets buffered up independently - this is more in line with how
// UE4 works. Someone doing crazy stuff with Mono console and threads would get different behavior, but
// we'll put that down as cost of doing business
template <class SuperClass>
class TMonoLogBridge  : public TThreadSingleton<TMonoLogBridge<SuperClass>>
{
	friend class TThreadSingleton<TMonoLogBridge<SuperClass>>;

protected:
	TMonoLogBridge()
		: ReadIndex(0),
		WriteIndex(0)
	{
	}

public:
	void Write(const TCHAR* InputBuffer,uint32 Count)
	{
		check(InputBuffer);
		const TCHAR* SourceStart = InputBuffer;
		const TCHAR* SourceEnd = InputBuffer + Count;

		for (; SourceStart != SourceEnd; ++SourceStart)
		{
			check(*SourceStart); // shouldn't get null terminators in here, but want to catch if I do so I know I need to handle them
			if (*SourceStart == '\n' || *SourceStart == '\r')
			{
				Flush(false);
				// don't write linefeeds
			}
			else
			{
				Storage[WriteIndex] = *SourceStart;
				AdvanceWritePointer();
			}
		}

	}

	void UserFlush()
	{
		Flush(false);
	}

private:

	void Flush(bool bBufferFull)
	{
		if (ReadIndex == WriteIndex && !bBufferFull)
		{
			return;
		}
		if (ReadIndex < WriteIndex)
		{
			// NULL terminate
			Storage[WriteIndex] = '\0';
			GLog->Serialize(&Storage[ReadIndex], SuperClass::GetLogVerbosity(), SuperClass::GetLogCategoryName());
		}
		else
		{
			// wrap case, copy to intermediate buffer as we have to serialize this in one atomic call
			TCHAR TempBuffer[MAX_LINE_LENGTH+1];
			const uint32 FirstBlockCount = MAX_LINE_LENGTH - ReadIndex;
			FMemory::Memcpy(TempBuffer, &Storage[ReadIndex], FirstBlockCount*sizeof(TCHAR));
			FMemory::Memcpy(&TempBuffer[FirstBlockCount], &Storage[0], WriteIndex*sizeof(TCHAR));
			// null terminate
			TempBuffer[FirstBlockCount + WriteIndex] = '\0';
			GLog->Serialize(TempBuffer, SuperClass::GetLogVerbosity(), SuperClass::GetLogCategoryName());
		}
		ReadIndex = WriteIndex;
	}

	void AdvanceWritePointer()
	{
		WriteIndex = (WriteIndex + 1) % MAX_LINE_LENGTH;

		if (WriteIndex == ReadIndex)
		{
			// we've run into the read index, buffer is full, flush
			Flush(true);
		}
	}

	static const uint32 MAX_LINE_LENGTH = 1024;
	TStaticArray<TCHAR, MAX_LINE_LENGTH> Storage;
	uint32 ReadIndex;
	uint32 WriteIndex;

};

class FMonoLogBridge : public TMonoLogBridge<FMonoLogBridge>
{
public:
	static ELogVerbosity::Type GetLogVerbosity()
	{
		return ELogVerbosity::Log;
	}

	static FName GetLogCategoryName()
	{
		return LogMono.GetCategoryName();
	}
};

#endif // !NO_LOGGING