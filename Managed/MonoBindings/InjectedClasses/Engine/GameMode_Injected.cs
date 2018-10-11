// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using UnrealEngine.Runtime;

namespace UnrealEngine.Engine
{
    // Mirrors the MatchState namespace in GameMode.h
    public static partial class MatchState
    {
        public static readonly Name EnteringMap = new Name("EnteringMap");
        public static readonly Name WaitingToStart = new Name("WaitingToStart");
        public static readonly Name InProgress = new Name("InProgress");
        public static readonly Name WaitingPostMatch = new Name("WaitingPostMatch");
        public static readonly Name LeavingMap = new Name("LeavingMap");
        public static readonly Name Aborted = new Name("Aborted");
    }
}
