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
            Gdal.AllRegister();

            Gdal.SetConfigOption(ConfigKey, UnicodeString);
            var configValue = Gdal.GetConfigOption(ConfigKey, string.Empty);
            AssertEqual(UnicodeString, configValue, nameof(Gdal.GetConfigOption));

            Gdal.SetThreadLocalConfigOption(ConfigKey, UnicodeString);
            configValue = Gdal.GetThreadLocalConfigOption(ConfigKey, string.Empty);
            AssertEqual(UnicodeString, configValue, nameof(Gdal.GetThreadLocalConfigOption));

            Gdal.SetPathSpecificOption("/", ConfigKey, UnicodeString);
            configValue = Gdal.GetPathSpecificOption("/", ConfigKey, string.Empty);
            AssertEqual(UnicodeString, configValue, nameof(Gdal.GetPathSpecificOption));

            Gdal.SetCredential("/", ConfigKey, UnicodeString);
            configValue = Gdal.GetCredential("/", ConfigKey, string.Empty);
            AssertEqual(UnicodeString, configValue, nameof(Gdal.GetCredential));
        }

        private static void AssertEqual(string expected, string actual, string funcName)
        {
            if (expected == actual)
            {
                Console.WriteLine($"{funcName} successfully returned the expected test string.");
            }
            else
            {
                throw new Exception($"Expected '{expected}' but {funcName} returned '{actual ?? "NULL"}'");
            }
        }
    }
}
