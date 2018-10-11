// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Reflection;

namespace UnrealEngine.Runtime
{
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Enum | AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Struct | AttributeTargets.Method, AllowMultiple=true)]
    public sealed class UMetaDataAttribute : Attribute
    {       
        public UMetaDataAttribute(string key, string value=null)
        {
        }
    }
}
