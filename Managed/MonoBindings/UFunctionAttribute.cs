// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealEngine.Runtime
{
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Class | AttributeTargets.Property)]
    class FunctionFlagsMapAttribute : Attribute
    {
        public FunctionFlags Flags;

        public FunctionFlagsMapAttribute(FunctionFlags flags = FunctionFlags.None)
        {
            Flags = flags;
        }
    }

    [Flags]
    public enum UserFunctionFlags
    {
        [FunctionFlagsMap(FunctionFlags.None)]
        None = 0,
        [FunctionFlagsMap(FunctionFlags.Exec)]
        Exec = 0x1,
        [FunctionFlagsMap(FunctionFlags.Final)]
        SealedEvent = 0x2,
        [FunctionFlagsMap(FunctionFlags.BlueprintAuthorityOnly)]
        BlueprintAuthorityOnly = 0x4,
        [FunctionFlagsMap(FunctionFlags.BlueprintCosmetic)]
        BlueprintCosmetic = 0x8,
        [FunctionFlagsMap(FunctionFlags.Const)]
        Const = 0x10,
    }

    [AttributeUsage(AttributeTargets.Method)]
    [FunctionFlagsMap(FunctionFlags.Native)] // All UFunctions are native, save BlueprintImplementableEvents.  We'll strip the flag in that case.
    public sealed class UFunctionAttribute : Attribute
    {
        public UFunctionAttribute(UserFunctionFlags flags = UserFunctionFlags.None)
        {
            Flags = flags;
        }

        public UserFunctionFlags Flags { get; private set; }
    }

    // UFunction can be called from blueprint code and should be exposed to the user of blueprint editing tools.
    // Mutually exclusive with [BlueprintImplementable].
    [AttributeUsage(AttributeTargets.Method)]
    [FunctionFlagsMap(FunctionFlags.BlueprintCallable)]
    public sealed class BlueprintCallableAttribute : Attribute
    {
        // BlueprintPure UFunctions fulfill a contract of producing no side effects.
        [FunctionFlagsMap(FunctionFlags.BlueprintPure)]
        public bool Pure { get; set; }
    }

    // UFunction is designed to be overridden by a blueprint.
    // Mutually exclusive with [BlueprintCallable].
    [AttributeUsage(AttributeTargets.Method)]
    [FunctionFlagsMap(FunctionFlags.Event | FunctionFlags.BlueprintEvent)]
    public sealed class BlueprintImplementableAttribute : Attribute
    {
    }

    public enum Endpoint
    {
        [FunctionFlagsMap(FunctionFlags.NetServer)]
        Server,
        [FunctionFlagsMap(FunctionFlags.NetClient)]
        Client,
        [FunctionFlagsMap(FunctionFlags.NetMulticast)]
        Multicast,
    }

    [AttributeUsage(AttributeTargets.Method)]
    [FunctionFlagsMap(FunctionFlags.Event | FunctionFlags.Net)]
    public sealed class RPCAttribute : Attribute
    {
        public RPCAttribute(Endpoint endpoint)
        {
            Endpoint = endpoint;
        }

        public Endpoint Endpoint { get; private set; }

        [FunctionFlagsMap(FunctionFlags.NetReliable)]
        public bool Reliable { get; set; }

        [FunctionFlagsMap(FunctionFlags.NetValidate)]
        public bool WithValidation { get; set; }
    }

    public enum ServiceType
    {
        [FunctionFlagsMap(FunctionFlags.NetRequest)]
        Request,
        [FunctionFlagsMap(FunctionFlags.NetResponse)]
        Response
    }

    [AttributeUsage(AttributeTargets.Method)]
    [FunctionFlagsMap(FunctionFlags.Event | FunctionFlags.Net | FunctionFlags.NetReliable)]
    public sealed class ServiceAttribute : Attribute
    {
        public ServiceAttribute(ServiceType service)
        {
            ServiceType = service;
        }

        public ServiceType ServiceType { get; private set; }
    }
}
