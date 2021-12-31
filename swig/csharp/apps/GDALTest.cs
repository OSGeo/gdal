using System;
using OSGeo.GDAL;

namespace testapp
{
    class GdalTest
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Testing GDAL C# Bindings");
            Gdal.UseExceptions();
            Console.WriteLine($"Gdal version {Gdal.VersionInfo(null)}");
        }
    }
}
