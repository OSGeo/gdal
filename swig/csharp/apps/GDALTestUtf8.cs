using System;

using OSGeo.GDAL;

namespace testapp
{
    class GdalTestUtf8
    {
        const string ConfigKey = "UNICODE_STRING";
        const string UnicodeString = "Ĥĕļĺō Ŵŏŕľď";

        public static void Main()
        {
            Console.OutputEncoding = System.Text.Encoding.UTF8;
            RunTest("ConfigOption", Gdal.GetConfigOption, Gdal.SetConfigOption);
            RunTest("ThreadLocalConfigOption", Gdal.GetThreadLocalConfigOption, Gdal.SetThreadLocalConfigOption);
            RunTest("ThreadLocalConfigOption", (k, d) => Gdal.GetPathSpecificOption("/", k, d), (k, v) => Gdal.SetPathSpecificOption("/", k, v));
            RunTest("Credential", (k, d) => Gdal.GetCredential("/", k, d), (k, v) => Gdal.SetCredential("/", k, v));

            XMLNode node = new XMLNode(XMLNodeType.CXT_Element, "TestElement");
            node.SetXMLValue(".", UnicodeString);
            string serializedXml = node.SerializeXMLTree();
            string expectedXml = $"<TestElement>{UnicodeString}</TestElement>";
            AssertEqual(expectedXml, serializedXml?.TrimEnd('\n'), $"{nameof(XMLNode)}.{nameof(node.SerializeXMLTree)}");

            Console.WriteLine("Testing string property getters and setters");
            GCP gcp = new GCP(0, 0, 0, 0, 0, UnicodeString, "Id");
            AssertEqual(gcp.Info, UnicodeString, $"{nameof(GCP)}.{nameof(gcp.Info)}");
            gcp.Id = UnicodeString;
            AssertEqual(gcp.Id, UnicodeString, $"{nameof(GCP)}.{nameof(gcp.Id)}");
        }

        private static void RunTest(string name, Func<string, string, string> getter, Action<string, string> setter)
        {
            Console.WriteLine($"Testing {name}");
            Console.WriteLine($"  Setting and getting Unicode string");
            setter(ConfigKey, UnicodeString);
            var configValue = getter(ConfigKey, null);
            AssertEqual(UnicodeString, configValue, name);

            Console.WriteLine($"  Setting null value");
            setter(ConfigKey, null);
            configValue = getter(ConfigKey, null);
            AssertEqual(null, configValue, name);

            Console.WriteLine($"  Getting unset key with Unicode default value");
            configValue = getter(ConfigKey, UnicodeString);
            AssertEqual(UnicodeString, configValue, name);
        }

        private static void AssertEqual(string expected, string actual, string funcName)
        {
            if (expected == actual)
            {
                Console.WriteLine($"    {funcName} returned the expected value: '{expected ?? "NULL"}'");
            }
            else
            {
                throw new Exception($"Expected '{expected}' but {funcName} returned '{actual ?? "NULL"}'");
            }
        }
    }
}

