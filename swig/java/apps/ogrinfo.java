/******************************************************************************
 * $Id$
 *
 * Name:     ogrinfo.java
 * Project:  GDAL SWIG Interface
 * Purpose:  Java port of ogrinfo application, simple client for viewing OGR driver data.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 * Port from ogrinfo.cpp by Frank Warmerdam
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
 * Copyright (c) 1999, Frank Warmerdam
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
import org.gdal.ogr.*;
import org.gdal.osr.*;
import java.util.*;


/* Note : this is the most direct port of ogrinfo.cpp possible */
/* It could be made much more java'ish ! */

public class ogrinfo
{
    static boolean     bReadOnly = false;
    static boolean     bVerbose = true;
    static boolean     bSummaryOnly = false;
    
    static final int OGRNullFID = -1;
    static int     nFetchFID = OGRNullFID;
    
    public static void main(String[] args)
    {
        String pszWHERE = null;
        String pszDataSource = null;
        Vector papszLayers = new Vector();
        Geometry poSpatialFilter = null;
        int         nRepeatCount = 1;
        boolean bAllLayers = false;
        String pszSQLStatement = null;
        String pszDialect = null;

        ogr.DontUseExceptions();

/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
        if( ogr.GetDriverCount() == 0 )
            ogr.RegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
        args = ogr.GeneralCmdLineProcessor(args);

        for(int i=0;i<args.length;i++)
        {
            if (args[i].equals("-ro"))
                bReadOnly = true;
            else if (args[i].equals("-q"))
                bVerbose = false;
            else if (args[i].equals("-spat") && i + 4 < args.length)
            {
                Geometry oRing = new Geometry(ogrConstants.wkbLinearRing);
                double xmin = new Double(args[++i]).doubleValue();
                double ymin = new Double(args[++i]).doubleValue();
                double xmax = new Double(args[++i]).doubleValue();
                double ymax = new Double(args[++i]).doubleValue();
                oRing.AddPoint(xmin, ymin);
                oRing.AddPoint(xmin, ymax);
                oRing.AddPoint(xmax, ymax);
                oRing.AddPoint(xmax, ymin);
                oRing.AddPoint(xmin, ymin);
                
                poSpatialFilter = new Geometry(ogrConstants.wkbPolygon);
                poSpatialFilter.AddGeometry(oRing);
            }
            else if (args[i].equals("-where") && i + 1 < args.length)
            {
                pszWHERE = args[++i];
            }
            else if(args[i].equals("-sql") && i + 1 < args.length)
            {
                pszSQLStatement = args[++i];
            }
            else if(args[i].equals("-dialect") && i + 1 < args.length)
            {
                pszDialect = args[++i];
            }
            else if( args[i].equals("-rc") && i + 1 < args.length)
            {
                nRepeatCount = new Integer(args[++i]).intValue();
            }
            else if( args[i].equals("-al") )
            {
                bAllLayers = true;
            }
            else if( args[i].equals("-so")  || args[i].equals("-summary")  )
            {
                bSummaryOnly = true;
            }
            /*
            else if( EQUALN(papszArgv[iArg],"-fields=", strlen("-fields=")) )
            {
                char* pszTemp = (char*)CPLMalloc(32 + strlen(papszArgv[iArg]));
                sSystem.out.print(pszTemp, "DISPLAY_FIELDS=%s", papszArgv[iArg] + strlen("-fields="));
                papszOptions = CSLAddString(papszOptions, pszTemp);
                CPLFree(pszTemp);
            }
            else if( EQUALN(papszArgv[iArg],"-geom=", strlen("-geom=")) )
            {
                char* pszTemp = (char*)CPLMalloc(32 + strlen(papszArgv[iArg]));
                sSystem.out.print(pszTemp, "DISPLAY_GEOMETRY=%s", papszArgv[iArg] + strlen("-geom="));
                papszOptions = CSLAddString(papszOptions, pszTemp);
                CPLFree(pszTemp);
            }
            */
            else if( args[i].charAt(0) == '-' )
            {
                Usage();
                return;
            }
            else if( pszDataSource == null )
                pszDataSource = args[i];
            else
            {
                papszLayers.addElement( args[i] );
                bAllLayers = false;
            }
        }

        if( pszDataSource == null )
        {
            Usage();
            return;
        }
/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
        
        DataSource poDS = ogr.Open(pszDataSource, !bReadOnly);
        if (poDS == null && !bReadOnly)
        {
            poDS = ogr.Open(pszDataSource, false);
            if (poDS == null && bVerbose)
            {
                System.out.println( "Had to open data source read-only.");
            }
        }
        
/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
        if( poDS == null )
        {
            System.out.print( "FAILURE:\n" +
                "Unable to open datasource `" + pszDataSource + "' with the following drivers.\n");
            
            for( int iDriver = 0; iDriver < ogr.GetDriverCount(); iDriver++ )
            {
                System.out.println( "  -> " + ogr.GetDriver(iDriver).GetName() );
            }
            
            return;
        }
        
        Driver poDriver = poDS.GetDriver();

/* -------------------------------------------------------------------- */
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
        if( bVerbose )
            System.out.println( "INFO: Open of `" + pszDataSource + "'\n" +
                    "      using driver `" + poDriver.GetName() + "' successful." );

/* -------------------------------------------------------------------- */
/*      Special case for -sql clause.  No source layers required.       */
/* -------------------------------------------------------------------- */
        if( pszSQLStatement != null )
        {
            nRepeatCount = 0;  // skip layer reporting.
    
            if( papszLayers.size() > 0 )
                System.out.println( "layer names ignored in combination with -sql." );
            
            Layer poResultSet = poDS.ExecuteSQL( pszSQLStatement, poSpatialFilter, pszDialect );
    
            if( poResultSet != null )
            {
                if( pszWHERE != null )
                {
                    if( poResultSet.SetAttributeFilter( pszWHERE ) != ogr.OGRERR_NONE )
                    {
                        System.err.println("FAILURE: SetAttributeFilter(" + pszWHERE + ") failed.");
                        return;
                    }
                }
    
                ReportOnLayer( poResultSet, null, null );
                poDS.ReleaseResultSet( poResultSet );
            }
        }

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
        //CPLDebug( "OGR", "GetLayerCount() = %d\n", poDS->GetLayerCount() );
    
        for( int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++ )
        {
            if (papszLayers.size() == 0)
            {
/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
                for( int iLayer = 0; iLayer < poDS.GetLayerCount(); iLayer++ )
                {
                    Layer        poLayer = poDS.GetLayer(iLayer);

                    if( poLayer == null )
                    {
                        System.out.println( "FAILURE: Couldn't fetch advertised layer " + iLayer + "!");
                        return;
                    }

                    if (!bAllLayers)
                    {
                        System.out.print(
                                (iLayer+1) + ": " + poLayer.GetLayerDefn().GetName() );

                        if( poLayer.GetLayerDefn().GetGeomType() != ogrConstants.wkbUnknown )
                            System.out.print( " (" +
                                    ogr.GeometryTypeToName(
                                        poLayer.GetLayerDefn().GetGeomType()) + ")" );

                        System.out.println();
                    }
                    else
                    {
                        if( iRepeat != 0 )
                            poLayer.ResetReading();

                        ReportOnLayer( poLayer, pszWHERE, poSpatialFilter );
                    }
                }
            }
            else
            {
/* -------------------------------------------------------------------- */
/*      Process specified data source layers.                           */
/* -------------------------------------------------------------------- */
                for(int i = 0; i < papszLayers.size(); i++)
                {
                    Layer poLayer = poDS.GetLayerByName((String)papszLayers.get(i));

                    if( poLayer == null )
                    {
                        System.out.println( "FAILURE: Couldn't fetch requested layer " +
                                            (String)papszLayers.get(i) + "!");
                        return;
                    }

                    if( iRepeat != 0 )
                        poLayer.ResetReading();

                    ReportOnLayer( poLayer, pszWHERE, poSpatialFilter );
                }
            }
        }

    }

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

    public static void Usage()
    {
        System.out.print( "Usage: ogrinfo [--help-general] [-ro] [-q] [-where restricted_where]\n" +
                "               [-spat xmin ymin xmax ymax] [-fid fid]\n" +
                "               [-sql statement] [-al] [-so]\n" +
                "               [--formats]\n" +
                "               datasource_name [layer [layer ...]]\n");
    }

/************************************************************************/
/*                           ReportOnLayer()                            */
/************************************************************************/

    static void ReportOnLayer(Layer poLayer, String pszWHERE, Geometry poSpatialFilter)
    {
        FeatureDefn poDefn = poLayer.GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      Set filters if provided.                                        */
/* -------------------------------------------------------------------- */
        if( pszWHERE != null )
        {
            if( poLayer.SetAttributeFilter( pszWHERE ) != ogr.OGRERR_NONE )
            {
                System.err.println("FAILURE: SetAttributeFilter(" + pszWHERE + ") failed.");
                return;
            }
        }
    
        if( poSpatialFilter != null )
            poLayer.SetSpatialFilter( poSpatialFilter );

/* -------------------------------------------------------------------- */
/*      Report various overall information.                             */
/* -------------------------------------------------------------------- */
        System.out.println();
        
        System.out.println( "Layer name: "+  poDefn.GetName() );
    
        if( bVerbose )
        {
            System.out.println( "Geometry: " +
                    ogr.GeometryTypeToName( poDefn.GetGeomType() ) );
            
            System.out.println( "Feature Count: " + poLayer.GetFeatureCount() );
            
            double oExt[] = poLayer.GetExtent(true);
            if (oExt != null)
                System.out.println("Extent: (" + oExt[0] + ", " + oExt[2] + ") - (" + oExt[1] + ", " + oExt[3] + ")");
    
            String pszWKT;
            
            if( poLayer.GetSpatialRef() == null )
                pszWKT = "(unknown)";
            else
            {
                pszWKT = poLayer.GetSpatialRef().ExportToPrettyWkt();
            }            
    
            System.out.println( "Layer SRS WKT:\n" + pszWKT );
        
            if( poLayer.GetFIDColumn().length() > 0 )
                System.out.println( "FID Column = " + poLayer.GetFIDColumn() );
        
            if( poLayer.GetGeometryColumn().length() > 0 )
                System.out.println( "Geometry Column = " +  poLayer.GetGeometryColumn() );
    
            for( int iAttr = 0; iAttr < poDefn.GetFieldCount(); iAttr++ )
            {
                FieldDefn  poField = poDefn.GetFieldDefn( iAttr );
                
                System.out.println( poField.GetNameRef() + ": " + poField.GetFieldTypeName( poField.GetFieldType() ) + " (" + poField.GetWidth() + "." + poField.GetPrecision() + ")");
            }
        }
/* -------------------------------------------------------------------- */
/*      Read, and dump features.                                        */
/* -------------------------------------------------------------------- */
        Feature  poFeature;
    
        if( nFetchFID == OGRNullFID && !bSummaryOnly )
        {
            while( (poFeature = poLayer.GetNextFeature()) != null )
            {
                poFeature.DumpReadable();
            }
        }
        else if( nFetchFID != OGRNullFID )
        {
            poFeature = poLayer.GetFeature( nFetchFID );
            if( poFeature == null )
            {
                System.out.println( "Unable to locate feature id " + nFetchFID + " on this layer.");
            }
            else
            {
                poFeature.DumpReadable();
            }
        }
    }
}
