/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Java port of a simple client for translating between formats.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 * Port from ogr2ogr.cpp by Frank Warmerdam
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

import java.util.Vector;
import java.util.Enumeration;
import java.util.StringTokenizer;

import org.gdal.gdal.gdal;
import org.gdal.gdal.ProgressCallback;
import org.gdal.gdal.TermProgressCallback;
import org.gdal.ogr.ogr;
import org.gdal.ogr.ogrConstants;
import org.gdal.ogr.Driver;
import org.gdal.ogr.DataSource;
import org.gdal.ogr.Layer;
import org.gdal.ogr.Feature;
import org.gdal.ogr.FeatureDefn;
import org.gdal.ogr.FieldDefn;
import org.gdal.ogr.Geometry;
import org.gdal.osr.SpatialReference;
import org.gdal.osr.CoordinateTransformation;

/* Note : this is the most direct port of ogr2ogr.cpp possible */
/* It could be made much more java'ish ! */

class GDALScaledProgress extends ProgressCallback
{
    private double pctMin;
    private double pctMax;
    private ProgressCallback mainCbk;

    public GDALScaledProgress(double pctMin, double pctMax,
                              ProgressCallback mainCbk)
    {
        this.pctMin = pctMin;
        this.pctMax = pctMax;
        this.mainCbk = mainCbk;
    }

    public int run(double dfComplete, String message)
    {
        return mainCbk.run(pctMin + dfComplete * (pctMax - pctMin), message);
    }
};


public class ogr2ogr
{
    static boolean bSkipFailures = false;
    static int nGroupTransactions = 200;
    static boolean bPreserveFID = false;
    static final int OGRNullFID = -1;
    static int nFIDToFetch = OGRNullFID;

/************************************************************************/
/*                                main()                                */
/************************************************************************/

    public static void main(String[] args)
    {
        String pszFormat = "ESRI Shapefile";
        String pszDataSource = null;
        String pszDestDataSource = null;
        Vector papszLayers = new Vector();
        Vector papszDSCO = new Vector(), papszLCO = new Vector();
        boolean bTransform = false;
        boolean bAppend = false, bUpdate = false, bOverwrite = false;
        String pszOutputSRSDef = null;
        String pszSourceSRSDef = null;
        SpatialReference poOutputSRS = null;
        SpatialReference poSourceSRS = null;
        String pszNewLayerName = null;
        String pszWHERE = null;
        Geometry poSpatialFilter = null;
        String pszSelect;
        Vector papszSelFields = new Vector();
        String pszSQLStatement = null;
        int    eGType = -2;
        double dfMaxSegmentLength = 0;
        Vector papszFieldTypesToString = new Vector();
        boolean bDisplayProgress = false;
        ProgressCallback pfnProgress = null;

    /* -------------------------------------------------------------------- */
    /*      Register format(s).                                             */
    /* -------------------------------------------------------------------- */
        ogr.RegisterAll();
    
    /* -------------------------------------------------------------------- */
    /*      Processing command line arguments.                              */
    /* -------------------------------------------------------------------- */
        args = ogr.GeneralCmdLineProcessor( args );
        
        if( args.length < 2 )
        {
            Usage();
            System.exit( -1 );
        }
    
        for( int iArg = 0; iArg < args.length; iArg++ )
        {
            if( args[iArg].equalsIgnoreCase("-f") && iArg < args.length-1 )
            {
                pszFormat = args[++iArg];
            }
            else if( args[iArg].equalsIgnoreCase("-dsco") && iArg < args.length-1 )
            {
                papszDSCO.addElement(args[++iArg] );
            }
            else if( args[iArg].equalsIgnoreCase("-lco") && iArg < args.length-1 )
            {
                papszLCO.addElement(args[++iArg] );
            }
            else if( args[iArg].equalsIgnoreCase("-preserve_fid") )
            {
                bPreserveFID = true;
            }
            else if( args[iArg].length() >= 5 && args[iArg].substring(0, 5).equalsIgnoreCase("-skip") )
            {
                bSkipFailures = true;
                nGroupTransactions = 1; /* #2409 */
            }
            else if( args[iArg].equalsIgnoreCase("-append") )
            {
                bAppend = true;
            }
            else if( args[iArg].equalsIgnoreCase("-overwrite") )
            {
                bOverwrite = true;
            }
            else if( args[iArg].equalsIgnoreCase("-update") )
            {
                bUpdate = true;
            }
            else if( args[iArg].equalsIgnoreCase("-fid") && args[iArg+1] != null )
            {
                nFIDToFetch = Integer.parseInt(args[++iArg]);
            }
            else if( args[iArg].equalsIgnoreCase("-sql") && args[iArg+1] != null )
            {
                pszSQLStatement = args[++iArg];
            }
            else if( args[iArg].equalsIgnoreCase("-nln") && iArg < args.length-1 )
            {
                pszNewLayerName = args[++iArg];
            }
            else if( args[iArg].equalsIgnoreCase("-nlt") && iArg < args.length-1 )
            {
                if( args[iArg+1].equalsIgnoreCase("NONE") )
                    eGType = ogr.wkbNone;
                else if( args[iArg+1].equalsIgnoreCase("GEOMETRY") )
                    eGType = ogr.wkbUnknown;
                else if( args[iArg+1].equalsIgnoreCase("POINT") )
                    eGType = ogr.wkbPoint;
                else if( args[iArg+1].equalsIgnoreCase("LINESTRING") )
                    eGType = ogr.wkbLineString;
                else if( args[iArg+1].equalsIgnoreCase("POLYGON") )
                    eGType = ogr.wkbPolygon;
                else if( args[iArg+1].equalsIgnoreCase("GEOMETRYCOLLECTION") )
                    eGType = ogr.wkbGeometryCollection;
                else if( args[iArg+1].equalsIgnoreCase("MULTIPOINT") )
                    eGType = ogr.wkbMultiPoint;
                else if( args[iArg+1].equalsIgnoreCase("MULTILINESTRING") )
                    eGType = ogr.wkbMultiLineString;
                else if( args[iArg+1].equalsIgnoreCase("MULTIPOLYGON") )
                    eGType = ogr.wkbMultiPolygon;
                else if( args[iArg+1].equalsIgnoreCase("GEOMETRY25D") )
                    eGType = ogr.wkbUnknown | ogr.wkb25DBit;
                else if( args[iArg+1].equalsIgnoreCase("POINT25D") )
                    eGType = ogr.wkbPoint25D;
                else if( args[iArg+1].equalsIgnoreCase("LINESTRING25D") )
                    eGType = ogr.wkbLineString25D;
                else if( args[iArg+1].equalsIgnoreCase("POLYGON25D") )
                    eGType = ogr.wkbPolygon25D;
                else if( args[iArg+1].equalsIgnoreCase("GEOMETRYCOLLECTION25D") )
                    eGType = ogr.wkbGeometryCollection25D;
                else if( args[iArg+1].equalsIgnoreCase("MULTIPOINT25D") )
                    eGType = ogr.wkbMultiPoint25D;
                else if( args[iArg+1].equalsIgnoreCase("MULTILINESTRING25D") )
                    eGType = ogr.wkbMultiLineString25D;
                else if( args[iArg+1].equalsIgnoreCase("MULTIPOLYGON25D") )
                    eGType = ogr.wkbMultiPolygon25D;
                else
                {
                    System.err.println("-nlt " + args[iArg+1] + ": type not recognised.");
                    System.exit( 1 );
                }
                iArg++;
            }
            else if( (args[iArg].equalsIgnoreCase("-tg") ||
                    args[iArg].equalsIgnoreCase("-gt")) && iArg < args.length-1 )
            {
                nGroupTransactions = Integer.parseInt(args[++iArg]);
            }
            else if( args[iArg].equalsIgnoreCase("-s_srs") && iArg < args.length-1 )
            {
                pszSourceSRSDef = args[++iArg];
            }
            else if( args[iArg].equalsIgnoreCase("-a_srs") && iArg < args.length-1 )
            {
                pszOutputSRSDef = args[++iArg];
            }
            else if( args[iArg].equalsIgnoreCase("-t_srs") && iArg < args.length-1 )
            {
                pszOutputSRSDef = args[++iArg];
                bTransform = true;
            }
            else if (args[iArg].equalsIgnoreCase("-spat") && iArg + 4 < args.length)
            {
                Geometry oRing = new Geometry(ogrConstants.wkbLinearRing);
                double xmin = new Double(args[++iArg]).doubleValue();
                double ymin = new Double(args[++iArg]).doubleValue();
                double xmax = new Double(args[++iArg]).doubleValue();
                double ymax = new Double(args[++iArg]).doubleValue();
                oRing.AddPoint(xmin, ymin);
                oRing.AddPoint(xmin, ymax);
                oRing.AddPoint(xmax, ymax);
                oRing.AddPoint(xmax, ymin);
                oRing.AddPoint(xmin, ymin);
                
                poSpatialFilter = new Geometry(ogrConstants.wkbPolygon);
                poSpatialFilter.AddGeometry(oRing);
            }
            else if( args[iArg].equalsIgnoreCase("-where") && args[iArg+1] != null )
            {
                pszWHERE = args[++iArg];
            }
            else if( args[iArg].equalsIgnoreCase("-select") && args[iArg+1] != null)
            {
                pszSelect = args[++iArg];
                StringTokenizer tokenizer = new StringTokenizer(pszSelect, " ,");
                while(tokenizer.hasMoreElements())
                    papszSelFields.addElement(tokenizer.nextToken());
            }
            else if( args[iArg].equalsIgnoreCase("-segmentize") && iArg < args.length-1 )
            {
                dfMaxSegmentLength = new Double(args[++iArg]).doubleValue();
            }
            else if( args[iArg].equalsIgnoreCase("-fieldTypeToString") && iArg < args.length-1 )
            {
                StringTokenizer tokenizer = new StringTokenizer(args[++iArg], " ,");
                while(tokenizer.hasMoreElements())
                {
                    String token = (String)tokenizer.nextToken();
                    if (token.equalsIgnoreCase("Integer") ||
                        token.equalsIgnoreCase("Real") ||
                        token.equalsIgnoreCase("String") ||
                        token.equalsIgnoreCase("Date") ||
                        token.equalsIgnoreCase("Time") ||
                        token.equalsIgnoreCase("DateTime") ||
                        token.equalsIgnoreCase("Binary") ||
                        token.equalsIgnoreCase("IntegerList") ||
                        token.equalsIgnoreCase("RealList") ||
                        token.equalsIgnoreCase("StringList"))
                    {
                        papszFieldTypesToString.addElement(token);
                    }
                    else if (token.equalsIgnoreCase("All"))
                    {
                        papszFieldTypesToString = null;
                        papszFieldTypesToString.addElement("All");
                        break;
                    }
                    else
                    {
                        System.err.println("Unhandled type for fieldtypeasstring option : " + token);
                        Usage();
                    }
                }
            }
            else if( args[iArg].equalsIgnoreCase("-progress") )
            {
                bDisplayProgress = true;
            }
            else if( args[iArg].charAt(0) == '-' )
            {
                Usage();
            }
            else if( pszDestDataSource == null )
                pszDestDataSource = args[iArg];
            else if( pszDataSource == null )
                pszDataSource = args[iArg];
            else
                papszLayers.addElement (args[iArg] );
        }
    
        if( pszDataSource == null )
            Usage();
    
    /* -------------------------------------------------------------------- */
    /*      Open data source.                                               */
    /* -------------------------------------------------------------------- */
        DataSource poDS;
            
        poDS = ogr.Open( pszDataSource, false );
    
    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if( poDS == null )
        {
            System.err.println("FAILURE:\n" + 
                    "Unable to open datasource ` " + pszDataSource + "' with the following drivers.");
    
            for( int iDriver = 0; iDriver < ogr.GetDriverCount(); iDriver++ )
            {
                System.err.println("  . " + ogr.GetDriver(iDriver).GetName() );
            }
    
            System.exit( 1 );
        }
    
    /* -------------------------------------------------------------------- */
    /*      Try opening the output datasource as an existing, writable      */
    /* -------------------------------------------------------------------- */
        DataSource       poODS;
        
        if( bUpdate )
        {
            poODS = ogr.Open( pszDestDataSource, true );
            if( poODS == null )
            {
                System.err.println("FAILURE:\n" +
                        "Unable to open existing output datasource `" + pszDestDataSource + "'.");
                System.exit( 1 );
            }
    
            if( papszDSCO.size() > 0 )
            {
                System.err.println("WARNING: Datasource creation options ignored since an existing datasource\n" +
                        "         being updated." );
            }
        }
    
    /* -------------------------------------------------------------------- */
    /*      Find the output driver.                                         */
    /* -------------------------------------------------------------------- */
        else
        {
            Driver          poDriver = null;
            int                  iDriver;
    
            for( iDriver = 0;
                iDriver < ogr.GetDriverCount() && poDriver == null;
                iDriver++ )
            {
                if( ogr.GetDriver(iDriver).GetName().equalsIgnoreCase(pszFormat) )
                {
                    poDriver = ogr.GetDriver(iDriver);
                }
            }
    
            if( poDriver == null )
            {
                System.err.println("Unable to find driver `" + pszFormat +"'." );
                System.err.println( "The following drivers are available:" );
            
                for( iDriver = 0; iDriver < ogr.GetDriverCount(); iDriver++ )
                {
                    System.err.println("  . " + ogr.GetDriver(iDriver).GetName() );
                }
                System.exit( 1 );
            }
    
            if( poDriver.TestCapability( ogr.ODrCCreateDataSource ) == false )
            {
                System.err.println( pszFormat + " driver does not support data source creation.");
                System.exit( 1 );
            }
    
    /* -------------------------------------------------------------------- */
    /*      Create the output data source.                                  */
    /* -------------------------------------------------------------------- */
            poODS = poDriver.CreateDataSource( pszDestDataSource, papszDSCO );
            if( poODS == null )
            {
                System.err.println( pszFormat + " driver failed to create "+ pszDestDataSource );
                System.exit( 1 );
            }
        }
    
    /* -------------------------------------------------------------------- */
    /*      Parse the output SRS definition if possible.                    */
    /* -------------------------------------------------------------------- */
        if( pszOutputSRSDef != null )
        {
            poOutputSRS = new SpatialReference();
            if( poOutputSRS.SetFromUserInput( pszOutputSRSDef ) != 0 )
            {
                System.err.println( "Failed to process SRS definition: " + pszOutputSRSDef );
                System.exit( 1 );
            }
        }
    
    /* -------------------------------------------------------------------- */
    /*      Parse the source SRS definition if possible.                    */
    /* -------------------------------------------------------------------- */
        if( pszSourceSRSDef != null )
        {
            poSourceSRS = new SpatialReference();
            if( poSourceSRS.SetFromUserInput( pszSourceSRSDef ) != 0 )
            {
                System.err.println( "Failed to process SRS definition: " + pszSourceSRSDef );
                System.exit( 1 );
            }
        }
    
    /* -------------------------------------------------------------------- */
    /*      Special case for -sql clause.  No source layers required.       */
    /* -------------------------------------------------------------------- */
        if( pszSQLStatement != null )
        {
            Layer poResultSet;
    
            if( pszWHERE != null )
                System.err.println( "-where clause ignored in combination with -sql." );
            if( papszLayers.size() > 0 )
                System.err.println( "layer names ignored in combination with -sql." );
            
            poResultSet = poDS.ExecuteSQL( pszSQLStatement, poSpatialFilter, 
                                            null );
    
            if( poResultSet != null )
            {
                long nCountLayerFeatures = 0;
                if (bDisplayProgress)
                {
                    if (!poResultSet.TestCapability(ogr.OLCFastFeatureCount))
                    {
                        System.err.println( "Progress turned off as fast feature count is not available.");
                        bDisplayProgress = false;
                    }
                    else
                    {
                        nCountLayerFeatures = poResultSet.GetFeatureCount();
                        pfnProgress = new TermProgressCallback();
                    }
                }
                if( !TranslateLayer( poDS, poResultSet, poODS, papszLCO, 
                                    pszNewLayerName, bTransform, poOutputSRS,
                                    poSourceSRS, papszSelFields, bAppend, eGType,
                                    bOverwrite, dfMaxSegmentLength, papszFieldTypesToString,
                                    nCountLayerFeatures, pfnProgress ))
                {
                    System.err.println(
                            "Terminating translation prematurely after failed\n" +
                            "translation from sql statement." );
    
                    System.exit( 1 );
                }
                poDS.ReleaseResultSet( poResultSet );
            }
        }
        else
        {
            int nLayerCount = 0;
            Layer[] papoLayers = null;

    /* -------------------------------------------------------------------- */
    /*      Process each data source layer.                                 */
    /* -------------------------------------------------------------------- */
            if ( papszLayers.size() == 0)
            {
                nLayerCount = poDS.GetLayerCount();
                papoLayers = new Layer[nLayerCount];

                for( int iLayer = 0; 
                    iLayer < nLayerCount; 
                    iLayer++ )
                {
                    Layer        poLayer = poDS.GetLayer(iLayer);

                    if( poLayer == null )
                    {
                        System.err.println("FAILURE: Couldn't fetch advertised layer " + iLayer + "!");
                        System.exit( 1 );
                    }

                    papoLayers[iLayer] = poLayer;
                }
            }
    /* -------------------------------------------------------------------- */
    /*      Process specified data source layers.                           */
    /* -------------------------------------------------------------------- */
            else
            {
                nLayerCount = papszLayers.size();
                papoLayers = new Layer[nLayerCount];

                for( int iLayer = 0; 
                    iLayer < papszLayers.size(); 
                    iLayer++ )
                {
                    Layer        poLayer = poDS.GetLayerByName((String)papszLayers.get(iLayer));

                    if( poLayer == null )
                    {
                        System.err.println("FAILURE: Couldn't fetch advertised layer " + (String)papszLayers.get(iLayer) + "!");
                        System.exit( 1 );
                    }

                    papoLayers[iLayer] = poLayer;
                }
            }

            long[] panLayerCountFeatures = new long[nLayerCount];
            long nCountLayersFeatures = 0;
            long nAccCountFeatures = 0;

            /* First pass to apply filters and count all features if necessary */
            for( int iLayer = 0; 
                iLayer < nLayerCount; 
                iLayer++ )
            {
                Layer        poLayer = papoLayers[iLayer];

                if( pszWHERE != null )
                    poLayer.SetAttributeFilter( pszWHERE );

                if( poSpatialFilter != null )
                    poLayer.SetSpatialFilter( poSpatialFilter );

                if (bDisplayProgress)
                {
                    if (!poLayer.TestCapability(ogr.OLCFastFeatureCount))
                    {
                        System.err.println("Progress turned off as fast feature count is not available.");
                        bDisplayProgress = false;
                    }
                    else
                    {
                        panLayerCountFeatures[iLayer] = poLayer.GetFeatureCount();
                        nCountLayersFeatures += panLayerCountFeatures[iLayer];
                    }
                }
            }

            /* Second pass to do the real job */
            for( int iLayer = 0; 
                iLayer < nLayerCount; 
                iLayer++ )
            {
                Layer        poLayer = papoLayers[iLayer];

                if (bDisplayProgress)
                {
                    pfnProgress = new GDALScaledProgress(
                            nAccCountFeatures * 1.0 / nCountLayersFeatures,
                            (nAccCountFeatures + panLayerCountFeatures[iLayer]) * 1.0 / nCountLayersFeatures,
                            new TermProgressCallback());
                }

                nAccCountFeatures += panLayerCountFeatures[iLayer];

                if( !TranslateLayer( poDS, poLayer, poODS, papszLCO, 
                                    pszNewLayerName, bTransform, poOutputSRS,
                                    poSourceSRS, papszSelFields, bAppend, eGType,
                                    bOverwrite, dfMaxSegmentLength, papszFieldTypesToString,
                                    panLayerCountFeatures[iLayer], pfnProgress) 
                    && !bSkipFailures )
                {
                    System.err.println(
                            "Terminating translation prematurely after failed\n" +
                            "translation of layer " + poLayer.GetLayerDefn().GetName() + " (use -skipfailures to skip errors)");

                    System.exit( 1 );
                }
            }
        }

    /* -------------------------------------------------------------------- */
    /*      Close down.                                                     */
    /* -------------------------------------------------------------------- */
        /* We must explicetely destroy the output dataset in order the file */
        /* to be properly closed ! */
        poODS.delete();
        poDS.delete();
    }
    
    /************************************************************************/
    /*                               Usage()                                */
    /************************************************************************/
    
    static void Usage()
    
    {
        System.out.print( "Usage: ogr2ogr [--help-general] [-skipfailures] [-append] [-update] [-gt n]\n" +
                "               [-select field_list] [-where restricted_where] \n" +
                "               [-progress] [-sql <sql statement>] \n" + 
                "               [-spat xmin ymin xmax ymax] [-preserve_fid] [-fid FID]\n" +
                "               [-a_srs srs_def] [-t_srs srs_def] [-s_srs srs_def]\n" +
                "               [-f format_name] [-overwrite] [[-dsco NAME=VALUE] ...]\n" +
                "               [-segmentize max_dist] [-fieldTypeToString All|(type1[,type2]*)]\n" +
                "               dst_datasource_name src_datasource_name\n" +
                "               [-lco NAME=VALUE] [-nln name] [-nlt type] [layer [layer ...]]\n" +
                "\n" +
                " -f format_name: output file format name, possible values are:\n");
        
        for( int iDriver = 0; iDriver < ogr.GetDriverCount(); iDriver++ )
        {
            Driver poDriver = ogr.GetDriver(iDriver);
    
            if( poDriver.TestCapability( ogr.ODrCCreateDataSource ) )
                System.out.print( "     -f \"" + poDriver.GetName() + "\"\n" );
        }
    
        System.out.print( " -append: Append to existing layer instead of creating new if it exists\n" +
                " -overwrite: delete the output layer and recreate it empty\n" +
                " -update: Open existing output datasource in update mode\n" +
                " -progress: Display progress on terminal. Only works if input layers have the \"fast feature count\" capability\n" +
                " -select field_list: Comma-delimited list of fields from input layer to\n" +
                "                     copy to the new layer (defaults to all)\n" + 
                " -where restricted_where: Attribute query (like SQL WHERE)\n" + 
                " -sql statement: Execute given SQL statement and save result.\n" +
                " -skipfailures: skip features or layers that fail to convert\n" +
                " -gt n: group n features per transaction (default 200)\n" +
                " -spat xmin ymin xmax ymax: spatial query extents\n" +
                " -segmentize max_dist: maximum distance between 2 nodes.\n" +
                "                       Used to create intermediate points\n" +
                " -dsco NAME=VALUE: Dataset creation option (format specific)\n" +
                " -lco  NAME=VALUE: Layer creation option (format specific)\n" +
                " -nln name: Assign an alternate name to the new layer\n" +
                " -nlt type: Force a geometry type for new layer.  One of NONE, GEOMETRY,\n" +
                "      POINT, LINESTRING, POLYGON, GEOMETRYCOLLECTION, MULTIPOINT,\n" +
                "      MULTIPOLYGON, or MULTILINESTRING.  Add \"25D\" for 3D layers.\n" +
                "      Default is type of source layer.\n" +
                " -fieldTypeToString type1,...: Converts fields of specified types to\n" +
                "      fields of type string in the new layer. Valid types are : \n" +
                "      Integer, Real, String, Date, Time, DateTime, Binary, IntegerList, RealList,\n" +
                "      StringList. Special value All can be used to convert all fields to strings.\n");
    
        System.out.print(" -a_srs srs_def: Assign an output SRS\n" +
            " -t_srs srs_def: Reproject/transform to this SRS on output\n" +
            " -s_srs srs_def: Override source SRS\n" +
            "\n" + 
            " Srs_def can be a full WKT definition (hard to escape properly),\n" +
            " or a well known definition (ie. EPSG:4326) or a file with a WKT\n" +
            " definition.\n" );
    
        System.exit( 1 );
    }

    static int CSLFindString(Vector v, String str)
    {
        int i = 0;
        Enumeration e = v.elements();
        while(e.hasMoreElements())
        {
            String strIter = (String)e.nextElement();
            if (strIter.equalsIgnoreCase(str))
                return i;
            i ++;
        }
        return -1;
    }

    /************************************************************************/
    /*                           TranslateLayer()                           */
    /************************************************************************/
    
    static boolean TranslateLayer( DataSource poSrcDS, 
                            Layer poSrcLayer,
                            DataSource poDstDS,
                            Vector papszLCO,
                            String pszNewLayerName,
                            boolean bTransform, 
                            SpatialReference poOutputSRS,
                            SpatialReference poSourceSRS,
                            Vector papszSelFields,
                            boolean bAppend, int eGType, boolean bOverwrite,
                            double dfMaxSegmentLength,
                            Vector papszFieldTypesToString,
                            long nCountLayerFeatures,
                            ProgressCallback pfnProgress)
    
    {
        Layer    poDstLayer;
        FeatureDefn poFDefn;
        int      eErr;
        boolean         bForceToPolygon = false;
        boolean         bForceToMultiPolygon = false;
    
        if( pszNewLayerName == null )
            pszNewLayerName = poSrcLayer.GetLayerDefn().GetName();
    
        if( (eGType & (~ogrConstants.wkb25DBit)) == ogr.wkbPolygon )
            bForceToPolygon = true;
        else if( (eGType & (~ogrConstants.wkb25DBit)) == ogr.wkbMultiPolygon )
            bForceToMultiPolygon = true;
    
    /* -------------------------------------------------------------------- */
    /*      Setup coordinate transformation if we need it.                  */
    /* -------------------------------------------------------------------- */
        CoordinateTransformation poCT = null;
    
        if( bTransform )
        {
            if( poSourceSRS == null )
                poSourceSRS = poSrcLayer.GetSpatialRef();
    
            if( poSourceSRS == null )
            {
                System.err.println("Can't transform coordinates, source layer has no\n" +
                        "coordinate system.  Use -s_srs to set one." );
                System.exit( 1 );
            }
    
            /*CPLAssert( null != poSourceSRS );
            CPLAssert( null != poOutputSRS );*/
    
            poCT = new CoordinateTransformation( poSourceSRS, poOutputSRS );
            if( poCT == null )
            {
                String pszWKT = null;
    
                System.err.println("Failed to create coordinate transformation between the\n" +
                    "following coordinate systems.  This may be because they\n" +
                    "are not transformable, or because projection services\n" +
                    "(PROJ.4 DLL/.so) could not be loaded." );
                
                pszWKT = poSourceSRS.ExportToPrettyWkt( 0 );
                System.err.println( "Source:\n" + pszWKT );
                
                pszWKT = poOutputSRS.ExportToPrettyWkt( 0 );
                System.err.println( "Target:\n" + pszWKT );
                System.exit( 1 );
            }
        }
        
    /* -------------------------------------------------------------------- */
    /*      Get other info.                                                 */
    /* -------------------------------------------------------------------- */
        poFDefn = poSrcLayer.GetLayerDefn();
        
        if( poOutputSRS == null )
            poOutputSRS = poSrcLayer.GetSpatialRef();
    
    /* -------------------------------------------------------------------- */
    /*      Find the layer.                                                 */
    /* -------------------------------------------------------------------- */
        int iLayer = -1;
        poDstLayer = null;
    
        for( iLayer = 0; iLayer < poDstDS.GetLayerCount(); iLayer++ )
        {
            Layer        poLayer = poDstDS.GetLayer(iLayer);
    
            if( poLayer != null 
                && poLayer.GetLayerDefn().GetName().equalsIgnoreCase(pszNewLayerName) )
            {
                poDstLayer = poLayer;
                break;
            }
        }
        
    /* -------------------------------------------------------------------- */
    /*      If the user requested overwrite, and we have the layer in       */
    /*      question we need to delete it now so it will get recreated      */
    /*      (overwritten).                                                  */
    /* -------------------------------------------------------------------- */
        if( poDstLayer != null && bOverwrite )
        {
            if( poDstDS.DeleteLayer( iLayer ) != 0 )
            {
                System.err.println(
                        "DeleteLayer() failed when overwrite requested." );
                return false;
            }
            poDstLayer = null;
        }
    
    /* -------------------------------------------------------------------- */
    /*      If the layer does not exist, then create it.                    */
    /* -------------------------------------------------------------------- */
        if( poDstLayer == null )
        {
            if( eGType == -2 )
                eGType = poFDefn.GetGeomType();
    
            if( poDstDS.TestCapability( ogr.ODsCCreateLayer ) == false)
            {
                System.err.println(
                "Layer " + pszNewLayerName + "not found, and CreateLayer not supported by driver.");
                return false;
            }
    
            gdal.ErrorReset();
    
            poDstLayer = poDstDS.CreateLayer( pszNewLayerName, poOutputSRS,
                                              eGType, papszLCO );
    
            if( poDstLayer == null )
                return false;
    
            bAppend = false;
        }
    
    /* -------------------------------------------------------------------- */
    /*      Otherwise we will append to it, if append was requested.        */
    /* -------------------------------------------------------------------- */
        else if( !bAppend )
        {
            System.err.println("FAILED: Layer " + pszNewLayerName + "already exists, and -append not specified.\n" +
                                "        Consider using -append, or -overwrite.");
            return false;
        }
        else
        {
            if( papszLCO.size() > 0 )
            {
                System.err.println("WARNING: Layer creation options ignored since an existing layer is\n" +
                        "         being appended to." );
            }
        }
    
    /* -------------------------------------------------------------------- */
    /*      Add fields.  Default to copy all field.                         */
    /*      If only a subset of all fields requested, then output only      */
    /*      the selected fields, and in the order that they were            */
    /*      selected.                                                       */
    /* -------------------------------------------------------------------- */
        int         iField;
    
        if (papszSelFields.size() > 0 && !bAppend )
        {
            for( iField=0; iField < papszSelFields.size(); iField++)
            {
                int iSrcField = poFDefn.GetFieldIndex((String)papszSelFields.get(iField));
                if (iSrcField >= 0)
                {
                    if (papszFieldTypesToString != null &&
                        (CSLFindString(papszFieldTypesToString, "All") != -1 ||
                        CSLFindString(papszFieldTypesToString,
                                    ogr.GetFieldTypeName(poFDefn.GetFieldDefn(iSrcField).GetFieldType())) != -1))
                    {
                        FieldDefn oFieldDefn = new FieldDefn( poFDefn.GetFieldDefn(iSrcField).GetName() );
                        oFieldDefn.SetType(ogr.OFTString);
                        poDstLayer.CreateField( oFieldDefn );
                    }
                    else
                        poDstLayer.CreateField( poFDefn.GetFieldDefn(iSrcField) );
                }
                else
                {
                    System.err.println("Field '" + (String)papszSelFields.get(iField) + "' not found in source layer.");
                        if( !bSkipFailures )
                            return false;
                }
            }
        }
        else if( !bAppend )
        {
            for( iField = 0; iField < poFDefn.GetFieldCount(); iField++ )
            {
                if (papszFieldTypesToString != null &&
                    (CSLFindString(papszFieldTypesToString, "All") != -1 ||
                    CSLFindString(papszFieldTypesToString,
                                ogr.GetFieldTypeName(poFDefn.GetFieldDefn(iField).GetFieldType())) != -1))
                {
                    FieldDefn oFieldDefn = new FieldDefn( poFDefn.GetFieldDefn(iField).GetName() );
                    oFieldDefn.SetType(ogr.OFTString);
                    poDstLayer.CreateField( oFieldDefn );
                }
                else
                    poDstLayer.CreateField( poFDefn.GetFieldDefn(iField) );
            }
        }
    
    /* -------------------------------------------------------------------- */
    /*      Transfer features.                                              */
    /* -------------------------------------------------------------------- */
        Feature  poFeature;
        int         nFeaturesInTransaction = 0;
        long        nCount = 0;
        
        poSrcLayer.ResetReading();
    
        if( nGroupTransactions > 0)
            poDstLayer.StartTransaction();
    
        while( true )
        {
            Feature      poDstFeature = null;
    
            if( nFIDToFetch != OGRNullFID )
            {
                // Only fetch feature on first pass.
                if( nFeaturesInTransaction == 0 )
                    poFeature = poSrcLayer.GetFeature(nFIDToFetch);
                else
                    poFeature = null;
            }
            else
                poFeature = poSrcLayer.GetNextFeature();
            
            if( poFeature == null )
                break;
    
            if( ++nFeaturesInTransaction == nGroupTransactions )
            {
                poDstLayer.CommitTransaction();
                poDstLayer.StartTransaction();
                nFeaturesInTransaction = 0;
            }
    
            gdal.ErrorReset();
            poDstFeature = new Feature( poDstLayer.GetLayerDefn() );
    
            if( poDstFeature.SetFrom( poFeature, 1 ) != 0 )
            {
                if( nGroupTransactions > 0)
                    poDstLayer.CommitTransaction();
                
                System.err.println(
                        "Unable to translate feature " + poFeature.GetFID() + " from layer " +
                        poFDefn.GetName() );
                
                poFeature.delete();
                poFeature = null;
                poDstFeature.delete();
                poDstFeature = null;
                return false;
            }
    
            if( bPreserveFID )
                poDstFeature.SetFID( poFeature.GetFID() );
    
            /*if (poDstFeature.GetGeometryRef() != null && dfMaxSegmentLength > 0)
                poDstFeature.GetGeometryRef().segmentize(dfMaxSegmentLength);*/
    
            if( poCT != null && poDstFeature.GetGeometryRef() != null )
            {
                eErr = poDstFeature.GetGeometryRef().Transform( poCT );
                if( eErr != 0 )
                {
                    if( nGroupTransactions > 0)
                        poDstLayer.CommitTransaction();
    
                    System.err.println("Failed to reproject feature" + poFeature.GetFID() + " (geometry probably out of source or destination SRS).");
                    if( !bSkipFailures )
                    {
                        poFeature.delete();
                        poFeature = null;
                        poDstFeature.delete();
                        poDstFeature = null;
                        return false;
                    }
                }
            }
    
            if( poDstFeature.GetGeometryRef() != null && bForceToPolygon )
            {
                poDstFeature.SetGeometryDirectly(ogr.ForceToPolygon(poDstFeature.GetGeometryRef()));
            }
                        
            if( poDstFeature.GetGeometryRef() != null && bForceToMultiPolygon )
            {
                poDstFeature.SetGeometryDirectly(ogr.ForceToMultiPolygon(poDstFeature.GetGeometryRef()));
            }
                        
            poFeature.delete();
            poFeature = null;
    
            gdal.ErrorReset();
            if( poDstLayer.CreateFeature( poDstFeature ) != 0 
                && !bSkipFailures )
            {
                if( nGroupTransactions > 0 )
                    poDstLayer.RollbackTransaction();
    
                poDstFeature.delete();
                poDstFeature = null;
                return false;
            }
    
            poDstFeature.delete();
            poDstFeature = null;

            /* Report progress */
            nCount ++;
            if (pfnProgress != null)
                pfnProgress.run(nCount * 1.0 / nCountLayerFeatures, "");

        }
    
        if( nGroupTransactions > 0 )
            poDstLayer.CommitTransaction();
    
        return true;
    }
}
