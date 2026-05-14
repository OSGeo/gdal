using System;
using System.IO;

using OSGeo.GDAL;
using OSGeo.OGR;
using OSGeo.OSR;

namespace testapp
{
    class GdalTestUtf8
    {
        const string ConfigKey = "UNICODE_STRING";
        const string UnicodeString = "Ĥĕļĺō Ŵŏŕľď";

        public static void Main()
        {
            Console.OutputEncoding = System.Text.Encoding.UTF8;
            Gdal.AllRegister();
            TestOptionGettingAndSetting();
            TestXMLNodeStrings();
            TestUnicodeStringProperties();
            TestStringsByReference();
            TestUnicodeDatasetLayerName();
            TestUnicodeFieldDefs();
            TestUnicodeVrtFiles();
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
        private static void TestOptionGettingAndSetting()
        {
            Console.WriteLine("Test setting, getting, and clearing Unicode string config options.");
            TestOptionGettingAndSetting(nameof(Gdal.GetConfigOption), Gdal.GetConfigOption, Gdal.SetConfigOption);
            TestOptionGettingAndSetting(nameof(Gdal.GetThreadLocalConfigOption), Gdal.GetThreadLocalConfigOption, Gdal.SetThreadLocalConfigOption);
            TestOptionGettingAndSetting(nameof(Gdal.GetPathSpecificOption), (k, d) => Gdal.GetPathSpecificOption("/", k, d), (k, v) => Gdal.SetPathSpecificOption("/", k, v));
            TestOptionGettingAndSetting(nameof(Gdal.GetCredential), (k, d) => Gdal.GetCredential("/", k, d), (k, v) => Gdal.SetCredential("/", k, v));
        }
        private static void TestOptionGettingAndSetting(string name, Func<string, string, string> getter, Action<string, string> setter)
        {
            Console.WriteLine($"Testing {name}");
            Console.WriteLine($"  Setting and getting Unicode string");
            setter(ConfigKey, UnicodeString);
            string configValue = getter(ConfigKey, null);
            AssertEqual(UnicodeString, configValue, name);

            Console.WriteLine($"  Setting null value");
            setter(ConfigKey, null);
            configValue = getter(ConfigKey, null);
            AssertEqual(null, configValue, name);

            Console.WriteLine($"  Getting unset key with Unicode default value");
            configValue = getter(ConfigKey, UnicodeString);
            AssertEqual(UnicodeString, configValue, name);
        }
        private static void TestXMLNodeStrings()
        {
            Console.WriteLine("Test creating and serializing XMLNode with Unicode strings.");
            using (XMLNode node = new XMLNode(XMLNodeType.CXT_Element, "TestElement"))
            {
                node.SetXMLValue(".", UnicodeString);
                string serializedXml = node.SerializeXMLTree();
                string expectedXml = $"<TestElement>{UnicodeString}</TestElement>";
                AssertEqual(expectedXml, serializedXml?.TrimEnd('\n'), $"{nameof(XMLNode)}.{nameof(node.SerializeXMLTree)}");
            }
        }
        private static void TestUnicodeStringProperties()
        {
            Console.WriteLine("Testing C# Property setting and getting with Unicode strings.");
            using (GCP gcp = new GCP(0, 0, 0, 0, 0, UnicodeString, "Id"))
            {
                AssertEqual(gcp.Info, UnicodeString, $"{nameof(GCP)}.{nameof(gcp.Info)}");
                gcp.Id = UnicodeString;
                AssertEqual(gcp.Id, UnicodeString, $"{nameof(GCP)}.{nameof(gcp.Id)}");
            }
        }
        private static void TestStringsByReference()
        {
            Console.WriteLine("Testing 'out string' and 'ref string' parameters");
            string wkText;
            using (SpatialReference sr = new SpatialReference(null))
            {
                sr.ImportFromEPSG(4326); // WGS 84
                sr.ExportToWkt(out wkText, null);
                string name = sr.GetName();
                int nameStart = wkText.IndexOf(name);
                wkText = wkText.Substring(0, nameStart) + UnicodeString + wkText.Substring(nameStart + name.Length);
            }

            using (SpatialReference sr2 = new SpatialReference(null))
            {
                sr2.ImportFromWkt(ref wkText);
                string name = sr2.GetName();
                AssertEqual(UnicodeString, name, nameof(sr2.GetName));
                sr2.ExportToWkt(out string wkTextImportExport, null);
                AssertEqual(wkText, wkTextImportExport, $"{nameof(SpatialReference)}.{nameof(sr2.ExportToWkt)}");
            }

        }
        private static void TestUnicodeDatasetLayerName()
        {
            Console.WriteLine("Test Unicode layer names");
            string fileName = UnicodeString + ".gdb";
            if (File.Exists(fileName))
                File.Delete(fileName);
            using (OSGeo.OGR.Driver shpDriver = Ogr.GetDriverByName("OpenFileGDB"))
            {
                if (shpDriver != null)
                    using (DataSource shpSrc = shpDriver.CreateDataSource(fileName, null))
                        shpSrc.CreateLayer("图层", null, wkbGeometryType.wkbPoint, null).Dispose();
            }
            using (DataSource shpSrc = Ogr.Open(fileName, 0))
            {
                if (shpSrc != null)
                {
                    AssertEqual(fileName, shpSrc.GetName(), $"{nameof(DataSource)}.{nameof(shpSrc.GetName)}");
                    using (Layer shpLyr = shpSrc.GetLayerByName("图层"))
                        AssertEqual("图层", shpLyr.GetName(), $"{nameof(Layer)}.{nameof(shpSrc.GetName)}");
                }
            }
        }
        private static void TestUnicodeFieldDefs()
        {
            Console.WriteLine("Test Unicode Field Definitions");
            string[] nameFieldValues = new string[]
            {
                "Drâ’ Berrani",
                "Drâ’ Ali Ben Amar",
                "图层",
                UnicodeString,
            };

            string fileName = UnicodeString + ".shp";
            if (File.Exists(fileName))
                File.Delete(fileName);

            using (OSGeo.OGR.Driver shpDriver = Ogr.GetDriverByName("ESRI Shapefile"))
            using (DataSource shpSrc = shpDriver.CreateDataSource(fileName, null))
            using (Layer shpLyr = shpSrc.CreateLayer(UnicodeString, null, wkbGeometryType.wkbPoint, new string[] { "ENCODING=UTF-8" }))
            using (FeatureDefn layerDef = shpLyr.GetLayerDefn())
            {
                using (FieldDefn fieldDef = new FieldDefn("图层", FieldType.OFTString))
                    if (shpLyr.CreateField(fieldDef, 1) != 0)
                        throw new Exception("Failed to create a field definition on layer.");

                foreach (string nameString in nameFieldValues)
                {
                    using (Feature feature = new Feature(layerDef))
                    {
                        feature.SetField("图层", nameString);
                        if (shpLyr.CreateFeature(feature) != 0)
                            throw new Exception("Failed to create feature on layer.");
                    }
                }
            }
            using (DataSource shpSrc = Ogr.Open(fileName, 0))
            {
                if (shpSrc == null)
                    throw new Exception($"Failed to open dataset: {shpSrc}");

                Layer shpLyr = shpSrc.GetLayerByName(UnicodeString)
                    ?? throw new Exception("Failed to get layer from shape file by name.");

                int featureIndex = 0;
                while (true) using (Feature feat = shpLyr.GetNextFeature())
                {
                    if (feat == null)
                        break;

                    string fieldValue = feat.GetFieldAsString("图层");
                    AssertEqual(nameFieldValues[featureIndex++], fieldValue, $"{nameof(Layer)}.{nameof(shpSrc.GetName)}");
                }
            }
        }
        private static void TestUnicodeVrtFiles()
        {
            Console.WriteLine("Testing BuildVRT with Unicode filenames.");
            string fileName1 = "File1 - " + UnicodeString + ".tif";
            if (File.Exists(fileName1))
                File.Delete(fileName1);

            string fileName2 = "File2 - " + UnicodeString + ".tif";
            if (File.Exists(fileName2))
                File.Delete(fileName2);

            string vrtFile = UnicodeString + ".vrt";
            if (File.Exists(vrtFile))
                File.Delete(vrtFile);

            using (SpatialReference webMerc = new SpatialReference(""))
            {
                webMerc.ImportFromEPSG(3857);
                using (OSGeo.GDAL.Driver tifDriver = Gdal.GetDriverByName("GTiff"))
                {
                    using (Dataset ds1 = tifDriver.Create(fileName1, 100, 100, 3, DataType.GDT_Byte, null))
                    {
                        ds1.SetSpatialRef(webMerc);
                        ds1.SetGeoTransform(new double[] { 0, 1, 0, 0, 0, -1 });
                    }

                    using (Dataset ds2 = tifDriver.Create(fileName2, 100, 100, 3, DataType.GDT_Byte, null))
                    {
                        ds2.SetSpatialRef(webMerc);
                        ds2.SetGeoTransform(new double[] { 100, 1, 0, 0, 0, -1 });
                    }
                }
            }

            Gdal.BuildVRT(vrtFile, new string[] { fileName1, fileName2 }, null, null, null).Dispose();
            using (Dataset vrt = Gdal.Open(vrtFile, Access.GA_ReadOnly))
            {
                if (vrt.RasterXSize != 200)
                    throw new Exception($"Expected VRT width of 200, got {vrt.RasterXSize}");

                if (vrt.RasterYSize != 100)
                    throw new Exception($"Expected VRT height of 100, got {vrt.RasterYSize}");

                string[] list = vrt.GetFileList();
                if (list.Length != 3)
                    throw new Exception($"Expected 3 files in VRT file list, got {list.Length}");

                AssertEqual(vrtFile, list[0], $"{nameof(Dataset)}.{nameof(vrt.GetFileList)}()[0]");
                AssertEqual(fileName1, list[1], $"{nameof(Dataset)}.{nameof(vrt.GetFileList)}()[1]");
                AssertEqual(fileName2, list[2], $"{nameof(Dataset)}.{nameof(vrt.GetFileList)}()[2]");
            }
        }
    }
}

