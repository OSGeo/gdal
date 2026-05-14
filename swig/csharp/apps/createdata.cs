/******************************************************************************
 *
 * Name:     createdata.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app to create a spatial data source and a layer.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2026, Paul Harwood
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/


using System;

using OSGeo.OGR;
using OSGeo.OSR;


/**

 * <p>Title: GDAL C# createdata example.</p>
 * <p>Description: A sample app to create a spatial data source and a layer.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to create a layer.
/// </summary>

class CreateData
{

    public static void usage()

    {
        Console.WriteLine("usage: createdata {data source name} {layername}");
        Environment.Exit(-1);
    }

    public static void Main(string[] args)
    {

        if (args.Length != 2) usage();

        // Using early initialization of System.Console
        Console.WriteLine("");

        /* -------------------------------------------------------------------- */
        /*      Register format(s).                                             */
        /* -------------------------------------------------------------------- */
        Ogr.RegisterAll();

        /* -------------------------------------------------------------------- */
        /*      Get driver                                                      */
        /* -------------------------------------------------------------------- */
        Driver drv = Ogr.GetDriverByName("ESRI Shapefile");

        if (drv == null)
        {
            Console.WriteLine("Can't get driver.");
            System.Environment.Exit(-1);
        }

        /* -------------------------------------------------------------------- */
        /*      Creating the datasource                                         */
        /* -------------------------------------------------------------------- */

        using (DataSource ds = drv.CreateDataSource(args[0], new string[] { }))
        {
            if (drv == null)
            {
                Console.WriteLine("Can't create the datasource.");
                System.Environment.Exit(-1);
            }

            /* -------------------------------------------------------------------- */
            /*      Creating the layer                                              */
            /* -------------------------------------------------------------------- */
            int i;
            for (i = 0; i < ds.GetLayerCount(); i++)
            {
                using (Layer layeri = ds.GetLayerByIndex(i))
                {
                    if (layeri != null && layeri.GetLayerDefn().GetName() == args[1])
                    {
                        Console.WriteLine("Layer already existed. Recreating it.\n");
                        ds.DeleteLayer(i);
                        break;
                    }
                }
            }

            using (Layer layer = ds.CreateLayer(args[1], null, wkbGeometryType.wkbPoint, new string[] { }))
            {
                if (layer == null)
                {
                    Console.WriteLine("Layer creation failed.");
                    System.Environment.Exit(-1);
                }

                /* -------------------------------------------------------------------- */
                /*      Adding attribute fields                                         */
                /* -------------------------------------------------------------------- */

                using (FieldDefn fdefn = new FieldDefn("Name", FieldType.OFTString))
                {
                    fdefn.SetWidth(32);

                    if (layer.CreateField(fdefn, 1) != 0)
                    {
                        Console.WriteLine("Creating Name field failed.");
                        Environment.Exit(-1);
                    }
                }

                using (FieldDefn fdefn = new FieldDefn("IntField", FieldType.OFTInteger))
                {
                    if (layer.CreateField(fdefn, 1) != 0)
                    {
                        Console.WriteLine("Creating IntField field failed.");
                        Environment.Exit(-1);
                    }
                }

                using (FieldDefn fdefn = new FieldDefn("DbleField", FieldType.OFTReal))
                {
                    if (layer.CreateField(fdefn, 1) != 0)
                    {
                        Console.WriteLine("Creating DbleField field failed.");
                        Environment.Exit(-1);
                    }
                }

                using (FieldDefn fdefn = new FieldDefn("DateField", FieldType.OFTDate))
                {
                    if (layer.CreateField(fdefn, 1) != 0)
                    {
                        Console.WriteLine("Creating DateField field failed.");
                        Environment.Exit(-1);
                    }
                }
                /* -------------------------------------------------------------------- */
                /*      Adding features                                                 */
                /* -------------------------------------------------------------------- */

                using (Feature feature = new Feature(layer.GetLayerDefn()))
                {
                    feature.SetField("Name", "value");
                    feature.SetField("IntField", (int)123);
                    feature.SetField("DbleField", (double)12.345);
                    feature.SetField("DateField", 2007, 3, 15, 18, 24, 30, 0);

                    using (Geometry geom = Geometry.CreateFromWkt("POINT(47.0 19.2)"))
                    {

                        if (feature.SetGeometry(geom) != 0)
                        {
                            Console.WriteLine("Failed add geometry to the feature");
                            System.Environment.Exit(-1);
                        }
                    }

                    if (layer.CreateFeature(feature) != 0)
                    {
                        Console.WriteLine("Failed to create feature in shapefile");
                        System.Environment.Exit(-1);
                    }
                }
                ReportLayer(layer);
            }
        }
    }


    public static void ReportLayer(Layer layer)
    {
        using (FeatureDefn def = layer.GetLayerDefn())
        {
            using (Envelope ext = new Envelope())
            {
                Console.WriteLine("Layer name: " + def.GetName());
                Console.WriteLine("Feature Count: " + layer.GetFeatureCount(1));
                layer.GetExtent(ext, 1);
                Console.WriteLine("Extent: " + ext.MinX + "," + ext.MaxX + "," +
                    ext.MinY + "," + ext.MaxY);

            }

            /* -------------------------------------------------------------------- */
            /*      Reading the spatial reference                                   */
            /* -------------------------------------------------------------------- */
            using (OSGeo.OSR.SpatialReference sr = layer.GetSpatialRef())
            {
                string srs_wkt;
                if (sr != null)
                {
                    sr.ExportToPrettyWkt(out srs_wkt, 1);
                }
                else
                    srs_wkt = "(unknown)";


                Console.WriteLine("SRS WKT: " + srs_wkt);
            }

            /* -------------------------------------------------------------------- */
            /*      Reading the fields                                              */
            /* -------------------------------------------------------------------- */
            Console.WriteLine("Field definition:");

            for (int iAttr = 0; iAttr < def.GetFieldCount(); iAttr++)
                using (FieldDefn fdef = def.GetFieldDefn(iAttr))
                {
                    Console.WriteLine(fdef.GetNameRef() + ": " +
                        fdef.GetFieldTypeName(fdef.GetFieldType()) + " (" +
                        fdef.GetWidth() + "." +
                        fdef.GetPrecision() + ")");
                }

            /* -------------------------------------------------------------------- */
            /*      Reading the shapes                                              */
            /* -------------------------------------------------------------------- */
            Console.WriteLine("");
            Feature feat;
            while ((feat = layer.GetNextFeature()) != null)
            {
                ReportFeature(feat, def);
                feat.Dispose();
            }
        }
    }

    public static void ReportFeature(Feature feat, FeatureDefn def)
    {
        Console.WriteLine("Feature(" + def.GetName() + "): " + feat.GetFID());

        for (int iField = 0; iField < feat.GetFieldCount(); iField++)
            using (FieldDefn fdef = def.GetFieldDefn(iField))
            {
                Console.Write(fdef.GetNameRef() + " (" +
                    fdef.GetFieldTypeName(fdef.GetFieldType()) + ") = ");

                if (feat.IsFieldSet(iField))
                    Console.WriteLine(feat.GetFieldAsString(iField));
                else
                    Console.WriteLine("(null)");
            }

        if (feat.GetStyleString() != null)
            Console.WriteLine("  Style = " + feat.GetStyleString());

        using (Geometry geom = feat.GetGeometryRef())
        using (Envelope env = new Envelope())
            if (geom != null)
            {
                Console.WriteLine("  " + geom.GetGeometryName() +
                    "(" + geom.GetGeometryType() + ")");

                geom.GetEnvelope(env);
                Console.WriteLine("   ENVELOPE: " + env.MinX + "," + env.MaxX + "," +
                    env.MinY + "," + env.MaxY);

                string geom_wkt;
                geom.ExportToWkt(out geom_wkt);
                Console.WriteLine("  " + geom_wkt);

                Console.WriteLine("");
            }
    }
}
