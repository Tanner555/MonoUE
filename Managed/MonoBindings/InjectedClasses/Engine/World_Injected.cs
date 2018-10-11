using System;
using System.Runtime.CompilerServices;
using UnrealEngine.Runtime;


namespace UnrealEngine.Engine
{
    partial class World
    {
        /// <summary>
        /// Spawn an actor into the world.
        /// </summary>
        /// <typeparam name="T">Return type of the spawned actor</typeparam>
        /// <param name="unrealClass">SubclassOf wrapper for the native UClass to spawn</param>
        /// <param name="name">A name to assign as the Name of the Actor being spawned. If no value is specified, the name of the spawned Actor will be automatically generated using the form [Class]_[Number].</param>
        /// <param name="template">An Actor to use as a template when spawning the new Actor. The spawned Actor will be initialized using the property values of the template Actor. If null, the class default object (CDO) will be used to initialize the spawned Actor.</param>
        /// <param name="owner">The Actor that spawned this Actor..</param>
        /// <param name="instigator">The APawn that is responsible for damage done by the spawned Actor..</param>
        /// <param name="overrideLevel">The sublevel to spawn the Actor in, i.e. the Outer of the Actor. If left null, the Outer of the Owner is used. If the Owner is also null, the persistent level is used.</param>
        /// <param name="spawnCollisionHandlingOverride">Method for resolving collisions at the spawn point. Undefined means no override, use the actor's setting.</param>
        /// <param name="noFail">Determines whether spawning will not fail if certain conditions are not met. If true, spawning will not fail because the class being spawned is `bStatic=true` or because the class of the template Actor is not the same as the class of the Actor being spawned.</param>
        /// <param name="allowDuringConstructionScript">Determines whether or not the actor may be spawned when running a construction script. If true spawning will fail if a construction script is being run.</param>
        /// <returns></returns>
        public T SpawnActor<T>(SubclassOf<T> unrealClass, Name name=default(Name), Actor template=null, Actor owner=null, Pawn instigator=null, Level overrideLevel=null, SpawnActorCollisionHandlingMethod spawnCollisionHandlingOverride = SpawnActorCollisionHandlingMethod.Undefined, bool noFail = false, bool allowDuringConstructionScript=false) where T : Actor
        {
            return (T)SpawnActorNative(NativeObject, unrealClass.NativeClass, name, template, owner, instigator, overrideLevel, spawnCollisionHandlingOverride, noFail, allowDuringConstructionScript);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static Actor SpawnActorNative(IntPtr nativeWorld, IntPtr nativeClass, Name name, Actor template, Actor owner, Pawn instigator, Level overrideLevel, SpawnActorCollisionHandlingMethod spawnCollisionHandlingOverride, bool bNoFail, bool bAllowDuringConstructionScript);
    }
}