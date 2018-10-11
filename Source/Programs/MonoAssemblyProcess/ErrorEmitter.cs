// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using Mono.Cecil;
using Mono.Cecil.Cil;

namespace MonoAssemblyProcess
{
    [Serializable]
    class MonoAssemblyProcessError : Exception
    {
        public string File { get; private set; }
        public int Line { get; private set; }
        public int Column { get; private set; }

        public MonoAssemblyProcessError(string message)
            : base(message) 
        {
            Line = -1;
            Column = -1;
        }

        public MonoAssemblyProcessError(string message, string file, int line, int column = -1)
            : base(message) 
        {
            File = file;
            Line = line;
            Column = column;
        }

        public MonoAssemblyProcessError (string message, SequencePoint point)
            : base(message)
        {
           if (point != null)
           {
               File = point.Document.Url.ToString();
               Line = point.StartLine;
               Column = point.StartColumn;
           }
           else
           {
               Line = -1;
           }
        }

        public MonoAssemblyProcessError(string message, Exception innerException)
            : base(message,innerException) 
        {
            Line = -1;
        }

        public MonoAssemblyProcessError(string message, Exception innerException, SequencePoint point)
            : base(message, innerException)
        {
            if (point != null)
            {
                File = point.Document.Url.ToString();
                Line = point.StartLine;
               Column = point.StartColumn;
            }
            else
            {
                Line = -1;
            }
        }

        public virtual ErrorCode Code => ErrorCode.Generic;
    }

    static class ErrorEmitter
    {
        public static void Error (MonoAssemblyProcessError error)
        {
            Error(error.Code, error.Message, error.File, error.Line, error.Column);
        }

        public static void Error(ErrorCode code, string message, string file = null, int line = -1, int col = -1)
        {
            if (file != null && file.Length != 0)
            {
                Console.Error.Write(file);
                if (line != -1)
                {
                    if (col != -1)
                    {
                        Console.Error.Write($"({line},{col})");
                    }
                    else
                    {
                        Console.Error.Write($"({line})");
                    }
                }

                Console.Error.Write(" : ");
            }
            else
            {
                Console.Error.Write("MonoAssemblyProcess : ");
            }

            Console.Error.WriteLine($"error MRW{(int)code:D3}: {message}");
        }

        public static void Error (ErrorCode code, string message, SequencePoint location)
        {
            string file = null;
            int line = -1, col = -1;
            if (location != null)
            {
                file = location.Document.Url;
                line = location.StartLine;
                col = location.StartColumn;
            }

            Error(code, message, file, line, col);
        }

        public static Dictionary<TKey,TValue> ToDictionaryErrorEmit<TType,TKey, TValue> (this IEnumerable<TType> values, Func<TType,TKey> keyfunc, Func<TType,TValue> valuefunc, out bool hadError)
        {
            Dictionary<TKey, TValue> result = new Dictionary<TKey, TValue>();
            hadError = false;

            foreach (var item in values)
            {
                try
                {
                    result.Add(keyfunc(item), valuefunc(item));
                }
                catch (MonoAssemblyProcessError ex)
                {
                    hadError = true;
                    Error(ex);
                }
            }

            return result;
        }

        public static IEnumerable<TResultType> SelectWhereErrorEmit<TType, TResultType>(this IEnumerable<TType> values, Func<TType, bool> wherefunc, Func<TType, TResultType> selectfunc, out bool hadError)
        {
            List<TResultType> result = new List<TResultType>();
            hadError = false;
            foreach(var item in values)
            {
                try
                {
                    if (wherefunc(item))
                    {
                        result.Add(selectfunc(item));
                    }
                }
                catch (MonoAssemblyProcessError ex)
                {
                    Error(ex);
                    hadError = true;
                }
            }

            return result;
        }

        private static SequencePoint ExtractFirstSequencePoint (MethodDefinition method)
        {
            return method?.DebugInformation?.SequencePoints.FirstOrDefault ();
        }

        public static SequencePoint GetSequencePointFromMemberDefinition(IMemberDefinition member)
        {
            if (member is PropertyDefinition)
            {
                PropertyDefinition prop = member as PropertyDefinition;
                SequencePoint point = ExtractFirstSequencePoint(prop.GetMethod);
                if (point != null)
                {
                    return point;
                }
                point = ExtractFirstSequencePoint(prop.SetMethod);
                if (point != null)
                {
                    return point;
                }
                return GetSequencePointFromMemberDefinition(member.DeclaringType);
            }
            else if (member is MethodDefinition)
            {
                MethodDefinition method = member as MethodDefinition;
                SequencePoint point = ExtractFirstSequencePoint(method);
                if (point != null)
                {
                    return point;
                }
                return GetSequencePointFromMemberDefinition(member.DeclaringType);
            }
            else if (member is TypeDefinition)
            {
                TypeDefinition type = member as TypeDefinition;
                foreach(MethodDefinition method in type.Methods)
                {
                    SequencePoint point = ExtractFirstSequencePoint(method);
                    if (point != null)
                    {
                        return point;
                    }
                }
            }
            return null;
        }
    }

    enum ErrorCode
    {
        Generic = 0,
        InvalidConstructor = 1,
        ConstructorNotFound = 2,
        UnvalidUnrealClass = 3,
        InvalidUnrealStruct = 4,
        InvalidUnrealEnum = 5,
        InvalidUnrealProperty = 6,
        InvalidEnumMember = 7,
        InvalidUnrealFunction = 8,
        NotDerivableClass = 9,
        PEVerificationFailed = 10,
        UnableToFixPropertyBackingReference = 11,
        UnsupportedPropertyInitializer = 12,

        Internal = 999,
        CollidingTypeName = 1000
    }
}
