// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using Microsoft.VisualStudio.Shell;
using System.Runtime.InteropServices;

namespace MonoUE.VisualStudio
{
    [PackageRegistration(UseManagedResourcesOnly = true)]
    [InstalledProductRegistration("#100", "#101", "1.0")]
    [Guid(Constants.PackageGuid)]
    public sealed class MonoUEPackage : Package
    {
        protected override void Initialize()
        {
            base.Initialize();
        }
    }
}
