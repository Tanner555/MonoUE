// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Diagnostics;
using System.Xml;
using Mono.Cecil;
using Mono.Cecil.Cil;
using NDesk.Options;
using Microsoft.Build.Utilities;
using UnrealEngine.Runtime;

namespace MonoAssemblyProcess
{
    partial class Program
    {
        static string StripQuotes (string s)
        {
            string strippedPath = s.Replace ("\"", "");
            return strippedPath;
        }

        static int Main (string [] args)
        {
            var assemblyPaths = new List<string> ();
            string baseUnrealBindingsNamespace = "UnrealEngine";
            bool showHelp = false;
            bool verify = false;
            string outputDir = null;
            var options = new OptionSet ()
            {
                { "p|path=", "Additional search paths for assemblies", v => assemblyPaths.Add(StripQuotes(v))},
                { "n|namespace=", "Base unreal bindings namespace", v => baseUnrealBindingsNamespace = v},
                { "o|output=", "Output directory", v => outputDir = v},
                { "h|help", "Show this message and exit", v => showHelp = v != null},
                { "verify", "Verify processed assemblies", v => verify = v != null}
            };

            List<string> extra;
            try {
                extra = options.Parse (args);
            } catch (OptionException e) {
                Console.Error.Write ("MonoAssemblyProcess: ");
                Console.Error.WriteLine (e.Message);
                Console.Error.WriteLine ("Try 'MonoAssemblyProcess --help' for more information");
                return 1;
            }

            if (showHelp) {
                ShowHelp (options);
                return 0;
            }

            if (extra.Count == 0) {
                Console.Error.WriteLine ("Need at least one assembly to process!");
                Console.Error.WriteLine ("Try 'MonoAssemblyProcess --help' for more information");
                return 2;
            }

            NativeTypeManifest manifest = new NativeTypeManifest ();
            DefaultAssemblyResolver resolver = new DefaultAssemblyResolver ();

            foreach (var lookupPath in assemblyPaths) {
                if (Directory.Exists (lookupPath)) {
                    string nativeClassManifest = Path.Combine (lookupPath, "AllNativeClasses.manifest");

                    if (File.Exists (nativeClassManifest)) {
                        manifest = new NativeTypeManifest (nativeClassManifest);
                    }
                    resolver.AddSearchDirectory (lookupPath);
                } else {
                    Console.Error.WriteLine ("Warning: Assembly resolve path {0} does not exist, skipping", lookupPath);
                }
            }

            string [] knownPaths = assemblyPaths.ToArray ();

            BaseUnrealNamespace = baseUnrealBindingsNamespace;
            BindingsNamespace = baseUnrealBindingsNamespace + "." + BindingsSubnamespace;

            AssemblyDefinition bindingsAssembly = resolver.Resolve (new AssemblyNameReference (BindingsNamespace, new Version(0,0,0,0)));

            if (null == bindingsAssembly) {
                Console.Error.WriteLine ("Could not find bindings assembly: " + BindingsNamespace);
                return 3;
            }

            foreach (var quotedAssemblyPath in extra) {
                var assemblyPath = StripQuotes (quotedAssemblyPath);
                if (outputDir == null) {

                    if (!ProcessAssemblyInPlace (bindingsAssembly, assemblyPath, resolver, knownPaths, manifest, verify))
                        return 4;
                } else {
                    var outputPath = Path.Combine (outputDir, Path.GetFileName (assemblyPath));
                    string nativeClassManifest = Path.Combine (outputPath, "AllNativeClasses.manifest");

                    if (File.Exists (nativeClassManifest)) {
                        manifest = new NativeTypeManifest (nativeClassManifest);
                    }

                    Directory.CreateDirectory (outputDir);
                    try {
                        ProcessAssembly (bindingsAssembly, assemblyPath, outputPath, resolver, knownPaths, manifest, verify);
                    } catch (MonoAssemblyProcessError error) {
                        ErrorEmitter.Error (error);
                    } catch (Exception ex) {
                        Console.Error.WriteLine ("Exception processing {0}", assemblyPath);
                        Console.Error.WriteLine (ex.Message);
                        Console.Error.WriteLine (ex.StackTrace);
                        return 4;
                    }
                }
            }

            return 0;
        }

        static void ShowHelp (OptionSet options)
        {
            Console.WriteLine ("Usage: MonoAssemblyProcess [OPTIONS]+ AssemblyPath1 AssemblyPath2 ...");
            Console.WriteLine ();
            Console.WriteLine ("Options:");
            options.WriteOptionDescriptions (Console.Out);
        }



        public class NativeTypeManifest
        {
            class NativeTypeManifestMirror
            {
                public string [] Classes;
                public string [] Structs;
                public string [] Enums;

                public NativeTypeManifestMirror ()
                {
                    Classes = null;
                    Structs = null;
                    Enums = null;
                }
            }

            HashSet<string> Classes;
            HashSet<string> Structs;
            HashSet<string> Enums;

            public NativeTypeManifest ()
            {
                Classes = new HashSet<string> ();
                Structs = new HashSet<string> ();
                Enums = new HashSet<string> ();
            }

            public NativeTypeManifest (string nativeTypeManifest)
            {
                Classes = new HashSet<string> ();
                Structs = new HashSet<string> ();
                Enums = new HashSet<string> ();
                NativeTypeManifestMirror mirror = new NativeTypeManifestMirror ();
                fastJSON.JSON.FillObject (mirror, File.ReadAllText (nativeTypeManifest));

                foreach (var klass in mirror.Classes) {
                    Classes.Add (klass);
                }

                foreach (var strukt in mirror.Structs) {
                    Structs.Add (strukt);
                }

                foreach (var enom in mirror.Enums) {
                    Enums.Add (enom);
                }
            }

            public IEnumerable<T> CheckForCollisions<T> (IEnumerable<T> collection, string type) where T : IMemberDefinition
            {
                void FindCollisions(HashSet<string> names, string kind)
                {
                    var collisions = collection.Where(item => names.Contains(item.Name));
                    foreach (var collision in collisions)
                    {
                        SequencePoint point = ErrorEmitter.GetSequencePointFromMemberDefinition(collision);
                        ErrorEmitter.Error(ErrorCode.CollidingTypeName, $"All Unreal types live in same namespace. {type} '{collision.Name}' conflicts with existing native {kind}", point);
                    }
                    collection = collection.Except(collisions);
                }

                FindCollisions(Classes, "class");
                FindCollisions(Structs, "struct");
                FindCollisions(Enums, "enum");

                return collection;
            }
        }

        public static bool IsNativeClass (TypeDefinition type)
        {
            ClassFlags classFlags = (ClassFlags)MetadataBase.GatherFlags (type, "ClassFlagsMapAttribute");

            return 0 != (classFlags & ClassFlags.Native);
        }

        static TypeReferenceMetadata GetUnrealClass (TypeDefinition type)
        {
            TypeReference baseClass = type.BaseType;

            while (null != baseClass) {
                try {
                    TypeDefinition baseClassDef = baseClass.Resolve ();
                    if (IsNativeClass (baseClassDef)) {
                        return new TypeReferenceMetadata (baseClass); ;
                    } else {
                        baseClass = baseClassDef.BaseType;
                    }
                } catch (AssemblyResolutionException e) {
                    Console.Error.WriteLine ("Warning: could not resolve base class {0}", baseClass.ToString ());
                    Console.Error.WriteLine (e.Message);
                    return null;
                }

            }

            return null;
        }

        static public CustomAttribute FindAttributeByType (IEnumerable<CustomAttribute> customAttributes, string typeNamespace, string typeName)
        {
            CustomAttribute [] attribs = FindAttributesByType (customAttributes, typeNamespace, typeName);

            if (attribs.Length == 0) {
                return null;
            }
            return attribs [0];
        }

        static public CustomAttribute [] FindAttributesByType (IEnumerable<CustomAttribute> customAttributes, string typeNamespace, string typeName)
        {
            return (from attrib in customAttributes
                    where attrib.AttributeType.Namespace == typeNamespace && attrib.AttributeType.Name == typeName
                    select attrib).ToArray ();
        }

        public static CustomAttributeArgument? FindAttributeConstructorArgument (CustomAttribute attribute, string argumentType)
        {
            foreach (var arg in attribute.ConstructorArguments) {
                if (arg.Type.FullName == argumentType) {
                    return arg;
                }
            }
            return null;
        }

        public static CustomAttributeNamedArgument? FindAttributeProperty (CustomAttribute attribute, string propertyName)
        {
            foreach (var prop in attribute.Properties) {
                if (prop.Name == propertyName) {
                    return prop;
                }
            }
            return null;
        }

        public static CustomAttributeArgument? FindAttributeField (CustomAttribute attribute, string fieldName)
        {
            foreach (var field in attribute.Fields) {
                if (field.Name == fieldName) {
                    return field.Argument;
                }
            }
            return null;
        }

        static public CustomAttribute [] FindMetaDataAttributes (IEnumerable<CustomAttribute> customAttributes)
        {
            return FindAttributesByType (customAttributes, Program.BindingsNamespace, "UMetaDataAttribute");
        }

        static Tuple<string, string> FindMetaDataByKey (IEnumerable<CustomAttribute> customAttributes, string key)
        {
            CustomAttribute [] metaDataAttributes = FindMetaDataAttributes (customAttributes);

            foreach (var attrib in metaDataAttributes) {
                if (attrib.ConstructorArguments.Count >= 1
                    && attrib.ConstructorArguments [0].Value.Equals (key)) {
                    if (attrib.ConstructorArguments.Count == 2) {
                        return Tuple.Create ((string)attrib.ConstructorArguments [0].Value, (string)attrib.ConstructorArguments [1].Value);
                    } else {
                        return Tuple.Create<string, string> ((string)attrib.ConstructorArguments [0].Value, null);
                    }
                }
            }
            return null;
        }

        static public bool HasMetaData (TypeDefinition type, string key)
        {
            return FindMetaDataByKey (type.CustomAttributes, key) != null;
        }

        public enum MetaDataResult
        {
            Present,
            NotPresent
        }

        static Tuple<bool, MetaDataResult> GetBoolMetaDataHelper (TypeDefinition type, string key)
        {
            var metaData = FindMetaDataByKey (type.CustomAttributes, key);
            if (metaData != null && metaData.Item2 != null) {
                return Tuple.Create (String.Compare ("true", metaData.Item2, StringComparison.InvariantCultureIgnoreCase) == 0, MetaDataResult.Present);
            }
            return Tuple.Create (false, MetaDataResult.NotPresent);
        }

        static public bool GetBoolMetaDataHeirarchical (TypeDefinition type, string key)
        {
            while (type != null) {
                var result = GetBoolMetaDataHelper (type, key);
                if (result.Item2 == MetaDataResult.Present) {
                    return result.Item1;
                }
                if (type.BaseType != null) {
                    type = type.BaseType.Resolve ();
                } else {
                    return false;
                }
            }
            return false;
        }

        public static TypeDefinition FindBaseType (TypeDefinition type, string baseTypeNamespace, string baseTypeName)
        {
            do {
                try {
                    type = type.BaseType.Resolve ();
                } catch (AssemblyResolutionException e) {
                    Console.Error.WriteLine ("Warning: could not resolve base class {0}", type.BaseType);
                    Console.Error.WriteLine (e.Message);
                    break;
                }
                if (type.Name == baseTypeName && type.Namespace == baseTypeNamespace)
                    return type;
            } while (type.BaseType != null);

            return null;
        }

        public static bool IsDerivedFromActor (TypeDefinition type)
        {
            string engineNamespace = BaseUnrealNamespace + ".Engine";
            return FindBaseType (type, engineNamespace, "Actor") != null;
        }

        public static MethodDefinition GetClassOverrideMethod (TypeDefinition type, string methodName)
        {
            return type.Methods.FirstOrDefault (m => m.Name == methodName && m.IsReuseSlot && m.IsVirtual);
        }

        public static bool GetClassOverridesMethodHeirarchical (TypeDefinition type, string methodName)
        {
            while (null != type) {
                if (null != GetClassOverrideMethod (type, methodName)) {
                    return true;
                }

                if (type.BaseType != null) {
                    type = type.BaseType.Resolve ();
                } else {
                    type = null;
                }
            }

            return false;
        }

        const string BindingsSubnamespace = "Runtime";
        public static string BindingsNamespace;
        public static string BaseUnrealNamespace;

        public static MethodDefinition GetObjectInitializerConstructor (TypeDefinition type)
        {
            foreach (var method in type.Methods) {
                if (method.IsConstructor) {
                    if (method.Parameters.Count == 1) {
                        if (method.Parameters [0].ParameterType.Namespace == BindingsNamespace
                            && method.Parameters [0].ParameterType.Name == "ObjectInitializer") {
                            if (method.IsPublic) {
                                throw new InvalidConstructorException (method, String.Format ("ObjectInitializer constructor on class '{0}' can not be public.", type.ToString ()));
                            }
                            return method;
                        }
                    }
                }
            }
            return null;
        }

        static bool IsUnrealClass (TypeDefinition type)
        {
            bool isUnrealClass = null != GetUnrealClass (type);
            if (isUnrealClass) {
                if (type.Name.EndsWith ("_WrapperOnly")) {
                    // Only the base class needs processing, in cases where we generated a wrapper-only version.
                    return false;
                }
                return true;
            }
            return false;
        }

        static void SafeCopyBackup (string source, string dest)
        {
            try {
                if (File.Exists (source)) {
                    File.Copy (source, dest, true);
                }
            } catch (Exception e) {
                Console.WriteLine ("Warning: couldn't copy file {0} to {1}", source, dest);
                Console.WriteLine (e.Message);
            }
        }

        static bool _RunningUnderMono = Type.GetType ("Mono.Runtime") != null;
        public static bool IsRunningUnderMono {
            get {
                return _RunningUnderMono;
            }
        }

        public static bool IsPathInDirectory (string directory, string path)
        {
            directory = Path.GetFullPath(directory);
            path = Path.GetFullPath(path);

            if (directory.Length < 1)
            {
                return false;
            }

            if (directory[directory.Length-1] != Path.DirectorySeparatorChar)
            {
                directory = directory + Path.DirectorySeparatorChar;
            }

            return path.StartsWith(directory, StringComparison.OrdinalIgnoreCase) && path.Length > directory.Length;
        }

        static void VerifyAssembly (string assemblyPath, string [] knownPaths, bool force)
        {
            string verifierPath = null;

            //TODO: VerifyAssembly under Mono
            //TODO: search more paths
            if (!IsRunningUnderMono) {
                verifierPath = @"C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.7 Tools\PEVerify.exe";
                //verifierPath = Path.Combine (ToolLocationHelper.GetPathToWindowsSdk(TargetDotNetFrameworkVersion.Version461), @"bin\NETFX 4.6.1 Tools\peverify.exe");
            }

            if (verifierPath == null || !File.Exists (verifierPath))
            {
                if (!force)
                {
                    return;
                }
                throw new PEVerificationFailedException(assemblyPath, "Could not find PEVerify");
            }

            // we ignore 0x80131860 because we are generating unsafe code and that error is "Expected ByRef on stack", which happens even when we hand write the unsafe code
            // ignore 0x8013184A - we're loading from an unsafe pointer so it doesn't strictly know the type same issue happens on handwritten code
            // ignore 0x80131854 - "unexpected type on stack" - same issue happens on handwritten code
            // ignore 0x801318DE - "unmanaged pointers are not a verifiable type" - same issue happens on handwritten code
            // ignore 0x8013186E - "Instruction cannot be verified." - occurs for every use of stackalloc
            var args = string.Format ("\"{0}\" /verbose /nologo /hresult /ignore=0x80131860,0x8013184A,0x80131854,0x801318DE,0x8013186E", Path.GetFullPath (assemblyPath));

            string configFile = assemblyPath + ".config";

            // backup any existing config file
            // TODO: we may need to *modify* this config file with the probing path in the future, but this is simplest
            string backupConfigFile = Path.ChangeExtension (configFile, ".config.bak");
            bool hasBackup = false;

            if (File.Exists (configFile)) {
                hasBackup = true;
                File.Copy (configFile, backupConfigFile, true);
            }

            // generate a config file so PEVerify can find the references
            using (var Writer = new StreamWriter (configFile)) {
                using (var XmlStream = XmlWriter.Create (Writer, new XmlWriterSettings { CloseOutput = true, Indent = true, IndentChars = "\t", OmitXmlDeclaration = true })) {
                    XmlStream.WriteStartDocument ();
                    XmlStream.WriteStartElement ("configuration");
                    XmlStream.WriteStartElement ("runtime");
                    XmlStream.WriteStartElement ("assemblyBinding", "urn:schemas-microsoft-com:asm.v1");

                    XmlStream.WriteStartElement ("probing");
                    XmlStream.WriteAttributeString ("privatePath", "_dependencies");
                    XmlStream.WriteEndElement ();

                    XmlStream.WriteEndElement ();
                    XmlStream.WriteEndElement ();
                    XmlStream.WriteEndElement ();
                    XmlStream.WriteEndDocument ();
                }
            }

            string dependenciesDirectory = Path.Combine (Path.GetDirectoryName (assemblyPath), "_dependencies");

            if (!Directory.Exists (dependenciesDirectory)) {
                Directory.CreateDirectory (dependenciesDirectory);
            }


            var copiedFiles = new List<string> ();

            foreach (var path in knownPaths) {
                string [] dlls = Directory.GetFiles (path, "*.dll");

                foreach (var dll in dlls) {
                    var destPath = Path.Combine (dependenciesDirectory, Path.GetFileName (dll));
                    copiedFiles.Add (destPath);
                    File.Copy (dll, destPath, true);
                }
            }

            // now run peverify
            var process = new Process {
                StartInfo =
                {
                    FileName = verifierPath,
                    UseShellExecute = false,
                    WorkingDirectory = AppDomain.CurrentDomain.BaseDirectory,
                    Arguments = args
                }
            };
            process.Start ();
            process.WaitForExit ();

            // clean up
            foreach (var copiedFile in copiedFiles) {
                if (File.Exists (copiedFile)) {
                    File.Delete (copiedFile);
                }
            }

            if (Directory.Exists (dependenciesDirectory)) {
                Directory.Delete (dependenciesDirectory);
            }

            if (File.Exists (configFile)) {
                File.Delete (configFile);
            }

            if (hasBackup) {
                File.Copy (backupConfigFile, configFile, true);
            }

            if (process.ExitCode != 0) {
                throw new PEVerificationFailedException (assemblyPath, "Verification failed");
            }
        }

        static bool IsUnrealStruct (TypeDefinition strukt)
        {
            CustomAttribute structAttribute = FindAttributeByType (strukt.CustomAttributes, Program.BindingsNamespace, "UStructAttribute");
            if (structAttribute != null) {
                return true;
            }

            return false;
        }

        static bool IsUnrealEnum (TypeDefinition enom)
        {
            CustomAttribute enumAttribute = FindAttributeByType (enom.CustomAttributes, Program.BindingsNamespace, "UEnumAttribute");
            if (enumAttribute != null) {
                return true;
            }

            return false;
        }

        static void PushReferencedStructsFromAssembly (AssemblyDefinition assembly, TypeDefinition unrealStruct, Stack<TypeDefinition> structStack, HashSet<TypeDefinition> pushedStructs)
        {
            var referencedStructs = new List<TypeDefinition> ();
            foreach (var field in unrealStruct.Fields) {
                TypeDefinition fieldType = field.FieldType.Resolve ();
                // if it's not in the same assembly, it will have been processed already
                if (assembly != fieldType.Module.Assembly) {
                    continue;
                }
                if (fieldType.IsValueType && IsUnrealStruct (fieldType) && !pushedStructs.Contains (fieldType)) {
                    referencedStructs.Add (fieldType);
                    structStack.Push (fieldType);
                    pushedStructs.Add (fieldType);
                }
            }

            foreach (var strukt in referencedStructs) {
                PushReferencedStructsFromAssembly (assembly, strukt, structStack, pushedStructs);
            }
        }

        static bool ProcessAssemblyInPlace (AssemblyDefinition bindingsAssembly, string assemblyPath, BaseAssemblyResolver resolver, string [] knownPaths, NativeTypeManifest manifest, bool verify)
        {
            string backupFile = Path.ChangeExtension (assemblyPath, ".bak.dll");
            string pdbFile = Path.ChangeExtension (assemblyPath, ".pdb");
            string backupPdbFile = Path.ChangeExtension (pdbFile, ".bak.pdb");
            string metadataFileName = Path.ChangeExtension (assemblyPath, "json");

            // before we rewrite, copy the original assembly to a backup location
            // if we're debugging we might be using backup files directly, don't do the copy
            if (!assemblyPath.Contains (".bak")) {
                File.Copy (assemblyPath, backupFile, true);
                if (File.Exists (pdbFile)) {
                    File.Copy (pdbFile, backupPdbFile, true);
                }
            }

            try {
                ProcessAssembly (bindingsAssembly, assemblyPath, assemblyPath, resolver, knownPaths, manifest, verify);

                // if we got this far without an exception, safe to delete the bak files
                if (File.Exists (backupFile)) {
                    File.Delete (backupFile);
                }
                if (File.Exists (backupPdbFile)) {
                    File.Delete (backupPdbFile);
                }
            } catch (MonoAssemblyProcessError error) {
                ErrorEmitter.Error (error);
                // if we're debugging, we may be operating on the backup files
                if (!assemblyPath.Contains (".bak")) {
                    // delete originals so UE4 won't think the assembly is valid
                    if (File.Exists (assemblyPath)) {
                        File.Delete (assemblyPath);
                    }
                    if (File.Exists (pdbFile)) {
                        File.Delete (pdbFile);
                    }
                    if (File.Exists (metadataFileName)) {
                        File.Delete (metadataFileName);
                    }
                }
                return false;
            } catch (Exception e) {
                Console.Error.WriteLine ("Exception processing {0}", assemblyPath);
                Console.Error.WriteLine (e.Message);
                Console.Error.WriteLine (e.StackTrace);
                // if we're debugging, we may be operating on the backup files
                if (!assemblyPath.Contains (".bak")) {
                    // delete originals so UE4 won't think the assembly is valid
                    if (File.Exists (assemblyPath)) {
                        File.Delete (assemblyPath);
                    }
                    if (File.Exists (pdbFile)) {
                        File.Delete (pdbFile);
                    }
                    if (File.Exists (metadataFileName)) {
                        File.Delete (metadataFileName);
                    }
                }
                return false;
            }
            return true;
        }

        static void ProcessAssembly (AssemblyDefinition bindingsAssembly, string assemblyPath, string outputPath, BaseAssemblyResolver resolver, string [] knownPaths, NativeTypeManifest manifest, bool verify)
        {
            string metadataFileName = Path.ChangeExtension (outputPath, "json");

            var readerParams = new ReaderParameters {
                AssemblyResolver = resolver,
                ReadSymbols = true,
                SymbolReaderProvider = new PortablePdbReaderProvider (),
                ReadingMode = ReadingMode.Deferred
            };

            var assembly = AssemblyDefinition.ReadAssembly (assemblyPath, readerParams);
            var metadata = new AssemblyMetadata {
                AssemblyName = assembly.Name.Name,
                AssemblyPath = assemblyPath,
                References = (from module in assembly.Modules
                              from reference in module.AssemblyReferences
                              select AssemblyReferenceMetadata.ResolveReference (reference, resolver, assembly.Name.Name, knownPaths)).ToArray ()
            };


            ///////////////////////////////////////////////////////////////////

            //Enums
            var unrealEnums = (from module in assembly.Modules
                               from type in module.Types
                               where type.IsEnum && IsUnrealEnum (type)
                               select type);

            unrealEnums = manifest.CheckForCollisions (unrealEnums, "Enum");

            //Generate metadata
            bool errorGeneratingEnumMetadata = false;
            var enumMetadata = unrealEnums.ToDictionaryErrorEmit (type => type, type => new EnumMetadata (type), out errorGeneratingEnumMetadata);

            metadata.Enums = enumMetadata.Values.ToArray ();

            ///////////////////////////////////////////////////////////////////

            //Structs

            // Capture the current struct list in an array, as IL rewriting for UStructs may add array marshaler types.
            var unrealStructs = (from module in assembly.Modules
                                 from type in module.Types
                                 where type.IsValueType && IsUnrealStruct (type)
                                 select type);


            unrealStructs = manifest.CheckForCollisions (unrealStructs, "Struct");



            // We need to create struct metadata in the correct order to ensure that blittable structs have
            // their UStruct attributes updated before other referencing structs use them to create UnrealTypes.
            var structStack = new Stack<TypeDefinition> ();
            var pushedStructs = new HashSet<TypeDefinition> ();
            var structHandlingOrder = new List<TypeDefinition> ();
            var structMetadata = new Dictionary<TypeDefinition, StructMetadata> ();
            bool errorsGeneratingStructMetadata = false;
            foreach (var unrealStruct in unrealStructs) {
                if (!pushedStructs.Contains (unrealStruct)) {
                    structStack.Push (unrealStruct);
                    pushedStructs.Add (unrealStruct);

                    PushReferencedStructsFromAssembly (assembly, unrealStruct, structStack, pushedStructs);

                    while (structStack.Count > 0) {
                        var currentStruct = structStack.Pop ();
                        try {
                            if (structMetadata.ContainsKey (currentStruct)) {
                                throw new InternalRewriteException (currentStruct, "Attempted to create struct metadata twice");
                            }
                            var currentMetadata = new StructMetadata (currentStruct);

                            structHandlingOrder.Add (currentStruct);
                            structMetadata.Add (currentStruct, currentMetadata);
                        } catch (MonoAssemblyProcessError error) {
                            errorsGeneratingStructMetadata = true;
                            ErrorEmitter.Error (error);
                        }
                    }
                }
            }
            metadata.Structs = structMetadata.Values.ToArray ();

            // Rewrite as a separate pass.  For substructs, we need access to metadata.Structs to propagate hashes.
            foreach (var currentStruct in structHandlingOrder) {
                StructMetadata currentMetadata = structMetadata [currentStruct];
                RewriteUnrealStruct (metadata, assembly, currentStruct, currentMetadata, bindingsAssembly);
            }

            ///////////////////////////////////////////////////////////////////

            //Classes

            bool unrealClassError;
            // get all the unreal classes (we don't care if they are exported)
            var unrealClasses = assembly.Modules.SelectMany (x => x.Types).SelectWhereErrorEmit (type => type.IsClass && type.BaseType != null && IsUnrealClass (type), type => type, out unrealClassError);

            unrealClasses = manifest.CheckForCollisions (unrealClasses, "Class");

            bool errorGeneratingMetadata;
            var unrealClassDictionary = unrealClasses.ToDictionaryErrorEmit (type => type, type => ClassMetadata.Create (type, GetUnrealClass (type)), out errorGeneratingMetadata);
            if (errorGeneratingMetadata || errorsGeneratingStructMetadata || errorGeneratingEnumMetadata) {
                throw new MetaDataGenerationException ();
            }
            metadata.Classes = unrealClassDictionary.Values.ToArray ();

            if (unrealClassDictionary.Count > 0) {
                var rewriteStack = new Stack<KeyValuePair<TypeDefinition, ClassMetadata>> ();
                var rewrittenClasses = new HashSet<TypeDefinition> ();

                // make sure we write base classes before derived ones               
                foreach (var pair in unrealClassDictionary) {
                    var currentPair = pair;
                    bool needRewrite = true;
                    while (needRewrite) {
                        if (!rewrittenClasses.Contains (currentPair.Key)) {
                            rewriteStack.Push (currentPair);
                            var baseType = currentPair.Key.BaseType.Resolve ();

                            if (unrealClassDictionary.ContainsKey (baseType)) {
                                ClassMetadata baseClassMetadata = unrealClassDictionary [baseType];

                                currentPair = new KeyValuePair<TypeDefinition, ClassMetadata> (baseType, baseClassMetadata);
                            } else {
                                needRewrite = false;
                            }
                        } else {
                            needRewrite = false;
                        }
                    }

                    while (rewriteStack.Count > 0) {
                        currentPair = rewriteStack.Pop ();
                        if (rewrittenClasses.Contains (currentPair.Key)) {
                            throw new InternalRewriteException (currentPair.Key, "Attempted to rewrite class twice");

                        }
                        var baseType = currentPair.Key.BaseType;
                        if (baseType != null && unrealClassDictionary.ContainsKey (baseType.Resolve ())) {
                            ClassMetadata superClassMetadata = unrealClassDictionary [baseType.Resolve ()];
                            currentPair.Value.SetSuperClassHash (superClassMetadata);
                        }

                        RewriteUnrealClass (metadata, assembly, currentPair.Key, currentPair.Value, bindingsAssembly);
                        rewrittenClasses.Add (currentPair.Key);
                    }
                }

                var writer = new WriterParameters { WriteSymbols = true, SymbolWriterProvider = new PortablePdbWriterProvider() };
                assembly.Write (outputPath, writer);

                VerifyAssembly (outputPath, knownPaths, verify);

                //for some reason msbuild uses creation time or incrementral builds, not mtime
                //but cecil preserves the creation time from the original so we have to reset it
                File.SetCreationTimeUtc(outputPath, DateTime.UtcNow);

                string metadataContents = fastJSON.JSON.ToNiceJSON (metadata, new fastJSON.JSONParameters { UseExtensions = false });
                File.WriteAllText (metadataFileName, metadataContents);
            } else {
                File.Delete (metadataFileName);
            }

        }
    }
}
