/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2015, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "gdal_utils_priv.h"
#include "commonutils.h"

CPL_CVSID("$Id$");

static void Usage(int bShort = TRUE);
static void Usage(const char* pszAdditionalMsg, int bShort = TRUE);

/************************************************************************/
/*                 GDALVectorTranslateOptionsForBinaryNew()             */
/************************************************************************/

static GDALVectorTranslateOptionsForBinary *GDALVectorTranslateOptionsForBinaryNew(void)
{
    return (GDALVectorTranslateOptionsForBinary*) CPLCalloc(  1, sizeof(GDALVectorTranslateOptionsForBinary) );
}

/************************************************************************/
/*                  GDALVectorTranslateOptionsForBinaryFree()           */
/************************************************************************/

static void GDALVectorTranslateOptionsForBinaryFree( GDALVectorTranslateOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary )
    {
        CPLFree(psOptionsForBinary->pszDataSource);
        CPLFree(psOptionsForBinary->pszDestDataSource);
        CSLDestroy(psOptionsForBinary->papszOpenOptions);
        CPLFree(psOptionsForBinary->pszFormat);
        CPLFree(psOptionsForBinary);
    }
}

/* -------------------------------------------------------------------- */
/*                  CheckDestDataSourceNameConsistency()                */
/* -------------------------------------------------------------------- */

static
void CheckDestDataSourceNameConsistency(const char* pszDestFilename,
                                        const char* pszDriverName)
{
    int i;
    char* pszDestExtension = CPLStrdup(CPLGetExtension(pszDestFilename));

    CheckExtensionConsistency(pszDestFilename, pszDriverName);

    static const char* apszBeginName[][2] =  { { "PG:"      , "PostgreSQL" },
                                               { "MySQL:"   , "MySQL" },
                                               { "CouchDB:" , "CouchDB" },
                                               { "GFT:"     , "GFT" },
                                               { "MSSQL:"   , "MSSQLSpatial" },
                                               { "ODBC:"    , "ODBC" },
                                               { "OCI:"     , "OCI" },
                                               { "SDE:"     , "SDE" },
                                               { "WFS:"     , "WFS" },
                                               { NULL, NULL }
                                             };

    for(i=0; apszBeginName[i][0] != NULL; i++)
    {
        if (EQUALN(pszDestFilename, apszBeginName[i][0], strlen(apszBeginName[i][0])) &&
            !EQUAL(pszDriverName, apszBeginName[i][1]))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "The target file has a name which is normally recognized by the %s driver,\n"
                    "but the requested output driver is %s. Is it really what you want ?\n",
                    apszBeginName[i][1],
                    pszDriverName);
            break;
        }
    }

    CPLFree(pszDestExtension);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )
{
    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(papszArgv[0]))
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    nArgc = OGRGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );
    
    if( nArgc < 1 )
        exit( -nArgc );

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"--help") )
            Usage();
        else if ( EQUAL(papszArgv[iArg], "--long-usage") )
        {
            Usage(FALSE);
        }
    }

    GDALVectorTranslateOptionsForBinary* psOptionsForBinary = GDALVectorTranslateOptionsForBinaryNew();
    GDALVectorTranslateOptions *psOptions = GDALVectorTranslateOptionsNew(papszArgv + 1, psOptionsForBinary);
    CSLDestroy( papszArgv );

    if( psOptions == NULL )
    {
        Usage();
    }

    if( psOptionsForBinary->pszDataSource == NULL )
    {
        if( psOptionsForBinary->pszDestDataSource == NULL )
            Usage("no target datasource provided");
        else
            Usage("no source datasource provided");
    }

    if (!psOptionsForBinary->bQuiet && psOptionsForBinary->bFormatExplicitlySet)
        CheckDestDataSourceNameConsistency(psOptionsForBinary->pszDestDataSource, psOptionsForBinary->pszFormat);

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */

    GDALDatasetH hDS = NULL;
    GDALDatasetH hODS = NULL;
    int bCloseODS = TRUE;
    
    /* Avoid opening twice the same datasource if it is both the input and output */
    /* Known to cause problems with at least FGdb, SQlite and GPKG drivers. See #4270 */
    if (psOptionsForBinary->eAccessMode != ACCESS_CREATION &&
        strcmp(psOptionsForBinary->pszDestDataSource, psOptionsForBinary->pszDataSource) == 0)
    {
        hODS = GDALOpenEx( psOptionsForBinary->pszDataSource,
                GDAL_OF_UPDATE | GDAL_OF_VECTOR, NULL, psOptionsForBinary->papszOpenOptions, NULL );
        GDALDriverH hDriver = NULL;
        if( hODS != NULL )
            hDriver = GDALGetDatasetDriver(hODS);

        /* Restrict to those 3 drivers. For example it is known to break with */
        /* the PG driver due to the way it manages transactions... */
        if (hDriver && !(EQUAL(GDALGetDescription(hDriver), "FileGDB") ||
                         EQUAL(GDALGetDescription(hDriver), "SQLite") ||
                         EQUAL(GDALGetDescription(hDriver), "GPKG")))
        {
            hDS = GDALOpenEx( psOptionsForBinary->pszDataSource,
                        GDAL_OF_VECTOR, NULL, psOptionsForBinary->papszOpenOptions, NULL );
        }
        else
        {
            hDS = hODS;
            bCloseODS = FALSE;
        }
    }
    else
    {
        hDS = GDALOpenEx( psOptionsForBinary->pszDataSource,
                        GDAL_OF_VECTOR, NULL, psOptionsForBinary->papszOpenOptions, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( hDS == NULL )
    {
        OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
        
        fprintf( stderr, "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers.\n",
                psOptionsForBinary->pszDataSource );

        for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            fprintf( stderr, "  -> %s\n", poR->GetDriver(iDriver)->GetDescription() );
        }

        exit( 1 );
    }

    if( !(psOptionsForBinary->bQuiet) )
    {
        GDALVectorTranslateOptionsSetProgress(psOptions, GDALTermProgress, NULL);
    }
    
    int bUsageError = FALSE;
    GDALDatasetH hDstDS = GDALVectorTranslate(psOptionsForBinary->pszDestDataSource, hODS,
                                              1, &hDS, psOptions, &bUsageError);
    if( bUsageError )
        Usage();
    int nRetCode = (hDstDS) ? 0 : 1;

    GDALVectorTranslateOptionsFree(psOptions);
    GDALVectorTranslateOptionsForBinaryFree(psOptionsForBinary);

    if(hDS)
        GDALClose(hDS);
    if(bCloseODS)
        GDALClose(hODS);

    OGRCleanupAll();

    return nRetCode;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(int bShort)
{
    Usage(NULL, bShort);
}

static void Usage(const char* pszAdditionalMsg, int bShort)

{
    OGRSFDriverRegistrar        *poR = OGRSFDriverRegistrar::GetRegistrar();


    printf( "Usage: ogr2ogr [--help-general] [-skipfailures] [-append] [-update]\n"
            "               [-select field_list] [-where restricted_where]\n"
            "               [-progress] [-sql <sql statement>] [-dialect dialect]\n"
            "               [-preserve_fid] [-fid FID]\n"
            "               [-spat xmin ymin xmax ymax] [-spat_srs srs_def] [-geomfield field]\n"
            "               [-a_srs srs_def] [-t_srs srs_def] [-s_srs srs_def]\n"
            "               [-f format_name] [-overwrite] [[-dsco NAME=VALUE] ...]\n"
            "               dst_datasource_name src_datasource_name\n"
            "               [-lco NAME=VALUE] [-nln name] \n"
            "               [-nlt type|PROMOTE_TO_MULTI|CONVERT_TO_LINEAR]\n"
            "               [-dim 2|3|layer_dim] [layer [layer ...]]\n"
            "\n"
            "Advanced options :\n"
            "               [-gt n] [-ds_transaction]\n"
            "               [[-oo NAME=VALUE] ...] [[-doo NAME=VALUE] ...]\n"
            "               [-clipsrc [xmin ymin xmax ymax]|WKT|datasource|spat_extent]\n"
            "               [-clipsrcsql sql_statement] [-clipsrclayer layer]\n"
            "               [-clipsrcwhere expression]\n"
            "               [-clipdst [xmin ymin xmax ymax]|WKT|datasource]\n"
            "               [-clipdstsql sql_statement] [-clipdstlayer layer]\n"
            "               [-clipdstwhere expression]\n"
            "               [-wrapdateline][-datelineoffset val]\n"
            "               [[-simplify tolerance] | [-segmentize max_dist]]\n"
            "               [-addfields] [-unsetFid]\n"
            "               [-relaxedFieldNameMatch] [-forceNullable] [-unsetDefault]\n"
            "               [-fieldTypeToString All|(type1[,type2]*)] [-unsetFieldWidth]\n"
            "               [-mapFieldType srctype|All=dsttype[,srctype2=dsttype2]*]\n"
            "               [-fieldmap identity | index1[,index2]*]\n"
            "               [-splitlistfields] [-maxsubfields val]\n"
            "               [-explodecollections] [-zfield field_name]\n"
            "               [-gcp pixel line easting northing [elevation]]* [-order n | -tps]\n"
            "               [-nomd] [-mo \"META-TAG=VALUE\"]*\n");

    if (bShort)
    {
        printf( "\nNote: ogr2ogr --long-usage for full help.\n");
        if( pszAdditionalMsg )
            fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);
        exit( 1 );
    }

    printf("\n -f format_name: output file format name, possible values are:\n");

    for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
    {
        GDALDriver *poDriver = poR->GetDriver(iDriver);

        if( CSLTestBoolean( CSLFetchNameValueDef(poDriver->GetMetadata(), GDAL_DCAP_CREATE, "FALSE") ) )
            printf( "     -f \"%s\"\n", poDriver->GetDescription() );
    }

    printf( " -append: Append to existing layer instead of creating new if it exists\n"
            " -overwrite: delete the output layer and recreate it empty\n"
            " -update: Open existing output datasource in update mode\n"
            " -progress: Display progress on terminal. Only works if input layers have the \n"
            "                                          \"fast feature count\" capability\n"
            " -select field_list: Comma-delimited list of fields from input layer to\n"
            "                     copy to the new layer (defaults to all)\n" 
            " -where restricted_where: Attribute query (like SQL WHERE)\n" 
            " -wrapdateline: split geometries crossing the dateline meridian\n"
            "                (long. = +/- 180deg)\n" 
            " -datelineoffset: offset from dateline in degrees\n"
            "                (default long. = +/- 10deg,\n"
            "                geometries within 170deg to -170deg will be splited)\n" 
            " -sql statement: Execute given SQL statement and save result.\n"
            " -dialect value: select a dialect, usually OGRSQL to avoid native sql.\n"
            " -skipfailures: skip features or layers that fail to convert\n"
            " -gt n: group n features per transaction (default 20000). n can be set to unlimited\n"
            " -spat xmin ymin xmax ymax: spatial query extents\n"
            " -simplify tolerance: distance tolerance for simplification.\n"
            " -segmentize max_dist: maximum distance between 2 nodes.\n"
            "                       Used to create intermediate points\n"
            " -dsco NAME=VALUE: Dataset creation option (format specific)\n"
            " -lco  NAME=VALUE: Layer creation option (format specific)\n"
            " -oo   NAME=VALUE: Input dataset open option (format specific)\n"
            " -doo  NAME=VALUE: Destination dataset open option (format specific)\n"
            " -nln name: Assign an alternate name to the new layer\n"
            " -nlt type: Force a geometry type for new layer.  One of NONE, GEOMETRY,\n"
            "      POINT, LINESTRING, POLYGON, GEOMETRYCOLLECTION, MULTIPOINT,\n"
            "      MULTIPOLYGON, or MULTILINESTRING, or PROMOTE_TO_MULTI or CONVERT_TO_LINEAR.  Add \"25D\" for 3D layers.\n"
            "      Default is type of source layer.\n"
            " -dim dimension: Force the coordinate dimension to the specified value.\n"
            " -fieldTypeToString type1,...: Converts fields of specified types to\n"
            "      fields of type string in the new layer. Valid types are : Integer,\n"
            "      Integer64, Real, String, Date, Time, DateTime, Binary, IntegerList, Integer64List, RealList,\n"
            "      StringList. Special value All will convert all fields to strings.\n"
            " -fieldmap index1,index2,...: Specifies the list of field indexes to be\n"
            "      copied from the source to the destination. The (n)th value specified\n"
            "      in the list is the index of the field in the target layer definition\n"
            "      in which the n(th) field of the source layer must be copied. Index count\n"
            "      starts at zero. There must be exactly as many values in the list as\n"
            "      the count of the fields in the source layer. We can use the 'identity'\n"
            "      setting to specify that the fields should be transferred by using the\n"
            "      same order. This setting should be used along with the append setting.");

    printf(" -a_srs srs_def: Assign an output SRS\n"
           " -t_srs srs_def: Reproject/transform to this SRS on output\n"
           " -s_srs srs_def: Override source SRS\n"
           "\n" 
           " Srs_def can be a full WKT definition (hard to escape properly),\n"
           " or a well known definition (ie. EPSG:4326) or a file with a WKT\n"
           " definition.\n" );

    if( pszAdditionalMsg )
        fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);

    exit( 1 );
}
