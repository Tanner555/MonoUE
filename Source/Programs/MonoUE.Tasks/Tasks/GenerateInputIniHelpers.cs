// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using Microsoft.Build.Framework;
using Microsoft.Build.Utilities;
using System.Text;
using System.Linq;


namespace MonoUE.Tasks
{
    public class GenerateInputIniHelpers : Task
    {
        [Required]
        public string EngineDir { get; set; }

        [Required]
        public string GameDir { get; set; }

        public string Namespace { get; set; }

        [Output]
        public string OutFile { get; set; }

        public override bool Execute()
        {
            var ini = ReadIniHierarchy(EngineDir, GameDir, "Input");

            List<KeyValuePair<string, string>> input;
            ini.TryGetValue("/Script/Engine.InputSettings", out input);

            var template = new IniHelperTemplate {
                Namespace = Namespace,
                AxisMappings = new HashSet<string>(),
                ActionMappings = new HashSet<string>()
            };

            foreach (var val in input ?? Enumerable.Empty <KeyValuePair<string, string>>())
            {
                switch (val.Key)
                {
                    case "ActionMappings":
                        {
                            var a = ExtractStringValue(val.Value, "ActionName");
                            if (a != null)
                                template.ActionMappings.Add(a);
                            continue;
                        }
                    case "AxisMappings":
                        {
                            var a = ExtractStringValue(val.Value, "AxisName");
                            if (a != null)
                                template.AxisMappings.Add(a);
                            continue;
                        }
                }
            }

            Directory.CreateDirectory(Path.GetDirectoryName(OutFile));

            File.WriteAllText (OutFile, template.TransformText(), Encoding.UTF8);

            return true;
        }

        //HACK: parse this properly
        static string ExtractStringValue(string val, string name)
        {
            if (val == null)
                return null;

            int idx = val.IndexOf(name, StringComparison.Ordinal);
            if (idx < 0)
                return null;

            idx = val.IndexOf('"', idx + name.Length);
            if (idx < 0)
                return null;

            int end = val.IndexOf('"', idx + 1);
            if (end < 0)
                return null;

            string s = val.Substring(idx + 1, end - idx - 1);
            if (string.IsNullOrWhiteSpace(s))
                return null;

            return s;
        }

        static Dictionary<string, List<KeyValuePair<string,string>>> ReadIniHierarchy(string engineDir, string gameDir, string category, string platform=null)
        {
            var data = new Dictionary<string, List<KeyValuePair<string,string>>>();
            foreach (var file in GetConfigurationFileHierarchy (engineDir, gameDir, category, platform))
            {
                if (File.Exists(file))
                {
                    using (var fs = File.OpenText(file))
                    {
                        ReadIni(data,fs);
                    }
                }
            }
            return data;
        }

        //TODO: value escaping/quoting, multiline values
        static void ReadIni (Dictionary<string,List<KeyValuePair<string,string>>> data, TextReader file)
        {
            List<KeyValuePair<string,string>> section = null;
            string line;
            while ((line = file.ReadLine()) != null)
            {
                line = line.Trim();
                if (line.Length == 0 || line[0] == ';')
                    continue;

                if (line[0] == '[' && line[line.Length - 1] == ']')
                {
                    string sectionName = line.Substring(1, line.Length - 2);
                    if (!data.TryGetValue(sectionName, out section))
                        data[sectionName] = section = new List<KeyValuePair<string, string>>();
                    continue;
                }

                if (section == null)
                    continue;

                int eq = line.IndexOf('=');
                if (eq < 1)
                    return;

                string key = line.Substring(0, eq).TrimEnd(), value = line.Substring (eq + 1).TrimStart();
                if (key.Length == 0)
                    continue;

                char action = key[0];
                if (action == '.' || action == '!' || action == '+' || action == '-')
                {
                    key = key.Substring(1).TrimStart();
                    if (key.Length == 0)
                        continue;
                }

                var kv = new KeyValuePair<string, string>(key, value);

                switch (action)
                {
                    //add regardless
                    case '.':
                        section.Add(kv);
                        continue;
                    //add if no existing identical value
                    case '+':
                        if (section.FindIndex(s => s.Key == key && s.Value == value) < 0)
                            goto case '.';
                        continue;
                    //remove with same name
                    case '!':
                        section.RemoveAll(s => s.Key == key);
                        continue;
                    //remove with same name and value
                    case '-':
                        section.RemoveAll(s => s.Key == key && s.Value == value);
                        continue;
                    //set value
                    default:
                        int existing = section.FindIndex(s => s.Key == key);
                        if (existing >= 0)
                            section[existing] = kv;
                        else
                            section.Add(kv);
                        continue;
                }
            }
        }

        //
        // see https://docs.unrealengine.com/latest/INT/Programming/Basics/ConfigurationFiles/index.html#filehierarchy
        //
        static IEnumerable<string> GetConfigurationFileHierarchy(string engineDir, string gameDir, string category, string platform=null)
        {
            yield return Path.Combine(engineDir, "Engine", "Config", "Base.ini");
            yield return Path.Combine(engineDir, "Engine", "Config", "Base" + category + " .ini");
            if (platform != null)
                yield return Path.Combine(engineDir, "Engine", "Config", platform, platform + category + ".ini");
            yield return Path.Combine(gameDir, "Config", "Default" + category + ".ini");
            if (platform != null)
                yield return Path.Combine(gameDir, "Config", platform, platform + category + ".ini");
            yield return Path.Combine(gameDir, "Saved", "Config", category + ".ini");
        }
    }
}

