// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using UnrealEngine.Runtime;

namespace UnrealEngine.Core
{
    public partial struct Color
    {
        public Color(byte r, byte g, byte b, byte a=255)
        {
            R = r;
            G = g;
            B = b;
            A = a;
        }
    }
}
