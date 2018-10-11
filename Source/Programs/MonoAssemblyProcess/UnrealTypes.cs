// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Linq;
using Mono.Cecil;
using Mono.Cecil.Cil;
using Mono.Cecil.Rocks;
using System.Collections.Generic;
using Mono.Collections.Generic;

namespace MonoAssemblyProcess
{
    abstract class UnrealType
    {
        internal TypeReference CSharpType;
        public readonly string UnrealPropertyClass;
        public readonly int ArrayDim;
        internal bool NeedsNativePropertyField;
        internal bool NeedsElementSizeField;

        // Generic instance type for fixed-size array wrapper.  Populated only when ArrayDim > 1.
        private TypeReference FixedSizeArrayWrapperType = null;
        // Instance backing field for fixed-size array wrapper.  Populated only when ArrayDim > 1.
        private FieldDefinition FixedSizeArrayWrapperField = null;

        private TypeReference ToNativeDelegateType = null;

        private TypeReference FromNativeDelegateType = null;

        public virtual bool IsBlittable { get { return false; } }
        public virtual bool IsPlainOldData { get { return false; } }

        public UnrealType(TypeReference typeRef, string unrealClass, int arrayDim)
        {
            CSharpType = typeRef;
            UnrealPropertyClass = unrealClass;
            ArrayDim = arrayDim;
            NeedsNativePropertyField = false;
            NeedsElementSizeField = false;
        }

        protected static ILProcessor InitPropertyAccessor(MethodDefinition method)
        {
            method.Body = new MethodBody(method);
            method.CustomAttributes.Clear();
            ILProcessor processor = method.Body.GetILProcessor();
            method.Body.Instructions.Clear();
            return processor;
        }

        protected static void WriteLoadNativeBufferPtr(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField)
        {
            if (loadBufferInstruction != null)
            {
                processor.Append(loadBufferInstruction);
            }
            else
            {
                processor.Emit(OpCodes.Ldarg_0);
                processor.Emit(OpCodes.Call, helper.NativeObjectGetter);
            }
            processor.Emit(OpCodes.Ldsfld, offsetField);
            processor.Emit(OpCodes.Call, helper.IntPtrAdd);
        }

        public static Instruction[] GetLoadNativeBufferInstructions(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField)
        {
            List<Instruction> instructionBuffer = new List<Instruction>();
            if (loadBufferInstruction != null)
            {
                instructionBuffer.Add(loadBufferInstruction);
            }
            else
            {
                instructionBuffer.Add(processor.Create(OpCodes.Ldarg_0));
                instructionBuffer.Add(processor.Create(OpCodes.Call, helper.NativeObjectGetter));
            }
            instructionBuffer.Add(processor.Create(OpCodes.Ldsfld, offsetField));
            instructionBuffer.Add(processor.Create(OpCodes.Call, helper.IntPtrAdd));

            return instructionBuffer.ToArray();
        }

        protected static void WriteObjectDestroyedCheck(ILProcessor processor, RewriteHelper helper)
        {
            // emit check that object is not destroyed
            processor.Emit(OpCodes.Ldarg_0);
            processor.Emit(OpCodes.Call, helper.CheckDestroyedByUnrealGCMethod);
        }

        protected static ILProcessor BeginSimpleGetter(RewriteHelper helper, MethodDefinition getter, FieldDefinition offsetField)
        {
            ILProcessor processor = InitPropertyAccessor(getter);
            /*
            .method public hidebysig specialname instance int32 
                    get_TestReadableInt32() cil managed
            {
              // Code size       25 (0x19)
              .maxstack  2
              .locals init ([0] int32 ToReturn)
              IL_0000:  ldarg.0
              IL_0001:  call       instance native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::get_NativeObject()
              IL_0006:  ldsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestReadableInt32_Offset
              IL_000b:  call       native int [mscorlib]System.IntPtr::Add(native int,
                                                                           int32)
              IL_0010:  call       void* [mscorlib]System.IntPtr::op_Explicit(native int)
              IL_0015:  ldind.i4
              IL_0018:  ret
            } // end of method MonoTestsObject::get_TestReadableInt32
             */
            WriteObjectDestroyedCheck(processor, helper);
            return processor;
        }

        protected static void EndSimpleGetter(ILProcessor processor, RewriteHelper helper, MethodDefinition getter)
        {
            processor.Emit(OpCodes.Ret);
            getter.Body.OptimizeMacros();
        }


        protected static ILProcessor BeginSimpleSetter(RewriteHelper helper, MethodDefinition setter, FieldDefinition offsetField)
        {
            ILProcessor processor = InitPropertyAccessor(setter);
            /*
             .method public hidebysig specialname instance void 
                    set_TestReadWriteFloat(float32 'value') cil managed
            {
              // Code size       24 (0x18)
              .maxstack  8
              IL_0000:  ldarg.0
              IL_0001:  call       instance native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::get_NativeObject()
              IL_0006:  ldsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestReadWriteFloat_Offset
              IL_000b:  call       native int [mscorlib]System.IntPtr::Add(native int,
                                                                           int32)
              IL_0010:  call       void* [mscorlib]System.IntPtr::op_Explicit(native int)
              IL_0015:  ldarg.1
              IL_0016:  stind.r4
              IL_0017:  ret
            } // end of method MonoTestsObject::set_TestReadWriteFloat
             */
            WriteObjectDestroyedCheck(processor, helper);
            return processor;
        }

        protected static void EndSimpleSetter(ILProcessor processor, RewriteHelper helper, MethodDefinition setter)
        {
            processor.Emit(OpCodes.Ret);
            setter.Body.OptimizeMacros();
        }

        // Subclasses may override to do additional prep, such as adding additional backing fields.
        public virtual void PrepareForRewrite(RewriteHelper helper, FunctionMetadata functionMetadata, PropertyMetadata propertyMetadata)
        {
            if (ArrayDim > 1)
            {
                PropertyDefinition propertyDef = propertyMetadata.FindPropertyDefinition(helper.TargetType);
                if (propertyDef != null)
                {
                    // Suppress the setter.  All modifications should be done by modifying the FixedSizeArray wrapper
                    // returned by the getter, which will apply the changes to the underlying native array.
                    propertyDef.DeclaringType.Methods.Remove(propertyDef.SetMethod);
                    propertyDef.SetMethod = null;

                    // Add an instance backing field to hold the fixed-size array wrapper.
                    FixedSizeArrayWrapperType = helper.FindGenericTypeInAssembly(helper.BindingsAssembly, Program.BindingsNamespace, "FixedSizeArrayReadWrite`1", new TypeReference[] { CSharpType });
                    FixedSizeArrayWrapperField = new FieldDefinition(propertyDef.Name + "_Wrapper", FieldAttributes.Private, FixedSizeArrayWrapperType);
                    helper.TargetType.Fields.Add(FixedSizeArrayWrapperField);
                }
            }

            var marshalingDelegates = helper.FindGenericTypeInAssembly(helper.BindingsAssembly, Program.BindingsNamespace, "MarshalingDelegates`1", new TypeReference[] { CSharpType });

            ToNativeDelegateType = helper.TargetAssembly.MainModule.ImportReference((from type in marshalingDelegates.Resolve().NestedTypes where type.Name == "ToNative" select type).First());
            FromNativeDelegateType = helper.TargetAssembly.MainModule.ImportReference((from type in marshalingDelegates.Resolve().NestedTypes where type.Name == "FromNative" select type).First());
        }

        protected void EmitDelegate(ILProcessor processor, RewriteHelper helper, TypeReference delegateType, MethodReference method)
        {
            processor.Emit(OpCodes.Ldnull);
            method = helper.TargetAssembly.MainModule.ImportReference(method);
            processor.Emit(OpCodes.Ldftn, method);
            MethodReference ctor = (from constructor in delegateType.Resolve().GetConstructors() where constructor.Parameters.Count == 2 select constructor).First().Resolve();
            ctor = Program.MakeMethodDeclaringTypeGeneric(ctor, CSharpType);
            ctor = helper.TargetAssembly.MainModule.ImportReference(ctor);
            processor.Emit(OpCodes.Newobj, ctor);
        }

        // Emits IL for a default constructible and possibly generic fixed array marshaling helper object.
        // If typeParams is null, a non-generic type is assumed.
        protected void EmitSimpleMarshalerDelegates(ILProcessor processor, RewriteHelper helper, string marshalerTypeName, TypeReference[] typeParams)
        {
            AssemblyDefinition marshalerAssembly;
            string marshalerNamespace;
            // Marshalers for Matrix4, Vector3, and all built-in types live in the bindings namespace.
            // Marshalers for generated struct mirrors live in the same namespace as the struct.
            if (CSharpType.Namespace == "OpenTK" ||
                CSharpType.Namespace == "System" ||
                marshalerTypeName == "BlittableTypeMarshaler`1" |
                marshalerTypeName == "EnumMarshaler`1" ||
                marshalerTypeName == "UnrealObjectMarshaler`1")
            {
                marshalerAssembly = helper.BindingsAssembly;
                marshalerNamespace = Program.BindingsNamespace;
            }
            else
            {
                marshalerAssembly = CSharpType.Module.Assembly;
                marshalerNamespace = CSharpType.Namespace;
            }

            TypeReference marshalerType;
            if (typeParams != null)
            {
                marshalerType = helper.FindGenericTypeInAssembly(marshalerAssembly, marshalerNamespace, marshalerTypeName, typeParams);
            }
            else
            {
                marshalerType = helper.FindTypeInAssembly(marshalerAssembly, marshalerNamespace, marshalerTypeName);
            }

            MethodReference fromNative = (from method in marshalerType.Resolve().GetMethods() where method.IsStatic && method.Name == "FromNative" select method).ToArray()[0];
            MethodReference toNative = (from method in marshalerType.Resolve().GetMethods() where method.IsStatic && method.Name == "ToNative" select method).ToArray()[0];

            if (typeParams != null)
            {
                fromNative = Program.MakeMethodDeclaringTypeGeneric(fromNative, typeParams);
                toNative = Program.MakeMethodDeclaringTypeGeneric(toNative, typeParams);
            }

            EmitDelegate(processor, helper, ToNativeDelegateType, toNative);
            EmitDelegate(processor, helper, FromNativeDelegateType, fromNative);
        }

        public abstract void EmitFixedArrayMarshalerDelegates(ILProcessor processor, RewriteHelper helper);
        public virtual void EmitDynamicArrayMarshalerDelegates(ILProcessor processor, RewriteHelper helper)
        {
            EmitFixedArrayMarshalerDelegates(processor, helper);
        }

        public void WriteGetter(RewriteHelper helper, MethodDefinition getter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            if (ArrayDim == 1)
            {
                EmitGetter(helper, getter, offsetField, nativePropertyField);
            }
            else
            {
                /*
                  IL_0000:  ldarg.0
                  IL_0001:  ldfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.FixedSizeArrayReadWrite`1<int32> UnrealEngine.MonoRuntime.MonoTestsObject::TestStaticIntArray_Wrapper
                  IL_0006:  brtrue.s   IL_0023
                  IL_0008:  ldarg.0
                  IL_0009:  ldarg.0
                  IL_000a:  ldsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestStaticIntArray_Offset
                  IL_000f:  ldsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestStaticIntArray_Length
                  IL_0014:  newobj     instance void class [UnrealEngine.Runtime]UnrealEngine.Runtime.BlittableFixedSizeArrayMarshaler`1<int32>::.ctor()
                  IL_0019:  newobj     instance void class [UnrealEngine.Runtime]UnrealEngine.Runtime.FixedSizeArrayReadWrite`1<int32>::.ctor(class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject,
                                                                                                                                              int32,
                                                                                                                                              int32,
                                                                                                                                              class [UnrealEngine.Runtime]UnrealEngine.Runtime.FixedSizeArrayMarshaler`1<!0>)
                  IL_001e:  stfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.FixedSizeArrayReadWrite`1<int32> UnrealEngine.MonoRuntime.MonoTestsObject::TestStaticIntArray_Wrapper
                  IL_0023:  ldarg.0
                  IL_0024:  ldfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.FixedSizeArrayReadWrite`1<int32> UnrealEngine.MonoRuntime.MonoTestsObject::TestStaticIntArray_Wrapper
                  IL_0029:  ret
                 */
                ILProcessor processor = InitPropertyAccessor(getter);

                processor.Emit(OpCodes.Ldarg_0);
                processor.Emit(OpCodes.Ldfld, FixedSizeArrayWrapperField);

                // Store branch position for later insertion
                processor.Emit(OpCodes.Ldarg_0);
                Instruction branchPosition = processor.Body.Instructions[processor.Body.Instructions.Count - 1];
                processor.Emit(OpCodes.Ldarg_0);
                processor.Emit(OpCodes.Ldsfld, offsetField);
                processor.Emit(OpCodes.Ldc_I4, ArrayDim);

                // Allow subclasses to control construction of their own marshalers, as there may be
                // generics and/or ctor parameters involved.
                EmitFixedArrayMarshalerDelegates(processor, helper);


                var constructors = (from method in FixedSizeArrayWrapperType.Resolve().GetConstructors()
                                    where (!method.IsStatic
                                           && method.HasParameters
                                           && method.Parameters.Count == 5
                                           && method.Parameters[0].ParameterType.FullName == "UnrealEngine.Runtime.UnrealObject"
                                           && method.Parameters[1].ParameterType.FullName == "System.Int32"
                                           && method.Parameters[2].ParameterType.FullName == "System.Int32"
                                           && method.Parameters[3].ParameterType.IsGenericInstance
                                           && ((GenericInstanceType)method.Parameters[3].ParameterType).GetElementType().FullName == "UnrealEngine.Runtime.MarshalingDelegates`1/ToNative"
                                           && ((GenericInstanceType)method.Parameters[4].ParameterType).GetElementType().FullName == "UnrealEngine.Runtime.MarshalingDelegates`1/FromNative")
                                    select method).ToArray();
                Program.VerifySingleResult(constructors, helper.TargetType, "FixedSizeArrayWrapper UObject-backed constructor");
                processor.Emit(OpCodes.Newobj, helper.TargetAssembly.MainModule.ImportReference(Program.MakeMethodDeclaringTypeGeneric(constructors[0], new TypeReference[] { CSharpType })));
                processor.Emit(OpCodes.Stfld, FixedSizeArrayWrapperField);

                // Store branch target
                processor.Emit(OpCodes.Ldarg_0);
                Instruction branchTarget = processor.Body.Instructions[processor.Body.Instructions.Count - 1];
                processor.Emit(OpCodes.Ldfld, FixedSizeArrayWrapperField);

                // Insert branch
                processor.InsertBefore(branchPosition, processor.Create(OpCodes.Brtrue, branchTarget));

                EndSimpleGetter(processor, helper, getter);
            }
        }

        public void WriteSetter(RewriteHelper helper, MethodDefinition setter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            if (ArrayDim == 1)
            {
                EmitSetter(helper, setter, offsetField, nativePropertyField);
            }
            else
            {
                throw new NotSupportedException("Fixed-size array property setters should be stripped, not rewritten.");
            }
        }

        public virtual bool CanRewritePropertyInitializer => true;

        protected abstract void EmitGetter(RewriteHelper helper, MethodDefinition getter, FieldDefinition offsetField, FieldDefinition nativePropertyField);
        protected abstract void EmitSetter(RewriteHelper helper, MethodDefinition setter, FieldDefinition offsetField, FieldDefinition nativePropertyField);

        // Subclasses must implement to handle loading of values from a native buffer.
        // Returns the local variable containing the loaded value.
        public abstract void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, VariableDefinition localVar);
        public abstract void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, FieldDefinition destField);

        // Subclasses must implement to handle storing of a value into a native buffer.
        // Return value is a list of instructions that must be executed to clean up the value in the buffer, or null if no cleanup is required.
        public abstract IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, byte argIndex);
        public abstract IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, FieldDefinition srcField);

        public abstract void WriteMarshalFromNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner);
        public abstract void WriteMarshalToNative(ILProcessor processor, RewriteHelper helper, Instruction [] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction [] loadSource);
        
        public void WriteMarshalToNative (ILProcessor processor, RewriteHelper helper, Instruction [] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction loadSource)
        {
            WriteMarshalToNative(processor, helper, loadBufferPtr, loadArrayIndex, loadOwner, new Instruction[] { loadSource });
        }

        public virtual IList<Instruction> WriteMarshalToNativeWithCleanup(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction[] loadSource)
        {
            WriteMarshalToNative(processor, helper, loadBufferPtr, loadArrayIndex, loadOwner, loadSource);
            return null;
        }

        public virtual UnrealEngine.Runtime.PropertyFlags AutomaticPropertyFlags
        {
            get { return 0; }
        }

        protected void PropagateStructHash(RewriteHelper helper, PropertyMetadata property)
        {
            // If this property references a user struct, propagate its struct hash.
            // This will cause the hash of the struct or class which owns this property to change,
            // which will trigger a reinstance on hot reload.
            var structs = (from strukt in helper.Metadata.Structs
                           where strukt.Namespace == CSharpType.Namespace && strukt.Name == CSharpType.Name
                           select strukt).ToArray();
            if (structs.Length > 0)
            {
                property.ValueHash = structs[0].StructHash;
            }
        }

        public static UnrealType FromTypeReference(TypeReference typeRef, string propertyName, Collection<CustomAttribute> customAttributes)
        {
            int arrayDim = 1;
            TypeDefinition typeDef = typeRef.Resolve();
            SequencePoint sequencePoint = ErrorEmitter.GetSequencePointFromMemberDefinition(typeDef);

            if (customAttributes != null)
            {
                CustomAttribute propertyAttribute = Program.FindAttributeByType(customAttributes, Program.BindingsNamespace, "UPropertyAttribute");
                if (propertyAttribute != null)
                {
                    CustomAttributeArgument? arrayDimArg = Program.FindAttributeField(propertyAttribute, "ArrayDim");

                    GenericInstanceType GenericType = typeRef as GenericInstanceType;
                    if (GenericType != null && GenericType.GetElementType().FullName == "UnrealEngine.Runtime.FixedSizeArrayReadWrite`1")
                    {
                        if (arrayDimArg.HasValue)
                        {
                            arrayDim = (int)arrayDimArg.Value.Value;

                            // Unreal doesn't have a separate type for fixed arrays, so we just want to generate the inner UProperty type with an arrayDim.
                            typeRef = GenericType.GenericArguments[0];
                            typeDef = typeRef.Resolve();
                        }
                        else
                        {
                            throw new InvalidUnrealPropertyException(propertyName, sequencePoint, "Fixed array properties must specify an ArrayDim in their [UProperty] attribute");
                        }
                    }
                    else if (arrayDimArg.HasValue)
                    {
                        throw new InvalidUnrealPropertyException(propertyName, sequencePoint, "ArrayDim is only valid for FixedSizeArray properties.");
                    }
                }
            }

            switch (typeRef.FullName)
            {
                case "System.Double":
                    return new UnrealBuiltinType(typeRef, "DoubleProperty", arrayDim);
                case "System.Single":
                    return new UnrealBuiltinType(typeRef, "FloatProperty", arrayDim);

                case "System.SByte":
                    return new UnrealBuiltinType(typeRef, "Int8Property", arrayDim);
                case "System.Int16":
                    return new UnrealBuiltinType(typeRef, "Int16Property", arrayDim);
                case "System.Int32":
                    return new UnrealBuiltinType(typeRef, "IntProperty", arrayDim);
                case "System.Int64":
                    return new UnrealBuiltinType(typeRef, "Int64Property", arrayDim);

                case "System.Byte":
                    return new UnrealBuiltinType(typeRef, "ByteProperty", arrayDim);
                case "System.UInt16":
                    return new UnrealBuiltinType(typeRef, "UInt16Property", arrayDim);
                case "System.UInt32":
                    return new UnrealBuiltinType(typeRef,"UInt32Property", arrayDim);
                case "System.UInt64":
                    return new UnrealBuiltinType(typeRef, "UInt64Property", arrayDim);

                case "System.Boolean":
                    return new UnrealBooleanType(typeRef, "BoolProperty", arrayDim);

                case "System.String":
                    return new UnrealStringType(typeRef, arrayDim);

                default:
                    bool isSubobjectWrapper = false;

                    // If this is a subobject wrapper, we're only interested in the inner object type.
                    if (typeRef.IsGenericInstance)
                    {
                        GenericInstanceType GenericType = (GenericInstanceType)typeRef;
                        if (GenericType.GetElementType().FullName == "UnrealEngine.Runtime.Subobject`1")
                        {
                            typeDef = GenericType.GenericArguments[0].Resolve();
                            isSubobjectWrapper = true;
                        }
                        else if (GenericType.GetElementType().FullName == "UnrealEngine.Runtime.SubclassOf`1")
                        {
                            TypeDefinition requiredClassTypeDef = GenericType.GenericArguments[0].Resolve();
                            return new UnrealClassType(typeRef, arrayDim, requiredClassTypeDef);
                        }
                        else if (GenericType.GetElementType().FullName == "UnrealEngine.Runtime.WeakObject`1")
                        {
                            TypeDefinition innerTypeDef = GenericType.GenericArguments[0].Resolve();
                            return new UnrealWeakObjectType(typeRef, arrayDim, innerTypeDef);
                        }
                        else if (GenericType.GetElementType().FullName == "System.Collections.Generic.IList`1")
                        {
                            TypeReference innerType = GenericType.GenericArguments[0];
                            return new UnrealArrayType(typeRef, arrayDim, innerType);
                        }
                    }

                    if (typeDef.IsEnum)
                    {
                        CustomAttribute enumAttribute = Program.FindAttributeByType(typeDef.CustomAttributes, Program.BindingsNamespace, "UEnumAttribute");
                        if (null == enumAttribute)
                        {
                            throw new InvalidUnrealPropertyException(propertyName, sequencePoint, "Enum properties must use an unreal enum: " + typeRef.FullName);
                        }
                        if (Mono.Cecil.Rocks.TypeDefinitionRocks.GetEnumUnderlyingType(typeDef).FullName != "System.Byte")
                        {
                            throw new InvalidUnrealPropertyException(propertyName, sequencePoint, "Enum properties must have an underlying type of System.Byte: " + typeRef.FullName);
                        }

                        // Use the C# name as the native name, if not otherwise specified.
                        string nativeEnumName = typeDef.Name;
                        var nativeEnumNameAttrib = Program.FindAttributeField(enumAttribute, "NativeEnumName");
                        if (nativeEnumNameAttrib.HasValue)
                        {
                            nativeEnumName = (string)nativeEnumNameAttrib.Value.Value;
                        }

                        string nativeClassOwner = "";
                        var nativeClassOwnerAttrib = Program.FindAttributeField(enumAttribute, "NativeClassOwner");
                        if (nativeClassOwnerAttrib.HasValue)
                        {
                            nativeClassOwner = (string)nativeClassOwnerAttrib.Value.Value;
                        }

                        return new UnrealEnumType(typeDef, arrayDim, nativeClassOwner, nativeEnumName);
                    }
                    else if (typeDef.IsClass)
                    {
                        // see if its a UObject
                        if (typeDef.Namespace == Program.BindingsNamespace && typeDef.Name == "Text")
                        {
                            return new UnrealTextType(typeRef, arrayDim);
                        }
                        else
                        {
                            TypeDefinition superType = typeDef;
                            while (superType != null && superType.FullName != "UnrealEngine.Runtime.UnrealObject")
                            {
                                TypeReference superTypeRef = superType.BaseType;
                                superType = (superTypeRef != null ? superTypeRef.Resolve() : null);
                            }

                            if (superType != null)
                            {
                                return new UnrealObjectType(typeRef, "ObjectProperty", arrayDim, typeDef, isSubobjectWrapper);
                            }
                        }

                        // See if this is a struct
                        CustomAttribute structAttribute = Program.FindAttributeByType(typeDef.CustomAttributes, Program.BindingsNamespace, "UStructAttribute");
                        if (null != structAttribute || typeDef.Namespace == "OpenTK")
                        {
                            if (typeDef.Namespace == Program.BindingsNamespace && typeDef.Name == "Name")
                            {
                                return new UnrealNameType(typeDef, arrayDim);
                            }
                            else if (typeDef.Namespace == "OpenTK"
                                || (typeDef.Namespace == Program.BindingsNamespace && typeDef.Name == "Rotator")
                                )
                            {
                                // core type
                                return new UnrealCoreStructType(typeDef, arrayDim);
                            }
                            else
                            {
                                string nativeClassOwner = "";
                                var nativeClassOwnerAttrib = Program.FindAttributeField(structAttribute, "NativeClassOwner");
                                if (nativeClassOwnerAttrib.HasValue)
                                {
                                    nativeClassOwner = (string)nativeClassOwnerAttrib.Value.Value;
                                }

                                bool isBlittable = false;
                                var blittableAttrib = Program.FindAttributeField(structAttribute, "NativeBlittable");
                                if (blittableAttrib.HasValue)
                                {
                                    isBlittable = (bool)blittableAttrib.Value.Value;
                                }
                                if (isBlittable)
                                {
                                    return new UnrealBlittableStructType(typeDef, arrayDim, nativeClassOwner);
                                }
                                else
                                {
                                    return new UnrealStructType(typeDef, arrayDim, nativeClassOwner);
                                }
                            }
                        }
                    }
                    throw new InvalidUnrealPropertyException(propertyName, sequencePoint, "No Unreal type for " + typeRef.FullName);
            }
        }

        /// <summary>
        /// Emit an default initializer for the given property. Only necessary if zeroing it is not adequate.
        /// </summary>
        public virtual IEnumerable<Instruction> EmitDefaultPropertyInitializer(RewriteHelper helper, PropertyMetadata prop, PropertyDefinition def, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            return Array.Empty<Instruction>();
        }
    }

    abstract class UnrealSimpleType : UnrealType
    {
        private string MarshalerTypeName;
        TypeReference MarshalerType;
        MethodReference ToNative;
        MethodReference FromNative;

        public UnrealSimpleType(TypeReference typeRef, string marshalerTypeName, string unrealClass, int arrayDim)
            : base(typeRef, unrealClass, arrayDim)
        {
            LoadByReference = false;
            MarshalerTypeName = marshalerTypeName;
        }

        public override void PrepareForRewrite(RewriteHelper helper, FunctionMetadata functionMetadata, PropertyMetadata propertyMetadata)
        {
            base.PrepareForRewrite(helper, functionMetadata, propertyMetadata);

            string marshalerNamespace = "";
            AssemblyDefinition marshalerAssembly = null;
            if (CSharpType.Namespace == "OpenTK"
            || CSharpType.Namespace == "System"
            || MarshalerTypeName == "BlittableTypeMarshaler`1"
            || MarshalerTypeName == "EnumMarshaler`1"
            || MarshalerTypeName == "UnrealObjectMarshaler`1")
            {
                marshalerAssembly = helper.BindingsAssembly;
                marshalerNamespace = Program.BindingsNamespace;
            }
            else
            {
                marshalerAssembly = CSharpType.Module.Assembly;
                marshalerNamespace = CSharpType.Namespace;
            }

            TypeReference[] typeParams = null;
            if (MarshalerTypeName.EndsWith("`1"))
            {
                if (!CSharpType.IsGenericInstance)
                {
                    typeParams = new TypeReference[] { CSharpType };
                }
                else
                {
                    GenericInstanceType generic = (GenericInstanceType)CSharpType;
                    typeParams = new TypeReference[] { helper.TargetAssembly.MainModule.ImportReference(generic.GenericArguments[0].Resolve()) };
                }

                MarshalerType = helper.FindGenericTypeInAssembly(marshalerAssembly, marshalerNamespace, MarshalerTypeName, typeParams);

            }
            else
            {
                MarshalerType = helper.FindTypeInAssembly(marshalerAssembly, marshalerNamespace, MarshalerTypeName);
            }

            ToNative = (from method in MarshalerType.Resolve().GetMethods() where method.IsStatic && method.Name == "ToNative" select method).ToArray()[0];
            FromNative = (from method in MarshalerType.Resolve().GetMethods() where method.IsStatic && method.Name == "FromNative" select method).ToArray()[0];

            if (MarshalerTypeName.EndsWith("`1"))
            {
                ToNative = Program.MakeMethodDeclaringTypeGeneric(ToNative, typeParams);
                FromNative = Program.MakeMethodDeclaringTypeGeneric(FromNative, typeParams);
            }

            ToNative = helper.TargetAssembly.MainModule.ImportReference(ToNative);
            FromNative = helper.TargetAssembly.MainModule.ImportReference(FromNative);

        }

        public override bool IsPlainOldData { get { return true; } }

        protected override void EmitGetter(RewriteHelper helper, MethodDefinition getter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            ILProcessor processor = BeginSimpleGetter(helper, getter, offsetField);
            Instruction loadOwner = processor.Create(OpCodes.Ldarg_0);
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, null, offsetField);
            WriteMarshalFromNative(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), loadOwner);
            EndSimpleGetter(processor, helper, getter);
        }

        protected override void EmitSetter(RewriteHelper helper, MethodDefinition setter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            ILProcessor processor = BeginSimpleSetter(helper, setter, offsetField);
            Instruction loadValue = processor.Create(LoadByReference ? OpCodes.Ldarga : OpCodes.Ldarg, 1);
            Instruction loadOwner = processor.Create(OpCodes.Ldarg_0);
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, null, offsetField);
            WriteMarshalToNative(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), loadOwner, loadValue);
            EndSimpleSetter(processor, helper, setter);
        }

        public override void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBuffer, FieldDefinition offsetField, VariableDefinition localVar)
        {
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
            WriteMarshalFromNative(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull));
            processor.Emit(OpCodes.Stloc, localVar);
        }

        public override void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBuffer, FieldDefinition offsetField, FieldDefinition destField)
        {
            processor.Emit(OpCodes.Ldarg_0);
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
            WriteMarshalFromNative(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull));
            processor.Emit(OpCodes.Stfld, destField);
        }

        public override IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBuffer, FieldDefinition offsetField, byte argIndex)
        {
            Instruction loadArg = processor.Create(LoadByReference ? OpCodes.Ldarga_S : OpCodes.Ldarg_S, argIndex);
            Instruction loadOwner = processor.Create(OpCodes.Ldnull);
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
            return WriteMarshalToNativeWithCleanup(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), loadOwner, new Instruction[] { loadArg });
        }

        public override IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBuffer, FieldDefinition offsetField, FieldDefinition srcField)
        {
            Instruction loadOwner = processor.Create(OpCodes.Ldnull);
            Instruction[] loadField = new Instruction[]
            {
                processor.Create(OpCodes.Ldarg_0),
                processor.Create(LoadByReference ? OpCodes.Ldflda : OpCodes.Ldfld, srcField),
            };
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
            return WriteMarshalToNativeWithCleanup(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), loadOwner, loadField);
        }

        // Allow subclasses to override in order to load the setter argument by reference.
        protected bool LoadByReference { get; set; }


        public override void WriteMarshalToNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction[] loadSource)
        {
            foreach (var i in loadBufferPtr)
            {
                processor.Append(i);
            }
            processor.Append(loadArrayIndex);
            processor.Append(loadOwner);
            foreach (Instruction i in loadSource) //source
            {
                processor.Append(i);
            }
            processor.Emit(OpCodes.Call, ToNative);
        }

        public override void WriteMarshalFromNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner)
        {
            foreach (var i in loadBufferPtr)
            {
                processor.Append(i);
            }
            processor.Append(loadArrayIndex);
            processor.Append(loadOwner);
            processor.Emit(OpCodes.Call, FromNative);
        }

        public override void EmitFixedArrayMarshalerDelegates(ILProcessor processor, RewriteHelper helper)
        {
            TypeReference[] typeParams = null;
            if (MarshalerTypeName.EndsWith("`1"))
            {
                if (!CSharpType.IsGenericInstance)
                {
                    typeParams = new TypeReference[] { CSharpType };
                }
                else
                {
                    GenericInstanceType generic = (GenericInstanceType)CSharpType;
                    typeParams = new TypeReference[] { helper.TargetAssembly.MainModule.ImportReference(generic.GenericArguments[0].Resolve()) };
                }
            }

            EmitSimpleMarshalerDelegates(processor, helper, MarshalerTypeName, typeParams);
        }
    }

    class UnrealBuiltinType : UnrealSimpleType
    {
        public override bool IsBlittable { get { return true; } }

        public UnrealBuiltinType(TypeReference typeRef, string unrealClass, int arrayDim)
            : base(typeRef, "BlittableTypeMarshaler`1", unrealClass, arrayDim)
        {
        }
    }

    class UnrealBooleanType : UnrealSimpleType
    {
        public UnrealBooleanType(TypeReference typeRef, string unrealClass, int arrayDim)
            : base(typeRef, "BoolMarshaler", unrealClass, arrayDim)
        {
            LoadByReference = false;
        }

        public override bool IsPlainOldData { get { return false; } }
    }

    class UnrealBlittableStructTypeBase : UnrealSimpleType
    {
        public UnrealBlittableStructTypeBase(TypeReference structType, string unrealClass, int arrayDim)
            : base(structType, "BlittableTypeMarshaler`1", unrealClass, arrayDim)
        {
        }

        public override bool IsBlittable { get { return true; } }

        public override void PrepareForRewrite(RewriteHelper helper, FunctionMetadata functionMetadata, PropertyMetadata propertyMetadata)
        {
            base.PrepareForRewrite(helper, functionMetadata, propertyMetadata);

            PropagateStructHash(helper, propertyMetadata);
        }
    }

    class UnrealNameType : UnrealBlittableStructTypeBase
    {
        public UnrealNameType(TypeReference structType, int arrayDim)
            : base(structType, "NameProperty", arrayDim)
        {
        }
    }


    class UnrealCoreStructType : UnrealBlittableStructTypeBase
    {
        public readonly string StructName;

        public UnrealCoreStructType(TypeReference structType, int arrayDim)
            : base(structType, "CoreStructProperty", arrayDim)
        {
            switch (structType.Name)
            {
                case "Quaternion":
                    StructName = "Quat";
                    break;
                case "Vector2":
                    StructName = "Vector2D";
                    break;
                case "Vector3":
                    StructName = "Vector";
                    break;
                case "Matrix4":
                    StructName = "Matrix";
                    break;
                default:
                    StructName = structType.Name;
                    break;
            }
        }
    }

    class UnrealBlittableStructType : UnrealBlittableStructTypeBase
    {
        public readonly string NativeClassOwner;
        public readonly TypeReferenceMetadata TypeRef;

        public UnrealBlittableStructType(TypeReference structType, int arrayDim, string nativeClassOwner)
            : base(structType, "StructProperty", arrayDim)
        {
            NativeClassOwner = nativeClassOwner;
            TypeRef = new TypeReferenceMetadata(structType);
        }
    }

    class UnrealEnumType : UnrealSimpleType
    {
        public readonly string NativeClassOwner;
        public readonly string NativeEnumName;
        public readonly TypeReferenceMetadata TypeRef;

        public UnrealEnumType(TypeReference typeRef, int arrayDim, string nativeClassOwner, string nativeEnumName)
            : base(typeRef, "EnumMarshaler`1", "EnumProperty", arrayDim)
        {
            NativeClassOwner = nativeClassOwner;
            NativeEnumName = nativeEnumName;
            TypeRef = new TypeReferenceMetadata(typeRef);
        }
    }

    class UnrealObjectType : UnrealSimpleType
    {
        public readonly TypeReferenceMetadata TypeRef;
        public readonly bool IsSubobjectWrapper;

        public UnrealObjectType(TypeReference propertyTypeRef, string unrealClass, int arrayDim, TypeDefinition classTypeDef, bool isSubobjectWrapper)
            : base(propertyTypeRef, (!isSubobjectWrapper ? "UnrealObjectMarshaler`1" : "SubobjectMarshaler`1"), unrealClass, arrayDim)
        {
            TypeRef = new TypeReferenceMetadata(classTypeDef);
            IsSubobjectWrapper = isSubobjectWrapper;
        }

        public override UnrealEngine.Runtime.PropertyFlags AutomaticPropertyFlags
        {
            get
            {
                if (IsSubobjectWrapper)
                {
                    return UnrealEngine.Runtime.PropertyFlags.PersistentInstance | UnrealEngine.Runtime.PropertyFlags.ExportObject | UnrealEngine.Runtime.PropertyFlags.InstancedReference;
                }
                else
                {
                    return 0;
                }
            }
        }
    };

    class UnrealClassType : UnrealSimpleType
    {
        internal TypeReference MetaCSharpType;
        public readonly TypeReferenceMetadata TypeRef;

        public UnrealClassType(TypeReference typeRef, int arrayDim, TypeReference metaClassTypeRef)
            : base(typeRef, "SubclassOfMarshaler`1", "ClassProperty", arrayDim)
        {
            MetaCSharpType = metaClassTypeRef;
            TypeRef = new TypeReferenceMetadata(metaClassTypeRef);
        }
    }

    class UnrealWeakObjectType : UnrealSimpleType
    {
        internal TypeReference InnerCSharpType;
        public readonly TypeReferenceMetadata TypeRef;

        public UnrealWeakObjectType(TypeReference typeRef, int arrayDim, TypeReference innerTypeRef)
            : base(typeRef, "WeakObjectMarshaler`1", "WeakObjectProperty", arrayDim)
        {
            InnerCSharpType = innerTypeRef;
            TypeRef = new TypeReferenceMetadata(InnerCSharpType);
        }
    }

    class UnrealStringType : UnrealType
    {
        MethodReference ToNative;
        MethodReference FromNative;
        MethodReference ToNativeWithCleanup;
        MethodReference DestructInstance;

        public UnrealStringType(TypeReference typeRef, int arrayDim)
            : base(typeRef, "StrProperty", arrayDim)
        {
            NeedsNativePropertyField = true;
        }

        public override void PrepareForRewrite(RewriteHelper helper, FunctionMetadata functionMetadata, PropertyMetadata propertyMetadata)
        {
            base.PrepareForRewrite(helper, functionMetadata, propertyMetadata);

            string marshalerNamespace = Program.BindingsNamespace;
            AssemblyDefinition marshalerAssembly = helper.BindingsAssembly;

            TypeDefinition MarshalerType = helper.FindTypeInAssembly(marshalerAssembly, marshalerNamespace, "StringMarshaler").Resolve();
            TypeDefinition MarshalerTypeWithCleanup = helper.FindTypeInAssembly(marshalerAssembly, marshalerNamespace, "StringMarshalerWithCleanup").Resolve();

            ToNative = helper.TargetAssembly.MainModule.ImportReference((from method in MarshalerType.GetMethods() where method.IsStatic && method.Name == "ToNative" select method).ToArray()[0]);
            FromNative = helper.TargetAssembly.MainModule.ImportReference((from method in MarshalerType.GetMethods() where method.IsStatic && method.Name == "FromNative" select method).ToArray()[0]);
            ToNativeWithCleanup = helper.TargetAssembly.MainModule.ImportReference((from method in MarshalerTypeWithCleanup.GetMethods() where method.IsStatic && method.Name == "ToNative" select method).ToArray()[0]);
            DestructInstance = helper.TargetAssembly.MainModule.ImportReference((from method in MarshalerTypeWithCleanup.GetMethods() where method.IsStatic && method.Name == "DestructInstance" select method).ToArray()[0]);
        }

        public override void EmitFixedArrayMarshalerDelegates(ILProcessor processor, RewriteHelper helper)
        {
            EmitSimpleMarshalerDelegates(processor, helper, "StringMarshaler", null);
        }

        private void EmitLoadAsScriptArray(ILProcessor processor, RewriteHelper helper)
        {
            processor.Body.InitLocals = true;
            VariableDefinition scriptArray = new VariableDefinition(new PointerType(helper.ScriptArrayType));
            processor.Body.Variables.Add(scriptArray);
            processor.Emit(OpCodes.Stloc_S, scriptArray);
            processor.Emit(OpCodes.Ldloc_S, scriptArray);
            processor.Emit(OpCodes.Ldfld, helper.ScriptArrayDataField);
            processor.Emit(OpCodes.Call, helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "MarshalIntPtrAsString"));
        }
        protected override void EmitGetter(RewriteHelper helper, MethodDefinition getter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            ILProcessor processor = BeginSimpleGetter(helper, getter, offsetField);
            Instruction loadOwner = processor.Create(OpCodes.Ldarg_0);
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, null, offsetField);
            WriteMarshalFromNative(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), loadOwner);
            EndSimpleGetter(processor, helper, getter);
        }
        protected override void EmitSetter(RewriteHelper helper, MethodDefinition setter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            ILProcessor processor = BeginSimpleSetter(helper, setter, offsetField);
            Instruction loadValue = processor.Create(OpCodes.Ldarg_1);
            Instruction loadOwner = processor.Create(OpCodes.Ldarg_0);
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, null, offsetField);
            WriteMarshalToNative(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), loadOwner, loadValue);
            EndSimpleSetter(processor, helper, setter);
        }

        public override void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBuffer, FieldDefinition offsetField, VariableDefinition localVar)
        {
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
            WriteMarshalFromNative(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull));
            processor.Emit(OpCodes.Stloc, localVar);
        }
        public override void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBuffer, FieldDefinition offsetField, FieldDefinition destField)
        {
            processor.Emit(OpCodes.Ldarg_0);
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
            WriteMarshalFromNative(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull));
            processor.Emit(OpCodes.Stfld, destField);
        }

        public override void WriteMarshalToNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction[] loadSourceInstructions)
        {
            foreach (var i in loadBufferPtr)
            {
                processor.Append(i);
            }
            processor.Append(loadArrayIndex);
            processor.Append(loadOwner);
            foreach (Instruction i in loadSourceInstructions) //source
            {
                processor.Append(i);
            }
            processor.Emit(OpCodes.Call, ToNative);
        }

        public override void WriteMarshalFromNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner)
        {
            foreach (var i in loadBufferPtr)
            {
                processor.Append(i);
            }
            processor.Append(loadArrayIndex);
            processor.Append(loadOwner);
            processor.Emit(OpCodes.Call, FromNative);
        }

        public override IList<Instruction> WriteMarshalToNativeWithCleanup(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction[] loadSourceInstructions)
        {
            foreach (var i in loadBufferPtr)
            {
                processor.Append(i);
            }
            processor.Append(loadArrayIndex);
            processor.Append(loadOwner);
            foreach (Instruction i in loadSourceInstructions) //source
            {
                processor.Append(i);
            }
            processor.Emit(OpCodes.Call, ToNativeWithCleanup);

            IList<Instruction> cleanupInstructions = new List<Instruction>();
            foreach (var i in loadBufferPtr)
            {
                cleanupInstructions.Add(i);
            }
            cleanupInstructions.Add(loadArrayIndex);
            cleanupInstructions.Add(processor.Create(OpCodes.Call, DestructInstance));
            return cleanupInstructions;
        }

        public override IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBuffer, FieldDefinition offsetField, byte argIndex)
        {
            Instruction[] loadSource = new Instruction[]
            {
                 processor.Create(OpCodes.Ldarg_S, argIndex)
            };
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
            return WriteMarshalToNativeWithCleanup(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull), loadSource);
        }
        public override IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBuffer, FieldDefinition offsetField, FieldDefinition srcField)
        {
            Instruction[] loadSource = new Instruction[]
            {
                processor.Create(OpCodes.Ldarg_0),
                processor.Create(OpCodes.Ldfld, srcField),
            };
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
            return WriteMarshalToNativeWithCleanup(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull), loadSource);
        }
    }

    class UnrealStructType : UnrealSimpleType
    {
        internal readonly TypeDefinition StructType;
        public readonly string NativeClassOwner;
        public readonly TypeReferenceMetadata TypeRef;

        public UnrealStructType(TypeReference structType, int arrayDim, string nativeClassOwner)
            : base(structType, structType.Name + "Marshaler", "StructProperty", arrayDim)
        {
            StructType = structType.Resolve();

            NativeClassOwner = nativeClassOwner;
            TypeRef = new TypeReferenceMetadata(structType);
        }

        public override void PrepareForRewrite(RewriteHelper helper, FunctionMetadata func, PropertyMetadata property)
        {
            base.PrepareForRewrite(helper, func, property);

            PropagateStructHash(helper, property);
        }
    }

    class UnrealTextType : UnrealType
    {
        public UnrealTextType(TypeReference textType, int arrayDim)
            : base(textType, "TextProperty", arrayDim)
        {

        }

        private FieldDefinition MarshalerField;
        private MethodReference MarshalerCtor;
        private MethodReference FromNative;

        public override void PrepareForRewrite(RewriteHelper helper, FunctionMetadata functionMetadata, PropertyMetadata propertyMetadata)
        {
            base.PrepareForRewrite(helper, functionMetadata, propertyMetadata);

            // Ensure that Text itself is imported.
            helper.TargetAssembly.MainModule.ImportReference(CSharpType);

            TypeDefinition marshalerType = helper.FindTypeInAssembly(helper.BindingsAssembly, Program.BindingsNamespace, "TextMarshaler").Resolve();
            MarshalerCtor = helper.TargetAssembly.MainModule.ImportReference((from method in marshalerType.GetConstructors() 
                                                                     where method.Parameters.Count == 1 
                                                                           && method.Parameters[0].ParameterType.FullName == "System.Int32" 
                                                                     select method).ToArray()[0]);
            FromNative = helper.TargetAssembly.MainModule.ImportReference((from method in marshalerType.GetMethods() 
                                                                  where method.Name == "FromNative" 
                                                                  select method).ToArray()[0]);

            // If this is a rewritten autoproperty, we need an additional backing field for the Text marshaling wrapper.
            // Otherwise, we're copying data for a struct UProp, parameter, or return value.
            string prefix = propertyMetadata.Name + "_";
            PropertyDefinition propertyDef = propertyMetadata.FindPropertyDefinition(helper.TargetType);
            if (propertyDef != null)
            {

                // Add a field to store the array wrapper for the getter.                
                MarshalerField = new FieldDefinition(prefix + "Wrapper", FieldAttributes.Private, helper.TargetAssembly.MainModule.ImportReference(marshalerType));
                propertyDef.DeclaringType.Fields.Add(MarshalerField);

                // Suppress the setter.  All modifications should be done by modifying the Text object returned by the getter,
                // which will propagate the changes to the underlying native FText memory.
                propertyDef.DeclaringType.Methods.Remove(propertyDef.SetMethod);
                propertyDef.SetMethod = null;
            }
        }

        public override IEnumerable<Instruction> EmitDefaultPropertyInitializer(RewriteHelper helper, PropertyMetadata prop, PropertyDefinition def, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            var textStruct = helper.FindTypeInAssembly(helper.BindingsAssembly, "UnrealEngine.Runtime", "Text").Resolve();
            MethodReference setEmptyMeth = textStruct.Methods.First(m => m.Name == "SetEmpty");
            var getMeth = def.GetMethod;
            setEmptyMeth = def.DeclaringType.Module.ImportReference(setEmptyMeth);
            yield return Instruction.Create(OpCodes.Ldarg_0);
            yield return Instruction.Create(OpCodes.Call, getMeth);
            yield return Instruction.Create(OpCodes.Call, setEmptyMeth);
        }

        public override void EmitFixedArrayMarshalerDelegates(ILProcessor processor, RewriteHelper helper)
        {
            throw new NotImplementedException();
        }

        protected override void EmitGetter(RewriteHelper helper, MethodDefinition getter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            ILProcessor processor = InitPropertyAccessor(getter);

            /*
              IL_0000:  ldarg.0
              IL_0001:  ldfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.TextMarshaler UnrealEngine.MonoRuntime.MonoTestsObject::TestReadWriteText_Wrapper
              IL_0006:  brtrue.s   IL_0014
              IL_0008:  ldarg.0
              IL_0009:  ldc.i4.1
              IL_000a:  newobj     instance void [UnrealEngine.Runtime]UnrealEngine.Runtime.TextMarshaler::.ctor(int32)
              IL_000f:  stfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.TextMarshaler UnrealEngine.MonoRuntime.MonoTestsObject::TestReadWriteText_Wrapper
              IL_0014:  ldarg.0
              IL_0015:  ldfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.TextMarshaler UnrealEngine.MonoRuntime.MonoTestsObject::TestReadWriteText_Wrapper
              IL_001a:  ldarg.0
              IL_001b:  call       instance native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::get_NativeObject()
              IL_0020:  ldsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestReadWriteText_Offset
              IL_0025:  call       native int [mscorlib]System.IntPtr::op_Addition(native int,
                                                                                   int32)
              IL_002a:  ldc.i4.0
              IL_002b:  ldarg.0
              IL_002c:  callvirt   instance class [UnrealEngine.Runtime]UnrealEngine.Runtime.Text [UnrealEngine.Runtime]UnrealEngine.Runtime.TextMarshaler::FromNative(native int,
                                                                                                                                                                       int32,
                                                                                                                                                                       class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject)
             */
            processor.Emit(OpCodes.Ldarg_0);
            processor.Emit(OpCodes.Ldfld, MarshalerField);

            Instruction branchPosition = processor.Create(OpCodes.Ldarg_0);
            processor.Append(branchPosition);
            processor.Emit(OpCodes.Ldc_I4_1);
            processor.Emit(OpCodes.Newobj, MarshalerCtor);
            processor.Emit(OpCodes.Stfld, MarshalerField);

            Instruction branchTarget = processor.Create(OpCodes.Ldarg_0);
            processor.Append(branchTarget);
            processor.Emit(OpCodes.Ldfld, MarshalerField);
            processor.Emit(OpCodes.Ldarg_0);
            processor.Emit(OpCodes.Call, helper.NativeObjectGetter);
            processor.Emit(OpCodes.Ldsfld, offsetField);
            processor.Emit(OpCodes.Call, helper.IntPtrAdd);
            processor.Emit(OpCodes.Ldc_I4_0);
            processor.Emit(OpCodes.Ldarg_0);
            processor.Emit(OpCodes.Callvirt, FromNative);

            Instruction branch = processor.Create(OpCodes.Brtrue, branchTarget);
            processor.InsertBefore(branchPosition, branch);

            EndSimpleGetter(processor, helper, getter);

        }

        public override bool CanRewritePropertyInitializer => false;

        protected override void EmitSetter(RewriteHelper helper, MethodDefinition setter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            throw new NotSupportedException("Text property setters should be stripped, not rewritten.");
        }

        public override void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, VariableDefinition localVar)
        {
            throw new NotImplementedException();
        }

        public override void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, FieldDefinition destField)
        {
            throw new NotImplementedException();
        }

        public override IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, byte argIndex)
        {
            throw new NotImplementedException();
        }

        public override IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, FieldDefinition srcField)
        {
            throw new NotImplementedException();
        }

        public override void WriteMarshalFromNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner)
        {
            throw new NotImplementedException();
        }

        public override void WriteMarshalToNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction[] loadSource)
        {
            throw new NotImplementedException();
        }
    }
    class UnrealArrayType : UnrealType
    {
        public readonly PropertyMetadata InnerProperty;

        private TypeReference ArrayWrapperType;
        private TypeReference CopyArrayWrapperType;
        private FieldDefinition ArrayWrapperField;
        private TypeReference[] ArrayWrapperTypeParameters;
        private MethodReference FromNative;

        private MethodReference CopyArrayWrapperCtor;
        private MethodReference CopyFromNative;
        private MethodReference CopyToNative;
        private MethodReference CopyDestructInstance;

        private VariableDefinition MarshalingLocal;
        private FieldDefinition ElementSizeField;

        public UnrealArrayType(TypeReference arrayType, int arrayDim, TypeReference innerType)
            : base(arrayType, "ArrayProperty", arrayDim)
        {
            InnerProperty = PropertyMetadata.FromTypeReference(innerType, "Inner");
            NeedsNativePropertyField = true;
            NeedsElementSizeField = true;
        }

        public override void EmitFixedArrayMarshalerDelegates(ILProcessor processor, RewriteHelper helper)
        {
            throw new NotImplementedException("Fixed-size arrays of dynamic arrays not yet supported.");
        }

        public override void PrepareForRewrite(RewriteHelper helper, FunctionMetadata functionMetadata, PropertyMetadata propertyMetadata)
        {
            base.PrepareForRewrite(helper, functionMetadata, propertyMetadata);

            // Ensure that IList<T> itself is imported.
            helper.TargetAssembly.MainModule.ImportReference(CSharpType);

            if (!(InnerProperty.UnrealPropertyType is UnrealSimpleType))
            {
                throw new InvalidUnrealPropertyException(propertyMetadata.Name, ErrorEmitter.GetSequencePointFromMemberDefinition(helper.TargetType), "Only UObjectProperty, UStructProperty, and blittable property types are supported as array inners.");
            }

            InnerProperty.UnrealPropertyType.PrepareForRewrite(helper, functionMetadata, InnerProperty);

            // Instantiate generics for the direct access and copying marshalers.
            string wrapperTypeName = "UnrealArrayReadWriteMarshaler`1";
            string copyWrapperTypeName = "UnrealArrayCopyMarshaler`1";

            ArrayWrapperTypeParameters = new TypeReference[] { helper.TargetAssembly.MainModule.ImportReference(InnerProperty.UnrealPropertyType.CSharpType) };

            var genericWrapperTypeRef = (from type in helper.BindingsAssembly.MainModule.Types
                                         where type.Namespace == Program.BindingsNamespace
                                               && type.Name == wrapperTypeName
                                         select type).ToArray()[0];
            var genericCopyWrapperTypeRef = (from type in helper.BindingsAssembly.MainModule.Types
                                             where type.Namespace == Program.BindingsNamespace
                                                   && type.Name == copyWrapperTypeName
                                             select type).ToArray()[0];

            ArrayWrapperType = helper.TargetAssembly.MainModule.ImportReference(genericWrapperTypeRef.Resolve().MakeGenericInstanceType(ArrayWrapperTypeParameters));
            CopyArrayWrapperType = helper.TargetAssembly.MainModule.ImportReference(genericCopyWrapperTypeRef.Resolve().MakeGenericInstanceType(ArrayWrapperTypeParameters));

            TypeDefinition arrTypeDef = ArrayWrapperType.Resolve();
            FromNative = helper.TargetAssembly.MainModule.ImportReference((from method in arrTypeDef.GetMethods() where method.Name == "FromNative" select method).ToArray()[0]);
            FromNative = Program.MakeMethodDeclaringTypeGeneric(FromNative, ArrayWrapperTypeParameters);

            TypeDefinition copyArrTypeDef = CopyArrayWrapperType.Resolve();
            CopyArrayWrapperCtor = (from method in copyArrTypeDef.GetConstructors()
                                    where (!method.IsStatic
                                           && method.HasParameters
                                           && method.Parameters.Count == 4
                                           && method.Parameters[0].ParameterType.FullName == "System.Int32"
                                           && ((GenericInstanceType)method.Parameters[1].ParameterType).GetElementType().FullName == "UnrealEngine.Runtime.MarshalingDelegates`1/ToNative"
                                           && ((GenericInstanceType)method.Parameters[2].ParameterType).GetElementType().FullName == "UnrealEngine.Runtime.MarshalingDelegates`1/FromNative"
                                           && method.Parameters[3].ParameterType.FullName == "System.Int32")
                                    select method).ToArray()[0];
            CopyArrayWrapperCtor = helper.TargetAssembly.MainModule.ImportReference(CopyArrayWrapperCtor);
            CopyArrayWrapperCtor = Program.MakeMethodDeclaringTypeGeneric(CopyArrayWrapperCtor, ArrayWrapperTypeParameters);
            CopyFromNative = helper.TargetAssembly.MainModule.ImportReference((from method in copyArrTypeDef.GetMethods() where method.Name == "FromNative" select method).ToArray()[0]);
            CopyFromNative = Program.MakeMethodDeclaringTypeGeneric(CopyFromNative, ArrayWrapperTypeParameters);
            CopyToNative = helper.TargetAssembly.MainModule.ImportReference((from method in copyArrTypeDef.GetMethods() where method.Name == "ToNative" select method).ToArray()[0]);
            CopyToNative = Program.MakeMethodDeclaringTypeGeneric(CopyToNative, ArrayWrapperTypeParameters);
            CopyDestructInstance = helper.TargetAssembly.MainModule.ImportReference((from method in copyArrTypeDef.GetMethods() where method.Name == "DestructInstance" select method).ToArray()[0]);
            CopyDestructInstance = Program.MakeMethodDeclaringTypeGeneric(CopyDestructInstance, ArrayWrapperTypeParameters);

            // If this is a rewritten autoproperty, we need an additional backing field for the array wrapper.
            // Otherwise, we're copying data for a struct UProp, parameter, or return value.
            string prefix = propertyMetadata.Name + "_";
            PropertyDefinition propertyDef = propertyMetadata.FindPropertyDefinition(helper.TargetType);
            if (propertyDef != null)
            {

                // Add a field to store the array wrapper for the getter.                
                ArrayWrapperField = new FieldDefinition(prefix + "Wrapper", FieldAttributes.Private, ArrayWrapperType);
                propertyDef.DeclaringType.Fields.Add(ArrayWrapperField);

                // Suppress the setter.  All modifications should be done by modifying the IList<T> returned by the getter,
                // which will propagate the changes to the underlying native TArray memory.
                propertyDef.DeclaringType.Methods.Remove(propertyDef.SetMethod);
                propertyDef.SetMethod = null;
            }
            else
            {
                prefix = functionMetadata.Name + "_" + prefix;

                MarshalingLocal = new VariableDefinition(CopyArrayWrapperType);
                ElementSizeField = helper.FindFieldInType(helper.TargetType, prefix + "ElementSize").Resolve();
            }
        }


        protected override void EmitGetter(RewriteHelper helper, MethodDefinition getter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            ILProcessor processor = InitPropertyAccessor(getter);

            /*
                .method public hidebysig specialname instance class [mscorlib]System.Collections.Generic.IList`1<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name> 
                            get_Tags() cil managed
                    {
                      // Code size       79 (0x4f)
                      .maxstack  6
                      IL_0000:  ldarg.0
                      IL_0001:  ldfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealArrayReadWriteMarshaler`1<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name> UnrealEngine.Engine.Actor::Tags_Wrapper
                      IL_0006:  brtrue.s   IL_0031
                      IL_0008:  ldarg.0
                      IL_0009:  ldc.i4.1
                      IL_000a:  ldsfld     native int UnrealEngine.Engine.Actor::Tags_NativeProperty
                      IL_000f:  ldnull
                      IL_0010:  ldftn      void class [UnrealEngine.Runtime]UnrealEngine.Runtime.BlittableTypeMarshaler`1<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name>::ToNative(native int,
                                                                                                                                                                                               int32,
                                                                                                                                                                                               class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject,
                                                                                                                                                                                               !0)
                      IL_0016:  newobj     instance void class [UnrealEngine.Runtime]UnrealEngine.Runtime.MarshalingDelegates`1/ToNative<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name>::.ctor(object,
                                                                                                                                                                                                           native int)
                      IL_001b:  ldnull
                      IL_001c:  ldftn      !0 class [UnrealEngine.Runtime]UnrealEngine.Runtime.BlittableTypeMarshaler`1<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name>::FromNative(native int,
                                                                                                                                                                                               int32,
                                                                                                                                                                                               class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject)
                      IL_0022:  newobj     instance void class [UnrealEngine.Runtime]UnrealEngine.Runtime.MarshalingDelegates`1/FromNative<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name>::.ctor(object,
                                                                                                                                                                                                             native int)
                      IL_0027:  newobj     instance void class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealArrayReadWriteMarshaler`1<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name>::.ctor(int32,
                                                                                                                                                                                                            native int,
                                                                                                                                                                                                            class [UnrealEngine.Runtime]UnrealEngine.Runtime.MarshalingDelegates`1/ToNative<!0>,
                                                                                                                                                                                                            class [UnrealEngine.Runtime]UnrealEngine.Runtime.MarshalingDelegates`1/FromNative<!0>)
                      IL_002c:  stfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealArrayReadWriteMarshaler`1<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name> UnrealEngine.Engine.Actor::Tags_Wrapper
                      IL_0031:  ldarg.0
                      IL_0032:  ldfld      class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealArrayReadWriteMarshaler`1<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name> UnrealEngine.Engine.Actor::Tags_Wrapper
                      IL_0037:  ldarg.0
                      IL_0038:  call       instance native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::get_NativeObject()
                      IL_003d:  ldsfld     int32 UnrealEngine.Engine.Actor::Tags_Offset
                      IL_0042:  call       native int [mscorlib]System.IntPtr::Add(native int,
                                                                                   int32)
                      IL_0047:  ldc.i4.0
                      IL_0048:  ldarg.0
                      IL_0049:  callvirt   instance class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealArrayReadWrite`1<!0> class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealArrayReadWriteMarshaler`1<valuetype [UnrealEngine.Runtime]UnrealEngine.Runtime.Name>::FromNative(native int,
                                                                                                                                                                                                                                                                                        int32,
                                                                                                                                                                                                                                                                                        class [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject)
                      IL_004e:  ret
                    } // end of method Actor::get_Tags
             */


            processor.Emit(OpCodes.Ldarg_0);
            processor.Emit(OpCodes.Ldfld, ArrayWrapperField);

            // Save the position of the branch instruction for later, when we have a reference to its target.
            processor.Emit(OpCodes.Ldarg_0);
            Instruction branchPosition = processor.Body.Instructions[processor.Body.Instructions.Count - 1];

            processor.Emit(OpCodes.Ldc_I4_1);
            processor.Emit(OpCodes.Ldsfld, nativePropertyField);
            InnerProperty.UnrealPropertyType.EmitDynamicArrayMarshalerDelegates(processor,helper);

            var constructor = (from method in ArrayWrapperType.Resolve().GetConstructors()
                               where (!method.IsStatic
                                      && method.HasParameters
                                      && method.Parameters.Count == 4
                                      && method.Parameters[0].ParameterType.FullName == "System.Int32"
                                      && method.Parameters[1].ParameterType.FullName == "System.IntPtr"
                                      && ((GenericInstanceType)method.Parameters[2].ParameterType).GetElementType().FullName == "UnrealEngine.Runtime.MarshalingDelegates`1/ToNative"
                                      && ((GenericInstanceType)method.Parameters[3].ParameterType).GetElementType().FullName == "UnrealEngine.Runtime.MarshalingDelegates`1/FromNative")
                               select method).ToArray()[0];
            processor.Emit(OpCodes.Newobj, Program.MakeMethodDeclaringTypeGeneric(helper.TargetAssembly.MainModule.ImportReference(constructor), ArrayWrapperTypeParameters));
            processor.Emit(OpCodes.Stfld, ArrayWrapperField);

            // Store the branch destination
            processor.Emit(OpCodes.Ldarg_0);
            Instruction branchTarget = processor.Body.Instructions[processor.Body.Instructions.Count - 1];
            processor.Emit(OpCodes.Ldfld, ArrayWrapperField);
            processor.Emit(OpCodes.Ldarg_0);
            processor.Emit(OpCodes.Call, helper.NativeObjectGetter);
            processor.Emit(OpCodes.Ldsfld, offsetField);
            processor.Emit(OpCodes.Call, helper.IntPtrAdd);
            processor.Emit(OpCodes.Ldc_I4_0);
            processor.Emit(OpCodes.Ldarg_0);
            processor.Emit(OpCodes.Callvirt, FromNative);
            //Now insert the branch
            Instruction branchInstruction = processor.Create(OpCodes.Brtrue_S, branchTarget);
            processor.InsertBefore(branchPosition, branchInstruction);

            EndSimpleGetter(processor, helper, getter);
        }

        protected override void EmitSetter(RewriteHelper helper, MethodDefinition setter, FieldDefinition offsetField, FieldDefinition nativePropertyField)
        {
            throw new NotSupportedException("Array property setters should be stripped, not rewritten.");
        }

        public override void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, VariableDefinition localVar)
        {
            WriteMarshalFromNative(processor, helper, GetLoadNativeBufferInstructions(processor, helper, loadBufferInstruction, offsetField), processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull));
            processor.Emit(OpCodes.Stloc, localVar);
        }
        public override void WriteLoad(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, FieldDefinition destField)
        {
            WriteMarshalFromNative(processor, helper, GetLoadNativeBufferInstructions(processor, helper, loadBufferInstruction, offsetField), processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull));
            processor.Emit(OpCodes.Stfld, destField);
        }

        public override IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, byte argIndex)
        {
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBufferInstruction, offsetField);
            return WriteMarshalToNativeWithCleanup(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull), new Instruction[] { processor.Create(OpCodes.Ldarg_S, argIndex) });
        }
        public override IList<Instruction> WriteStore(ILProcessor processor, RewriteHelper helper, Instruction loadBufferInstruction, FieldDefinition offsetField, FieldDefinition srcField)
        {
            Instruction[] loadBufferInstructions = GetLoadNativeBufferInstructions(processor, helper, loadBufferInstruction, offsetField);
            return WriteMarshalToNativeWithCleanup(processor, helper, loadBufferInstructions, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull), new Instruction[] { processor.Create(OpCodes.Ldfld, srcField) });
        }

        public override void WriteMarshalFromNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner)
        {
            processor.Body.Variables.Add(MarshalingLocal);

            processor.Emit(OpCodes.Ldc_I4_1);
            InnerProperty.UnrealPropertyType.EmitDynamicArrayMarshalerDelegates(processor, helper);
            processor.Emit(OpCodes.Ldsfld, ElementSizeField);
            processor.Emit(OpCodes.Newobj, CopyArrayWrapperCtor);
            processor.Emit(OpCodes.Stloc, MarshalingLocal);

            processor.Emit(OpCodes.Ldloc, MarshalingLocal);
            foreach (Instruction inst in loadBufferPtr)
            {
                processor.Body.Instructions.Add(inst);
            }
            processor.Body.Instructions.Add(loadArrayIndex);
            processor.Body.Instructions.Add(loadOwner);
            processor.Emit(OpCodes.Callvirt, CopyFromNative);
        }

        public override void WriteMarshalToNative(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction[] loadSource)
        {
            processor.Body.Variables.Add(MarshalingLocal);

            processor.Emit(OpCodes.Ldc_I4_1);
            InnerProperty.UnrealPropertyType.EmitDynamicArrayMarshalerDelegates(processor, helper);
            processor.Emit(OpCodes.Ldsfld, ElementSizeField);
            processor.Emit(OpCodes.Newobj, CopyArrayWrapperCtor);
            processor.Emit(OpCodes.Stloc, MarshalingLocal);

            processor.Emit(OpCodes.Ldloc, MarshalingLocal);
            foreach (var i in loadBufferPtr)
            {
                processor.Append(i);
            }
            processor.Append(loadArrayIndex);
            processor.Append(loadOwner);
            foreach( var i in loadSource)
            {
                processor.Append(i);
            }
            processor.Emit(OpCodes.Callvirt, CopyToNative);
        }
        public override IList<Instruction> WriteMarshalToNativeWithCleanup(ILProcessor processor, RewriteHelper helper, Instruction[] loadBufferPtr, Instruction loadArrayIndex, Instruction loadOwner, Instruction[] loadSource)
        {
             WriteMarshalToNative(processor, helper, loadBufferPtr, loadArrayIndex, loadOwner, loadSource);

             IList<Instruction> cleanupInstructions = new List<Instruction>();
             foreach (var i in loadBufferPtr)
             {
                 cleanupInstructions.Add(i);
             }
             cleanupInstructions.Add(processor.Create(OpCodes.Ldloc, MarshalingLocal));
             cleanupInstructions.Add(processor.Create(OpCodes.Ldc_I4_0));
             cleanupInstructions.Add(processor.Create(OpCodes.Call, CopyDestructInstance));
             return cleanupInstructions;
        }
    }
}