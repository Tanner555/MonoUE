// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using Mono.Cecil;
using Mono.Cecil.Cil;

namespace MonoAssemblyProcess
{
    [Serializable]
    class InvalidConstructorException : MonoAssemblyProcessError
    {
        public InvalidConstructorException(MethodDefinition constructor, string message)
            : base(message, ErrorEmitter.GetSequencePointFromMemberDefinition(constructor))
        {
        }

        public override ErrorCode Code => ErrorCode.InvalidConstructor;
    }

    [Serializable]
    class ConstructorNotFoundException : MonoAssemblyProcessError
    {
        public ConstructorNotFoundException(TypeDefinition type, string message)
            : base(message, ErrorEmitter.GetSequencePointFromMemberDefinition(type))
        {
        }
        public override ErrorCode Code => ErrorCode.ConstructorNotFound;
    }

    [Serializable]
    class InvalidUnrealClassException : MonoAssemblyProcessError
    {
        public InvalidUnrealClassException(TypeDefinition klass, string message)
            : this(klass.FullName, ErrorEmitter.GetSequencePointFromMemberDefinition(klass), message)
        {
        }

        public InvalidUnrealClassException(string propertyName, SequencePoint sequencePoint, string message)
            : base(String.Format("Class '{0}' is not a valid Unreal class: {1}", propertyName, message), sequencePoint)
        {
        }

        public override ErrorCode Code => ErrorCode.UnvalidUnrealClass;
    }

    [Serializable]
    class InvalidUnrealStructException : MonoAssemblyProcessError
    {
        public InvalidUnrealStructException(TypeDefinition strukt, string message)
            : base(String.Format("Struct '{0}' is not a valid Unreal struct: {1}", strukt.FullName, message), ErrorEmitter.GetSequencePointFromMemberDefinition(strukt))
        {
        }

        public override ErrorCode Code => ErrorCode.InvalidUnrealStruct;
    }

    [Serializable]
    class InvalidUnrealEnumException : MonoAssemblyProcessError
    {
        public InvalidUnrealEnumException(TypeDefinition enom, string message)
            : base(String.Format("Enum '{0}' is not a valid Unreal enum: {1}", enom.FullName, message), ErrorEmitter.GetSequencePointFromMemberDefinition(enom))
        {
        }

        public override ErrorCode Code => ErrorCode.InvalidUnrealEnum;
    }

    [Serializable]
    class InvalidUnrealPropertyException : MonoAssemblyProcessError
    {
        public InvalidUnrealPropertyException(IMemberDefinition property, string message)
            : this(property.FullName, ErrorEmitter.GetSequencePointFromMemberDefinition(property), message)
        {
        }

        public InvalidUnrealPropertyException(string propertyName, SequencePoint sequencePoint, string message)
            : base(String.Format("Property '{0}' is not a valid Unreal property: {1}", propertyName, message), sequencePoint)
        {
        }

        public override ErrorCode Code => ErrorCode.InvalidUnrealProperty;
    }


    [Serializable]
    class InvalidUnrealFunctionException : MonoAssemblyProcessError
    {
        public InvalidUnrealFunctionException(MethodDefinition method, string message, Exception innerException = null)
            : base(String.Format("Method '{0}' is not a valid Unreal function: {1}", method.Name, message), innerException, ErrorEmitter.GetSequencePointFromMemberDefinition(method))
        {
        }

        public override ErrorCode Code => ErrorCode.InvalidUnrealFunction;
    }

    [Serializable]
    class InvalidEnumMemberException : MonoAssemblyProcessError
    {
        public InvalidEnumMemberException(TypeDefinition enom, FieldReference field, string message, Exception innerException = null)
            : base(String.Format("Enum '{0}' has invalid field '{1}': {2}", enom.Name, field.Name, message), innerException, ErrorEmitter.GetSequencePointFromMemberDefinition(enom))
        {
        }

        public override ErrorCode Code => ErrorCode.InvalidEnumMember;
    }

    [Serializable]
    class NotDerivableClassException : MonoAssemblyProcessError
    {
        public NotDerivableClassException(TypeDefinition klass, TypeDefinition superKlass)
            : base(String.Format("Class '{0}' is invalid because '{1}' may not be derived from in managed code.", klass.FullName, superKlass.FullName), ErrorEmitter.GetSequencePointFromMemberDefinition(klass))
        {
        }

        public override ErrorCode Code => ErrorCode.NotDerivableClass;
    }

    [Serializable]
    class PEVerificationFailedException : MonoAssemblyProcessError
    {
        public PEVerificationFailedException(string assemblyPath, string message)
            : base(String.Format("{0} : {1}", assemblyPath, message))
        {
        }

        public override ErrorCode Code => ErrorCode.PEVerificationFailed;
    }

    [Serializable]
    class UnableToFixPropertyBackingReferenceException : MonoAssemblyProcessError
    {
        public UnableToFixPropertyBackingReferenceException(MethodDefinition constructor, PropertyDefinition property, OpCode opCode)
            : base($"The type {constructor.DeclaringType.FullName}'s constructor references the property {property.Name} using an unsupported IL pattern", ErrorEmitter.GetSequencePointFromMemberDefinition(constructor))
        {
        }

        public override ErrorCode Code => ErrorCode.UnableToFixPropertyBackingReference;
    }

    [Serializable]
    class UnsupportedPropertyInitializerException : MonoAssemblyProcessError
    {
        public UnsupportedPropertyInitializerException(PropertyDefinition property, SequencePoint seq)
            : base($"Property initializers are not supported for properties of type {property.PropertyType.FullName}", seq ?? ErrorEmitter.GetSequencePointFromMemberDefinition(property))
        {
        }

        public override ErrorCode Code => ErrorCode.UnsupportedPropertyInitializer;
    }

    [Serializable]
    class MetaDataGenerationException : Exception
    {
        public MetaDataGenerationException()
            : base("Errors analyzing meta data. Can't continue.") { }
    }

    [Serializable]
    class InternalRewriteException : MonoAssemblyProcessError
    {
        public InternalRewriteException(TypeDefinition type, string message)
            : base($"Internal error rewriting type {type.FullName}: {message}", ErrorEmitter.GetSequencePointFromMemberDefinition(type))
        {
        }

        public override ErrorCode Code => ErrorCode.Internal;
    }
}
