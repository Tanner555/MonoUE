// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace UnrealEngine.Runtime
{
    [UStruct(NativeBlittable=true),StructLayout(LayoutKind.Sequential)]
    public struct Name : IEquatable<Name>, IComparable<Name>
    {
#if !CONFIG_SHIPPING
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern private static void CheckSizeof(int size);

        static unsafe Name()
        {
            CheckSizeof(sizeof(Name));
        }
#endif

#if TARGET_EDITOR
        public readonly static Name None = new Name(0, 0, 0);
#else
        public readonly static Name None = new Name(0, 0);
#endif

        public int ComparisonIndex;
#if TARGET_EDITOR
      // Index into the names array, used to find the string part of the string/number pair.
		public int DisplayIndex;
#endif

        // Number part of the string/number pair.
        public int Number;

        // Corresponds to native EFindName enum.
        public enum EFindName
        {
            // Create a valid Name only if the string already exists in the name table.
            Find,

            // Use the existing string in the name table if present, otherwise create it.
            Add,
        }

        public Name(string name, EFindName findType = EFindName.Add)
        {
            FName_FromString(out this, name, findType);
        }

        public Name(string name, int number, EFindName findType = EFindName.Add)
        {
            FName_FromStringAndNumber(out this, name, number, findType);
        }

        // internal use only
#if TARGET_EDITOR
        Name(int comparisonIndex, int displayIndex, int number)
        {
            ComparisonIndex = comparisonIndex;
            DisplayIndex = displayIndex;
            Number = number;
        }
#else
        Name(int comparisonIndex, int number)
        {
            ComparisonIndex = comparisonIndex;
            Number = number;
        }
#endif

        // Returns the string part of the name, with no trailing number.
        public string PlainName 
        { 
            get { return FName_GetPlainName(this); } 
        }

        public override string ToString()
        {
            return FName_ToString(this);
        }
#region Equality and comparison

        public static bool operator ==(Name lhs, Name rhs)
        {
            return lhs.ComparisonIndex == rhs.ComparisonIndex && lhs.Number == rhs.Number;
        }

        public static bool operator !=(Name lhs, Name rhs)
        {
            return !(lhs == rhs);
        }

        public static implicit operator Name(string name)
        {
            if (string.IsNullOrEmpty (name)) {
                return Name.None;
            }
            return new Name(name);
        }

        public bool Equals(Name other)
        {
            return this == other;
        }

        public override bool Equals(object obj)
        {
            if (obj is Name)
            {
                return this == (Name)obj;
            }

            return false;
        }

        public override int GetHashCode()
        {
            return ComparisonIndex;
        }

        public int CompareTo(Name other)
        {
            int diff = ComparisonIndex - other.ComparisonIndex;
            if (diff != 0)
            {
                return diff;
            }

#if TARGET_EDITOR
            diff = DisplayIndex - other.DisplayIndex;
            if (diff != 0)
            {
                return diff;
            }
#endif

            return Number - other.Number;
        }

#endregion

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static string FName_ToString(Name name);

        [DllImport("__MonoRuntime", EntryPoint = "FName_FromString")]
        extern private static void FName_FromString(out Name name, [MarshalAs(UnmanagedType.LPWStr)] string value, Name.EFindName findType);

        [DllImport("__MonoRuntime", EntryPoint = "FName_FromStringAndNumber")]
        extern private static void FName_FromStringAndNumber(out Name name, [MarshalAs(UnmanagedType.LPWStr)] string value, int Number, Name.EFindName findType);

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern private static string FName_GetPlainName(Name name);
    }
}
