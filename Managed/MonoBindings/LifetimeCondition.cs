// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealEngine.Runtime
{
    // Secondary condition to check before considering the replication of a lifetime property.
    // Mirrors native enum ELifetimeCondition.
    public enum LifetimeCondition
    {
        None = 0,		        // This property has no condition, and will send anytime it changes
        InitialOnly = 1,		// This property will only attempt to send on the initial bunch
        OwnerOnly = 2,		    // This property will only send to the actor's owner
        SkipOwner = 3,		    // This property send to every connection EXCEPT the owner
        SimulatedOnly = 4,		// This property will only send to simulated actors
        AutonomousOnly = 5,		// This property will only send to autonomous actors
        SimulatedOrPhysics = 6,	// This property will send to simulated OR bRepPhysics actors
        InitialOrOwner = 7,		// This property will send on the initial packet, or to the actors owner
        Custom = 8,		        // This property has no particular condition, but wants the ability to toggle on/off via SetCustomIsActiveOverride
    };

}
