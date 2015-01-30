/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Program to generate a UMN MapServer compatible tile index for a
 *           set of OGR data sources. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

import org.gdal.gdal.gdal;
import org.gdal.ogr.DataSource;
import org.gdal.ogr.Driver;
import org.gdal.ogr.Feature;
import org.gdal.ogr.FeatureDefn;
import org.gdal.ogr.FieldDefn;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.Layer;
import org.gdal.ogr.ogr;
import org.gdal.osr.SpatialReference;

/* Note : this is the most direct port of ogrtindex.cpp possible */
/* It could be made much more java'ish ! */

public class ogrtindex {

   public static void main(String[] args) {

      boolean bLayersWildcarded = true; 
      int nFirstSourceDataset = -1; 
      String pszFormat = "ESRI Shapefile";
      String pszTileIndexField = "LOCATION";
      String pszOutputName = null;
      boolean write_absolute_path = false;
      boolean skip_different_projection = false;
      String current_path = null;
      boolean accept_different_schemas = false;
      boolean bFirstWarningForNonMatchingAttributes = true;

      ogr.DontUseExceptions();

      /* -------------------------------------------------------------------- */
      /*      Register format(s).                                             */
      /* -------------------------------------------------------------------- */
      // fixed: http://osgeo-org.1803224.n2.nabble.com/GDAL-Java-Binding-meomory-problem-under-intensive-method-calls-td7155011.html#a7157916
      if( ogr.GetDriverCount() == 0 )
         ogr.RegisterAll(); 

      /* -------------------------------------------------------------------- */
      /*      Processing command line arguments.                              */
      /* -------------------------------------------------------------------- */
      args = ogr.GeneralCmdLineProcessor( args );

      if( args.length < 2 )
      {
         Usage();
         return;
      }

      for( int iArg = 0; iArg < args.length; iArg++ )
      {
         if( args[iArg].equalsIgnoreCase("-f") && iArg < args.length-1 )
         {
            pszFormat = args[++iArg];
         }
         else if( args[iArg].equalsIgnoreCase("-write_absolute_path") )
         {
            write_absolute_path = true;
         }
         else if( args[iArg].equalsIgnoreCase("-skip_different_projection") )
         {
            skip_different_projection = true;
         }
         else if( args[iArg].equalsIgnoreCase("-accept_different_schemas") )
         {
            accept_different_schemas = true;
         }
         else if( args[iArg].equalsIgnoreCase("-tileindex") && iArg < args.length-1 )
         {
            pszTileIndexField = args[++iArg];
         }
         else if( args[iArg].equalsIgnoreCase("-lnum") 
               || args[iArg].equalsIgnoreCase("-lname") )
         {
            iArg++;
            bLayersWildcarded = false;
         }
         else if( args[iArg].charAt(0) == '-' )
            Usage();
         else if( pszOutputName == null )
            pszOutputName = args[iArg];
         else if( nFirstSourceDataset == -1 )
            nFirstSourceDataset = iArg;
      }

      if( pszOutputName == null || nFirstSourceDataset == -1 )
         Usage();

      /* -------------------------------------------------------------------- */
      /*      Try to open as an existing dataset for update access.           */
      /* -------------------------------------------------------------------- */
      DataSource poDstDS;
      Layer poDstLayer = null;

      poDstDS = ogr.Open( pszOutputName, true );

      /* -------------------------------------------------------------------- */
      /*      If that failed, find the driver so we can create the tile index.*/
      /* -------------------------------------------------------------------- */
      if( poDstDS == null )
      {        
         Driver poDriver = null;

         for( int iDriver = 0; iDriver < ogr.GetDriverCount() && poDriver == null; iDriver++ )
         {
            poDriver = ogr.GetDriverByName(pszFormat);
         }

         if( poDriver == null )
         {
            System.err.print("Unable to find driver '"+pszFormat+"'.\n");
            System.err.print("The following drivers are available:\n" );

            for( int iDriver = 0; iDriver < ogr.GetDriverCount(); iDriver++ )
            {
               System.err.print("  . '"+ogr.GetDriver(iDriver).GetName()+"'\n");
            }
            return;
         }

         if( !poDriver.TestCapability( ogr.ODrCCreateDataSource ) )
         {
            System.err.print(pszFormat + " driver does not support data source creation.\n");                  
            return;
         }

         /* -------------------------------------------------------------------- */
         /*      Now create it.                                                  */
         /* -------------------------------------------------------------------- */

         poDstDS = poDriver.CreateDataSource( pszOutputName );
         if( poDstDS == null )
         {
            System.err.print(pszFormat + " driver failed to create "+pszOutputName+"\n");
            return;
         }

         if ( poDstDS.GetLayerCount() == 0 )
         {
            FieldDefn oLocation = new FieldDefn( pszTileIndexField, ogr.OFTString );

            oLocation.SetWidth( 200 );

            if( nFirstSourceDataset < args.length-2 && args[nFirstSourceDataset].charAt(0) == '-' )
            {
               nFirstSourceDataset++;
            }

            SpatialReference poSrcSpatialRef = null;

            /* Fetches the SRS of the first layer and use it when creating the tileindex layer */
            if (nFirstSourceDataset < args.length)
            {
               DataSource poDS = ogr.Open( args[nFirstSourceDataset],false );

               if (poDS!=null)
               {
                  for(int iLayer = 0; iLayer < poDS.GetLayerCount(); iLayer++ )
                  {
                     boolean bRequested = bLayersWildcarded;
                     Layer poLayer = poDS.GetLayer(iLayer);

                     for(int iArg = 0; iArg < args.length && !bRequested; iArg++ )
                     {
                        if( args[iArg].equalsIgnoreCase("-lnum") 
                              && Integer.parseInt(args[iArg+1]) == iLayer )
                           bRequested = true;
                        else if( args[iArg].equalsIgnoreCase("-lname") 
                              && args[iArg+1].equalsIgnoreCase(poLayer.GetLayerDefn().GetName()) )
                           bRequested = true;
                     }

                     if( !bRequested )
                        continue;

                     if ( poLayer.GetSpatialRef() != null)
                        poSrcSpatialRef = poLayer.GetSpatialRef().Clone();
                     break;
                  }
               }

               poDS.delete();
            }

            poDstLayer = poDstDS.CreateLayer( "tileindex", poSrcSpatialRef );
            poDstLayer.CreateField( oLocation, ogr.OFTString );

            /* with the OGR Java bindings, avoid using the delete() methods,
             * except on the datasource objects, where it is necessary to close properly the
             * native file handles.
             */  
            // poSrcSpatialRef.delete();
         }
      }

      /* -------------------------------------------------------------------- */
      /*      Identify target layer and field.                                */
      /* -------------------------------------------------------------------- */
      int   iTileIndexField;

      poDstLayer = poDstDS.GetLayer(0);
      if( poDstLayer == null )
      {
         System.err.print("Can't find any layer in output tileindex!\n" );
         return;
      }

      iTileIndexField = 
         poDstLayer.GetLayerDefn().GetFieldIndex( pszTileIndexField );
      if( iTileIndexField == -1 )
      {
         System.err.print("Can't find "+pszTileIndexField+" field in tile index dataset.\n");
         return;
      }

      FeatureDefn poFeatureDefn = null;

      /* Load in memory existing file names in SHP */
      int nExistingLayers = 0;
      String[] existingLayersTab = null;
      SpatialReference alreadyExistingSpatialRef = null;
      boolean alreadyExistingSpatialRefValid = false;
      nExistingLayers = (int)poDstLayer.GetFeatureCount();
      if (nExistingLayers > 0)
      {
         existingLayersTab = new String[nExistingLayers];
         for(int i=0;i<nExistingLayers;i++)
         {
            Feature feature = poDstLayer.GetNextFeature();
            existingLayersTab[i] = feature.GetFieldAsString( iTileIndexField);
            if (i == 0)
            {
               DataSource       poDS;
               String filename = existingLayersTab[i];
               int j;
               for(j=filename.length()-1;j>=0;j--)
               {
                  if (filename.charAt(j) == ',')
                     break;
               }
               if (j >= 0)
               {
                  int iLayer = Integer.parseInt(filename.substring(j + 1));
                  filename = filename.substring(0, j);
                  poDS = ogr.Open(filename,false );
                  if (poDS!=null)
                  {
                     Layer poLayer = poDS.GetLayer(iLayer);
                     if (poLayer!=null)
                     {
                        alreadyExistingSpatialRefValid = true;
                        alreadyExistingSpatialRef =
                           (poLayer.GetSpatialRef()!=null) ? poLayer.GetSpatialRef().Clone() : null;

                           if (poFeatureDefn == null) {
                              poFeatureDefn = CloneFeatureDefn(poLayer.GetLayerDefn()); // XXX: no Clone supported in java binding!!
                           }
                     }
                     poDS.delete();
                  }
               }
            }
         }
      }

      /* ignore check */
      //if (write_absolute_path)
      //{
      //   current_path = CPLGetCurrentDir();
      //   if (current_path == null)
      //   {
      //      fprintf( stderr, "This system does not support the CPLGetCurrentDir call. "
      //      "The option -write_absolute_path will have no effect\n");
      //      write_absolute_path = false;
      //   }
      //}
      /* ==================================================================== */
      /*      Process each input datasource in turn.                          */
      /* ==================================================================== */

      for(; nFirstSourceDataset < args.length; nFirstSourceDataset++ )
      {
         DataSource       poDS;

         if( args[nFirstSourceDataset].charAt(0) == '-' )
         {
            nFirstSourceDataset++;
            continue;
         }

         String fileNameToWrite;

         //VSIStatBuf sStatBuf;
         // FIXME: handle absolute path check 
         //if (write_absolute_path && CPLIsFilenameRelative( args[nFirstSourceDataset] ) &&
         //      VSIStat( args[nFirstSourceDataset], &sStatBuf ) == 0)
         //{
         //   fileNameToWrite = CPLStrdup(CPLProjectRelativeFilename(current_path,args[nFirstSourceDataset]));
         //}
         //else
         //{
         //   fileNameToWrite = args[nFirstSourceDataset];
         //}
         fileNameToWrite = args[nFirstSourceDataset];

         poDS = ogr.Open( args[nFirstSourceDataset], false );

         if( poDS == null )
         {
            System.err.print("Failed to open dataset "+args[nFirstSourceDataset]+", skipping.\n");
            continue;
         }

         /* -------------------------------------------------------------------- */
         /*      Check all layers, and see if they match requests.               */
         /* -------------------------------------------------------------------- */
         for(int iLayer = 0; iLayer < poDS.GetLayerCount(); iLayer++ )
         {
            boolean bRequested = bLayersWildcarded;
            Layer poLayer = poDS.GetLayer(iLayer);

            for(int iArg = 0; iArg < args.length && !bRequested; iArg++ )
            {
               if( args[iArg].equalsIgnoreCase("-lnum") 
                     && Integer.parseInt(args[iArg+1]) == iLayer )
                  bRequested = true;
               else if( args[iArg].equalsIgnoreCase("-lname") 
                     && args[iArg+1].equalsIgnoreCase(poLayer.GetLayerDefn().GetName()) )
                  bRequested = true;
            }

            if( !bRequested )
               continue;

            /* Checks that the layer is not already in tileindex */
            int i;
            for(i=0;i<nExistingLayers;i++)
            {
               String szLocation = fileNameToWrite+","+iLayer;
               if (szLocation.equalsIgnoreCase(existingLayersTab[i]))
               {
                  System.err.println("Layer "+iLayer+" of "+args[nFirstSourceDataset]+" is already in tileindex. Skipping it.\n");
                  break;
               }
            }
            if (i != nExistingLayers)
            {
               continue;
            }

            SpatialReference spatialRef = poLayer.GetSpatialRef();
            if (alreadyExistingSpatialRefValid)
            {
               if ((spatialRef != null && alreadyExistingSpatialRef != null &&
                     spatialRef.IsSame(alreadyExistingSpatialRef) == 0) ||
                     ((spatialRef != null) != (alreadyExistingSpatialRef != null)))
               {
                  System.err.print("Warning : layer "+iLayer+" of "+args[nFirstSourceDataset]+" is not using the same projection system as "
                        + "other files in the tileindex. This may cause problems when "
                        + "using it in MapServer for example."+((skip_different_projection) ? " Skipping it" : "")+"\n");
                  ;
                  if (skip_different_projection)
                  {
                     continue;
                  }
               }
            }
            else
            {
               alreadyExistingSpatialRefValid = true;
               alreadyExistingSpatialRef = (spatialRef!=null) ? spatialRef.Clone() : null;
            }

            /* -------------------------------------------------------------------- */
            /*    Check if all layers in dataset have the same attributes  schema. */
            /* -------------------------------------------------------------------- */
            if( poFeatureDefn == null )
            {
               poFeatureDefn = CloneFeatureDefn(poLayer.GetLayerDefn()); // XXX: no Clone supported in java binding!!
            }
            else if ( !accept_different_schemas )
            {
               FeatureDefn poFeatureDefnCur = poLayer.GetLayerDefn();
               assert(null != poFeatureDefnCur);

               int fieldCount = poFeatureDefnCur.GetFieldCount();

               if( fieldCount != poFeatureDefn.GetFieldCount())
               {
                  System.err.print("Number of attributes of layer "+poLayer.GetLayerDefn().GetName()+" of "+args[nFirstSourceDataset]+" does not match ... skipping it.\n");

                  if (bFirstWarningForNonMatchingAttributes)
                  {
                     System.err.print("Note : you can override this behaviour with -accept_different_schemas option\n"
                           + "but this may result in a tileindex incompatible with MapServer\n");
                     bFirstWarningForNonMatchingAttributes = false;
                  }
                  continue;
               }

               boolean bSkip = false;
               for( int fn = 0; fn < poFeatureDefnCur.GetFieldCount(); fn++ )
               {
                  FieldDefn poField = poFeatureDefn.GetFieldDefn(fn);
                  FieldDefn poFieldCur = poFeatureDefnCur.GetFieldDefn(fn);

                  /* XXX - Should those pointers be checked against null? */ 
                  assert(null != poField);
                  assert(null != poFieldCur);

                  if( !poField.GetTypeName().equalsIgnoreCase(poFieldCur.GetTypeName()) 
                        || poField.GetWidth() != poFieldCur.GetWidth() 
                        || poField.GetPrecision() != poFieldCur.GetPrecision() 
                        || !poField.GetNameRef().equalsIgnoreCase(poFieldCur.GetNameRef()) )
                  {
                     System.err.print("Schema of attributes of layer "+poLayer.GetLayerDefn().GetName()+" of "+args[nFirstSourceDataset]+" does not match ... skipping it.\n");

                     if (bFirstWarningForNonMatchingAttributes)
                     {
                        System.err.print("Note : you can override this behaviour with -accept_different_schemas option\n"
                              + "but this may result in a tileindex incompatible with MapServer\n");
                        bFirstWarningForNonMatchingAttributes = false;
                     }
                     bSkip = true; 
                     break;
                  }
               }

               if (bSkip)
                  continue;
            }


            /* -------------------------------------------------------------------- */
            /*      Get layer extents, and create a corresponding polygon           */
            /*      geometry.                                                       */
            /* -------------------------------------------------------------------- */
            double sExtents[] = poLayer.GetExtent(true);
            Geometry/*Polygon*/ oRegion = new Geometry(ogr.wkbPolygon);
            Geometry/*LinearRing*/ oRing = new Geometry(ogr.wkbLinearRing);
             
            if (sExtents == null) {
               System.err.print("GetExtent() failed on layer "+poLayer.GetLayerDefn().GetName()+" of "+args[nFirstSourceDataset]+", skipping.\n");
               continue;
            }
                                    
            // XXX: sExtents [minX, maxX, minY, maxY]
            //oRing.addPoint( sExtents.MinX, sExtents.MinY );
            //oRing.addPoint( sExtents.MinX, sExtents.MaxY );
            //oRing.addPoint( sExtents.MaxX, sExtents.MaxY );
            //oRing.addPoint( sExtents.MaxX, sExtents.MinY );
            //oRing.addPoint( sExtents.MinX, sExtents.MinY );
            oRing.AddPoint_2D( sExtents[0], sExtents[2] );
            oRing.AddPoint_2D( sExtents[0], sExtents[3] );
            oRing.AddPoint_2D( sExtents[1], sExtents[3] );
            oRing.AddPoint_2D( sExtents[1], sExtents[2] );
            oRing.AddPoint_2D( sExtents[0], sExtents[2] );

            oRegion.AddGeometry( oRing );

            /* -------------------------------------------------------------------- */
            /*      Add layer to tileindex.                                         */
            /* -------------------------------------------------------------------- */
            String        szLocation = fileNameToWrite+","+iLayer;
            Feature  oTileFeat = new Feature( poDstLayer.GetLayerDefn() );

            oTileFeat.SetGeometry( oRegion );
            oTileFeat.SetField( iTileIndexField, szLocation );

            if( poDstLayer.CreateFeature( oTileFeat ) != ogr.OGRERR_NONE )
            {
               System.err.print("Failed to create feature on tile index ... terminating." );
               poDS.delete();
               poDstDS.delete();
               return;
            }
         }

         /* -------------------------------------------------------------------- */
         /*      Cleanup this data source.                                       */
         /* -------------------------------------------------------------------- */
         poDS.delete();
      }

      /* -------------------------------------------------------------------- */
      /*      Close tile index and clear buffers.                             */
      /* -------------------------------------------------------------------- */
      poDstDS.delete();
      //OGRFeatureDefn::DestroyFeatureDefn( poFeatureDefn );

      //if (alreadyExistingSpatialRef != null)
      //   alreadyExistingSpatialRef.delete();


   }



   /************************************************************************/
   /*                               Usage()                                */
   /************************************************************************/

   static void Usage()

   {
      System.out.print( 
            "Usage: ogrtindex [-lnum n]... [-lname name]... [-f output_format]\n" 
            + "                 [-write_absolute_path] [-skip_different_projection]\n"
            + "                 [-accept_different_schemas]\n"
            + "                 output_dataset src_dataset...\n" );
      System.out.print( "\n" );
      System.out.print( 
            "  -lnum n: Add layer number 'n' from each source file\n"
            + "           in the tile index.\n" );
      System.out.print( 
            "  -lname name: Add the layer named 'name' from each source file\n"
            + "               in the tile index.\n" );
      System.out.print( 
            "  -f output_format: Select an output format name.  The default\n"
            + "                    is to create a shapefile.\n" );
      System.out.print( 
            "  -tileindex field_name: The name to use for the dataset name.\n"
            + "                         Defaults to LOCATION.\n" );
      System.out.print( "  -write_absolute_path: Filenames are written with absolute paths.\n" );
      System.out.print( 
            "  -skip_different_projection: Only layers with same projection ref \n"
            + "        as layers already inserted in the tileindex will be inserted.\n" );
      System.out.print( 
            "  -accept_different_schemas: by default ogrtindex checks that all layers inserted\n"
            + "                             into the index have the same attribute schemas. If you\n"
            + "                             specify this option, this test will be disabled. Be aware that\n"
            + "                             resulting index may be incompatible with MapServer!\n" );
      System.out.print( "\n" );
      System.out.print( 
            "If no -lnum or -lname arguments are given it is assumed that\n"
            + "all layers in source datasets should be added to the tile index\n"
            + "as independent records.\n" );
   }

    /* Adhoc method to workaround the lack of a FeatureDefn.Clone() method */
    static FeatureDefn CloneFeatureDefn(FeatureDefn poSrcFeatureDefn)
    {
        FeatureDefn poFeatureDefn = new FeatureDefn(poSrcFeatureDefn.GetName());
        poFeatureDefn.SetGeomType(poSrcFeatureDefn.GetGeomType());
        for(int fi = 0; fi < poSrcFeatureDefn.GetFieldCount(); fi++)
            poFeatureDefn.AddFieldDefn(poSrcFeatureDefn.GetFieldDefn(fi));
        return poFeatureDefn;
    }
}
