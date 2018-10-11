// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

namespace UnrealEngine.Runtime
{
    [Serializable]
    public class FinderNotInitializedException : Exception
    {
        public FinderNotInitializedException(string message)
            : base(message)
        {
        }
    }


    [Serializable]
    public class ObjectNotFoundException : Exception
    {
        public ObjectNotFoundException(string message)
            : base(message)
        {

        }
    }
    

    abstract public partial class UnrealObject
    {
        protected enum FinderBehavior        
        {
            // Require that object exists
            Required,
            // Optional that object exists
            Optional
        }

        // Mirrors of ConstructorHelpers
        // Usage:
        // create static field
        // static ObjectFinder<Texture2D> MyDefaultTexture;
        // 
        // In creation constructor, call initialize. Initialize can *only* 
        // These can only be used within the creation constructor of an UnrealObject
        // TODO: right now above is enforced with native assertion, should be enforced with exception in managed code
        [Obsolete("This class will be removed. Please use ObjectInitializer.[Try]FindObject")]
        protected class ObjectFinder<T> where T : UnrealObject
        {
            private T _Object = null;
            private string SearchString = null;
            private FinderBehavior Behavior;
            private bool SearchedOnce = false;

            public T Object 
            { 
                get
                {
                    if(null == _Object)
                    {
                        if (!SearchedOnce || Behavior == FinderBehavior.Required)
                        {
                            if(null == SearchString)
                            {
                                throw new FinderNotInitializedException("Object finder not initialized!");
                            }

                            _Object = (T)ObjectFinder_FindNativeObject(typeof(T), SearchString);

                            SearchedOnce = true;

                        }
                    }

                    if (null == _Object && Behavior == FinderBehavior.Required)
                    {
                        throw new ObjectNotFoundException("Could not find required object: " + SearchString);
                    }
                    
                    return _Object;
                }
            }


            public ObjectFinder(FinderBehavior behavior = FinderBehavior.Required)
            {
                Behavior = behavior;
            }

            public void Initialize(string searchString)
            {               
                SearchString = searchString;
            }

        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern internal static UnrealObject ObjectFinder_FindNativeObject(Type objectType, string searchString);

        [Obsolete("This class will be removed. Please use ObjectInitializer.[Try]FindClass")]
        protected class ClassFinder<T> where T : UnrealObject
        {
            private SubclassOf<T> _Class = new SubclassOf<T>();
            private string SearchString = null;
            private FinderBehavior Behavior;
            private bool SearchedOnce = false;

            public SubclassOf<T> Class
            {
                get
                {
                    if (!_Class.Valid)
                    {
                        if (!SearchedOnce || Behavior == FinderBehavior.Required)
                        {
                            if (null == SearchString)
                            {
                                throw new FinderNotInitializedException("Class finder not initialized!");
                            }

                            _Class = new SubclassOf<T>(ClassFinder_FindNativeClass(SearchString));

                            SearchedOnce = true;

                        }
                    }

                    if (!_Class.Valid && Behavior == FinderBehavior.Required)
                    {
                        throw new ObjectNotFoundException("Could not find required class: " + SearchString);
                    }

                    return _Class;
                }
            }


            public ClassFinder(FinderBehavior behavior = FinderBehavior.Required)
            {
                Behavior = behavior;
            }

            public void Initialize(string searchString)
            {
                SearchString = searchString;
            }

        }

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        extern internal static IntPtr ClassFinder_FindNativeClass(string searchString);

    }
}
