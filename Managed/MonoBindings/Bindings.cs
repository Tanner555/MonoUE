// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace UnrealEngine.Runtime
{
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    struct LoadAssemblyReturnStruct
    {
        public Assembly LoadedAssembly;
        public ModuleHandle AssemblyImageHandle;
        public string ErrorString;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    struct FindUnrealClassesReturnStruct
    {
        public Type ManagedType;
        public Type ManagedWrapperType;
        public int UnrealClassIndex;
    }

    class Bindings
    {
        string RuntimeAssemblyPath;
        string GameAssemblyPath;

        Bindings(string runtimeAssemblyPath, string gameAssemblyPath)
        {
            RuntimeAssemblyPath = runtimeAssemblyPath;
            GameAssemblyPath = gameAssemblyPath;
            // set up assembly search path
            AppDomain CurrentDomain = AppDomain.CurrentDomain;
            CurrentDomain.UnhandledException += OnUnhandledException;
            CurrentDomain.AssemblyResolve += new ResolveEventHandler(MyResolveEventHandler);
        }

        static Bindings Initialize(string runtimeAssemblyPath, string gameAssemblyPath)
        {
            Console.SetOut(LogTextWriter.Create());
            Console.SetError(LogTextWriter.Create());
            return new Bindings(runtimeAssemblyPath, gameAssemblyPath);
        }

        static ModuleHandle AssemblyImageHandleFromAssembly(Assembly assembly)
        {
            var modules = assembly.GetModules();

            // just use the first one?
            return modules[0].ModuleHandle;
        }

        static LoadAssemblyReturnStruct LoadAssembly(string assemblyName)
        {
            LoadAssemblyReturnStruct ret = new LoadAssemblyReturnStruct();

            try
            {
                ret.LoadedAssembly = Assembly.Load(assemblyName);
                ret.AssemblyImageHandle = AssemblyImageHandleFromAssembly(ret.LoadedAssembly);
            }
            catch (Exception e)
            {
                ret.ErrorString = e.ToString();
            }

            return ret;
        }

        static FindUnrealClassesReturnStruct[] FindUnrealClassesInAssembly(Assembly assembly, string[] unrealClassNames, string moduleName)
        {
            var classNameLookup = new Dictionary<string, int>();

            for(int i = 0; i < unrealClassNames.Length; ++i)
            {
                classNameLookup[moduleName + "." + unrealClassNames[i]] = i;
            }

            var returnInfo = new List<FindUnrealClassesReturnStruct>();

            const string wrapperSuffix = "_WrapperOnly";

            foreach(var type in assembly.GetTypes())
            {
                if(type.IsClass // must be a class
                    // bindings classes always have a namespace
                    && type.Namespace != null
                    // it's of type unrealobject
                    && type.IsSubclassOf(typeof(UnrealObject))
                    // and not a wrapper
                    && !type.Name.EndsWith(wrapperSuffix)
                    )
                {                   
                    string lastPartOfNamespace;
                    int index = type.Namespace.LastIndexOf('.');
                    if(index == -1)
                    {
                        lastPartOfNamespace = type.Namespace;
                    }
                    else
                    {
                        lastPartOfNamespace = type.Namespace.Substring(index + 1);
                    }

                    string lookupName = lastPartOfNamespace + "." + type.Name;

                    int classNameIndex;
                    if(classNameLookup.TryGetValue(lookupName, out classNameIndex))
                    {
                        Type wrapperType = null;
                        if(type.IsAbstract)
                        {
                            // if the type is abstract, there should be a wrapper type
                            wrapperType = assembly.GetType(type.Namespace + "." + type.Name + wrapperSuffix, false);
                        }
                        returnInfo.Add(new FindUnrealClassesReturnStruct { UnrealClassIndex = classNameIndex, ManagedType = type, ManagedWrapperType = wrapperType });
                    }
                }
            }

            return returnInfo.ToArray();
        }

        private Assembly MyResolveEventHandler(object sender, ResolveEventArgs args)
        {
            string assemblyDllName = new AssemblyName(args.Name).Name + ".dll";

             //This handler is called only when the common language runtime tries to bind to the assembly and fails.
            string assemblyPath = Path.Combine(RuntimeAssemblyPath, assemblyDllName);
            if(!File.Exists(assemblyPath))
            {
                assemblyPath = Path.Combine(GameAssemblyPath, assemblyDllName);
                if (!File.Exists(assemblyPath))
                {
                    Console.WriteLine("Couldn't resolve assembly " + assemblyDllName);
                    return null;
                }
            }

            Assembly assembly = Assembly.LoadFrom(assemblyPath);
            return assembly;
        }

        static void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            try
            {
                Exception ex = (Exception)e.ExceptionObject;

                OnUnhandledExceptionNative(ex.Message, ex.StackTrace);
            }
            catch (Exception)
            {
                OnUnhandledExceptionNative("Unknown fatal error", "No stack trace");
            }
        }

        [DllImport("__MonoRuntime", EntryPoint = "Bindings_OnUnhandledExceptionNative")]
        extern private static void OnUnhandledExceptionNative([MarshalAs(UnmanagedType.LPWStr)]string message, [MarshalAs(UnmanagedType.LPWStr)]string stackTrace);

    }
}