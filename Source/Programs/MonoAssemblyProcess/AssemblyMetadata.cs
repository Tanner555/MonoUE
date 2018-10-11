// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using Mono.Cecil;
using Mono.Cecil.Cil;
using Mono.Cecil.Rocks;
using UnrealEngine.Runtime;
using Mono.Collections.Generic;

namespace MonoAssemblyProcess
{
    [Serializable]
    class InvalidEnumValueException : MonoAssemblyProcessError
    {
        public InvalidEnumValueException(CustomAttributeArgument arg)
            : base(String.Format("Unknown enum value {0} found of type {1}", arg.Value.ToString(), arg.Type.FullName))
        {
        }
    }

    [Serializable]
    class InvalidAttributeException : MonoAssemblyProcessError
    {
        public InvalidAttributeException(TypeDefinition attributeType, SequencePoint point, string message)
            : base(String.Format("Invalid attribute class {0}: {1}", attributeType.Name, message), point)
        {
        }
    }

    enum AccessModifier
    {
        Private,
        Protected,
        Public
    };

    struct KeyValuePairMetadata
    {
        public string Key;
        public string Value;

        public KeyValuePairMetadata(string key, string val)
        {
            Key = key;
            Value = val;
        }
    }

    class TypeReferenceMetadata : MetadataBase
    {
        public readonly string Namespace;
        public readonly string Name;
        public readonly string AssemblyName;

        public TypeReferenceMetadata(TypeReference typeReference)
        {
            // the Module of the type reference is the module *containing* the type reference, not the module that is being referenced
            TypeDefinition typeDef = typeReference.Resolve();
            AssemblyName = typeDef.Module.Assembly.Name.Name;
            Namespace = typeDef.Namespace;
            Name = typeDef.Name;
        }

        protected virtual string GetTypeHashString()
        {
            return fastJSON.JSON.ToJSON(this, new fastJSON.JSONParameters { UseExtensions = false });
        }

        protected string CreateTypeHash()
        {
            var contents = GetTypeHashString();
            byte[] stringBytes = Encoding.UTF8.GetBytes(contents);
            using (var hasher = SHA256Managed.Create())
            {
                var hash = hasher.ComputeHash(stringBytes);
                var hashStringBuilder = new StringBuilder(hash.Length * 2);

                foreach (var element in hash)
                {
                    hashStringBuilder.Append(element.ToString("x2"));
                }
                return hashStringBuilder.ToString();
            }
        }
    };

    class MetadataBase
    {
        static protected KeyValuePairMetadata[] MetadataDictionaryToArray(Dictionary<string, string> metadataDict)
        {
            // save off metadata array
            var metadataList = new List<KeyValuePairMetadata>();

            foreach (var pair in metadataDict)
            {
                metadataList.Add(new KeyValuePairMetadata(pair.Key, pair.Value));
            }

            return metadataList.ToArray();
        }

        public static ulong GetFlags(Collection<CustomAttribute> customAttributes, string flagsAttributeName)
        {
            CustomAttribute flagsAttribute = Program.FindAttributeByType(customAttributes, Program.BindingsNamespace, flagsAttributeName);

            if (null == flagsAttribute)
            {
                return 0;
            }

            return GetFlags(flagsAttribute);
        }

        public static ulong GetFlags(CustomAttribute flagsAttribute)
        {
            return ((ulong)flagsAttribute.ConstructorArguments[0].Value);
        }

        public static ulong ExtractEnumValueAsFlags(CustomAttributeArgument arg, string flagsAttributeName)
        {
            var enumDefinition = arg.Type.Resolve();
            int argVal = (int)arg.Value;
            ulong result = 0;

            foreach (var field in enumDefinition.Fields)
            {
                if (!field.IsRuntimeSpecialName && field.IsLiteral && field.IsStatic)
                {
                    int enumVal = (int)field.Constant;
                    if ((argVal & (int)field.Constant) != 0)
                    {
                        result |= GetFlags(field.CustomAttributes, flagsAttributeName);
                        argVal ^= enumVal;
                    }
                }
            }
            if (argVal != 0)
            {
                throw new InvalidEnumValueException(arg);
            }
            return result;
        }

        public static ulong ExtractBoolAsFlags(TypeDefinition attributeType, CustomAttributeNamedArgument namedArg, string flagsAttributeName)
        {
            var arg = namedArg.Argument;
            if ((bool)arg.Value)
            {
                // Find the property definition for this argument to resolve the true value to the desired flags map.
                var properties = (from prop in attributeType.Properties
                                  where prop.Name == namedArg.Name
                                  select prop).ToArray();
                Program.VerifySingleResult(properties, attributeType, "attribute property " + namedArg.Name);
                return GetFlags(properties[0].CustomAttributes, flagsAttributeName);
            }

            return 0;
        }

        public static ulong ExtractStringAsFlags(TypeDefinition attributeType, CustomAttributeNamedArgument namedArg, string flagsAttributeName)
        {
            var arg = namedArg.Argument;
            string argValue = (string)arg.Value;
            if (argValue != null && argValue.Length > 0)
            {
                // The named argument has a valid string value.  Find its property definition to resolve it to the desired flags map.
                var properties = (from prop in attributeType.Properties
                                  where prop.Name == namedArg.Name
                                  select prop).ToArray();
                Program.VerifySingleResult(properties, attributeType, "attribute property " + namedArg.Name);
                return GetFlags(properties[0].CustomAttributes, flagsAttributeName);
            }

            return 0;
        }

        public static ulong ExtractClassAsFlags(TypeReference klassRef, string flagsAttributeName)
        {
            TypeDefinition klass = klassRef.Resolve();
            if (!klass.HasCustomAttributes)
            {
                return 0;
            }

            return GetFlags(klass.Resolve().CustomAttributes, flagsAttributeName);
        }

        public static ulong ExtractEnumFlagsAsFlags(CustomAttributeArgument arg, string flagsAttributeName)
        {
            ulong flags = 0;

            var enumFlagsDefinition = arg.Type.Resolve();

            int value = ((Int32)arg.Value);

            foreach (FieldDefinition field in enumFlagsDefinition.Fields)
            {
                if (!field.IsRuntimeSpecialName
                    && field.IsLiteral && field.IsStatic)
                {
                    int flagValue = ((Int32)field.Constant);

                    if (flagValue != 0)
                    {
                        if (0 != (value & flagValue))
                        {
                            flags |= GetFlags(field.CustomAttributes, flagsAttributeName);
                        }
                    }
                }
            }

            return flags;
        }

        public static ulong GatherFlags(IMemberDefinition member, string flagsAttributeName)
        {
            return GatherFlags(member, flagsAttributeName, out int rawValue);
        }

        public static ulong GatherFlags(IMemberDefinition member, string flagsAttributeName, out int rawValue)
        {
            SequencePoint sequencePoint = ErrorEmitter.GetSequencePointFromMemberDefinition(member);
            var customAttributes = member.CustomAttributes;
            ulong flags = 0;
            rawValue = 0;

            foreach (CustomAttribute attribute in customAttributes)
            {
                TypeDefinition attributeClass = attribute.AttributeType.Resolve();

                // Only consider attributes whose class is, itself, tagged with the flag map attribute.
                // In the event that we need an attribute that has no inherent flags to contribute,
                // we can always use a [FlagsMap(Flags.None)] attribute to force its inclusion.
                CustomAttribute flagsMap = FindUnrealAttribute(attributeClass.CustomAttributes, flagsAttributeName);
                if (flagsMap != null)
                {
                    flags |= GetFlags(flagsMap);

                    if (attribute.HasConstructorArguments)
                    {
                        foreach (CustomAttributeArgument arg in attribute.ConstructorArguments)
                        {
                            if (arg.Type.Resolve().IsEnum)
                            {
                                rawValue |= (int) arg.Value;
                                flags |= ExtractEnumValueAsFlags(arg, flagsAttributeName);
                            }
                            else
                            {
                                throw new InvalidAttributeException(attributeClass,sequencePoint, "only enums are supported as attribute constructor args.");
                            }
                        }
                    }

                    if (attribute.HasProperties)
                    {
                        foreach (CustomAttributeNamedArgument arg in attribute.Properties)
                        {
                            TypeDefinition argType = arg.Argument.Type.Resolve();
                            if (argType.IsValueType && argType.Namespace == "System" && argType.Name == "Boolean")
                            {
                                flags |= ExtractBoolAsFlags(attributeClass, arg, flagsAttributeName);
                            }
                            else if (argType.Namespace == "System" && argType.Name == "String")
                            {
                                flags |= ExtractStringAsFlags(attributeClass, arg, flagsAttributeName);
                            }
                            else
                            {
                                throw new InvalidAttributeException(attributeClass, sequencePoint, String.Format("{0} is not supported as an attribute property type.", argType.FullName));
                            }
                        }
                    }
                }
            }

            return flags;
        }

        public static void AddExplicitMetadata(Dictionary<string, string> metadataDict, Collection<CustomAttribute> customAttributes, string attributeName)
        {
            CustomAttribute attribute = FindUnrealAttribute(customAttributes, attributeName);
            if (attribute != null)
            {
                const string attributeSuffix = "Attribute";
                string metadataName = (attributeName.EndsWith(attributeSuffix) ? attributeName.Substring(0, attributeName.Length - attributeSuffix.Length) : attributeName);

                // We expect explicit metadata strings to be specified as mandatory constructor arguments.
                string metadataValue = "true";
                if (attribute.HasConstructorArguments)
                {
                    metadataValue = (string)attribute.ConstructorArguments[0].Value;
                }
                
                metadataDict.Add(metadataName, metadataValue);
            }

        }

        public void AddMetadataAttributes(Dictionary<string, string> metadata, Collection<CustomAttribute> customAttributes)
        {
            var metaDataAttributes = Program.FindMetaDataAttributes(customAttributes);

            foreach (var attrib in metaDataAttributes)
            {
                if (attrib.ConstructorArguments.Count >= 1)
                {
                    if (attrib.ConstructorArguments.Count == 1)
                    {
                        metadata.Add((string)attrib.ConstructorArguments[0].Value, "");
                    }
                    else
                    {
                        metadata.Add((string)attrib.ConstructorArguments[0].Value, (string)attrib.ConstructorArguments[1].Value);
                    }
                }
            }
        }

        public static bool GetBoolMetadata(Dictionary<string, string> dictionary, string key)
        {
            string val;
            if (dictionary.TryGetValue(key, out val))
            {
                if (0 == StringComparer.OrdinalIgnoreCase.Compare(val, "true"))
                {
                    return true;
                }
            }
            return false;
        }

        public static bool HasMetadata(Dictionary<string, string> dictionary, string key)
        {
            string val;
            if (dictionary.TryGetValue(key, out val))
            {
                return val != "";
            }
            return false;
        }

        public static CustomAttribute FindUnrealAttribute(Collection<CustomAttribute> customAttributes, string attributeName)
        {
            return Program.FindAttributeByType(customAttributes, Program.BindingsNamespace, attributeName);
        }
    }

    class PropertyMetadata : MetadataBase
    {
        public string Name;
        public AccessModifier Protection;
        public UnrealType UnrealPropertyType;
        public string Flags;
        public KeyValuePairMetadata[] Metadata;
        public bool PreStripped;

        internal bool Replicated { get; private set; }
        internal bool ReferenceParam { get; private set; }
        internal bool OutParam { get; private set; }

        // Set non-null to force the property owner's hash to change along with a type referenced by the property.
        public string ValueHash;

        public string RepNotifyFunctionName;

        static bool MethodIsCompilerGenerated(MethodDefinition method)
        {
            return null != Program.FindAttributeByType(method.CustomAttributes, "System.Runtime.CompilerServices", "CompilerGeneratedAttribute");
        }

        private PropertyMetadata(TypeReference typeRef, string paramName, ParameterType modifier)
        {
            Name = paramName;
            UnrealPropertyType = UnrealType.FromTypeReference(typeRef, paramName, null);
            
            PropertyFlags flags = PropertyFlags.None;
            if (modifier != ParameterType.None)
            {
                flags |= PropertyFlags.Parm;
            }
            
            if (modifier == ParameterType.Out)
            {
                flags |= PropertyFlags.OutParm;
                ReferenceParam = true;
                OutParam = true;
            }
            else if (modifier == ParameterType.Ref)
            {
                flags |= PropertyFlags.OutParm | PropertyFlags.ReferenceParm;
                ReferenceParam = true;
            }

            Flags = ((ulong)flags).ToString();
            Metadata = new KeyValuePairMetadata[0];
        }
        public static PropertyMetadata FromTypeReference(TypeReference typeRef, string paramName, ParameterType modifier = ParameterType.None)
        {
            return new PropertyMetadata(typeRef, paramName, modifier);
        }

        public PropertyMetadata(PropertyDefinition property)
        {
            MethodDefinition getter = property.GetMethod;
            MethodDefinition setter = property.SetMethod;

            if (null == getter)
            {
                throw new InvalidUnrealPropertyException(property, "Unreal properties must have a default get method");
            }

            if (!MethodIsCompilerGenerated(getter))
            {
                throw new InvalidUnrealPropertyException(property, "Getter can not have a body for Unreal properties");
            }

            if (setter != null && !MethodIsCompilerGenerated(setter))
            {
                throw new InvalidUnrealPropertyException(property, "Setter can not have a body for Unreal properties");
            }

            if (getter.IsPrivate)
            {
                Protection = AccessModifier.Private;
            }
            else if (getter.IsPublic)
            {
                Protection = AccessModifier.Public;
            }
            else
            {
                // if not private or public, assume protected?
                Protection = AccessModifier.Protected;
            }
            Initialize(property, property.PropertyType, Protection);
        }

        public PropertyMetadata(FieldDefinition property)
        {
            if (property.IsPrivate)
            {
                Protection = AccessModifier.Private;
            }
            else if (property.IsPublic)
            {
                Protection = AccessModifier.Public;
            }
            else
            {
                // if not private or public, assume protected?
                Protection = AccessModifier.Protected;
            }
            Initialize(property, property.FieldType, Protection);
        }

        private void Initialize(IMemberDefinition property, TypeReference propertyType, AccessModifier protection)
        {
            const int UserPropertyFlags_Instanced = 0x1000;

            Name = property.Name;
            UnrealPropertyType = UnrealType.FromTypeReference(propertyType, property.FullName, property.CustomAttributes);

            PropertyFlags flags = (PropertyFlags)GatherFlags(property, "PropertyFlagsMapAttribute", out int rawValue);
            flags |= UnrealPropertyType.AutomaticPropertyFlags;

            var metadata = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

            AddExplicitMetadata(metadata, property.CustomAttributes, "CategoryAttribute");
            AddMetadataAttributes(metadata, property.CustomAttributes);

            if ((rawValue & UserPropertyFlags_Instanced) != 0)
            {
                metadata["EditInline"] = "True";
            }

            Metadata = MetadataDictionaryToArray(metadata);

            // do some extra verification, matches verification in UE4 header parser
            if (Protection == AccessModifier.Private 
                && 0 != (flags & PropertyFlags.BlueprintVisible))
            {
                if(!GetBoolMetadata(metadata, "AllowPrivateAccess"))
                {
                    throw new InvalidUnrealPropertyException(property, "Blueprint visible properties can not be private");
                }                
            }

            if(GetBoolMetadata(metadata, "ExposeOnSpawn"))
            {
                if(0 != (flags & PropertyFlags.DisableEditOnInstance))
                {
                    Console.WriteLine("{0}: Property cannot have 'DisableEditOnInstance' and 'ExposeOnSpawn' flags", property.FullName);
                }

                if(0 != (flags & PropertyFlags.BlueprintVisible))
                {
                    Console.WriteLine("{0}: Property cannot have 'ExposeOnSpawn' with 'BlueprintVisible' flag.", property.FullName);
                }

                flags |= PropertyFlags.ExposeOnSpawn;
            }

            if(!HasMetadata(metadata, "Category"))
            {
                if(0 != (flags & (PropertyFlags.Edit|PropertyFlags.BlueprintVisible)))
                {
                    throw new InvalidUnrealPropertyException(property, "Property is exposed to the editor or blueprints but has no Category specified.");
                }
            }
            else
            {
                if (0 == (flags & (PropertyFlags.Edit|PropertyFlags.BlueprintVisible|PropertyFlags.BlueprintAssignable|PropertyFlags.BlueprintCallable)))
	            {
                    throw new InvalidUnrealPropertyException(property, "Property has a Category set but is not exposed to the editor or Blueprints with EditAnywhere, BlueprintReadWrite, VisibleAnywhere, BlueprintReadOnly, BlueprintAssignable, BlueprintCallable keywords.");
	            }
            }

            if (flags.HasFlag(PropertyFlags.Net))
            {
                Replicated = true;

                CustomAttribute replicatedAttribute = FindUnrealAttribute(property.CustomAttributes, "ReplicatedAttribute");
                if (replicatedAttribute == null)
                {
                    throw new InvalidUnrealPropertyException(property, "Property is flagged for replication, but has no [Replicated] attribute.");
                }

                // We allow "no condition, with a custom condition method" as a shorthand for LifetimeCondition.Custom.
                // The CustomConditionMethod property enforces this at runtime, but all we can see here is the explicit arguments in the user's source.
                CustomAttributeArgument? conditionArg = Program.FindAttributeConstructorArgument(replicatedAttribute, typeof(LifetimeCondition).FullName);
                CustomAttributeNamedArgument? conditionMethodProperty = Program.FindAttributeProperty(replicatedAttribute, "CustomConditionMethod");
                if (!conditionArg.HasValue || (LifetimeCondition)conditionArg.Value.Value == LifetimeCondition.Custom)
                {
                    if (conditionMethodProperty.HasValue)
                    {
                        string conditionMethodName = (string)conditionMethodProperty.Value.Argument.Value;
                        MethodDefinition notifyMethod = Program.FindMethod(property.DeclaringType, conditionMethodName, "System.Boolean");
                        if (notifyMethod == null)
                        {
                            throw new InvalidUnrealPropertyException(property, String.Format("CustomConditionMethod '{0}' not found on {1}.  Custom condition methods must return bool and take no parameters.", conditionMethodName, property.DeclaringType.Name));
                        }
                        else if (notifyMethod.IsPrivate)
                        {
                            throw new InvalidUnrealPropertyException(property, String.Format("CustomConditionMethod '{0}' is private to {1}.  Custom condition methods must be public or protected so subclasses can set up replication.", conditionMethodName, property.DeclaringType.Name));
                        }
                    }
                    else if (conditionArg.HasValue && (LifetimeCondition)conditionArg.Value.Value == LifetimeCondition.Custom)
                    {
                        throw new InvalidUnrealPropertyException(property, "Property has custom replication lifetime, but specifies no CustomConditionMethod in the [Replicated] attribute.");
                    }
                }
                else if (conditionMethodProperty.HasValue)
                {
                    throw new InvalidUnrealPropertyException(property, "CustomConditionMethod is only allowed with LifetimeCondition.Custom, or with no condition specified.");
                }

                if (flags.HasFlag(PropertyFlags.RepNotify))
                {
                    CustomAttributeNamedArgument? notifyMethodProperty = Program.FindAttributeProperty(replicatedAttribute, "NotificationMethod");
                    if (!notifyMethodProperty.HasValue)
                    {
                        throw new InvalidUnrealPropertyException(property, "Property is flagged for replication notifications, but specifies no NotificationMethod in the [Replicated] attribute.");
                    }

                    string notifyMethodName = (string)notifyMethodProperty.Value.Argument.Value;
                    MethodDefinition notifyMethod = Program.FindMethod(property.DeclaringType, notifyMethodName, "System.Void");
                    if (notifyMethod == null)
                    {
                        // Notification method may also take a single argument containing the UProperty's previous value.
                        notifyMethod = Program.FindMethod(property.DeclaringType, notifyMethodName, "System.Void", new string[] { propertyType.FullName });
                        if (notifyMethod == null)
                        {
                            throw new InvalidUnrealPropertyException(property, String.Format("RepNotify method '{0}' not found on {1}.  Notification methods must return void and take either no parameters, or a single parameter matching the property type.", notifyMethodName, property.DeclaringType.Name));
                        }
                    }

                    if (!FunctionMetadata.IsUnrealFunction(notifyMethod))
                    {
                        throw new InvalidUnrealPropertyException(property, String.Format("RepNotify method '{0}' is not a UFunction.", notifyMethodName));
                    }

                    RepNotifyFunctionName = notifyMethodName;
                }
            }
            else if (flags.HasFlag(PropertyFlags.RepNotify))
            {
                throw new InvalidUnrealPropertyException(property, "Property is flagged for replication notifications, but not replication.");
            }

            // TODO: expose on spawn validation

            Flags = ((ulong)flags).ToString();

        }

        public static bool IsUnrealProperty(FieldDefinition field)
        {
            if (field.HasCustomAttributes)
            {
                CustomAttribute propertyAttribute = PropertyMetadata.FindUnrealAttribute(field.CustomAttributes, "UPropertyAttribute");
                return (propertyAttribute != null);
            }

            return false;
        }

        public static bool IsUnrealProperty(PropertyDefinition property)
        {
            if (property.HasCustomAttributes)
            {
                CustomAttribute propertyAttribute = PropertyMetadata.FindUnrealAttribute(property.CustomAttributes, "UPropertyAttribute");

                if (propertyAttribute != null)
                {
                    // property can't have parameters
                    if (property.HasParameters)
                    {
                        throw new InvalidUnrealPropertyException(property, "Unreal properties can not have parameters");
                    }

                    if(property.HasOtherMethods)
                    {
                        throw new InvalidUnrealPropertyException(property, "Unreal properties can not have other methods");
                    }

                    return true;
                }

            }
            return false;
        }

        public PropertyDefinition FindPropertyDefinition(TypeDefinition type)
        {
            PropertyDefinition[] definitions = (from propDef in type.Properties
                                                where propDef.Name == Name
                                                select propDef).ToArray();

            return (definitions.Length > 0 ? definitions[0] : null);
        }
    };

    enum ParameterType
    {
        None,
        Value,
        Ref,
        Out,
    };

    class FunctionMetadata : MetadataBase
    {
        public string Name;
        public AccessModifier Protection;
        public PropertyMetadata ReturnValueProperty;
        public PropertyMetadata[] ParamProperties;
        public string Flags;
        public KeyValuePairMetadata[] Metadata;

        public bool IsBlueprintEvent { get; private set; }
        public bool IsRPC { get; private set; }
        public bool IsNetService { get; private set; }
        public bool NeedsValidation { get; private set; }

        public FunctionMetadata(MethodDefinition method)
        {
            Name = method.Name;

            if (method.IsPrivate)
            {
                Protection = AccessModifier.Private;
            }
            else if (method.IsPublic)
            {
                Protection = AccessModifier.Public;
            }
            else
            {
                Protection = AccessModifier.Protected;
            }

            bool hasOutParams = false;
            if (method.ReturnType.FullName != "System.Void")
            {
                hasOutParams = true;
                try
                {
                    ReturnValueProperty = PropertyMetadata.FromTypeReference(method.ReturnType, "ReturnValue");
                }
                catch (InvalidUnrealPropertyException e)
                {
                    throw new InvalidUnrealFunctionException(method, String.Format("'{0}' is invalid for unreal function return value.", method.ReturnType.FullName), e);
                }
            }
            else
            {
                ReturnValueProperty = null;
            }

            ParamProperties = new PropertyMetadata[method.Parameters.Count];
            for (int i = 0; i < method.Parameters.Count; ++i)
            {
                ParameterDefinition param = method.Parameters[i];
                if (param.IsOut)
                {
                    hasOutParams = true;
                }

                try
                {
                    bool byReference = false;
                    TypeReference paramType = param.ParameterType;
                    if (paramType.IsByReference)
                    {
                        byReference = true;
                        paramType = ((ByReferenceType)paramType).ElementType;
                    }

                    ParameterType modifier = ParameterType.Value;
                    if (param.IsOut)
                    {
                        modifier = ParameterType.Out;
                    }
                    else if (byReference)
                    {
                        modifier = ParameterType.Ref;
                    }
                    ParamProperties[i] = PropertyMetadata.FromTypeReference(paramType, param.Name, modifier);
                }
                catch (InvalidUnrealPropertyException e)
                {
                    throw new InvalidUnrealFunctionException(method, String.Format("'{0}' is invalid for unreal function parameter '{1}'.", param.ParameterType.FullName, param.Name), e);
                }
            }

            FunctionFlags flags = (FunctionFlags)GatherFlags(method, "FunctionFlagsMapAttribute");

            if (hasOutParams)
            {
                flags ^= FunctionFlags.HasOutParms;
            }

            bool explicitlyFinal = flags.HasFlag(FunctionFlags.Final);

            var metadata = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

            AddExplicitMetadata(metadata, method.CustomAttributes, "CategoryAttribute");
            AddMetadataAttributes(metadata, method.CustomAttributes);

            Metadata = MetadataDictionaryToArray(metadata);

            // Do some extra verification.  Matches functionality in FHeaderParser.
            if (flags.HasFlag(FunctionFlags.BlueprintEvent))
            {
                IsBlueprintEvent = true;

                // A Blueprint event with no default implementation corresponds to a BlueprintImplementableEvent,
                // which is the only case where UHT does not apply the FUNC_Native flag to a UFunction.
                // We also allow BlueprintImplementable methods to be virtual, in which case we also treat the UFunction
                // as native, since we must always call the base managed implementation in that case.
                if (!method.IsVirtual && method.Body.Instructions.Count == 1 && method.Body.Instructions[0].OpCode == OpCodes.Ret)
                {
                    flags ^= FunctionFlags.Native;
                }

                if (flags.HasFlag(FunctionFlags.Net))
                {
                    throw new InvalidUnrealFunctionException(method, String.Format("BlueprintImplementable methods cannot be replicated!"));
                }
                else if (flags.HasFlag(FunctionFlags.BlueprintCallable))
                {
                    throw new InvalidUnrealFunctionException(method, String.Format("Functions cannot be both BlueprintImplementable and BlueprintCallable."));
                }
                else if (Protection == AccessModifier.Private)
                {
                    throw new InvalidUnrealFunctionException(method, String.Format("A Private method cannot be BlueprintImplementable!"));
                }
            }
            if (flags.HasFlag(FunctionFlags.Exec) && flags.HasFlag(FunctionFlags.Net))
            {
                throw new InvalidUnrealFunctionException(method, "Exec functions cannot be replicated!");
            }
            if (flags.HasFlag(FunctionFlags.NetServer) && !flags.HasFlag(FunctionFlags.NetValidate))
            {
                throw new InvalidUnrealFunctionException(method, "Server RPC missing 'WithValidation' flag in the UFunction() attribute.  Required for security purposes.");
            }
            if (flags.HasFlag(FunctionFlags.Net))
            {
                if (flags.HasFlag(FunctionFlags.Static))
                {
                    throw new InvalidUnrealFunctionException(method, "Static functions can't be replicated.");
                }
            }
            else if (flags.HasFlag(FunctionFlags.NetReliable))
            {
                throw new InvalidUnrealFunctionException(method, "Reliable flag specified without setting Replication property.");
            }
            if (explicitlyFinal && !flags.HasFlag(FunctionFlags.Event))
            {
                throw new InvalidUnrealFunctionException(method, "SealedEvent may only be used when the BlueprintCallability property is set.");
            }

            bool implicitlyFinal = true;
            if (flags.HasFlag(FunctionFlags.Net) || flags.HasFlag(FunctionFlags.BlueprintEvent))
            {
                implicitlyFinal = false;
            }

            if (flags.HasFlag(FunctionFlags.BlueprintCallable | FunctionFlags.BlueprintPure))
            {
                bool internalOnly = GetBoolMetadata(metadata, "BlueprintInternalUseOnly");
                bool deprecated = HasMetadata(metadata, "DeprecatedFunction");
                bool hasCategory = HasMetadata(metadata, "Category");
                if (!hasCategory && !internalOnly && !deprecated)
                {
                    throw new InvalidUnrealFunctionException(method, "Blueprint accessible functions must have a category specified.");
                }
            }

            switch (Protection)
            {
                case AccessModifier.Public:
                    flags |= FunctionFlags.Public;
                    break;
                case AccessModifier.Protected:
                    flags |= FunctionFlags.Protected;
                    break;
                case AccessModifier.Private:
                    flags |= FunctionFlags.Private;

                    // The usual implicit Final checks don't apply in this case, so set the flag explicitly and disable them.
                    flags |= FunctionFlags.Final;
                    implicitlyFinal = false;
                    break;
                default:
                    throw new InvalidUnrealFunctionException(method, "Unknown access level");
            }

            if (method.IsStatic)
            {
                flags |= FunctionFlags.Static;
            }

            if (method.IsVirtual)
            {
                implicitlyFinal = false;
            }

            // TODO: CLASS_Interface checks
            //       -Can't be BlueprintPure
            //       -Can't be final, explicitly or implicitly
            //       -BlueprintEvents can't be virtual; non-events must be.
            // This doesn't really matter until we're handling UClass flags in game assemblies.

            if (implicitlyFinal || method.IsFinal)
            {
                flags |= FunctionFlags.Final;
            }

            if (flags.HasFlag(FunctionFlags.Net))
            {
                IsNetService = flags.HasFlag(FunctionFlags.NetRequest | FunctionFlags.NetResponse);
                if (method.ReturnType.FullName != "System.Void" && !IsNetService)
                {
                    throw new InvalidUnrealFunctionException(method, "Replicated functions can't have return values.");
                }

                IsRPC = true;
                if (flags.HasFlag(FunctionFlags.NetValidate))
                {
                    NeedsValidation = true;
                }
            }

            bool hasAnyOutputs = (method.ReturnType != null);
            foreach (var param in method.Parameters)
            {
                if (param.IsOut)
                {
                    hasAnyOutputs = true;
                }
            }            
            if (!hasAnyOutputs && flags.HasFlag(FunctionFlags.BlueprintPure))
            {
                throw new InvalidUnrealFunctionException(method, "BlueprintPure is not allowed for functions with no return value and no output parameters.");
            }

            if (flags.HasFlag(FunctionFlags.Const))
            {
                if (flags.HasFlag(FunctionFlags.BlueprintPure))
                {
                    throw new InvalidUnrealFunctionException(method, "Functions cannot be both 'BlueprintPure' and 'Const'");
                }
                else if (hasAnyOutputs && flags.HasFlag(FunctionFlags.BlueprintCallable))
                {
                    flags |= FunctionFlags.BlueprintPure;
                }
            }

            Flags = ((ulong)flags).ToString();
        }

        public static bool IsUnrealFunction(MethodDefinition method)
        {
            if (method.HasCustomAttributes)
            {
                CustomAttribute functionAttribute = PropertyMetadata.FindUnrealAttribute(method.CustomAttributes, "UFunctionAttribute");

                if (functionAttribute != null)
                {
                    return true;
                }
            }
            return false;
        }

        public static bool IsBlueprintEventOverride(MethodDefinition method)
        {
            MethodDefinition basemostMethod = method.GetOriginalBaseMethod();
            if (basemostMethod != method && basemostMethod.HasCustomAttributes)
            {
                CustomAttribute blueprintImplementableAttribute = PropertyMetadata.FindUnrealAttribute(basemostMethod.CustomAttributes, "BlueprintImplementableAttribute");
                if (blueprintImplementableAttribute != null)
                {
                    return true;
                }
            }

            return false;
        }
    };


    class ClassMetadata : TypeReferenceMetadata
    {
        // Note: all members must be readonly and set in constructor, as the code which computes ClassHash
        // assumes this is the case
        public readonly List<string> VirtualFunctions;
        public readonly List<PropertyMetadata> Properties;
        public readonly FunctionMetadata[] Functions;
        public readonly TypeReferenceMetadata BaseClass;
        public readonly TypeReferenceMetadata BaseUnrealNativeClass;
        public readonly bool ChildCanTick;
        public readonly bool OverridesBindInput;
        public readonly string BlueprintUse;
        public readonly string Transience;
        public readonly string Placeablity;
        public readonly string ConfigFile;
        public readonly bool Abstract;
        public readonly bool Deprecated;
        public readonly string Group;
        public readonly string Flags;
        // Hash so we can quickly tell if a class changed
        public string ClassHash;
        public string SuperClassHash;

        internal TypeDefinition MyTypeDefinition;
        internal MethodDefinition[] BlueprintEventOverrides;

        ClassMetadata(TypeDefinition type, TypeDefinition superClass, TypeReferenceMetadata unrealClass)
            : base(type)
        {
            MyTypeDefinition = type;

            BlueprintUse = "Inherit";
            Transience = "Inherit";
            Placeablity = "Inherit";
            Group = "";
            ConfigFile = "";
            bool verifyError = false;
            try
            {
                VerifyUnrealClassRequirements();
            }
            catch(MonoAssemblyProcessError error)
            {
                verifyError = true;
                ErrorEmitter.Error(error);
            }

            ClassFlags flags = (ClassFlags)ExtractClassAsFlags(type, "ClassFlagsMapAttribute");
            CustomAttribute attribute = Program.FindAttributeByType(type.CustomAttributes, Program.BindingsNamespace, "UClassAttribute");
            if (attribute != null)
            {
                foreach (CustomAttributeNamedArgument arg in attribute.Properties)
                {
                    if (arg.Name == "ConfigFile")
                    {
                        ConfigFile = arg.Argument.Value as string;
                    }
                    else if (arg.Name == "Group")
                    {
                        Group = arg.Argument.Value as string;
                    }
                }
            }

            Abstract = type.IsAbstract;


            CustomAttribute obsoleteAttribute = Program.FindAttributeByType(type.CustomAttributes, "System", "ObsoleteAttribute");
            Deprecated = obsoleteAttribute != null;

            CustomAttribute transientAttribute = Program.FindAttributeByType(type.CustomAttributes, Program.BindingsNamespace, "TransientAttribute");
            if (transientAttribute != null)
            {
                Transience = "Transient";
                if (transientAttribute.Properties.Count > 0 &&
                    transientAttribute.Properties[0].Name == "Enabled" &&
                    transientAttribute.Properties[0].Argument.Type.FullName == "System.Boolean")
                {
                    Transience = ((bool)transientAttribute.ConstructorArguments[0].Value) ? "Transient" : "NotTransient";
                }                    
            }

            CustomAttribute placeableAttribute = Program.FindAttributeByType(type.CustomAttributes, Program.BindingsNamespace, "PlaceableAttribute");
            if (placeableAttribute != null)
            {
                Placeablity = "Placeable";
                if (placeableAttribute.Properties.Count > 0 &&
                    placeableAttribute.Properties[0].Name == "Enabled" &&
                    placeableAttribute.Properties[0].Argument.Type.FullName == "System.Boolean")
                {
                    Placeablity = ((bool)placeableAttribute.ConstructorArguments[0].Value) ? "Placeable" : "NotPlaceable";
                }
            }

            CustomAttribute blueprintuseAttribute = Program.FindAttributeByType(type.CustomAttributes, Program.BindingsNamespace, "BlueprintUseAttribute");
            if (blueprintuseAttribute != null)
            {
                int usage = 2;
                if (blueprintuseAttribute.ConstructorArguments.Count > 0 &&
                    blueprintuseAttribute.ConstructorArguments[0].Type.FullName == "UnrealEngine.Runtime.BlueprintAccess")
                {
                    usage = (int)blueprintuseAttribute.ConstructorArguments[0].Value;
                }

                if (usage == 0) //Blueprint.None
                {
                    BlueprintUse = "None";
                }
                else if (usage == 1) //BlueprintAccess.Accessible
                {
                    BlueprintUse = "Accessible";
                }
                else if (usage == 2) //BlueprintAccess.Derivable
                {
                    BlueprintUse = "Derivable";
                }
            }

            var invalidProperties = (from field in type.Fields
                                     where PropertyMetadata.IsUnrealProperty(field)
                                     select field).ToArray();

            SequencePoint point = ErrorEmitter.GetSequencePointFromMemberDefinition(MyTypeDefinition);
            bool hasInvalidProperties = invalidProperties.Any();

            foreach (var prop in invalidProperties)
            {
                ErrorEmitter.Error(ErrorCode.InvalidUnrealProperty, $"UProperties in a UClass must be property accessors. {prop.Name} is a field.", point);
            }

            VirtualFunctions = type.Methods.Where(x => x.IsVirtual).Select(x => x.Name).ToList();
            bool propError;
            Properties = type.Properties.SelectWhereErrorEmit(x => PropertyMetadata.IsUnrealProperty(x), x => new PropertyMetadata(x), out propError).ToList();
            bool funcError;
            Functions = type.Methods.SelectWhereErrorEmit(x => FunctionMetadata.IsUnrealFunction(x),x => new FunctionMetadata(x), out funcError).ToArray();

            BlueprintEventOverrides = type.Methods.Where(x => FunctionMetadata.IsBlueprintEventOverride(x)).Select(x => x).ToArray();
            
            BaseClass = new TypeReferenceMetadata(type.BaseType);
            BaseUnrealNativeClass = unrealClass;
            ChildCanTick = (unrealClass.Namespace == (Program.BaseUnrealNamespace + ".Engine") && unrealClass.Name == "Actor") || Program.HasMetaData(superClass, "ChildCanTick");

            OverridesBindInput = false;

            Flags = ((ulong)flags).ToString();

            if (Program.IsDerivedFromActor(type))
            {
                OverridesBindInput = Program.GetClassOverridesMethodHeirarchical(type, "BindInput");
            }

            if (propError || funcError || verifyError || hasInvalidProperties)
            {
                throw new InvalidUnrealClassException(MyTypeDefinition,"Errors in class declaration.");
            }
        }

        private void VerifyUnrealClassRequirements()
        {
            if (Program.IsNativeClass(MyTypeDefinition))
            {
                throw new InvalidUnrealClassException(MyTypeDefinition, "User-written classes can not have UserClassFlags.NativeBindingsClass flag in UClass attribute, it is for internal use only.");
            }

            MethodDefinition ObjectInitializerConstructor = Program.GetObjectInitializerConstructor(MyTypeDefinition);
            bool hasNativeConstructor = false;

            // verify it has the proper constructor type
            foreach (var method in MyTypeDefinition.Methods)
            {

                if (method.IsConstructor)
                {
                    bool invalidConstructor = true;
                    if (method == ObjectInitializerConstructor)
                    {
                        invalidConstructor = false;
                    }
                    else if (method.Parameters.Count == 1)
                    {
                        if (method.Parameters[0].ParameterType == MyTypeDefinition.Module.TypeSystem.IntPtr)
                        {
                            hasNativeConstructor = true;
                            if (method.IsPublic)
                            {
                                throw new InvalidConstructorException(method, String.Format("IntPtr constructor on class '{0}' can not be public.", MyTypeDefinition.ToString()));
                            }
                            invalidConstructor = false;
                        }
                    }

                    if (invalidConstructor)
                    {
                        throw new InvalidConstructorException(method, String.Format("Invalid constructor '{0}' found on class '{1}': only ObjectInitializer constructor (for object creation) and IntPtr constructor (for hot reload) are allowed.", method.ToString(), MyTypeDefinition.ToString()));
                    }
                }

                if (hasNativeConstructor)
                {
                    break;
                }
            }

            if (ObjectInitializerConstructor == null)
            {
                throw new ConstructorNotFoundException(MyTypeDefinition, String.Format("Constructor taking ObjectInitializer not found for class '{0}'. This is required and used when creating new objects.", MyTypeDefinition.ToString()));
            }

            if (!hasNativeConstructor)
            {
                throw new ConstructorNotFoundException(MyTypeDefinition, String.Format("Constructor taking IntPtr not found for class '{0}'. This is required and used when hot-reloading managed assemblies.", MyTypeDefinition.ToString()));
            }
        }

        public void SetSuperClassHash(ClassMetadata superClass)
        {
            SuperClassHash = superClass.ClassHash;
        }

        public void CreateClassHash()
        {
            // DO NOT SET ANY MEMBERS AFTER THIS POINT
            // set this since its part of the json serialization
            ClassHash = CreateTypeHash();
        }

        static string GetMethodILString(MethodDefinition method)
        {
            var s = new StringBuilder();

            var body = method.Body;

            foreach(var instruction in body.Instructions)
            {
                s.Append(instruction.ToString());    
            }

            return s.ToString();
        }

        protected override string GetTypeHashString()
        {
            var s = new StringBuilder();
            s.Append(base.GetTypeHashString());

            // include a hash of the ObjectInitializer constructor IL so if it changes, we force a reinstance
            var ObjectInitializerConstructor = Program.GetObjectInitializerConstructor(MyTypeDefinition);

            s.Append(GetMethodILString(ObjectInitializerConstructor));


            return s.ToString();
        }

        public static ClassMetadata Create(TypeDefinition type, TypeReferenceMetadata unrealClass)
        {
            TypeDefinition superClass = type.BaseType.Resolve();
            bool bIsDerivable = null != superClass
                                && (Program.GetBoolMetaDataHeirarchical(superClass, "IsBlueprintBase")
                                    || (superClass.Namespace == (Program.BaseUnrealNamespace + ".Core") && superClass.Name == "Object")
                                    || (superClass.Namespace == (Program.BaseUnrealNamespace + ".Engine") && superClass.Name == "BlueprintFunctionLibrary"));
            if(!bIsDerivable)
            {
                throw new NotDerivableClassException(type, superClass);
            }

            return new ClassMetadata(type, superClass, unrealClass);
        }
    };

    class StructMetadata : TypeReferenceMetadata
    {
        public PropertyMetadata[] Properties;
        public string Flags;
        public KeyValuePairMetadata[] Metadata;
        public string StructHash;

        public bool IsBlittable { get; private set; }

        public StructMetadata(TypeDefinition strukt)
            : base(strukt)
        {
            var structAttribute = FindUnrealAttribute(strukt.CustomAttributes, "UStructAttribute");
            if (Program.FindAttributeField(structAttribute, "NativeBlittable").HasValue)
            {
                throw new InvalidUnrealStructException(strukt, "User structs may not specify NativeBlittable.");
            }

            var invalidProperties = (from prop in strukt.Properties
                                     where PropertyMetadata.IsUnrealProperty(prop)
                                     select prop).ToArray();

            if (invalidProperties.Length > 0)
            {
                throw new InvalidUnrealStructException(strukt, "UProperties in a UStruct must be fields, not property accessors.");
            }

            // All fields of a UStruct must be treated as UProperties.  Otherwise, we'd have a sort of 
            // slicing problem, where the managed parts of the object would be lost after storing the
            // struct to and retrieving it from native memory.  The [UProperty] attribute is still
            // allowed for specifying user property flags, but it's optional.
            Properties = (from prop in strukt.Fields
                          where !prop.HasConstant
                          select new PropertyMetadata(prop)).ToArray();

            var metadata = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            AddExplicitMetadata(metadata, strukt.CustomAttributes, "BlueprintTypeAttribute");
            AddMetadataAttributes(metadata, strukt.CustomAttributes);

            Metadata = MetadataDictionaryToArray(metadata);

            StructFlags flags = (StructFlags)GatherFlags(strukt, "StructFlagsMapAttribute");

            //TODO: custom serialization flags

            IsBlittable = true;
            bool isPOD = true;
            foreach (var prop in Properties)
            {
                if (!prop.UnrealPropertyType.IsBlittable)
                {
                    IsBlittable = false;
                }
                if (!prop.UnrealPropertyType.IsPlainOldData)
                {
                    isPOD = false;
                }
            }

            if (isPOD)
            {
                flags |= StructFlags.IsPlainOldData;
                flags |= StructFlags.NoDestructor;
                flags |= StructFlags.ZeroConstructor;
            }

            if (IsBlittable)
            {
                // Set the struct as blittable now, rather than waiting until IL rewriting time, so that UProperties
                // referring to this struct will generate the correct UnrealType information.
                structAttribute.Fields.Add(new CustomAttributeNamedArgument("NativeBlittable", new CustomAttributeArgument(strukt.Module.TypeSystem.Boolean, true)));
            }

            Flags = flags.ToString();
        }

        public void CreateStructHash()
        {
            // DO NOT SET ANY MEMBERS AFTER THIS POINT
            // set this since its part of the json serialization
            StructHash = CreateTypeHash();
        }
    }

    class EnumMetadata : TypeReferenceMetadata
    {
        public string[] Items;
        public string EnumHash;
        public bool BlueprintVisible;

        public EnumMetadata(TypeDefinition enom)
            : base(enom)
        {
            List<string> names = new List<string>();

            if (Mono.Cecil.Rocks.TypeDefinitionRocks.GetEnumUnderlyingType(enom).FullName != "System.Byte")
            {
                throw new InvalidUnrealEnumException(enom, "UEnums must be backed by a System.Byte.");
            }

            var enumAttribute = FindUnrealAttribute(enom.CustomAttributes, "UEnumAttribute");
            var blueprintType = Program.FindAttributeField(enumAttribute, "BlueprintVisible");
            BlueprintVisible = false;
            if (blueprintType.HasValue)
            {
                BlueprintVisible = (bool)blueprintType.Value.Value;
            }

            int index = 0;
            foreach (FieldReference field in enom.Fields)
            {
                FieldDefinition fdef = field.Resolve();
                if (fdef.IsStatic && fdef.IsLiteral)
                {
                    names.Add(field.Name);
                    if ((byte)fdef.Constant != index)
                    {
                        throw new InvalidEnumMemberException(enom, field, string.Format("UEnums can't specify custom values for enum members. Expected {0} got {1}.", index, (int)fdef.Constant));
                    }
                    ++index;
                }
            }

            Items = names.ToArray();

            EnumHash = CreateTypeHash();
        }
    }

    class AssemblyReferenceMetadata
    {
        public string AssemblyName;
        public string AssemblyVersion;
        public string AssemblyPath;
        public bool InKnownLocation;
        public bool Resolved;

        public static AssemblyReferenceMetadata ResolveReference(AssemblyNameReference reference, BaseAssemblyResolver resolver, string referencer, string[] knownPaths)
        {
            AssemblyReferenceMetadata metadata = new AssemblyReferenceMetadata();

            metadata.AssemblyName = reference.Name;
            metadata.AssemblyVersion = reference.Version.ToString();
            try
            {
                AssemblyDefinition definition = resolver.Resolve(reference);
                metadata.AssemblyPath = definition.MainModule.FileName;
                metadata.Resolved = true;
                metadata.InKnownLocation = false;
                foreach (var path in knownPaths)
                {
                    if (Program.IsPathInDirectory(path, metadata.AssemblyPath))
                    {
                        metadata.InKnownLocation = true;
                        break;
                    }
                }
                if (!metadata.InKnownLocation)
                {
                    // if we're not in a known location, don't export the path
                    metadata.AssemblyPath = "";
                }
            }
            catch (AssemblyResolutionException e)
            {
                Console.Error.WriteLine("Warning: could not resolve assembly {0} referenced by {1}:", reference.Name, referencer);
                Console.Error.WriteLine(e.Message);
                metadata.Resolved = false;
                metadata.InKnownLocation = false;
                metadata.AssemblyPath = "";
            }
            return metadata;
        }
    };

    class AssemblyMetadata
    {
        public string AssemblyName;
        public string AssemblyPath;
        public AssemblyReferenceMetadata[] References;
        public ClassMetadata[] Classes;
        public StructMetadata[] Structs;
        public EnumMetadata[] Enums;
    };

}