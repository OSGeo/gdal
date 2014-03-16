/******************************************************************************
 * $Id$
 *
 * Project:  BSB Reader
 * Purpose:  BSBDataset implementation for BSB format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_pam.h"
#include "bsb_read.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_BSB(void);
CPL_C_END

//Disabled as people may worry about the BSB patent
//#define BSB_CREATE

/************************************************************************/
/* ==================================================================== */
/*				BSBDataset				*/
/* ==================================================================== */
/************************************************************************/

class BSBRasterBand;

class BSBDataset : public GDALPamDataset
{
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    CPLString   osGCPProjection;

    double      adfGeoTransform[6];
    int         bGeoTransformSet;

    void        ScanForGCPs( bool isNos, const char *pszFilename );
    void        ScanForGCPsNos( const char *pszFilename );
    void        ScanForGCPsBSB();

    static int IdentifyInternal( GDALOpenInfo *, bool & isNosOut );

  public:
                BSBDataset();
		~BSBDataset();
    
    BSBInfo     *psInfo;

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            BSBRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BSBRasterBand : public GDALPamRasterBand
{
    GDALColorTable	oCT;

  public:
    		BSBRasterBand( BSBDataset * );
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           BSBRasterBand()                            */
/************************************************************************/

BSBRasterBand::BSBRasterBand( BSBDataset *poDS )

{
    this->poDS = poDS;
    this->nBand = 1;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    // Note that the first color table entry is dropped, everything is
    // shifted down.
    for( int i = 0; i < poDS->psInfo->nPCTSize-1; i++ )
    {
        GDALColorEntry  oColor;

        oColor.c1 = poDS->psInfo->pabyPCT[i*3+0+3];
        oColor.c2 = poDS->psInfo->pabyPCT[i*3+1+3];
        oColor.c3 = poDS->psInfo->pabyPCT[i*3+2+3];
        oColor.c4 = 255;

        oCT.SetColorEntry( i, &oColor );
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BSBRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )

{
    BSBDataset *poGDS = (BSBDataset *) poDS;
    GByte *pabyScanline = (GByte*) pImage;

    if( BSBReadScanline( poGDS->psInfo, nBlockYOff, pabyScanline ) )
    {
        for( int i = 0; i < nBlockXSize; i++ )
        {
            /* The indices start at 1, except in case of some charts */
            /* where there are missing values, which are filled to 0 */
            /* by BSBReadScanline */
            if (pabyScanline[i] > 0)
                pabyScanline[i] -= 1;
        }

        return CE_None;
    }
    else
        return CE_Failure;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *BSBRasterBand::GetColorTable()

{
    return &oCT;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp BSBRasterBand::GetColorInterpretation()

{
    return GCI_PaletteIndex;
}

/************************************************************************/
/* ==================================================================== */
/*				BSBDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           BSBDataset()                               */
/************************************************************************/

BSBDataset::BSBDataset()

{
    psInfo = NULL;

    bGeoTransformSet = FALSE;

    nGCPCount = 0;
    pasGCPList = NULL;
    osGCPProjection = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",7030]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AUTHORITY[\"EPSG\",4326]]";

    adfGeoTransform[0] = 0.0;     /* X Origin (top left corner) */
    adfGeoTransform[1] = 1.0;     /* X Pixel size */
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;     /* Y Origin (top left corner) */
    adfGeoTransform[4] = 0.0;     
    adfGeoTransform[5] = 1.0;     /* Y Pixel Size */

}

/************************************************************************/
/*                            ~BSBDataset()                             */
/************************************************************************/

BSBDataset::~BSBDataset()

{
    FlushCache();

    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );

    if( psInfo != NULL )
        BSBClose( psInfo );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BSBDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    
    if( bGeoTransformSet )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BSBDataset::GetProjectionRef()

{
    if( bGeoTransformSet )
        return osGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                     GDALHeuristicDatelineWrap()                      */
/************************************************************************/

static void 
GDALHeuristicDatelineWrap( int nPointCount, double *padfX )

{
    int i;
    /* Following inits are useless but keep GCC happy */
    double dfX_PM_Min = 0, dfX_PM_Max = 0, dfX_Dateline_Min = 0, dfX_Dateline_Max = 0;
    int    bUsePMWrap;

    if( nPointCount < 2 )
        return;

/* -------------------------------------------------------------------- */
/*      Work out what the longitude range will be centering on the      */
/*      prime meridian (-180 to 180) and centering on the dateline      */
/*      (0 to 360).                                                     */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nPointCount; i++ )
    {
        double dfX_PM, dfX_Dateline;

        dfX_PM = padfX[i];
        if( dfX_PM > 180 )
            dfX_PM -= 360.0;

        dfX_Dateline = padfX[i];
        if( dfX_Dateline < 0 )
            dfX_Dateline += 360.0;

        if( i == 0 )
        {
            dfX_PM_Min = dfX_PM_Max = dfX_PM;
            dfX_Dateline_Min = dfX_Dateline_Max = dfX_Dateline;
        }
        else
        {
            dfX_PM_Min = MIN(dfX_PM_Min,dfX_PM);
            dfX_PM_Max = MAX(dfX_PM_Max,dfX_PM);
            dfX_Dateline_Min = MIN(dfX_Dateline_Min,dfX_Dateline);
            dfX_Dateline_Max = MAX(dfX_Dateline_Max,dfX_Dateline);
        }
    }

/* -------------------------------------------------------------------- */
/*      Do nothing if the range is always fairly small - no apparent    */
/*      wrapping issues.                                                */
/* -------------------------------------------------------------------- */
    if( (dfX_PM_Max - dfX_PM_Min) < 270.0
        && (dfX_Dateline_Max - dfX_Dateline_Min) < 270.0 )
        return;

/* -------------------------------------------------------------------- */
/*      Do nothing if both appproach have a wide range - best not to    */
/*      fiddle if we aren't sure we are improving things.               */
/* -------------------------------------------------------------------- */
    if( (dfX_PM_Max - dfX_PM_Min) > 270.0
        && (dfX_Dateline_Max - dfX_Dateline_Min) > 270.0 )
        return;

/* -------------------------------------------------------------------- */
/*      Pick which way to transform things.                             */
/* -------------------------------------------------------------------- */
    if( (dfX_PM_Max - dfX_PM_Min) > 270.0
        && (dfX_Dateline_Max - dfX_Dateline_Min) < 270.0 )
        bUsePMWrap = FALSE;
    else
        bUsePMWrap = TRUE;


/* -------------------------------------------------------------------- */
/*      Apply rewrapping.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nPointCount; i++ )
    {
        if( bUsePMWrap )
        {
            if( padfX[i] > 180 )
                padfX[i] -= 360.0;
        }
        else 
        {
            if( padfX[i] < 0 )
                padfX[i] += 360.0;
        }
    }
}

/************************************************************************/
/*                   GDALHeuristicDatelineWrapGCPs()                    */
/************************************************************************/

static void
GDALHeuristicDatelineWrapGCPs( int nPointCount, GDAL_GCP *pasGCPList )
{
    std::vector<double> oadfX;
    int i;

    oadfX.resize( nPointCount );
    for( i = 0; i < nPointCount; i++ )
        oadfX[i] = pasGCPList[i].dfGCPX;

    GDALHeuristicDatelineWrap( nPointCount, &(oadfX[0]) );

    for( i = 0; i < nPointCount; i++ )
        pasGCPList[i].dfGCPX = oadfX[i];
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void BSBDataset::ScanForGCPs( bool isNos, const char *pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Collect GCPs as appropriate to source.                          */
/* -------------------------------------------------------------------- */
    nGCPCount = 0;

    if ( isNos )
    {
        ScanForGCPsNos(pszFilename);
    } else {
        ScanForGCPsBSB();
    }

/* -------------------------------------------------------------------- */
/*      Apply heuristics to re-wrap GCPs to maintain continguity        */
/*      over the international dateline.                                */
/* -------------------------------------------------------------------- */
    if( nGCPCount > 1 )
        GDALHeuristicDatelineWrapGCPs( nGCPCount, pasGCPList );

/* -------------------------------------------------------------------- */
/*      Collect coordinate system related parameters from header.       */
/* -------------------------------------------------------------------- */
    int i;
    const char *pszKNP=NULL, *pszKNQ=NULL;

    for( i = 0; psInfo->papszHeader[i] != NULL; i++ )
    {
        if( EQUALN(psInfo->papszHeader[i],"KNP/",4) )
        {
            pszKNP = psInfo->papszHeader[i];
            SetMetadataItem( "BSB_KNP", pszKNP + 4 );
        }
        if( EQUALN(psInfo->papszHeader[i],"KNQ/",4) )
        {
            pszKNQ = psInfo->papszHeader[i]; 
            SetMetadataItem( "BSB_KNQ", pszKNQ + 4 );
        }
    }

    
/* -------------------------------------------------------------------- */
/*      Can we derive a reasonable coordinate system definition for     */
/*      this file?  For now we keep it simple, just handling            */
/*      mercator. In the future we should consider others.              */
/* -------------------------------------------------------------------- */
    CPLString osUnderlyingSRS;
    if( pszKNP != NULL )
    {
        const char *pszPR = strstr(pszKNP,"PR=");
        const char *pszGD = strstr(pszKNP,"GD=");
        const char *pszValue, *pszEnd = NULL;
        const char *pszGEOGCS = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AUTHORITY[\"EPSG\",\"4326\"]]";
        CPLString osPP;
        
        // Capture the PP string.
        pszValue = strstr(pszKNP,"PP=");
        if( pszValue )
            pszEnd = strstr(pszValue,",");
        if( pszValue && pszEnd )
            osPP.assign(pszValue+3,pszEnd-pszValue-3);
        
        // Look at the datum
        if( pszGD == NULL )
        {
            /* no match. We'll default to EPSG:4326 */
        }
        else if( EQUALN(pszGD,"GD=European 1950", 16) )
        {
            pszGEOGCS = "GEOGCS[\"ED50\",DATUM[\"European_Datum_1950\",SPHEROID[\"International 1924\",6378388,297,AUTHORITY[\"EPSG\",\"7022\"]],TOWGS84[-87,-98,-121,0,0,0,0],AUTHORITY[\"EPSG\",\"6230\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.01745329251994328,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4230\"]]";
        }

        // Look at the projection
        if( pszPR == NULL )
        {
            /* no match */
        }
        else if( EQUALN(pszPR,"PR=MERCATOR", 11) )
        {
            // We somewhat arbitrarily select our first GCPX as our 
            // central meridian.  This is mostly helpful to ensure 
            // that regions crossing the dateline will be contiguous 
            // in mercator.
            osUnderlyingSRS.Printf( "PROJCS[\"Global Mercator\",%s,PROJECTION[\"Mercator_2SP\"],PARAMETER[\"standard_parallel_1\",0],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",%d],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"Meter\",1]]",
                pszGEOGCS, (int) pasGCPList[0].dfGCPX );
        }

        else if( EQUALN(pszPR,"PR=TRANSVERSE MERCATOR", 22)
                 && osPP.size() > 0 )
        {
            
            osUnderlyingSRS.Printf( 
                "PROJCS[\"unnamed\",%s,PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",%s],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0]]",
                pszGEOGCS, osPP.c_str() );
        }

        else if( EQUALN(pszPR,"PR=UNIVERSAL TRANSVERSE MERCATOR", 32)
                 && osPP.size() > 0 )
        {
            // This is not *really* UTM unless the central meridian 
            // matches a zone which it does not in some (most?) maps. 
            osUnderlyingSRS.Printf( 
                "PROJCS[\"unnamed\",%s,PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",%s],PARAMETER[\"scale_factor\",0.9996],PARAMETER[\"false_easting\",500000],PARAMETER[\"false_northing\",0]]", 
                pszGEOGCS, osPP.c_str() );
        }

        else if( EQUALN(pszPR,"PR=POLYCONIC", 12) && osPP.size() > 0 )
        {
            osUnderlyingSRS.Printf( 
                "PROJCS[\"unnamed\",%s,PROJECTION[\"Polyconic\"],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",%s],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0]]", 
                pszGEOGCS, osPP.c_str() );
        }
        
        else if( EQUALN(pszPR,"PR=LAMBERT CONFORMAL CONIC", 26) 
                 && osPP.size() > 0 && pszKNQ != NULL )
        {
            CPLString osP2, osP3;
        
            // Capture the KNQ/P2 string.
            pszValue = strstr(pszKNQ,"P2=");
            if( pszValue )
                pszEnd = strstr(pszValue,",");
            if( pszValue && pszEnd )
                osP2.assign(pszValue+3,pszEnd-pszValue-3);
            
            // Capture the KNQ/P3 string.
            pszValue = strstr(pszKNQ,"P3=");
            if( pszValue )
                pszEnd = strstr(pszValue,",");
            if( pszValue )
            {
                if( pszEnd )
                    osP3.assign(pszValue+3,pszEnd-pszValue-3);
                else
                    osP3.assign(pszValue+3);
            }

            if( osP2.size() > 0 && osP3.size() > 0 )
                osUnderlyingSRS.Printf( 
                    "PROJCS[\"unnamed\",%s,PROJECTION[\"Lambert_Conformal_Conic_2SP\"],PARAMETER[\"standard_parallel_1\",%s],PARAMETER[\"standard_parallel_2\",%s],PARAMETER[\"latitude_of_origin\",0.0],PARAMETER[\"central_meridian\",%s],PARAMETER[\"false_easting\",0.0],PARAMETER[\"false_northing\",0.0]]",
                    pszGEOGCS, osP2.c_str(), osP3.c_str(), osPP.c_str() );

        }
    }

/* -------------------------------------------------------------------- */
/*      If we got an alternate underlying coordinate system, try        */
/*      converting the GCPs to that coordinate system.                  */
/* -------------------------------------------------------------------- */
    if( osUnderlyingSRS.length() > 0 )
    {
        OGRSpatialReference oGeog_SRS, oProjected_SRS;
        OGRCoordinateTransformation *poCT;
        
        oProjected_SRS.SetFromUserInput( osUnderlyingSRS );
        oGeog_SRS.CopyGeogCSFrom( &oProjected_SRS );
        
        poCT = OGRCreateCoordinateTransformation( &oGeog_SRS, 
                                                  &oProjected_SRS );
        if( poCT != NULL )
        {
            for( i = 0; i < nGCPCount; i++ )
            {
                poCT->Transform( 1, 
                                 &(pasGCPList[i].dfGCPX), 
                                 &(pasGCPList[i].dfGCPY), 
                                 &(pasGCPList[i].dfGCPZ) );
            }

            osGCPProjection = osUnderlyingSRS;

            delete poCT;
        }
        else
            CPLErrorReset();
    }

/* -------------------------------------------------------------------- */
/*      Attempt to prepare a geotransform from the GCPs.                */
/* -------------------------------------------------------------------- */
    if( GDALGCPsToGeoTransform( nGCPCount, pasGCPList, adfGeoTransform, 
                                FALSE ) )
    {
        bGeoTransformSet = TRUE;
    }
}

/************************************************************************/
/*                           ScanForGCPsNos()                           */
/*                                                                      */
/*      Nos files have an accompanying .geo file, that contains some    */
/*      of the information normally contained in the header section     */
/*      with BSB files. we try and open a file with the same name,      */
/*      but a .geo extension, and look for lines like...                */
/*      PointX=long lat line pixel    (using the same naming system     */
/*      as BSB) Point1=-22.0000 64.250000 197 744                       */
/************************************************************************/

void BSBDataset::ScanForGCPsNos( const char *pszFilename )
{
    char **Tokens;
    const char *geofile;
    const char *extension;
    int fileGCPCount=0;

    extension = CPLGetExtension(pszFilename);

    // pseudointelligently try and guess whether we want a .geo or a .GEO
    if (extension[1] == 'O')
    {
        geofile = CPLResetExtension( pszFilename, "GEO");
    } else {
        geofile = CPLResetExtension( pszFilename, "geo");
    }

    FILE *gfp = VSIFOpen( geofile, "r" );  // Text files
    if( gfp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Couldn't find a matching .GEO file: %s", geofile );
        return;
    }

    char *thisLine = (char *) CPLMalloc( 80 ); // FIXME

    // Count the GCPs (reference points) and seek the file pointer 'gfp' to the starting point
    while (fgets(thisLine, 80, gfp))
    {
        if( EQUALN(thisLine, "Point", 5) )
            fileGCPCount++;
    }
    VSIRewind( gfp );

    // Memory has not been allocated to fileGCPCount yet
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),fileGCPCount+1);

    while (fgets(thisLine, 80, gfp))
    {
        if( EQUALN(thisLine, "Point", 5) )
        {
            // got a point line, turn it into a gcp
            Tokens = CSLTokenizeStringComplex(thisLine, "= ", FALSE, FALSE);
            if (CSLCount(Tokens) >= 5)
            {
                GDALInitGCPs( 1, pasGCPList + nGCPCount );
                pasGCPList[nGCPCount].dfGCPX = atof(Tokens[1]);
                pasGCPList[nGCPCount].dfGCPY = atof(Tokens[2]);
                pasGCPList[nGCPCount].dfGCPPixel = atof(Tokens[4]);
                pasGCPList[nGCPCount].dfGCPLine = atof(Tokens[3]);

                CPLFree( pasGCPList[nGCPCount].pszId );
                char	szName[50];
                sprintf( szName, "GCP_%d", nGCPCount+1 );
                pasGCPList[nGCPCount].pszId = CPLStrdup( szName );

                nGCPCount++;
            }
            CSLDestroy(Tokens);
        }
    }

    CPLFree(thisLine);
    VSIFClose(gfp);
}


/************************************************************************/
/*                            ScanForGCPsBSB()                          */
/************************************************************************/

void BSBDataset::ScanForGCPsBSB()
{
/* -------------------------------------------------------------------- */
/*      Collect standalone GCPs.  They look like:                       */
/*                                                                      */
/*      REF/1,115,2727,32.346666666667,-60.881666666667			*/
/*      REF/n,pixel,line,lat,long                                       */
/* -------------------------------------------------------------------- */
    int fileGCPCount=0;
    int i;

    // Count the GCPs (reference points) in psInfo->papszHeader
    for( i = 0; psInfo->papszHeader[i] != NULL; i++ )
        if( EQUALN(psInfo->papszHeader[i],"REF/",4) )
            fileGCPCount++;

    // Memory has not been allocated to fileGCPCount yet
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),fileGCPCount+1);

    for( i = 0; psInfo->papszHeader[i] != NULL; i++ )
    {
        char	**papszTokens;
        char	szName[50];

        if( !EQUALN(psInfo->papszHeader[i],"REF/",4) )
            continue;

        papszTokens = 
            CSLTokenizeStringComplex( psInfo->papszHeader[i]+4, ",", 
                                      FALSE, FALSE );

        if( CSLCount(papszTokens) > 4 )
        {
            GDALInitGCPs( 1, pasGCPList + nGCPCount );

            pasGCPList[nGCPCount].dfGCPX = atof(papszTokens[4]);
            pasGCPList[nGCPCount].dfGCPY = atof(papszTokens[3]);
            pasGCPList[nGCPCount].dfGCPPixel = atof(papszTokens[1]);
            pasGCPList[nGCPCount].dfGCPLine = atof(papszTokens[2]);

            CPLFree( pasGCPList[nGCPCount].pszId );
            if( CSLCount(papszTokens) > 5 )
            {
                pasGCPList[nGCPCount].pszId = CPLStrdup(papszTokens[5]);
            }
            else
            {
                sprintf( szName, "GCP_%d", nGCPCount+1 );
                pasGCPList[nGCPCount].pszId = CPLStrdup( szName );
            }

            nGCPCount++;
        }
        CSLDestroy( papszTokens );
    }
}

/************************************************************************/
/*                          IdentifyInternal()                          */
/************************************************************************/

int BSBDataset::IdentifyInternal( GDALOpenInfo * poOpenInfo, bool& isNosOut )

{
/* -------------------------------------------------------------------- */
/*      Check for BSB/ keyword.                                         */
/* -------------------------------------------------------------------- */
    int     i;
    isNosOut = false;

    if( poOpenInfo->nHeaderBytes < 1000 )
        return FALSE;

    for( i = 0; i < poOpenInfo->nHeaderBytes - 4; i++ )
    {
        if( poOpenInfo->pabyHeader[i+0] == 'B'
            && poOpenInfo->pabyHeader[i+1] == 'S'
            && poOpenInfo->pabyHeader[i+2] == 'B'
            && poOpenInfo->pabyHeader[i+3] == '/' )
            break;
        if( poOpenInfo->pabyHeader[i+0] == 'N'
            && poOpenInfo->pabyHeader[i+1] == 'O'
            && poOpenInfo->pabyHeader[i+2] == 'S'
            && poOpenInfo->pabyHeader[i+3] == '/' )
        {
            isNosOut = true;
            break;
        }
        if( poOpenInfo->pabyHeader[i+0] == 'W'
            && poOpenInfo->pabyHeader[i+1] == 'X'
            && poOpenInfo->pabyHeader[i+2] == '\\'
            && poOpenInfo->pabyHeader[i+3] == '8' )
            break;
    }

    if( i == poOpenInfo->nHeaderBytes - 4 )
        return FALSE;

    /* Additional test to avoid false positive. See #2881 */
    const char* pszRA = strstr((const char*)poOpenInfo->pabyHeader + i, "RA=");
    if (pszRA == NULL) /* This may be a NO1 file */
        pszRA = strstr((const char*)poOpenInfo->pabyHeader + i, "[JF");
    if (pszRA == NULL || pszRA - ((const char*)poOpenInfo->pabyHeader + i) > 100 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BSBDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    bool isNos;
    return IdentifyInternal(poOpenInfo, isNos);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BSBDataset::Open( GDALOpenInfo * poOpenInfo )

{
    bool        isNos = false;
    if (!IdentifyInternal(poOpenInfo, isNos))
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The BSB driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    BSBDataset 	*poDS;

    poDS = new BSBDataset();

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    poDS->psInfo = BSBOpen( poOpenInfo->pszFilename );
    if( poDS->psInfo == NULL )
    {
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = poDS->psInfo->nXSize;
    poDS->nRasterYSize = poDS->psInfo->nYSize;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new BSBRasterBand( poDS ));

    poDS->ScanForGCPs( isNos, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int BSBDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *BSBDataset::GetGCPProjection()

{
    return osGCPProjection;
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *BSBDataset::GetGCPs()

{
    return pasGCPList;
}

#ifdef BSB_CREATE

/************************************************************************/
/*                             BSBIsSRSOK()                             */
/************************************************************************/

static int BSBIsSRSOK(const char *pszWKT)
{
    int bOK = FALSE;
    OGRSpatialReference oSRS, oSRS_WGS84, oSRS_NAD83;

    if( pszWKT != NULL && pszWKT[0] != '\0' )
    {
        char* pszTmpWKT = (char*)pszWKT;
        oSRS.importFromWkt( &pszTmpWKT );

        oSRS_WGS84.SetWellKnownGeogCS( "WGS84" );
        oSRS_NAD83.SetWellKnownGeogCS( "NAD83" );
        if ( (oSRS.IsSameGeogCS(&oSRS_WGS84) || oSRS.IsSameGeogCS(&oSRS_NAD83)) &&
              oSRS.IsGeographic() && oSRS.GetPrimeMeridian() == 0.0 )
        {
            bOK = TRUE;
        }
    }

    if (!bOK)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "BSB only supports WGS84 or NAD83 geographic projections.\n");
    }

    return bOK;
}

/************************************************************************/
/*                           BSBCreateCopy()                            */
/************************************************************************/

static GDALDataset *
BSBCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
               int bStrict, char ** papszOptions, 
               GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "BSB driver only supports one band images.\n" );

        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte 
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "BSB driver doesn't support data type %s. "
                  "Only eight bit bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the output file.                                           */
/* -------------------------------------------------------------------- */
    BSBInfo *psBSB;

    psBSB = BSBCreate( pszFilename, 0, 200, nXSize, nYSize );
    if( psBSB == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Prepare initial color table.colortable.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand	*poBand = poSrcDS->GetRasterBand(1);
    int			iColor;
    unsigned char       abyPCT[771];
    int                 nPCTSize;
    int                 anRemap[256];

    abyPCT[0] = 0;
    abyPCT[1] = 0;
    abyPCT[2] = 0;

    if( poBand->GetColorTable() == NULL )
    {
        /* map greyscale down to 63 grey levels. */
        for( iColor = 0; iColor < 256; iColor++ )
        {
            int nOutValue = (int) (iColor / 4.1) + 1;

            anRemap[iColor] = nOutValue;
            abyPCT[nOutValue*3 + 0] = (unsigned char) iColor;
            abyPCT[nOutValue*3 + 1] = (unsigned char) iColor;
            abyPCT[nOutValue*3 + 2] = (unsigned char) iColor;
        }
        nPCTSize = 64;
    }
    else
    {
        GDALColorTable	*poCT = poBand->GetColorTable();
        int nColorTableSize = poCT->GetColorEntryCount();
        if (nColorTableSize > 255)
            nColorTableSize = 255;

        for( iColor = 0; iColor < nColorTableSize; iColor++ )
        {
            GDALColorEntry	sEntry;

            poCT->GetColorEntryAsRGB( iColor, &sEntry );

            anRemap[iColor] = iColor + 1;
            abyPCT[(iColor+1)*3 + 0] = (unsigned char) sEntry.c1;
            abyPCT[(iColor+1)*3 + 1] = (unsigned char) sEntry.c2;
            abyPCT[(iColor+1)*3 + 2] = (unsigned char) sEntry.c3;
        }

        nPCTSize = nColorTableSize + 1;

        // Add entries for pixel values which apparently will not occur.
        for( iColor = nPCTSize; iColor < 256; iColor++ )
            anRemap[iColor] = 1;
    }

/* -------------------------------------------------------------------- */
/*      Boil out all duplicate entries.                                 */
/* -------------------------------------------------------------------- */
    int  i;

    for( i = 1; i < nPCTSize-1; i++ )
    {
        int  j;

        for( j = i+1; j < nPCTSize; j++ )
        {
            if( abyPCT[i*3+0] == abyPCT[j*3+0] 
                && abyPCT[i*3+1] == abyPCT[j*3+1] 
                && abyPCT[i*3+2] == abyPCT[j*3+2] )
            {
                int   k;

                nPCTSize--;
                abyPCT[j*3+0] = abyPCT[nPCTSize*3+0];
                abyPCT[j*3+1] = abyPCT[nPCTSize*3+1];
                abyPCT[j*3+2] = abyPCT[nPCTSize*3+2];

                for( k = 0; k < 256; k++ )
                {
                    // merge matching entries.
                    if( anRemap[k] == j )
                        anRemap[k] = i;

                    // shift the last PCT entry into the new hole.
                    if( anRemap[k] == nPCTSize )
                        anRemap[k] = j;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Boil out all duplicate entries.                                 */
/* -------------------------------------------------------------------- */
    if( nPCTSize > 128 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Having to merge color table entries to reduce %d real\n"
                  "color table entries down to 127 values.", 
                  nPCTSize );
    }

    while( nPCTSize > 128 )
    {
        int nBestRange = 768;
        int iBestMatch1=-1, iBestMatch2=-1;

        // Find the closest pair of color table entries.

        for( i = 1; i < nPCTSize-1; i++ )
        {
            int  j;
            
            for( j = i+1; j < nPCTSize; j++ )
            {
                int nRange = ABS(abyPCT[i*3+0] - abyPCT[j*3+0])
                    + ABS(abyPCT[i*3+1] - abyPCT[j*3+1])
                    + ABS(abyPCT[i*3+2] - abyPCT[j*3+2]);

                if( nRange < nBestRange )
                {
                    iBestMatch1 = i;
                    iBestMatch2 = j;
                    nBestRange = nRange;
                }
            }
        }

        // Merge the second entry into the first. 
        nPCTSize--;
        abyPCT[iBestMatch2*3+0] = abyPCT[nPCTSize*3+0];
        abyPCT[iBestMatch2*3+1] = abyPCT[nPCTSize*3+1];
        abyPCT[iBestMatch2*3+2] = abyPCT[nPCTSize*3+2];

        for( i = 0; i < 256; i++ )
        {
            // merge matching entries.
            if( anRemap[i] == iBestMatch2 )
                anRemap[i] = iBestMatch1;
            
            // shift the last PCT entry into the new hole.
            if( anRemap[i] == nPCTSize )
                anRemap[i] = iBestMatch2;
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the PCT.                                                  */
/* -------------------------------------------------------------------- */
    if( !BSBWritePCT( psBSB, nPCTSize, abyPCT ) )
    {
        BSBClose( psBSB );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write the GCPs.                                                 */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];
    int nGCPCount = poSrcDS->GetGCPCount();
    if (nGCPCount)
    {
        const char* pszGCPProjection = poSrcDS->GetGCPProjection();
        if ( BSBIsSRSOK(pszGCPProjection) )
        {
            const GDAL_GCP * pasGCPList = poSrcDS->GetGCPs();
            for( i = 0; i < nGCPCount; i++ )
            {
                VSIFPrintfL( psBSB->fp, 
                            "REF/%d,%f,%f,%f,%f\n", 
                            i+1,
                            pasGCPList[i].dfGCPPixel, pasGCPList[i].dfGCPLine,
                            pasGCPList[i].dfGCPY, pasGCPList[i].dfGCPX);
            }
        }
    }
    else if (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None)
    {
        const char* pszProjection = poSrcDS->GetProjectionRef();
        if ( BSBIsSRSOK(pszProjection) )
        {
            VSIFPrintfL( psBSB->fp, 
                        "REF/%d,%d,%d,%f,%f\n",
                        1,
                        0, 0,
                        adfGeoTransform[3] + 0 * adfGeoTransform[4] + 0 * adfGeoTransform[5],
                        adfGeoTransform[0] + 0 * adfGeoTransform[1] + 0 * adfGeoTransform[2]);
            VSIFPrintfL( psBSB->fp, 
                        "REF/%d,%d,%d,%f,%f\n",
                        2,
                        nXSize, 0,
                        adfGeoTransform[3] + nXSize * adfGeoTransform[4] + 0 * adfGeoTransform[5],
                        adfGeoTransform[0] + nXSize * adfGeoTransform[1] + 0 * adfGeoTransform[2]);
            VSIFPrintfL( psBSB->fp, 
                        "REF/%d,%d,%d,%f,%f\n",
                        3,
                        nXSize, nYSize,
                        adfGeoTransform[3] + nXSize * adfGeoTransform[4] + nYSize * adfGeoTransform[5],
                        adfGeoTransform[0] + nXSize * adfGeoTransform[1] + nYSize * adfGeoTransform[2]);
            VSIFPrintfL( psBSB->fp, 
                        "REF/%d,%d,%d,%f,%f\n",
                        4,
                        0, nYSize,
                        adfGeoTransform[3] + 0 * adfGeoTransform[4] + nYSize * adfGeoTransform[5],
                        adfGeoTransform[0] + 0 * adfGeoTransform[1] + nYSize * adfGeoTransform[2]);
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GByte 	*pabyScanline;
    CPLErr      eErr = CE_None;

    pabyScanline = (GByte *) CPLMalloc( nXSize );

    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                 pabyScanline, nXSize, 1, GDT_Byte,
                                 nBands, nBands * nXSize );
        if( eErr == CE_None )
        {
            for( i = 0; i < nXSize; i++ )
                pabyScanline[i] = (GByte) anRemap[pabyScanline[i]];

            if( !BSBWriteScanline( psBSB, pabyScanline ) )
                eErr = CE_Failure;
        }
    }

    CPLFree( pabyScanline );

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    BSBClose( psBSB );

    if( eErr != CE_None )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }
    else
        return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
}
#endif

/************************************************************************/
/*                        GDALRegister_BSB()                            */
/************************************************************************/

void GDALRegister_BSB()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "BSB" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "BSB" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Maptech BSB Nautical Charts" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#BSB" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
#ifdef BSB_CREATE
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
#endif
        poDriver->pfnOpen = BSBDataset::Open;
        poDriver->pfnIdentify = BSBDataset::Identify;
#ifdef BSB_CREATE
        poDriver->pfnCreateCopy = BSBCreateCopy;
#endif

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
