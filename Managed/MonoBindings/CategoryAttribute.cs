// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealEngine.Runtime
{
    [AttributeUsage(AttributeTargets.Property | AttributeTargets.Field | AttributeTargets.Method)]
    public sealed class CategoryAttribute : Attribute
    {
        public CategoryAttribute(string categoryName)
        {
            Name = categoryName;
        }

        public string Name { get; set; }
    }
}
