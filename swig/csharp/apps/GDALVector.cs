using OSGeo.GDAL;
using OSGeo.OGR;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace TestGdalCSharp;

public static class GDALVector
{
	const string UnicodeString = "Ĥĕļĺō Ŵŏŕľď";
	
	public static async Task Main()
	{
		Console.OutputEncoding = System.Text.Encoding.UTF8;
		Gdal.AllRegister();
		using var reg = Gdal.GetGlobalAlgorithmRegistry();

		Console.WriteLine("Listing all algorithms and their sub-algorithms with parameters:");
		foreach (var alg in reg.Algorithms())
		{
			Console.WriteLine($"  Algorithm: {alg.GetName()}");
			foreach (var subAlg in alg.SubAlgorithms())
			{
				Console.WriteLine($"    SubAlgorithm: {subAlg.GetName()}");
				foreach (var arg in subAlg.Args().OfListType(true))
				{
					Console.WriteLine($"      Parameter: {arg.GetName()}, Type: {Gdal.AlgorithmArgTypeName(arg.GetType_())}");
				}
			}
		}

		TestAlgorithmArgsIntegerList(reg);
		TestAlgorithmArgsDoubleList(reg);
		TestGdalVectorFieldsReadWrite();
	}

	private static void TestAlgorithmArgsDoubleList(AlgorithmRegistry reg)
	{
		Console.WriteLine("Testing algorithm arguments with double list type...");
		var band = reg.Algorithms().OfName("raster").SubAlgorithms().OfName("footprint").Args().OfName("input-nodata");
		var doubles = Enumerable.Range(0, 50).Select(_ => Random.Shared.NextDouble()).ToArray();
		if (!band.SetAsDoubleList(doubles))
			throw new Exception("Failed to set double list.");
		band.GetAsDoubleList(out var outDoubles);
		if (!doubles.SequenceEqual(outDoubles))
			throw new Exception("Mismatch between set and retrieved double list.");
		Console.WriteLine($"Successfully tested getting and setting algorithm arguments of type {Gdal.AlgorithmArgTypeName(band.GetType_())}.");
	}
	private static void TestAlgorithmArgsIntegerList(AlgorithmRegistry reg)
	{
		Console.WriteLine("Testing algorithm arguments with integer list type...");
		var band = reg.Algorithms().OfName("raster").SubAlgorithms().OfName("as-features").Args().OfName("band");
		var ints = Enumerable.Range(0, 50).Select(_ => Random.Shared.Next()).ToArray();
		if (!band.SetAsIntegerList(ints))
			throw new Exception("Failed to set integer list.");
		band.GetAsIntegerList(out var outInts);
		if (!ints.SequenceEqual(outInts))
			throw new Exception("Mismatch between set and retrieved integer list.");
		Console.WriteLine($"Successfully tested getting and setting algorithm arguments of type {Gdal.AlgorithmArgTypeName(band.GetType_())}.");
	}
	private static void TestGdalVectorFieldsReadWrite()
	{
		Console.WriteLine("Testing feature fields with various types...");
		using var driver = Gdal.GetDriverByName("GeoJSON");
		if (File.Exists("test.geojson"))
			File.Delete("test.geojson");
		using var ds = driver.CreateVector("test.geojson");

		using var dLayer = ds.CreateLayer("double_layer");
		CreateFieldDefn(dLayer, "int32_field", FieldType.OFTInteger);
		CreateFieldDefn(dLayer, "int64_field", FieldType.OFTInteger64);
		CreateFieldDefn(dLayer, "double_field", FieldType.OFTReal);
		CreateFieldDefn(dLayer, "string_field", FieldType.OFTString);
		CreateFieldDefn(dLayer, "date_time_field", FieldType.OFTDateTime);
		CreateFieldDefn(dLayer, "int32_list_field", FieldType.OFTIntegerList);
		CreateFieldDefn(dLayer, "int64_list_field", FieldType.OFTInteger64List);
		CreateFieldDefn(dLayer, "double_list_field", FieldType.OFTRealList);
		CreateFieldDefn(dLayer, "string_list_field", FieldType.OFTStringList);
		CreateFieldDefn(dLayer, "binary_field", FieldType.OFTBinary);

		using var dFeatureDef = dLayer.GetLayerDefn();
		using var feature = new Feature(dFeatureDef);
		TestField(
			"int32_field",
			Random.Shared.Next(),
			feature.SetField,
			feature.GetFieldAsInteger);

		TestField(
			"int64_field",
			Random.Shared.NextInt64(),
			feature.SetFieldInteger64,
			feature.GetFieldAsInteger64);

		TestField(
			"double_field",
			Random.Shared.NextDouble(),
			feature.SetField,
			feature.GetFieldAsDouble);

		TestField(
			"string_field",
			UnicodeString,
			feature.SetField,
			feature.GetFieldAsString);

		TestField(
			"date_time_field",
			DateTime.Now,
			feature.SetField,
			feature.GetFieldAsDateTime,
			TimesEqualAtMilliseconds);

		TestField(
			"date_time_field",
			DateTime.UtcNow,
			feature.SetField,
			feature.GetFieldAsDateTime,
			TimesEqualAtMilliseconds);

		TestField(
			"int32_list_field",
			Enumerable.Range(0, 50).Select(_ => Random.Shared.Next()).ToArray(),
			feature.SetFieldIntegerList,
			n => feature.GetFieldAsIntegerList(n, out var _),
			(a, b) => a.SequenceEqual(b));

		TestField(
			"int64_list_field",
			Enumerable.Range(0, 50).Select(_ => Random.Shared.NextInt64()).ToArray(),
			feature.SetFieldInteger64List,
			n => feature.GetFieldAsInteger64List(n, out var _),
			(a, b) => a.SequenceEqual(b));

		TestField(
			"double_list_field",
			Enumerable.Range(0, 50).Select(_ => Random.Shared.NextDouble()).ToArray(),
			feature.SetFieldDoubleList,
			n => feature.GetFieldAsDoubleList(n, out var _),
			(a, b) => a.SequenceEqual(b));

		TestField(
			"string_list_field",
			Enumerable.Range(0, 50).Select(_ => Random.Shared.Next().ToString()).ToArray(),
			feature.SetFieldStringList,
			feature.GetFieldAsStringList,
			(a, b) => a.SequenceEqual(b));

		TestField(
			"binary_field",
			Enumerable.Range(0, 50).Select(_ => (byte)Random.Shared.Next(0, 256)).ToArray(),
			feature.SetFieldBinary,
			n => feature.GetFieldAsBinary(n, out var _),
			(a, b) => a.SequenceEqual(b));
	}
	private static void CreateFieldDefn(Layer dLayer, string fieldName, FieldType type)
	{
		using var fieldDef = new FieldDefn(fieldName, type);
		if (dLayer.CreateField(fieldDef, 1) != 0)
			throw new Exception("Failed to create a field definition on layer.");
	}
	private static void TestField<T>(string fieldName, T value, Action<string, T> setField, Func<string, T> getField, Func<T, T, bool> equals = null)
	{
		Console.WriteLine($"  Testing field '{fieldName}' with type {typeof(T).Name}...");
		Console.WriteLine($"    Setting '{fieldName}'");
		setField(fieldName, value);
		Console.WriteLine($"    Getting '{fieldName}'");
		var valuesGet = getField(fieldName);
		Console.WriteLine($"    Comparing '{fieldName}' set values with retrieved values");
		if (!equals?.Invoke(value, valuesGet) ?? !value.Equals(valuesGet))
			throw new Exception($"Mismatch between '{fieldName}' set and retrieved values.");

		Console.WriteLine($"    Successfully set and got value from '{fieldName}'");
	}
	private static bool TimesEqualAtMilliseconds(DateTime a, DateTime b)
		=> a.Ticks / 10_000 == b.Ticks / 10_000 && a.Kind == b.Kind;

	public static AlgorithmArg OfName(this IEnumerable<AlgorithmArg> algorithms, string name)
		=> algorithms.Single(arg => arg.GetName() == name);
	public static IEnumerable<AlgorithmArg> OfTypeName(this IEnumerable<AlgorithmArg> algorithms, string typeName)
		=> algorithms.Where(arg => Gdal.AlgorithmArgTypeName(arg.GetType_()) == typeName);
	public static IEnumerable<AlgorithmArg> OfListType(this IEnumerable<AlgorithmArg> algorithms, bool isList)
		=> algorithms.Where(arg => Gdal.AlgorithmArgTypeIsList(arg.GetType_()) == isList);
	public static IEnumerable<AlgorithmArg> Args(this Algorithm algorithm)
		=> algorithm.GetArgNames().Select(argName => algorithm.GetArg(argName));
	public static Algorithm OfName(this IEnumerable<Algorithm> algorithms, string name)
		=> algorithms.Single(alg => alg.GetName() == name);
	public static IEnumerable<Algorithm> SubAlgorithms(this Algorithm algorithm)
		=> algorithm.HasSubAlgorithms() ? algorithm.GetSubAlgorithmNames().Select(name => algorithm.InstantiateSubAlgorithm(name)) : Enumerable.Empty<Algorithm>();
	public static IEnumerable<Algorithm> Algorithms(this AlgorithmRegistry reg)
		=> reg.GetAlgNames().Select(algName => reg.InstantiateAlg(algName));
}
