// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using UnrealEngine.Runtime;

namespace UnrealEngine.Core
{
    /// <summary>Generic axis enum (mirrored for native use in Axis.h).</summary>
    [UEnum(NativeEnumName = "EAxis", NativeClassOwner = "Object")]
    public enum Axis : byte
    {
        None = 0,
        X = 1,
        Y = 2,
        Z = 3,
    }
}
