// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Linq;
using System.Reflection;
using Mono.Cecil;
using Mono.Cecil.Cil;
using Mono.Cecil.Rocks;
using System.Runtime.InteropServices;

namespace MonoAssemblyProcess
{
    class RewriteHelper
    {
        public readonly AssemblyMetadata Metadata;
        public readonly AssemblyDefinition TargetAssembly;
        public readonly AssemblyDefinition BindingsAssembly;
        public readonly TypeDefinition     TargetType;
        public readonly MethodReference NativeObjectGetter;
        public readonly TypeReference IntPtrType;
        public readonly MethodReference IntPtrAdd;
        public readonly MethodReference IntPtrConversion;
        public readonly TypeReference UObjectType;
        public readonly TypeReference ScriptArrayType;
        public readonly FieldReference ScriptArrayDataField;
        public readonly FieldReference ScriptArrayNumField;
        public readonly FieldReference ScriptArrayMaxField;
        public readonly TypeReference StringType;
        public readonly MethodReference StringLengthGetter;
        public readonly MethodReference MarshalStringToCoTaskMemMethod;
        public readonly MethodReference MarshalFreeCoTaskMemMethod;
        public readonly MethodDefinition InitializerConstructor;
        public readonly MethodReference CheckDestroyedByUnrealGCMethod;
        public readonly MethodReference GetNativeFunctionFromInstanceAndNameMethod;
        public TypeSystem TypeSystem { get { return TargetAssembly.MainModule.TypeSystem; } }

        public RewriteHelper(AssemblyMetadata metadata,
            AssemblyDefinition assembly, 
            AssemblyDefinition bindingsAssembly,
            TypeDefinition targetType)
        {
            Metadata = metadata;
            TargetAssembly = assembly;
            BindingsAssembly = bindingsAssembly;
            TargetType = targetType;
            Type intPtrType = typeof(System.IntPtr);
            IntPtrType = assembly.MainModule.TypeSystem.IntPtr;
            IntPtrAdd = assembly.MainModule.ImportReference(intPtrType.GetMethod("Add"));
            var explicitConversionOp = (from method in intPtrType.GetMethods()
                                        where method.Name == "op_Explicit" && method.ReturnType != null && method.ReturnType == typeof(void*)
                                        select method).ToArray()[0];

            IntPtrConversion = assembly.MainModule.ImportReference(explicitConversionOp);

            NativeObjectGetter = assembly.MainModule.ImportReference(FindNativeObjectGetter(targetType));

            UObjectType = FindTypeInAssembly(BindingsAssembly, Program.BindingsNamespace, "UnrealObject");

            MethodReference checkDestroyedByUnrealGCMethodRef = (from method in UObjectType.Resolve().Methods where method.Name == "CheckDestroyedByUnrealGC" select method).FirstOrDefault();
            CheckDestroyedByUnrealGCMethod = assembly.MainModule.ImportReference(checkDestroyedByUnrealGCMethodRef);

            GetNativeFunctionFromInstanceAndNameMethod = assembly.MainModule.ImportReference(
                Program.FindMethod(UObjectType.Resolve(), "GetNativeFunctionFromInstanceAndName", "System.IntPtr", new string[] { "System.IntPtr", "System.String" }));

            ScriptArrayType = FindTypeInAssembly(BindingsAssembly, Program.BindingsNamespace, "ScriptArray");
            ScriptArrayDataField = FindFieldInType(ScriptArrayType.Resolve(), "Data");
            ScriptArrayNumField = FindFieldInType(ScriptArrayType.Resolve(), "ArrayNum");
            ScriptArrayMaxField = FindFieldInType(ScriptArrayType.Resolve(), "ArrayMax");

            StringType = assembly.MainModule.TypeSystem.String;
            StringLengthGetter = assembly.MainModule.ImportReference(typeof(string).GetProperty("Length").GetGetMethod());

            MarshalStringToCoTaskMemMethod = assembly.MainModule.ImportReference(typeof(Marshal).GetMethod("StringToCoTaskMemAuto"));
            MarshalFreeCoTaskMemMethod = assembly.MainModule.ImportReference(typeof(Marshal).GetMethod("FreeCoTaskMem"));

            InitializerConstructor = Program.GetObjectInitializerConstructor(targetType);
        }

        public TypeReference FindTypeInAssembly(AssemblyDefinition assembly, string typeNamespace, string typeName)
        {
            var types = (from type in assembly.MainModule.GetAllTypes()
                                                     where type.Namespace == typeNamespace && type.Name == typeName
                                                     select type).ToArray();
            if (types.Length == 0)
            {
                throw new TypeAccessException(string.Format("Type \"{0}.{1}\" not found in assembly {2}", typeNamespace, typeName, assembly.Name));
            }

            return TargetAssembly.MainModule.ImportReference(types[0]);
        }

        public TypeReference FindGenericTypeInAssembly(AssemblyDefinition assembly, string typeNamespace, string typeName, TypeReference[] typeParameters)
        {
            TypeReference typeRef = FindTypeInAssembly(BindingsAssembly, typeNamespace, typeName);
            return TargetAssembly.MainModule.ImportReference(typeRef.Resolve().MakeGenericInstanceType(typeParameters));
        }

        public FieldReference FindFieldInType(TypeDefinition typeDef, string fieldName)
        {
            var foundField = (from field in typeDef.Fields
                         where field.Name == fieldName
                         select field).ToArray()[0];
            return TargetAssembly.MainModule.ImportReference(foundField);
        }

        public MethodReference FindBindingsStaticMethod(string findNamespace, string findClass, string findMethod)
        {
            var types = from module in BindingsAssembly.Modules
                        from type in ModuleDefinitionRocks.GetAllTypes(module)
                        where (type.IsClass && type.Namespace == findNamespace && type.Name == findClass)
                        select type;

            var methods = (from type in types
                           from method in type.Methods
                           where (method.IsStatic && method.Name == findMethod)
                           select method).ToArray();

            return TargetAssembly.MainModule.ImportReference(methods[0]);
        }

        MethodDefinition FindNativeObjectGetter(TypeDefinition type)
        {
            TypeReference unrealObjectType = FindTypeInAssembly(BindingsAssembly, Program.BindingsNamespace, "UnrealObject");
            if (unrealObjectType == null)
            {
                throw new InternalRewriteException(type, String.Format("Did not find UnrealObject class."));
            }

            PropertyDefinition[] props = (from prop in unrealObjectType.Resolve().Properties
                                          where prop.Name == "NativeObject"
                                          select prop).ToArray();
            Program.VerifySingleResult(props, type, "NativeObject property");

            PropertyDefinition property = props[0];

            if (property.GetMethod == null)
            {
                throw new InternalRewriteException(type, "NativeObject property missing getter");
            }

            return property.GetMethod;
        }

    }
}
