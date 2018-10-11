// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using Mono.Cecil;
using Mono.Cecil.Cil;
using Mono.Cecil.Rocks;

namespace MonoAssemblyProcess
{
    partial class Program
    {
        public static void VerifySingleResult<T>(T[] results, TypeDefinition type, string endMessage)
        {
            if (results.Length == 0)
            {
                throw new InternalRewriteException(type, String.Format("Could not find {0}", endMessage));
            }

            if (results.Length > 1)
            {
                throw new InternalRewriteException(type, String.Format("Found more than one {0}", endMessage));
            }
        }

        public static void VerifyNoResults<T>(T[] results, TypeDefinition type, string endMessage)
        {
            if (results.Length > 0)
            {
                throw new InternalRewriteException(type, String.Format("Found existing {0}", endMessage));
            }
        }

        static string StripBackingField(TypeDefinition type, PropertyMetadata prop)
        {
            string backingFieldName = String.Format("<{0}>k__BackingField", prop.Name);

            FieldDefinition[] matchingFields = (from field in type.Fields
                                                where field.Name == backingFieldName
                                                select field).ToArray();

            VerifySingleResult(matchingFields, type, "backing field for property " + prop.Name);

            type.Fields.Remove(matchingFields[0]);
            return matchingFields[0].Name;
        }

        static FieldDefinition AddOffsetField(TypeDefinition type, PropertyMetadata prop, TypeReference int32TypeRef)
        {
            var field = new FieldDefinition(prop.Name + "_Offset", FieldAttributes.InitOnly | FieldAttributes.Static | FieldAttributes.Private, int32TypeRef);
            type.Fields.Add(field);
            return field;
        }

        static FieldDefinition AddOffsetField(TypeDefinition type, FunctionMetadata func, PropertyMetadata prop, TypeReference int32TypeRef)
        {
            var field = new FieldDefinition(func.Name + "_" + prop.Name + "_Offset", FieldAttributes.InitOnly | FieldAttributes.Static | FieldAttributes.Private, int32TypeRef);
            type.Fields.Add(field);
            return field;
        }

        static FieldDefinition AddNativePropertyField(TypeDefinition type, PropertyMetadata prop, TypeReference intPtrTypeRef)
        {
            if(!prop.UnrealPropertyType.NeedsNativePropertyField)
            {
                return null;
            }
            var field = new FieldDefinition(prop.Name + "_NativeProperty", FieldAttributes.InitOnly | FieldAttributes.Static | FieldAttributes.Private, intPtrTypeRef);
            type.Fields.Add(field);
            return field;
        }

        static FieldDefinition AddElementSizeField(TypeDefinition type, FunctionMetadata func, PropertyMetadata prop, TypeReference int32TypeRef)
        {
            if (!prop.UnrealPropertyType.NeedsElementSizeField)
            {
                return null;
            }
            var field = new FieldDefinition(func.Name + "_" + prop.Name + "_ElementSize", FieldAttributes.InitOnly | FieldAttributes.Static | FieldAttributes.Private, int32TypeRef);
            type.Fields.Add(field);
            return field;
        }

        static FieldDefinition AddRepIndexField(TypeDefinition type, PropertyMetadata prop, TypeReference uint16TypeRef)
        {
            var field = new FieldDefinition(prop.Name + "_RepIndex", FieldAttributes.InitOnly | FieldAttributes.Static | FieldAttributes.Private, uint16TypeRef);
            type.Fields.Add(field);
            return field;
        }

        static FieldDefinition AddNativeFunctionField(TypeDefinition type, FunctionMetadata func, TypeReference intPtrTypeRef, bool staticField)
        {
            FieldAttributes attributes = FieldAttributes.Private;
            if (staticField)
            {
                attributes |= FieldAttributes.InitOnly | FieldAttributes.Static;
            }

            var field = new FieldDefinition(func.Name + "_NativeFunction", attributes, intPtrTypeRef);
            type.Fields.Add(field);
            return field;
        }

        static FieldDefinition AddParamsSizeField(TypeDefinition type, FunctionMetadata func, TypeReference int32TypeRef)
        {
            var field = new FieldDefinition(func.Name + "_ParamsSize", FieldAttributes.InitOnly | FieldAttributes.Static | FieldAttributes.Private, int32TypeRef);
            type.Fields.Add(field);
            return field;
        }

        public static MethodDefinition FindMethod(TypeDefinition type, string methodName, string returnTypeName, string[] parameterTypes = null)
        {
            if (parameterTypes == null)
            {
                parameterTypes = new string[] { };
            }

            var methods = (from method in type.Methods
                           where method.Name == methodName
                                 && method.ReturnType.FullName == returnTypeName
                                 && (method.Parameters.Count == parameterTypes.Length)
                           select method).ToArray();

            foreach (var method in methods)
            {
                bool bParametersMatch = true;
                for (int i = 0; i < method.Parameters.Count; ++i)
                {
                    if (method.Parameters[i].ParameterType.FullName != parameterTypes[i])
                    {
                        bParametersMatch = false;
                    }
                }

                if (bParametersMatch)
                {
                    return method;
                }
            }

            return null;
        }

        public static MethodReference FindStaticMethod(AssemblyDefinition assembly, AssemblyDefinition bindingsAssembly, string findNamespace, string findClass, string findMethod)
        {
            var types = from module in bindingsAssembly.Modules
                        from type in ModuleDefinitionRocks.GetAllTypes(module)
                        where (type.IsClass && type.Namespace == findNamespace && type.Name == findClass)
                        select type;

            var methods = (from type in types
                          from method in type.Methods
                          where (method.IsStatic && method.Name == findMethod)
                          select method).ToArray();

            return assembly.MainModule.ImportReference(methods[0]);
        }

        public static FieldReference FindStaticField(AssemblyDefinition assembly, TypeDefinition type, string findField)
        {
            var fields = (from field in type.Fields
                          where (field.IsStatic && field.Name == findField)
                          select field).ToArray();

            return assembly.MainModule.ImportReference(fields[0]);
        }
        
        public static MethodReference MakeMethodGeneric(MethodReference method, params TypeReference[] args)
        {
            if (args.Length == 0)
                return method;

            if (method.GenericParameters.Count != args.Length)
                throw new ArgumentException("Invalid number of generic type arguments supplied");

            var genericMethodRef = new GenericInstanceMethod(method);
            foreach (var arg in args)
                genericMethodRef.GenericArguments.Add(arg);

            return genericMethodRef;
        }

        public static MethodReference MakeMethodDeclaringTypeGeneric(MethodReference method, params TypeReference[] args)
        {
            if (args.Length == 0)
                return method;

            if (method.DeclaringType.GenericParameters.Count != args.Length)
                throw new ArgumentException("Invalid number of generic type arguments supplied");

            var genericTypeRef = method.DeclaringType.MakeGenericInstanceType(args);

            var newMethodRef = new MethodReference(
                method.Name,
                method.ReturnType,
                genericTypeRef)
            {
                HasThis = method.HasThis,
                ExplicitThis = method.ExplicitThis,
                CallingConvention = method.CallingConvention
            };

            foreach (var parameter in method.Parameters)
            {
                newMethodRef.Parameters.Add(new ParameterDefinition(parameter.ParameterType));
            }

            foreach (var genericParam in method.GenericParameters)
            {
                newMethodRef.GenericParameters.Add(new GenericParameter(genericParam.Name, newMethodRef));
            }

            return newMethodRef;
        }

        static void HookStaticConstructor(RewriteHelper helper, 
                                          List<Tuple<FieldDefinition, PropertyMetadata>> propertyOffsetsToInitialize, 
                                          List<Tuple<FieldDefinition, PropertyMetadata>> propertyRepIndexesToInitialize,
                                          List<Tuple<FieldDefinition, PropertyMetadata>> propertyPointersToInitialize,
                                          List<Tuple<FunctionMetadata, List<Tuple<FieldDefinition, PropertyMetadata>>>> functionParamOffsetsToInitialize,
                                          List<Tuple<FunctionMetadata, List<Tuple<FieldDefinition, PropertyMetadata>>>> functionParamElementSizesToInitialize, 
                                          Dictionary<FunctionMetadata, FieldDefinition> functionPointersToInitialize,
                                          List<Tuple<FunctionMetadata, FieldDefinition>> functionParamSizesToInitialize, 
                                          FieldDefinition nativeClassField,
                                          FieldDefinition nativeDataSizeField)
        {
            MethodDefinition cctor = TypeDefinitionRocks.GetStaticConstructor(helper.TargetType);
            ILProcessor processor;

            if (null == cctor)
            {
                cctor = new MethodDefinition(".cctor", MethodAttributes.SpecialName | MethodAttributes.RTSpecialName | MethodAttributes.HideBySig | MethodAttributes.Static, helper.TargetAssembly.MainModule.ImportReference(typeof(void)));
                helper.TargetType.Methods.Add(cctor);

                processor = cctor.Body.GetILProcessor();
                processor.Emit(OpCodes.Ret);
            }
            else
            {
                processor = cctor.Body.GetILProcessor();
            }

            Instruction target = cctor.Body.Instructions[0];

            /*
            IL_0000:  ldstr      "MonoTestsObject"
             IL_0005:  call       native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealInterop::GetNativeClassFromName(string)
            */
            processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, helper.TargetType.Name));

            MethodReference nativeTypeFromNameMethod = (helper.TargetType.IsValueType ? helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "GetNativeStructFromName")
                                                                                      : helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "GetNativeClassFromName"));
            MethodReference propertyOffsetFromNameMethod = helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "GetPropertyOffsetFromName");
            MethodReference arrayElementSizeMethod = helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "GetArrayElementSize");
            MethodReference propertyRepIndexFromNameMethod = helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "GetPropertyRepIndexFromName");
            MethodReference nativePropertyFromNameMethod = helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "GetNativePropertyFromName");
            MethodReference nativeFunctionFromNameMethod = helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealObject", "GetNativeFunctionFromClassAndName");
            MethodReference nativeFunctionParamsSizeMethod = helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealObject", "GetNativeFunctionParamsSize");

            processor.InsertBefore(target, processor.Create(OpCodes.Call, nativeTypeFromNameMethod));
            Instruction loadNativeClassPtr = null;
            if (nativeClassField != null)
            {
                processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, nativeClassField));
                loadNativeClassPtr = processor.Create(OpCodes.Ldsfld, nativeClassField);
            }
            else
            {
                VariableDefinition nativeClassVar = new VariableDefinition(helper.IntPtrType);
                cctor.Body.Variables.Add(nativeClassVar);
                processor.InsertBefore(target, processor.Create(OpCodes.Stloc, nativeClassVar));
                loadNativeClassPtr = processor.Create(OpCodes.Ldloc, nativeClassVar);
            }

            foreach (var offset in propertyOffsetsToInitialize)
            {
                /*
                IL_0037:  ldsfld     native int UnrealEngine.MonoRuntime.MonoTestsObject::NativeClassPtr
                  IL_003c:  ldstr      "TestObjectArray"
                  IL_0041:  call       int32 [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealInterop::GetPropertyOffsetFromName(native int,
                                                                                                                                 string)
                  IL_0046:  stsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestObjectArray_Offset
                 */
                processor.InsertBefore(target, loadNativeClassPtr);
                processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, offset.Item2.Name));
                processor.InsertBefore(target, processor.Create(OpCodes.Call, propertyOffsetFromNameMethod));
                processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, offset.Item1));
            }

            if (propertyRepIndexesToInitialize != null)
            {
                foreach (var repIndex in propertyRepIndexesToInitialize)
                {
                    processor.InsertBefore(target, loadNativeClassPtr);
                    processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, repIndex.Item2.Name));
                    processor.InsertBefore(target, processor.Create(OpCodes.Call, propertyRepIndexFromNameMethod));
                    processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, repIndex.Item1));
                }
            }
            
            foreach (var nativeProp in propertyPointersToInitialize)
            {
                /*
                  IL_004b:  ldsfld     native int UnrealEngine.MonoRuntime.MonoTestsObject::NativeClassPtr
                  IL_0050:  ldstr      "TestObjectArray"
                  IL_0055:  call       native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealInterop::GetNativePropertyFromName(native int,
                                                                                                                                      string)
                  IL_005a:  stsfld     native int UnrealEngine.MonoRuntime.MonoTestsObject::TestObjectArray_NativeProperty
                */
                processor.InsertBefore(target, loadNativeClassPtr);
                processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, nativeProp.Item2.Name));
                processor.InsertBefore(target, processor.Create(OpCodes.Call, nativePropertyFromNameMethod));
                processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, nativeProp.Item1));
            }

            if (functionPointersToInitialize != null)
            {
                foreach (var nativeFunc in functionPointersToInitialize)
                {
                    processor.InsertBefore(target, loadNativeClassPtr);
                    processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, nativeFunc.Key.Name));
                    processor.InsertBefore(target, processor.Create(OpCodes.Call, nativeFunctionFromNameMethod));
                    processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, nativeFunc.Value));
                }
            }

            if (functionParamSizesToInitialize != null)
            {
                foreach (var paramsSize in functionParamSizesToInitialize)
                {
                    /*
                      IL_041a:  ldsfld     native int UnrealEngine.MonoRuntime.MonoTestsObject::TestOutParams_NativeFunction
                      IL_041f:  call       int16 [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::GetNativeFunctionParamsSize(native int)
                      IL_0424:  stsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestOutParams_ParamsSize
                     */
                    FunctionMetadata func = paramsSize.Item1;
                    FieldDefinition paramsSizeField = paramsSize.Item2;

                    if (functionPointersToInitialize.ContainsKey(func))
                    {
                        FieldDefinition nativeFunc = functionPointersToInitialize[func];
                        processor.InsertBefore(target, processor.Create(OpCodes.Ldsfld, nativeFunc));
                    }
                    else
                    {
                        processor.InsertBefore(target, loadNativeClassPtr);
                        processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, func.Name));
                        processor.InsertBefore(target, processor.Create(OpCodes.Call, nativeFunctionFromNameMethod));
                    }
                    processor.InsertBefore(target, processor.Create(OpCodes.Call, nativeFunctionParamsSizeMethod));
                    processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, paramsSizeField));
                }
            }

            if (functionParamOffsetsToInitialize != null)
            {
                foreach (var pair in functionParamOffsetsToInitialize)
                {
                    FunctionMetadata func = pair.Item1;
                    var paramOffsets = pair.Item2;

                    /*
                      IL_005b:  ldsfld     native int UnrealEngine.MonoRuntime.MonoTestsObject::NativeClassPtr
                      IL_005c:  ldstr      "TestOverridableFloatReturn"
                      IL_0061:  call       native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::GetNativeFunctionFromName(native int,
                                                                                                                                         string)
                      IL_0066:  stloc.0
                     */

                    Instruction loadNativeFunction = null;
                    if (functionPointersToInitialize.ContainsKey(func))
                    {
                        FieldDefinition nativeFunctionField = functionPointersToInitialize[func];
                        loadNativeFunction = processor.Create(OpCodes.Ldsfld, nativeFunctionField);
                    }
                    else
                    {
                        VariableDefinition nativeFunctionPointer = new VariableDefinition(helper.IntPtrType);
                        int varNum = processor.Body.Variables.Count;
                        processor.Body.Variables.Add(nativeFunctionPointer);

                        processor.InsertBefore(target, loadNativeClassPtr);
                        processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, func.Name));
                        processor.InsertBefore(target, processor.Create(OpCodes.Call, nativeFunctionFromNameMethod));
                        processor.InsertBefore(target, processor.Create(OpCodes.Stloc, varNum));

                        loadNativeFunction = processor.Create(OpCodes.Ldloc, varNum);
                    }

                    foreach (var paramPair in paramOffsets)
                    {
                        FieldDefinition offsetField = paramPair.Item1;
                        PropertyMetadata param = paramPair.Item2;

                        /*
                          IL_0067:  ldloc.0
                          IL_0068:  ldstr      "X"
                          IL_006d:  call       int32 [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealInterop::GetPropertyOffsetFromName(native int,
                                                                                                                                         string)
                          IL_0072:  stsfld     int32 UnrealEngine.MonoRuntime.MonoTestUserObjectBase::TestOverridableFloatReturn_X_Offset
                         */
                        processor.InsertBefore(target, loadNativeFunction);
                        processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, param.Name));
                        processor.InsertBefore(target, processor.Create(OpCodes.Call, propertyOffsetFromNameMethod));
                        processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, offsetField));
                    }
                }
            }

            if (functionParamElementSizesToInitialize != null)
            {
                foreach (var pair in functionParamElementSizesToInitialize)
                {
                    FunctionMetadata func = pair.Item1;
                    var paramElementSizes = pair.Item2;

                    /*
                      IL_005b:  ldsfld     native int UnrealEngine.MonoRuntime.MonoTestsObject::NativeClassPtr
                      IL_005c:  ldstr      "TestOverridableFloatReturn"
                      IL_0061:  call       native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::GetNativeFunctionFromName(native int,
                                                                                                                                         string)
                      IL_0066:  stloc.0
                     */

                    Instruction loadNativeFunction = null;
                    if (functionPointersToInitialize.ContainsKey(func))
                    {
                        FieldDefinition nativeFunctionField = functionPointersToInitialize[func];
                        loadNativeFunction = processor.Create(OpCodes.Ldsfld, nativeFunctionField);
                    }
                    else
                    {
                        VariableDefinition nativeFunctionPointer = new VariableDefinition(helper.IntPtrType);
                        int varNum = processor.Body.Variables.Count;
                        processor.Body.Variables.Add(nativeFunctionPointer);

                        processor.InsertBefore(target, loadNativeClassPtr);
                        processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, func.Name));
                        processor.InsertBefore(target, processor.Create(OpCodes.Call, nativeFunctionFromNameMethod));
                        processor.InsertBefore(target, processor.Create(OpCodes.Stloc, varNum));

                        loadNativeFunction = processor.Create(OpCodes.Ldloc, varNum);
                    }

                    foreach (var paramPair in paramElementSizes)
                    {
                        FieldDefinition elementSizeField = paramPair.Item1;
                        PropertyMetadata param = paramPair.Item2;

                        /*
                          IL_0067:  ldloc.0
                          IL_0068:  ldstr      "X"
                          IL_006d:  call       int32 [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealInterop::GetPropertyOffsetFromName(native int,
                                                                                                                                         string)
                          IL_0072:  stsfld     int32 UnrealEngine.MonoRuntime.MonoTestUserObjectBase::TestOverridableFloatReturn_X_Offset
                         */
                        processor.InsertBefore(target, loadNativeFunction);
                        processor.InsertBefore(target, processor.Create(OpCodes.Ldstr, param.Name));
                        processor.InsertBefore(target, processor.Create(OpCodes.Call, arrayElementSizeMethod));
                        processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, elementSizeField));
                    }
                }
            }


            if (nativeDataSizeField != null)
            {
                MethodReference nativeDataSizeMethod = helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "GetNativeStructSize");
                processor.InsertBefore(target, loadNativeClassPtr);
                processor.InsertBefore(target, processor.Create(OpCodes.Call, nativeDataSizeMethod));
                processor.InsertBefore(target, processor.Create(OpCodes.Stsfld, nativeDataSizeField));
            }

            if (cctor.Body.Variables.Count > 0)
            {
                cctor.Body.InitLocals = true;

                // Ensure we're using the stloc.0/ldloc.0 form wherever possible, even though we emitted "stloc <varname>"
                cctor.Body.SimplifyMacros();
                cctor.Body.OptimizeMacros();
            }
        }

        private static void WriteFunctionInvoker(RewriteHelper helper, TypeDefinition type, FunctionMetadata func, MethodDefinition methodToCall, List<Tuple<FieldDefinition, PropertyMetadata>> paramOffsetFields)
        {
            string invokerName = "Invoke_" + func.Name;
            MethodDefinition[] invokerDefinitions = (from methodDef in type.Methods
                                                where methodDef.Name == invokerName
                                                select methodDef).ToArray();
            VerifyNoResults(invokerDefinitions, type, invokerName);

            MethodDefinition invoker = new MethodDefinition(invokerName, MethodAttributes.Private, helper.TargetAssembly.MainModule.ImportReference(typeof(void)));

            TypeReference intPtrType = helper.IntPtrType;
            invoker.Parameters.Add(new ParameterDefinition("buffer", ParameterAttributes.None, intPtrType));
            invoker.Parameters.Add(new ParameterDefinition("returnBuffer", ParameterAttributes.None, intPtrType));
            
            helper.TargetType.Methods.Add(invoker);

            ILProcessor processor = invoker.Body.GetILProcessor();

            int paramIndex = 0;
            VariableDefinition[] paramVariables = new VariableDefinition[func.ParamProperties.Length];
            Instruction loadBuffer = processor.Create(OpCodes.Ldarg_1);
            foreach (var param in func.ParamProperties)
            {

                paramVariables[paramIndex] = new VariableDefinition(helper.TargetAssembly.MainModule.ImportReference(param.UnrealPropertyType.CSharpType));
                processor.Body.Variables.Add(paramVariables[paramIndex]);
                param.UnrealPropertyType.PrepareForRewrite(helper, func, param);

                //incoming out params will have junk values, don't try to marshal them
                if (!param.OutParam)
                {
                    param.UnrealPropertyType.WriteLoad(processor, helper, loadBuffer, paramOffsetFields[paramIndex].Item1, paramVariables[paramIndex]);
                }

                paramIndex++;
            }

            /*
                IL_0024:  ldarg.0
                IL_0025:  ldloc.0
                IL_0026:  ldloc.1
                IL_0027:  callvirt   instance float32 UnrealEngine.MonoRuntime.MonoTestUserObjectBase::TestOverridableFloatReturn(float32,
                                                                                                                                float32)
             */

            OpCode callOp = OpCodes.Callvirt;;
            if (methodToCall.IsStatic)
            {
                callOp = OpCodes.Call;
            }
            else
            {
                processor.Emit(OpCodes.Ldarg_0);
                if (methodToCall.IsVirtual)
                {
                    callOp = OpCodes.Call;
                }
            }

            for (int i = 0; i < paramVariables.Length; ++i)
            {
                VariableDefinition local = paramVariables[i];
                PropertyMetadata param = func.ParamProperties[i];
                OpCode loadCode = (param.ReferenceParam ? OpCodes.Ldloca : OpCodes.Ldloc);
                processor.Emit(loadCode, local);
            }
            int returnIndex = 0;
            if (func.ReturnValueProperty != null)
            {
                invoker.Body.Variables.Add(new VariableDefinition(helper.TargetAssembly.MainModule.ImportReference(func.ReturnValueProperty.UnrealPropertyType.CSharpType)));
                returnIndex = invoker.Body.Variables.Count - 1;
            }

            processor.Emit(callOp, helper.TargetAssembly.MainModule.ImportReference(methodToCall));

            // Marshal out params back to the native parameter buffer.
            for (int i = 0; i < paramVariables.Length; ++i)
            {
                VariableDefinition local = paramVariables[i];
                PropertyMetadata param = func.ParamProperties[i];
                FieldDefinition offsetField = paramOffsetFields[i].Item1;
                UnrealType unrealParamType = param.UnrealPropertyType;
                if (param.ReferenceParam)
                {
                    Instruction loadLocal = processor.Create(OpCodes.Ldloc, local);

                    unrealParamType.PrepareForRewrite(helper, func, param);
                    Instruction[] loadBufferPtr = UnrealType.GetLoadNativeBufferInstructions(processor, helper, loadBuffer, offsetField);
                    unrealParamType.WriteMarshalToNative(processor, helper, loadBufferPtr, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull), loadLocal);
                }
            }
            if (func.ReturnValueProperty != null)
            {
                UnrealType unrealReturnType = func.ReturnValueProperty.UnrealPropertyType;
                /*
                    IL_002c:  stloc.2
                    IL_002d:  ldarg.2
                    IL_0034:  call       void* [mscorlib]System.IntPtr::op_Explicit(native int)
                    IL_0039:  ldloc.2
                    IL_003a:  stind.r4
                 */
                processor.Emit(OpCodes.Stloc, returnIndex);
                
                
                Instruction loadReturnProperty = processor.Create(OpCodes.Ldloc, returnIndex);

                unrealReturnType.PrepareForRewrite(helper, func, func.ReturnValueProperty);
                Instruction[] loadBufferPtr = new Instruction [] { processor.Create(OpCodes.Ldarg_2) };
                unrealReturnType.WriteMarshalToNative(processor, helper, loadBufferPtr, processor.Create(OpCodes.Ldc_I4_0), processor.Create(OpCodes.Ldnull), loadReturnProperty);
            }

            processor.Emit(OpCodes.Ret);

            if (invoker.Body.Variables.Count > 0)
            {
                processor.Body.InitLocals = true;

                // Ensure we're using the stloc.0/ldloc.0 form wherever possible, even though we emitted "stloc <varname>"
                invoker.Body.SimplifyMacros();
                invoker.Body.OptimizeMacros();
            }
        }

        private static void RewriteMethodAsUFunctionInvoke(RewriteHelper helper, TypeDefinition type, FunctionMetadata func, FieldDefinition nativeFunctionField, FieldDefinition paramsSizeField, List<Tuple<FieldDefinition, PropertyMetadata>> paramOffsetFields)
        {
            MethodDefinition originalMethodDef = (from method in type.Methods
                                             where method.Name == func.Name
                                             select method).ToArray()[0];

            // Move the existing method body to a new method with an _Implementation suffix.  We're going to rewrite 
            // the body of the original method so that existing managed call sites will instead invoke the UFunction.
            MethodDefinition implementationMethodDef = new MethodDefinition(originalMethodDef.Name + "_Implementation", originalMethodDef.Attributes, originalMethodDef.ReturnType);
            foreach (ParameterDefinition param in originalMethodDef.Parameters)
            {
                implementationMethodDef.Parameters.Add(new ParameterDefinition(param.Name, param.Attributes, type.Module.ImportReference(param.ParameterType)));
            }
            implementationMethodDef.Body = originalMethodDef.Body;
            type.Methods.Add(implementationMethodDef);

            if (func.NeedsValidation)
            {
                // Inject a validation call at the beginning of the implementation method.
                // This is slightly different than how UHT handles things, which is to call both the validation
                // and implementation method from the generated invoker, but this simplifies the IL generation
                // and doesn't affect the relative timing of the calls.
                InjectRPCValidation(helper, type, func, implementationMethodDef);
            }

            // Replace the original method's body with one that pinvokes to call the UFunction on the native side.
            // For RPCs, this will allow the engine's ProcessEvent logic to route the call to the correct client or server.
            // For BlueprintImplementableEvents, it will call the correct overriden version of the UFunction.
            WriteNativeFunctionInvoker(helper, func, originalMethodDef, nativeFunctionField, paramsSizeField, paramOffsetFields);

            // Create a managed invoker, named to match the UFunction but which calls our the generated
            // _Implementation method with the user's original method body.  This is how the engine will actually invoke
            // the UFunction, when this class's version of it does need to run.
            WriteFunctionInvoker(helper, type, func, implementationMethodDef, paramOffsetFields);
        }

        private static void WriteNativeFunctionInvoker(RewriteHelper helper, FunctionMetadata metadata, MethodDefinition methodDef, FieldDefinition nativeFunctionField, FieldDefinition paramsSizeField, List<Tuple<FieldDefinition, PropertyMetadata>> paramOffsetFields)
        {
            methodDef.Body = new MethodBody(methodDef);

            bool staticNativeFunction = nativeFunctionField.IsStatic;

            /*
              .locals init ([0] uint8* ParamsBufferAllocation,
                       [1] native int ParamsBuffer,
                       [2] bool toReturn)
              IL_0000:  ldsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestBoolReturn_ParamsSize
              IL_0005:  conv.u
              IL_0006:  localloc
              IL_0008:  stloc.0
              IL_0009:  ldloca.s   ParamsBuffer
              IL_000b:  ldloc.0
              IL_000c:  call       instance void [mscorlib]System.IntPtr::.ctor(void*)
             */
            TypeReference byteType = helper.TargetAssembly.MainModule.TypeSystem.Byte;
            VariableDefinition bufferAllocation = new VariableDefinition(byteType);
            methodDef.Body.Variables.Add(bufferAllocation);
            VariableDefinition paramsBuffer = new VariableDefinition(helper.IntPtrType);
            methodDef.Body.Variables.Add(paramsBuffer);
            MethodReference IntPtrConstructor = helper.TargetAssembly.MainModule.ImportReference(typeof(System.IntPtr).GetConstructor(new Type[] { typeof(void*) }));

            ILProcessor processor = methodDef.Body.GetILProcessor();
            processor.Emit(OpCodes.Ldsfld, paramsSizeField);
            processor.Emit(OpCodes.Conv_U);
            processor.Emit(OpCodes.Localloc);
            processor.Emit(OpCodes.Stloc, bufferAllocation);
            processor.Emit(OpCodes.Ldloca, paramsBuffer);
            processor.Emit(OpCodes.Ldloc, bufferAllocation);
            processor.Emit(OpCodes.Call, IntPtrConstructor);

            List<Instruction> allCleanupInstructions = new List<Instruction>();
            Instruction loadParamBufferInstruction = processor.Create(OpCodes.Ldloc, paramsBuffer);
            for (byte i = 0; i < paramOffsetFields.Count; ++i)
            {
                var offsetField = paramOffsetFields[i].Item1;
                var unrealParamType = paramOffsetFields[i].Item2.UnrealPropertyType;
                unrealParamType.PrepareForRewrite(helper, metadata, paramOffsetFields[i].Item2);
                var cleanupInstructions = unrealParamType.WriteStore(processor, helper, loadParamBufferInstruction, offsetField, (byte)(i + 1));
                if (cleanupInstructions != null)
                {
                    allCleanupInstructions.AddRange(cleanupInstructions);
                }
            }

            /*
              IL_0035:  ldarg.0
              IL_0036:  call       instance native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::get_NativeObject()
              IL_003b:  ldsfld     native int UnrealEngine.MonoRuntime.MonoTestsObject::TestBoolReturn_NativeFunction
              IL_0040:  ldloc.1
              IL_0041:  ldsfld     int32 UnrealEngine.MonoRuntime.MonoTestsObject::TestBoolReturn_ParamsSize
              IL_0046:  call       void [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::InvokeFunction(native int,
                                                                                                                native int,
                                                                                                                native int,
                                                                                                                int32)
             */
            processor.Emit(OpCodes.Ldarg_0);
            processor.Emit(OpCodes.Call, helper.NativeObjectGetter);
            if (staticNativeFunction)
            {
                processor.Emit(OpCodes.Ldsfld, nativeFunctionField);
            }
            else
            {
                processor.Emit(OpCodes.Ldarg_0);
                processor.Emit(OpCodes.Ldfld, nativeFunctionField);
            }
            processor.Emit(OpCodes.Ldloc, paramsBuffer);
            processor.Emit(OpCodes.Ldsfld, paramsSizeField);
            processor.Emit(OpCodes.Call, helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealObject", "InvokeFunction"));

            foreach(Instruction instruction in allCleanupInstructions)
            {
                processor.Append(instruction);
            }

            if (methodDef.ReturnType.FullName != "System.Void")
            {
                throw new InvalidUnrealFunctionException(methodDef, "Return values not yet supported for IL rewriting.");
            }

            processor.Emit(OpCodes.Ret);

            if (!staticNativeFunction)
            {
                // Insert Lazy init of the native function field at the beginning of the method, so it will reference the correct 
                // version of the UFunction for this instance even if its actual type is a derived Blueprint class.
                /*
                  IL_0000:  ldarg.0
                  IL_0001:  ldfld      native int UnrealEngine.Engine.Actor::ReceiveTick_NativeFunction
                  IL_0006:  ldsfld     native int [mscorlib]System.IntPtr::Zero
                  IL_000b:  call       bool [mscorlib]System.IntPtr::op_Equality(native int,
                                                                                 native int)
                  IL_0010:  brfalse.s  IL_0028
                  IL_0012:  ldarg.0
                  IL_0013:  ldarg.0
                  IL_0014:  call       instance native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::get_NativeObject()
                  IL_0019:  ldstr      "ReceiveTick"
                  IL_001e:  call       native int [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealObject::GetNativeFunctionFromInstanceAndName(native int,
                                                                                                                                                string)
                  IL_0023:  stfld      native int UnrealEngine.Engine.Actor::ReceiveTick_NativeFunction
                 */
                Instruction branchTarget = processor.Body.Instructions[0];
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ldarg_0));
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ldfld, nativeFunctionField));
                FieldReference intPtrZero = helper.FindFieldInType(helper.IntPtrType.Resolve(), "Zero");
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ldsfld, intPtrZero));
                MethodReference intPtrEquals = helper.TargetAssembly.MainModule.ImportReference(
                    FindMethod(helper.IntPtrType.Resolve(), "op_Equality", "System.Boolean", new string[] { "System.IntPtr", "System.IntPtr" }));
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Call, intPtrEquals));
                Instruction branchPosition = processor.Create(OpCodes.Ldarg_0);
                processor.InsertBefore(branchTarget, branchPosition);
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ldarg_0));
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Call, helper.NativeObjectGetter));
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ldstr, methodDef.Name));
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Call, helper.GetNativeFunctionFromInstanceAndNameMethod));
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Stfld, nativeFunctionField));
                processor.InsertBefore(branchPosition, processor.Create(OpCodes.Brfalse, branchTarget));
            }

        }

        private static void InjectRPCValidation(RewriteHelper helper, TypeDefinition type, FunctionMetadata func, MethodDefinition methodDef)
        {
            string validationMethodName = func.Name + "_Validate";
            MethodDefinition[] validationMethods = (from method in type.Methods
                                                    where method.Name == validationMethodName
                                                    select method).ToArray();
            VerifySingleResult(validationMethods, type, "required validation method " + validationMethodName);

            /*
              IL_0000:  ldarg.0
              IL_0001:  ldarg.1
              IL_0002:  ldarg.2
              IL_0003:  call       instance bool ShooterGameMono.MonoShooterCharacter::ServerSetRunning_Validate(bool,
                                                                                                                 bool)
              IL_0008:  brtrue.s   IL_0015
              IL_000a:  ldstr      "ServerSetRunning_Validate"
              IL_000f:  call       void [UnrealEngine.Runtime]UnrealEngine.Runtime.UnrealInterop::RPCValidateFailed(string)
              IL_0014:  ret
             */

            ILProcessor processor = methodDef.Body.GetILProcessor();
            Instruction branchTarget = methodDef.Body.Instructions[0];

            processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ldarg_0));

            for (int i = 0; i < methodDef.Parameters.Count;  ++i)
            {
                processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ldarg, i + 1));
            }
            processor.InsertBefore(branchTarget, processor.Create(OpCodes.Call, validationMethods[0]));
            processor.InsertBefore(branchTarget, processor.Create(OpCodes.Brtrue_S, branchTarget));

            MethodReference rpcValidateFailedMethod = helper.FindBindingsStaticMethod(Program.BindingsNamespace, "UnrealInterop", "RPCValidateFailed");
            processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ldstr, validationMethods[0].Name));
            processor.InsertBefore(branchTarget, processor.Create(OpCodes.Call, rpcValidateFailedMethod));
            processor.InsertBefore(branchTarget, processor.Create(OpCodes.Ret));

            methodDef.Body.OptimizeMacros();
        }

        public const string ArrayMarshalerSuffix = "Marshaler";

        static void RewriteUnrealStruct(AssemblyMetadata assemblyMetadata, AssemblyDefinition assembly, TypeDefinition type, StructMetadata metadata, AssemblyDefinition bindingsAssembly)
        {
            RewriteHelper helper = new RewriteHelper(assemblyMetadata, assembly, bindingsAssembly, type);

            foreach (var prop in metadata.Properties)
            {
                prop.UnrealPropertyType.PrepareForRewrite(helper, null, prop);
            }

            metadata.CreateStructHash();

            if (!metadata.IsBlittable)
            {
                var constructors = (from ctor in type.GetConstructors()
                                    where ctor.Parameters.Count == 1 
                                          && ctor.Parameters[0].ParameterType.FullName == "System.IntPtr"
                                    select ctor).ToArray();
                VerifyNoResults(constructors, type, "marshaling constructor");

                var methods = (from method in type.Methods
                               where method.Parameters.Count == 1 
                                     && method.Parameters[0].ParameterType.FullName == "System.IntPtr"
                               select method).ToArray();
                VerifyNoResults(methods, type, "ToNative() method");

                string arrayMarshalerName = type.Name + ArrayMarshalerSuffix;
                var classes = (from klass in assembly.MainModule.Types
                               where klass.IsClass && klass.Namespace == type.Namespace
                                     && klass.Name == arrayMarshalerName
                               select klass).ToArray();
                VerifyNoResults(classes, type, "marshaler");

                AddStructMarshaling(helper, type, metadata);
            }
        }

        static MethodDefinition CreateMethod(TypeDefinition declaringType, string name, MethodAttributes attributes, TypeReference returnType = null, TypeReference[] parameters = null)
        {
            if (returnType == null)
            {
                returnType = declaringType.Module.TypeSystem.Void;
            }

            MethodDefinition def = new MethodDefinition(name, attributes, returnType);
            if (parameters != null)
            {
                foreach (var type in parameters)
                {
                    def.Parameters.Add(new ParameterDefinition(type));
                }
            }

            declaringType.Methods.Add(def);

            return def;
        }

        static void FinalizeMethod(MethodDefinition method)
        {
            if (method.Body.Variables.Count > 0)
            {
                method.Body.InitLocals = true;
            }

            method.Body.SimplifyMacros();
            method.Body.OptimizeMacros();
        }

        static void AddStructMarshaling(RewriteHelper helper, TypeDefinition type, StructMetadata metadata)
        {
            var ctorAttributes = MethodAttributes.Public | MethodAttributes.HideBySig | MethodAttributes.SpecialName | MethodAttributes.RTSpecialName;
            var ctor = CreateMethod(type, ".ctor", ctorAttributes, null, new TypeReference[] { helper.IntPtrType });

            var toNativeMethod = CreateMethod(type, "ToNative", MethodAttributes.Public, null, new TypeReference[] { helper.IntPtrType });

            var propertyOffsetsToInitialize = new List<Tuple<FieldDefinition, PropertyMetadata>>();
            var propertyPointersToInitialize = new List<Tuple<FieldDefinition, PropertyMetadata>>();

            TypeReference int32TypeRef = helper.TargetAssembly.MainModule.ImportReference(typeof(System.Int32));
            TypeReference intPtrTypeRef = helper.IntPtrType;

            ILProcessor ctorBody = ctor.Body.GetILProcessor();
            ILProcessor toNativeBody = toNativeMethod.Body.GetILProcessor();
            Instruction loadBufferInstruction = ctorBody.Create(OpCodes.Ldarg_1);
            foreach (var prop in metadata.Properties)
            {
                FieldDefinition offsetField = AddOffsetField(type, prop, int32TypeRef);
                FieldDefinition nativePropertyField = AddNativePropertyField(type, prop, intPtrTypeRef);

                // find the property
                FieldDefinition[] definitions = (from fieldDef in type.Fields
                                                 where fieldDef.Name == prop.Name
                                                 select fieldDef).ToArray();
                VerifySingleResult(definitions, type, "field " + prop.Name);
                FieldDefinition def = definitions[0];

                propertyOffsetsToInitialize.Add(Tuple.Create(offsetField, prop));
                if (null != nativePropertyField)
                {
                    propertyPointersToInitialize.Add(Tuple.Create(nativePropertyField, prop));
                }

                prop.UnrealPropertyType.WriteLoad(ctorBody, helper, loadBufferInstruction, offsetField, def);
                prop.UnrealPropertyType.WriteStore(toNativeBody, helper, loadBufferInstruction, offsetField, def);
            }

            ctorBody.Emit(OpCodes.Ret);
            FinalizeMethod(ctor);

            toNativeBody.Emit(OpCodes.Ret);
            FinalizeMethod(toNativeMethod);

            FieldDefinition nativeDataSizeField = new FieldDefinition("NativeDataSize", FieldAttributes.Public | FieldAttributes.Static | FieldAttributes.InitOnly, int32TypeRef);
            type.Fields.Add(nativeDataSizeField);

            HookStaticConstructor(helper, propertyOffsetsToInitialize, null, propertyPointersToInitialize, null, null, null, null, null, nativeDataSizeField);

            //Generate Marshaler
            TypeDefinition marshalerType = new TypeDefinition(type.Namespace, type.Name + "Marshaler", TypeAttributes.Class | TypeAttributes.Public | TypeAttributes.BeforeFieldInit);
            marshalerType.BaseType = helper.TargetAssembly.MainModule.TypeSystem.Object;
            helper.TargetAssembly.MainModule.Types.Add(marshalerType);
            TypeReference IntType = helper.TargetAssembly.MainModule.TypeSystem.Int32;
            TypeReference VoidType = helper.TargetAssembly.MainModule.TypeSystem.Void;
            
            //Create Function with signature: 
            //public static void ToNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner, <StructureType> obj)
            MethodDefinition marshalerToNative = CreateMethod(marshalerType,
                                                              "ToNative",
                                                              MethodAttributes.Static | MethodAttributes.Public | MethodAttributes.HideBySig,
                                                              VoidType,
                                                              new TypeReference[] { helper.IntPtrType, IntType, helper.UObjectType, type });


            ILProcessor marshalerToNativeBody = marshalerToNative.Body.GetILProcessor();
            marshalerToNativeBody.Emit(OpCodes.Ldarga, marshalerToNative.Parameters[3]);

                    marshalerToNativeBody.Emit(OpCodes.Ldarg_0);

                        marshalerToNativeBody.Emit(OpCodes.Ldarg_1); //arrIndex
                        marshalerToNativeBody.Emit(OpCodes.Ldsfld, nativeDataSizeField);
                    marshalerToNativeBody.Emit(OpCodes.Mul); //*

                marshalerToNativeBody.Emit(OpCodes.Call, helper.IntPtrAdd); //+

            marshalerToNativeBody.Emit(OpCodes.Call, toNativeMethod);
            marshalerToNativeBody.Emit(OpCodes.Ret);
            FinalizeMethod(marshalerToNative);

            //Create Function with signature:
            //public static <StructureType> FromNative(IntPtr nativeBuffer, int arrayIndex, UnrealObject owner)
            MethodDefinition marshalerFromNative = CreateMethod(marshalerType,
                                                                "FromNative",
                                                                MethodAttributes.Static | MethodAttributes.Public | MethodAttributes.HideBySig,
                                                                type,
                                                                new TypeReference[] { helper.IntPtrType, IntType, helper.UObjectType });

            ILProcessor marshalerFromNativeBody = marshalerFromNative.Body.GetILProcessor();
            marshalerFromNativeBody.Emit(OpCodes.Ldarg_0);
            marshalerFromNativeBody.Emit(OpCodes.Ldarg_1); //arrIndex
            marshalerFromNativeBody.Emit(OpCodes.Ldsfld, nativeDataSizeField);
            marshalerFromNativeBody.Emit(OpCodes.Mul); //*
            marshalerFromNativeBody.Emit(OpCodes.Call, helper.IntPtrAdd); //+
            marshalerFromNativeBody.Emit(OpCodes.Newobj, ctor);
            marshalerFromNativeBody.Emit(OpCodes.Ret);
            FinalizeMethod(marshalerFromNative);


            
        }

        static void RewriteUnrealClass(AssemblyMetadata assemblyMetadata, AssemblyDefinition assembly, TypeDefinition type, ClassMetadata metadata, AssemblyDefinition bindingsAssembly)
        {
            string expectedClassNamespace = Program.BindingsNamespace;
            string expectedBaseClassName = "UnrealObject";

            TypeDefinition currentType = type;

            while (currentType.Namespace != expectedClassNamespace
                && currentType.Name != expectedBaseClassName)
            {
                if (null == currentType.BaseType)
                {
                    throw new InternalRewriteException(type, String.Format("Did not find base class {0}.{1}", expectedClassNamespace, expectedBaseClassName));
                }

                currentType = currentType.BaseType.Resolve();

            }

            RewriteHelper helper = new RewriteHelper(assemblyMetadata, assembly, bindingsAssembly, type);

            // Hot reloading requires a native wrapper constructor 
            FieldDefinition nativeClassField = new FieldDefinition("NativeClassPtr", FieldAttributes.Static | FieldAttributes.InitOnly | FieldAttributes.Private, helper.IntPtrType);
            type.Fields.Add(nativeClassField);

            var propertyOffsetsToInitialize = new List<Tuple<FieldDefinition, PropertyMetadata>>();
            var propertyRepIndexesToInitialize = new List<Tuple<FieldDefinition, PropertyMetadata>>();
            var propertyPointersToInitialize = new List<Tuple<FieldDefinition, PropertyMetadata>>();

            //this must happen before rewriting since it can add a UProperty
            AddTaskRunnerCalls(assembly, type, metadata);

            TypeReference uint16TypeRef = assembly.MainModule.ImportReference(typeof(System.UInt16));
            TypeReference int32TypeRef = assembly.MainModule.ImportReference(typeof(System.Int32));
            TypeReference intPtrTypeRef = helper.IntPtrType;

            var strippedFields = new Dictionary<string, (PropertyMetadata, PropertyDefinition, FieldDefinition, FieldDefinition)>();

            foreach (var prop in metadata.Properties)
            {
                // Add offset field
                FieldDefinition offsetField = AddOffsetField(type, prop, int32TypeRef);
                FieldDefinition nativePropertyField = AddNativePropertyField(type, prop, intPtrTypeRef);
                if (prop.Replicated)
                {
                    FieldDefinition repIndexField = AddRepIndexField(type, prop, uint16TypeRef);
                    propertyRepIndexesToInitialize.Add(Tuple.Create(repIndexField, prop));
                }

                propertyOffsetsToInitialize.Add(Tuple.Create(offsetField, prop));
                if (null != nativePropertyField)
                {
                    propertyPointersToInitialize.Add(Tuple.Create(nativePropertyField, prop));
                }

                prop.UnrealPropertyType.PrepareForRewrite(helper, null, prop);

                PropertyDefinition def = prop.FindPropertyDefinition(type);
                prop.UnrealPropertyType.WriteGetter(helper, def.GetMethod, offsetField, nativePropertyField);
                if (null != def.SetMethod)
                {
                    prop.UnrealPropertyType.WriteSetter(helper, def.SetMethod, offsetField, nativePropertyField);
                }

                if (!prop.PreStripped)
                {
                    strippedFields.Add(StripBackingField(type, prop), (prop, def, offsetField, nativePropertyField));
                }
            }

            FixStrippedFieldRefsInCtors(type, helper, strippedFields);

            // Generate invokers to unpack parameter buffers for managed UFunctions.
            var functionPointersToInitialize = new Dictionary<FunctionMetadata, FieldDefinition>();
            var functionParamSizesToInitialize = new List<Tuple<FunctionMetadata, FieldDefinition>>();
            var functionParamOffsetsToInitialize = new List<Tuple<FunctionMetadata, List<Tuple<FieldDefinition, PropertyMetadata>>>>();
            var functionParamElementSizesToInitialize = new List<Tuple<FunctionMetadata, List<Tuple<FieldDefinition, PropertyMetadata>>>>();
            foreach (var func in metadata.Functions)
            {
                // Generate offset fields for all parameters, but not the return value, since a pointer to the return buffer is passed explicitly to the invoker.
                var paramOffsetFields = (from param in func.ParamProperties
                                         select Tuple.Create(AddOffsetField(type, func, param, int32TypeRef), param)).ToList();
                if (paramOffsetFields.Count > 0)
                {
                    functionParamOffsetsToInitialize.Add(Tuple.Create(func, paramOffsetFields));
                }
                var elementSizeFields = (from param in func.ParamProperties
                                         where param.UnrealPropertyType.NeedsElementSizeField
                                         select Tuple.Create(AddElementSizeField(type, func, param, int32TypeRef), param)).ToList();
                // The return value never needs an offset field, since the invoker receives a pointer directly to the
                // return value memory as a parameter, but it may need an element size for array marshaling purposes.
                if (func.ReturnValueProperty != null && func.ReturnValueProperty.UnrealPropertyType.NeedsElementSizeField)
                {
                    elementSizeFields.Add(Tuple.Create(AddElementSizeField(type, func, func.ReturnValueProperty, int32TypeRef), func.ReturnValueProperty));
                }
                if (elementSizeFields.Count > 0)
                {
                    functionParamElementSizesToInitialize.Add(Tuple.Create(func, elementSizeFields));
                }

                if (func.IsBlueprintEvent || func.IsRPC)
                {
                    bool staticField = func.IsRPC;
                    FieldDefinition nativeFunctionField = AddNativeFunctionField(type, func, intPtrTypeRef, staticField);
                    if (staticField)
                    {
                        functionPointersToInitialize.Add(func, nativeFunctionField);
                    }

                    FieldDefinition paramsSizeField = AddParamsSizeField(type, func, int32TypeRef);
                    functionParamSizesToInitialize.Add(Tuple.Create(func, paramsSizeField));

                    RewriteMethodAsUFunctionInvoke(helper, type, func, nativeFunctionField, paramsSizeField, paramOffsetFields);
                }
                else
                {
                    MethodDefinition[] methods = (from methodDef in type.Methods
                                                      where methodDef.Name == func.Name
                                                      select methodDef).ToArray();
                    VerifySingleResult(methods, type, func.Name);
                    WriteFunctionInvoker(helper, type, func, methods[0], paramOffsetFields);
                }

            }

            foreach (MethodDefinition method in metadata.BlueprintEventOverrides)
            {
                // Users will have overridden the [BlueprintImplementable] method itself, since the _Implementation
                // method didn't exist until IL rewriting time.  We need to retarget the override and update any
                // instructions that are trying to call the base class version of the method.
                string newMethodName = method.Name + "_Implementation";

                foreach (Instruction inst in method.Body.Instructions)
                {
                    if (inst.OpCode == OpCodes.Call || inst.OpCode == OpCodes.Callvirt)
                    {
                        MethodReference calledMethod = (MethodReference)inst.Operand;
                        if (calledMethod.Name == method.Name)
                        {
                            calledMethod = (from m in calledMethod.DeclaringType.Resolve().Methods
                                            where m.Name == newMethodName
                                            select m).ToArray()[0];
                            inst.Operand = helper.TargetAssembly.MainModule.ImportReference(calledMethod);
                        }
                    }
                }

                
                method.Name = newMethodName;
            }

            metadata.CreateClassHash();

            // we've rewritten the props, now initialize the offsets
            // add a static constructor if one does not already exist
            HookStaticConstructor(helper, propertyOffsetsToInitialize, propertyRepIndexesToInitialize, propertyPointersToInitialize, functionParamOffsetsToInitialize, functionParamElementSizesToInitialize, functionPointersToInitialize, functionParamSizesToInitialize, nativeClassField, null);
        }

        /// <summary>
        /// Some native fields may need to be initialized when the object is constructed to prevent Blueprint subclasses crashing, such as Text fields.
        /// This will find such fields that the user does not appear to have initialized themselves, and initialize them with default values.
        /// </summary>
        static IEnumerable<Instruction> GetDefaultPropertyInitializers(RewriteHelper helper, MethodDefinition ctor, Dictionary<string, (PropertyMetadata prop, PropertyDefinition def, FieldDefinition offsetField, FieldDefinition nativePropertyField)> strippedFields)
        {
            var fieldRefs = new HashSet<string>();
            var propRefs = new HashSet<string>();
            foreach (var instr in ctor.Body.Instructions)
            {
                if (instr.OpCode == OpCodes.Call || instr.OpCode == OpCodes.Callvirt || instr.OpCode == OpCodes.Calli)
                {
                    var mr = (MethodReference)instr.Operand;
                    if ((mr.Name.StartsWith("get_", StringComparison.Ordinal) || mr.Name.StartsWith("set_", StringComparison.Ordinal)) && mr.Resolve().IsSpecialName)
                    {
                        propRefs.Add(mr.Name.Substring(4));
                    }
                    continue;
                }

                if (instr.OpCode == OpCodes.Ldfld || instr.OpCode == OpCodes.Ldflda)
                {
                    var fr = (FieldReference)instr.Operand;
                    fieldRefs.Add(fr.Name);
                }
            }

            foreach (var (prop, def, offsetField, nativePropertyField) in strippedFields.Values)
            {
                if (propRefs.Contains (prop.Name) || fieldRefs.Contains(offsetField.Name) || (nativePropertyField != null && fieldRefs.Contains(nativePropertyField.Name)))
                {
                    continue;
                }
                foreach (var instr in prop.UnrealPropertyType.EmitDefaultPropertyInitializer(helper, prop, def, offsetField, nativePropertyField))
                {
                    yield return instr;
                }
            }
        }

        static void FixStrippedFieldRefsInCtors(TypeDefinition type, RewriteHelper helper, Dictionary<string, (PropertyMetadata prop, PropertyDefinition def, FieldDefinition offsetField, FieldDefinition nativePropertyField)> strippedFields)
        {
            foreach (var ctor in type.GetConstructors().ToList())
            {
                if (!ctor.HasBody)
                {
                    continue;
                }

                bool baseCallFound = false;
                var alteredInstructions = new List<Instruction>();
                var deferredInstructions = new List<Instruction>();

                if (ctor == helper.InitializerConstructor)
                {
                    deferredInstructions.AddRange (GetDefaultPropertyInitializers(helper, ctor, strippedFields));
                }

                foreach (var instr in ctor.Body.Instructions)
                {
                    alteredInstructions.Add(instr);

                    if (instr.Operand is MethodReference baseCtor && baseCtor.Name == ".ctor")
                    {
                        baseCallFound = true;
                        alteredInstructions.AddRange(deferredInstructions);
                    }

                    if (!(instr.Operand is FieldReference field))
                    {
                        continue;
                    }

                    if (!(strippedFields.TryGetValue(field.Name, out (PropertyMetadata meta, PropertyDefinition def, FieldDefinition offsetField, FieldDefinition nativePropertyField) prop)))
                    {
                        continue;
                    }

                    // for now, we only handle simple stores
                    if (instr.OpCode != OpCodes.Stfld)
                    {
                        throw new UnableToFixPropertyBackingReferenceException(ctor, prop.def, instr.OpCode);
                    }

                    if (!prop.meta.UnrealPropertyType.CanRewritePropertyInitializer)
                    {
                        throw new UnsupportedPropertyInitializerException(prop.def, ctor.DebugInformation?.GetSequencePoint(instr));
                    }

                    var m = prop.def.SetMethod;

                    //if the property did not have a setter, add a private one for the ctor to use
                    if (m == null)
                    {
                        var voidRef = type.Module.ImportReference(typeof(void));
                        prop.def.SetMethod = m = new MethodDefinition($"set_{prop.def.Name}", MethodAttributes.SpecialName | MethodAttributes.Private | MethodAttributes.HideBySig, voidRef);
                        m.Parameters.Add(new ParameterDefinition(prop.def.PropertyType));
                        type.Methods.Add(m);
                        prop.meta.UnrealPropertyType.WriteSetter(helper, prop.def.SetMethod, prop.offsetField, prop.nativePropertyField);
                    }

                    var newInstr = Instruction.Create((m.IsReuseSlot && m.IsVirtual) ? OpCodes.Callvirt : OpCodes.Call, m);
                    newInstr.Offset = instr.Offset;
                    alteredInstructions[alteredInstructions.Count - 1] = newInstr;

                    // now the hairy bit. initializers happen _before_ the base ctor call, so the NativeObject is not yet set, and they fail
                    //we need to relocate these to after the base ctor call
                    if (baseCallFound)
                    {
                        // if they're after the base ctor call it's fine
                        continue;
                    }

                    //handle the simple pattern `ldarg0; ldconst*; call set_*`
                    if (alteredInstructions[alteredInstructions.Count - 3].OpCode == OpCodes.Ldarg_0)
                    {
                        var ldconst = alteredInstructions[alteredInstructions.Count - 2];
                        if (IsLdconst(ldconst))
                        {
                            CopyLastElements(alteredInstructions, deferredInstructions, 3);
                            continue;
                        }
                    }

                    //TODO: special case more common patterns, or resurrect Cecil.FlowAnalysis for arbitrary pattern support
                    throw new UnsupportedPropertyInitializerException(prop.def, ctor.DebugInformation?.GetSequencePoint(instr));
                }

                //add back the instructions and fix up their offsets
                ctor.Body.Instructions.Clear();
                int offset = 0;
                foreach (var instr in alteredInstructions)
                {
                    int oldOffset = instr.Offset;
                    instr.Offset = offset;
                    ctor.Body.Instructions.Add(instr);

                    //fix up the sequence point offsets too
                    if (ctor.DebugInformation != null && oldOffset != offset)
                    {
                        //this only uses the offset so doesn't matter that we replaced the instruction
                        var seqPoint = ctor.DebugInformation?.GetSequencePoint(instr);
                        if (seqPoint != null)
                        {
                            ctor.DebugInformation.SequencePoints.Remove(seqPoint);
                            ctor.DebugInformation.SequencePoints.Add(
                                new SequencePoint(instr, seqPoint.Document)
                                {
                                    StartLine = seqPoint.StartLine,
                                    StartColumn = seqPoint.StartColumn,
                                    EndLine = seqPoint.EndLine,
                                    EndColumn = seqPoint.EndColumn
                                });
                        }
                    }
                }
            }
        }

        static bool IsLdconst(Instruction ldconst)
        {
            return ldconst.OpCode.Op1 == 0xff && ldconst.OpCode.Op2 >= 0x15 && ldconst.OpCode.Op2 <= 0x23;
        }

        static void CopyLastElements(List<Instruction> from, List<Instruction> to, int count)
        {
            int startIdx = from.Count - count;
            for (int i = startIdx; i < startIdx + count; i++)
            {
                to.Add(from[i]);
            }
            for (int i = startIdx + count -1; i >= startIdx; i--)
            {
                from.RemoveAt(i);
            }
        }

        static void AddTaskRunnerCalls(AssemblyDefinition assembly, TypeDefinition type, ClassMetadata metadata)
        {
            TypeDefinition actorType;
            string engineNamespace = BaseUnrealNamespace + ".Engine";
            if ((actorType = FindBaseType(type, engineNamespace, "Actor")) == null)
                return;

            var tbtrType = actorType.Module.GetAllTypes().First(t => t.Name == "TickBasedTaskRunner" && t.Namespace == engineNamespace);
            TypeReference tbtrTypeRef = assembly.MainModule.ImportReference(tbtrType);
            var tbtrTickRef = assembly.MainModule.ImportReference(tbtrType.GetMethods().Single (m => m.Name == "Tick"));
            var tbtrDisposeRef = assembly.MainModule.ImportReference(tbtrType.GetMethods().Single (m => m.Name == "Dispose"));
            var tbtrCtorRef = assembly.MainModule.ImportReference(tbtrType.GetConstructors ().Single ());
            var actorUpdateRef = assembly.MainModule.ImportReference(actorType.Methods.Single(m => m.Name == "Update"));
            var boolRef = assembly.MainModule.ImportReference(typeof(bool));
            var voidRef = assembly.MainModule.ImportReference(typeof(void));

            var funcCtor = ImportGenericCtor(assembly.MainModule, (GenericInstanceType)tbtrCtorRef.Parameters[0].ParameterType);

            var updateMethod = type.Methods.FirstOrDefault(m => m.Name == "Update" && m.IsReuseSlot && m.IsVirtual);
            if (updateMethod == null)
                return;

            //add field for storing task runner
            //
            var field = new FieldDefinition("__updateTaskRunner", FieldAttributes.Private, tbtrTypeRef);
            type.Fields.Add(field);

            //add a UProperty that will store whether to not we have a task runner
            //so that this info can survive hot reloads
            //
            var hasRunnerProp = new PropertyDefinition("__hasUpdateTaskRunner", PropertyAttributes.None, boolRef);
            const MethodAttributes propMethAtts = MethodAttributes.Private | MethodAttributes.SpecialName | MethodAttributes.HideBySig;
            hasRunnerProp.SetMethod = new MethodDefinition("set_" + hasRunnerProp.Name, propMethAtts, voidRef);
            hasRunnerProp.SetMethod.Parameters.Add(new ParameterDefinition(boolRef));
            hasRunnerProp.SetMethod.SemanticsAttributes = MethodSemanticsAttributes.Setter;
            hasRunnerProp.GetMethod = new MethodDefinition("get_" + hasRunnerProp.Name, propMethAtts, boolRef);
            hasRunnerProp.GetMethod.SemanticsAttributes = MethodSemanticsAttributes.Getter;
            type.Properties.Add(hasRunnerProp);
            type.Methods.Add(hasRunnerProp.GetMethod);
            type.Methods.Add(hasRunnerProp.SetMethod);

            var uprop = PropertyMetadata.FromTypeReference(boolRef, hasRunnerProp.Name);
            uprop.PreStripped = true;
            uprop.Protection = AccessModifier.Private;
            //TODO: EditorOnly, when we support it
            uprop.Flags = ((ulong)(UnrealEngine.Runtime.PropertyFlags.Transient)).ToString();
            metadata.Properties.Add(uprop);

            //add helper for creating task runner
            //
            var createRunnerMethod = new MethodDefinition("__UpdateCreateTaskRunner", MethodAttributes.Private, voidRef);
            var cril = createRunnerMethod.Body.GetILProcessor();
            //__updateTaskRunner = new TickBasedTaskRunner (Update);
            cril.Emit(OpCodes.Ldarg_0);
            cril.Emit(OpCodes.Ldarg_0);
            cril.Emit(OpCodes.Dup);
            cril.Emit(OpCodes.Ldvirtftn, actorUpdateRef);
            cril.Emit(OpCodes.Newobj, funcCtor);
            cril.Emit(OpCodes.Ldc_I4_0);
            cril.Emit(OpCodes.Newobj, tbtrCtorRef);
            cril.Emit(OpCodes.Stfld, field);
            cril.Emit (OpCodes.Ret);
            type.Methods.Add(createRunnerMethod);

            //when play starts, create task runner and mark as having one
            //
            //protected override void ReceiveBeginPlay ()
            InsertAtStart(GetOrCreateOverride(type, metadata, actorType.Methods.Single(m => m.Name == "ReceiveBeginPlay")),
                //__hasUpdateTaskRunner = true
                Instruction.Create(OpCodes.Ldarg_0),
                Instruction.Create(OpCodes.Ldc_I4_1),
                Instruction.Create(OpCodes.Call, hasRunnerProp.SetMethod),
                //__UpdateCreateTaskRunner();
                Instruction.Create(OpCodes.Ldarg_0),
                Instruction.Create(OpCodes.Call, createRunnerMethod)
            );

            //when play ends, dispose task runner and mark as no longer having one
            //
            //protected override void ReceiveEndPlay (EndPlayReason endPlayReason)
            InsertAtStart(GetOrCreateOverride(type, metadata, actorType.Methods.Single(m => m.Name == "ReceiveEndPlay")),
                //__updateTaskRunner.Dispose ();
                Instruction.Create(OpCodes.Ldarg_0),
                Instruction.Create(OpCodes.Ldfld, field),
                Instruction.Create(OpCodes.Call, tbtrDisposeRef),
                //__updateTaskRunner = null;
                Instruction.Create(OpCodes.Ldarg_0),
                Instruction.Create(OpCodes.Ldnull),
                Instruction.Create(OpCodes.Stfld, field),
                //__hasUpdateTaskRunner = false
                Instruction.Create(OpCodes.Ldarg_0),
                Instruction.Create(OpCodes.Ldc_I4_0),
                Instruction.Create(OpCodes.Call, hasRunnerProp.SetMethod)
            );

            //on tick, call the task runner's tick if it's not null
            //it could be null if "tick in editor" is enabled
            //
            //void ReceiveTick(float DeltaSeconds)
            var tickMethod = GetOrCreateOverride(type, metadata, actorType.Methods.Single(m => m.Name == "ReceiveTick"));
            InsertAtStart (tickMethod,
                //if(__updateTaskRunner != null)
                Instruction.Create (OpCodes.Ldarg_0),
                Instruction.Create (OpCodes.Ldfld, field),
                Instruction.Create (OpCodes.Ldnull),
                Instruction.Create (OpCodes.Ceq),
                Instruction.Create (OpCodes.Brtrue_S, tickMethod.Body.Instructions[0]),

                //__updateTaskRunner.Tick ();
                Instruction.Create(OpCodes.Ldarg_0),
                Instruction.Create(OpCodes.Ldfld, field),
                Instruction.Create(OpCodes.Call, tbtrTickRef)
            );

            //add code to the deserializer intptr to recreate the task runner if we had one
            //
            //.ctor(IntPtr handle)
            var intPtrCtor = type.GetConstructors ().Single (m => m.Parameters.Count == 1 && m.Parameters[0].ParameterType.Name == "IntPtr");
            var existing = intPtrCtor.Body.Instructions.ToList();
            var ret = existing.Last();
            if (ret.OpCode != OpCodes.Ret)
                throw new Exception();
            intPtrCtor.Body.Instructions.Clear();
            foreach (var i in existing.Take (existing.Count - 1))
                intPtrCtor.Body.Instructions.Add(i);
            var ctorilp = intPtrCtor.Body.GetILProcessor();
            //if(__hasUpdateTaskRunner)
            ctorilp.Emit(OpCodes.Ldarg_0);
            ctorilp.Emit(OpCodes.Call, hasRunnerProp.GetMethod);
            ctorilp.Emit(OpCodes.Brfalse_S, ret);
            //__UpdateCreateTaskRunner();
            ctorilp.Emit(OpCodes.Ldarg_0);
            ctorilp.Emit(OpCodes.Call, createRunnerMethod);
            ctorilp.Append (ret);
        }

        static void InsertAtStart(MethodDefinition method, params Instruction[] instructions)
        {
            var existing = method.Body.Instructions.ToList();
            method.Body.Instructions.Clear();
            foreach (var i in instructions)
                method.Body.Instructions.Add(i);
            foreach (var i in existing)
                method.Body.Instructions.Add(i);
        }

        //doesn't handle non-void return types
        //doesn't chain to base method, but chains to intermediate overrides
        //doesn't support generics
        static MethodDefinition GetOrCreateOverride (TypeDefinition type, ClassMetadata metadata, MethodDefinition baseMethod)
        {
            var o = type.GetMethods().SingleOrDefault(m => MethodMatch(m, baseMethod));
            if (o != null)
            {
                if (!o.IsReuseSlot || !o.IsVirtual)
                    throw new Exception(string.Format("{0}.{1} must be override", type.FullName, baseMethod));
                return o;
            }

            //try to find base method to chain to
            MethodDefinition chainTo = null;
            var bt = type.BaseType;
            while (chainTo == null && !(TypeRefsEqual(bt, baseMethod.DeclaringType)))
            {
                var rbt = bt.Resolve();
                chainTo = rbt.GetMethods().SingleOrDefault(m => MethodMatch(m, baseMethod));
                bt = rbt.BaseType;
            }

            var newOverride = new MethodDefinition(baseMethod.Name,
                (baseMethod.Attributes & ~MethodAttributes.VtableLayoutMask) | MethodAttributes.ReuseSlot,
                baseMethod.ReturnType)
            {
                HasThis = baseMethod.HasBody,
                ExplicitThis = baseMethod.ExplicitThis,
                CallingConvention = baseMethod.CallingConvention,
                ReturnType = type.Module.ImportReference(baseMethod.ReturnType)
            };

            foreach (var p in baseMethod.Parameters)
            {
                newOverride.Parameters.Add(new ParameterDefinition(p.Name, p.Attributes, type.Module.ImportReference(p.ParameterType)));
            }

            var ilp = newOverride.Body.GetILProcessor();

            if (chainTo != null)
            {
                //push the this ptr
                ilp.Emit(OpCodes.Ldarg_0);
                //pass remaining args through
                for (int i = 0; i < chainTo.Parameters.Count; i++)
                    ilp.Emit (OpCodes.Ldarga, i + 1);
                ilp.Emit(OpCodes.Call, type.Module.ImportReference(chainTo));
            }

            ilp.Emit(OpCodes.Ret);

            type.Methods.Add(newOverride);

            //make sure bindings code knows it's overridden
            metadata.VirtualFunctions.Add(baseMethod.Name);

            return newOverride;
        }

        static bool MethodMatch (MethodDefinition @override, MethodDefinition @base)
        {
            if (@base.Name != @override.Name)
                return false;
            if (((@override.Attributes ^ @base.Attributes) & ~MethodAttributes.VtableLayoutMask) != 0)
                return false;
            if (!TypeRefsEqual(@base.ReturnType, @override.ReturnType))
                return false;
            for (int i = 0; i < @override.Parameters.Count; i++)
            {
                var op = @override.Parameters[i];
                var bp = @base.Parameters[i];
                if (!TypeRefsEqual (op.ParameterType, bp.ParameterType) || op.Attributes != bp.Attributes)
                    return false;
            }
            for (int i = 0; i < @override.GenericParameters.Count; i++)
            {
                var op = @override.GenericParameters[i];
                var bp = @base.GenericParameters[i];
                if (!TypeRefsEqual (op, bp) || op.Attributes != bp.Attributes)
                    return false;
            }
            return true;
        }

        static bool TypeRefsEqual (TypeReference a, TypeReference b)
        {
            return a.Name == b.Name && a.Namespace == b.Namespace;
        }

        //cecil loses the type args from the type when importing, need to manually reconstruct
        static MethodReference ImportGenericCtor(ModuleDefinition module, GenericInstanceType instanceTypeRef)
        {
            var importedArgs = instanceTypeRef.GenericArguments.Select(a => module.ImportReference(a)).ToArray();
            var resolved = instanceTypeRef.Resolve();
            var ctor = resolved.GetConstructors().Single();

            var importedType = module.ImportReference(resolved);

            var closedTypeRef = importedType.MakeGenericInstanceType(importedArgs);
            var reference = new MethodReference(ctor.Name, module.ImportReference(ctor.ReturnType), closedTypeRef)
            {
                HasThis = ctor.HasThis
            };
            foreach (var parameter in ctor.Parameters)
            {
                reference.Parameters.Add(new ParameterDefinition(parameter.Name, parameter.Attributes, module.ImportReference(parameter.ParameterType)));
            }
            return reference;
        }
    }
}
