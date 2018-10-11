// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace UnrealEngine.Runtime
{

    // TextWriter which sends to UE4 log
    // Synchronized with TextWriter.Synchronize
    sealed class LogTextWriter : TextWriter
    {
        // string wrapper class so we can avoid copying but still use it in a generic which takes IList
        sealed class StringWrapper : IList<char>
        {
            public string String;

            public char this[int index]
            {
                get { return String[index]; }
                set
                {
                    throw new NotImplementedException();
                }
            }

            public int IndexOf(char item)
            {
                throw new NotImplementedException();
            }

            public void Insert(int index, char item)
            {
                throw new NotImplementedException();
            }

            public void RemoveAt(int index)
            {
                throw new NotImplementedException();
            }

            public void Add(char item)
            {
                throw new NotImplementedException();
            }

            public void Clear()
            {
                throw new NotImplementedException();
            }

            public bool Contains(char item)
            {
                throw new NotImplementedException();
            }

            public void CopyTo(char[] array, int arrayIndex)
            {
                throw new NotImplementedException();
            }

            public int Count
            {
                get { throw new NotImplementedException(); }
            }

            public bool IsReadOnly
            {
                get { throw new NotImplementedException(); }
            }

            public bool Remove(char item)
            {
                throw new NotImplementedException();
            }

            public IEnumerator<char> GetEnumerator()
            {
                throw new NotImplementedException();
            }

            System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
            {
                throw new NotImplementedException();
            }
        }

        const uint MaxLineLength = 1024;

        // ring buffer storage
        char[] Storage = new char[MaxLineLength];
        // temporary storage for the case where the string wraps, avoid allocating as a temporary
        char[] TempStorage = new char[MaxLineLength + 1];
        // temporary wrapper object for when writing a string
        StringWrapper Wrapper = new StringWrapper();
        // temporary storage for when writing character by character
        char[] TempSingleCharacterArray = new char[1];
        uint ReadIndex = 0;
        uint WriteIndex = 0;

        private LogTextWriter()
        {

        }

        public static TextWriter Create()
        {
            return TextWriter.Synchronized(new LogTextWriter());
        }

        public override Encoding Encoding
        {
            get { return Encoding.Default; }
        }

        public override void Flush()
        {
            InternalFlush(false);
        }

        public override void Write(char value)
        {
            TempSingleCharacterArray[0] = value;
            WriteInternal(TempSingleCharacterArray, 0, 1);
        }

		public override void Write (string value)
        {
            Wrapper.String = value;
            WriteInternal(Wrapper, 0, value.Length);
            Wrapper.String = null;
        }

        private void WriteInternal<T>(T buffer, int index, int count) where T : IList<char>
        {
            int endIndex = index + count;


            for (int i = index; i < endIndex; ++i)
            {
                // shouldn't get null terminators in here, but want to catch if I do so I know I need to handle them
                if (buffer[i] == '\0')
                {
                    throw new ArgumentException("LogTextWriter does not support null terminators in the middle of a written buffer");
                }
                if (buffer[i] == '\n' || buffer[i] == '\r')
                {
                    InternalFlush(false);
                }
                else
                {
                    Storage[WriteIndex] = buffer[i];
                    AdvanceWritePointer();
                }
            }
        }

		public override void Write (char[] buffer, int index, int count)
        {
            WriteInternal(buffer, index, count);
        }

	    void InternalFlush(bool bufferFull)
	    {
		    if (ReadIndex == WriteIndex && !bufferFull)
		    {
			    return;
		    }
		    if (ReadIndex < WriteIndex)
		    {
			    // NULL terminate
			    Storage[WriteIndex] = '\0';
                LogTextWriter_Serialize(Storage, ReadIndex);
		    }
		    else
		    {
			    // wrap case, copy to intermediate buffer as we have to serialize this in one atomic call
			    uint firstBlockCount = MaxLineLength - ReadIndex;
                Array.Copy(Storage, ReadIndex, TempStorage, 0, firstBlockCount);
                Array.Copy(Storage, 0, TempStorage, firstBlockCount, WriteIndex);
			    // null terminate
			    TempStorage[firstBlockCount + WriteIndex] = '\0';
                LogTextWriter_Serialize(TempStorage, 0);
		    }
		    ReadIndex = WriteIndex;
	    }

        void AdvanceWritePointer()
        {
            WriteIndex = (WriteIndex + 1) % MaxLineLength;

            if (WriteIndex == ReadIndex)
            {
                // we've run into the read index, buffer is full, flush
                InternalFlush(true);
            }

        }

        [DllImport("__MonoRuntime", CharSet=CharSet.Unicode)]
        private static extern void LogTextWriter_Serialize(char[] buffer, uint readOffset);
    }
}
