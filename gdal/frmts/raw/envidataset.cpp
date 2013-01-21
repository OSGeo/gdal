/******************************************************************************
 * $Id$
 *
 * Project:  ENVI .hdr Driver
 * Purpose:  Implementation of ENVI .hdr labelled raw raster support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Maintainer: Chris Padwick (cpadwick at ittvis.com)
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

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void GDALRegister_ENVI(void);
CPL_C_END

static const int anUsgsEsriZones[] =
{
  101, 3101,
  102, 3126,
  201, 3151,
  202, 3176,
  203, 3201,
  301, 3226,
  302, 3251,
  401, 3276,
  402, 3301,
  403, 3326,
  404, 3351,
  405, 3376,
  406, 3401,
  407, 3426,
  501, 3451,
  502, 3476,
  503, 3501,
  600, 3526,
  700, 3551,
  901, 3601,
  902, 3626,
  903, 3576,
 1001, 3651,
 1002, 3676,
 1101, 3701,
 1102, 3726,
 1103, 3751,
 1201, 3776,
 1202, 3801,
 1301, 3826,
 1302, 3851,
 1401, 3876,
 1402, 3901,
 1501, 3926,
 1502, 3951,
 1601, 3976,
 1602, 4001,
 1701, 4026,
 1702, 4051,
 1703, 6426,
 1801, 4076,
 1802, 4101,
 1900, 4126,
 2001, 4151,
 2002, 4176,
 2101, 4201,
 2102, 4226,
 2103, 4251,
 2111, 6351,
 2112, 6376,
 2113, 6401,
 2201, 4276,
 2202, 4301,
 2203, 4326,
 2301, 4351,
 2302, 4376,
 2401, 4401,
 2402, 4426,
 2403, 4451,
 2500,    0,
 2501, 4476,
 2502, 4501,
 2503, 4526,
 2600,    0,
 2601, 4551,
 2602, 4576,
 2701, 4601,
 2702, 4626,
 2703, 4651,
 2800, 4676,
 2900, 4701,
 3001, 4726,
 3002, 4751,
 3003, 4776,
 3101, 4801,
 3102, 4826,
 3103, 4851,
 3104, 4876,
 3200, 4901,
 3301, 4926,
 3302, 4951,
 3401, 4976,
 3402, 5001,
 3501, 5026,
 3502, 5051,
 3601, 5076,
 3602, 5101,
 3701, 5126,
 3702, 5151,
 3800, 5176,
 3900,    0,
 3901, 5201,
 3902, 5226,
 4001, 5251,
 4002, 5276,
 4100, 5301,
 4201, 5326,
 4202, 5351,
 4203, 5376,
 4204, 5401,
 4205, 5426,
 4301, 5451,
 4302, 5476,
 4303, 5501,
 4400, 5526,
 4501, 5551,
 4502, 5576,
 4601, 5601,
 4602, 5626,
 4701, 5651,
 4702, 5676,
 4801, 5701,
 4802, 5726,
 4803, 5751,
 4901, 5776,
 4902, 5801,
 4903, 5826,
 4904, 5851,
 5001, 6101,
 5002, 6126,
 5003, 6151,
 5004, 6176,
 5005, 6201,
 5006, 6226,
 5007, 6251,
 5008, 6276,
 5009, 6301,
 5010, 6326,
 5101, 5876,
 5102, 5901,
 5103, 5926,
 5104, 5951,
 5105, 5976,
 5201, 6001,
 5200, 6026,
 5200, 6076,
 5201, 6051,
 5202, 6051,
 5300,    0, 
 5400,    0
};

/************************************************************************/
/*                           ITTVISToUSGSZone()                         */
/*                                                                      */
/*      Convert ITTVIS style state plane zones to NOS style state       */
/*      plane zones.  The ENVI default is to use the new NOS zones,     */
/*      but the old state plane zones can be used.  Handle this.        */ 
/************************************************************************/

static int ITTVISToUSGSZone( int nITTVISZone )

{
    int		nPairs = sizeof(anUsgsEsriZones) / (2*sizeof(int));
    int		i;
    
	// Default is to use the zone as-is, as long as it is in the 
	// available list
    for( i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i*2] == nITTVISZone )
            return anUsgsEsriZones[i*2];
    }

	// If not found in the new style, see if it is present in the
	// old style list and convert it.  We don't expect to see this
	// often, but older files allowed it and may still exist.
    for( i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i*2+1] == nITTVISZone )
            return anUsgsEsriZones[i*2];
    }

    return nITTVISZone; // perhaps it *is* the USGS zone?
}

/************************************************************************/
/* ==================================================================== */
/*				ENVIDataset				*/
/* ==================================================================== */
/************************************************************************/

class ENVIDataset : public RawDataset
{
    VSILFILE	*fpImage;	// image data file.
    VSILFILE	*fp;		// header file
    char	*pszHDRFilename;

    int		bFoundMapinfo;

    int         bHeaderDirty;

    double      adfGeoTransform[6];

    char	*pszProjection;

    char        **papszHeader;

    CPLString   osStaFilename;

    int         ReadHeader( VSILFILE * );
    int         ProcessMapinfo( const char * );
    void        ProcessRPCinfo( const char * ,int ,int);
    void        ProcessStatsFile();
    int         byteSwapInt(int);
    float       byteSwapFloat(float);
    double      byteSwapDouble(double);
    void        SetENVIDatum( OGRSpatialReference *, const char * );
    void        SetENVIEllipse( OGRSpatialReference *, char ** );
    void        WriteProjectionInfo();
    int         ParseRpcCoeffsMetaDataString(const char *psName, char *papszVal[], int& idx);
    int         WriteRpcInfo();
    int         WritePseudoGcpInfo();
    
    char        **SplitList( const char * );

    enum Interleave { BSQ, BIL, BIP } interleave;
    static int GetEnviType(GDALDataType eType);

  public:
            ENVIDataset();
            ~ENVIDataset();

    virtual void    FlushCache( void );
    virtual CPLErr  GetGeoTransform( double * padfTransform );
    virtual CPLErr  SetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
    virtual CPLErr  SetProjection( const char * );
    virtual char  **GetFileList(void);

    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" );
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" );
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/*                            ENVIDataset()                             */
/************************************************************************/

ENVIDataset::ENVIDataset()
{
    fpImage = NULL;
    fp = NULL;
    pszHDRFilename = NULL;
    pszProjection = CPLStrdup("");

    papszHeader = NULL;

    bFoundMapinfo = FALSE;

    bHeaderDirty = FALSE;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~ENVIDataset()                            */
/************************************************************************/

ENVIDataset::~ENVIDataset()

{
    FlushCache();
    if( fpImage )
        VSIFCloseL( fpImage );
    if( fp )
        VSIFCloseL( fp );
    CPLFree( pszProjection );
    CSLDestroy( papszHeader );
    CPLFree(pszHDRFilename);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void ENVIDataset::FlushCache()

{
    RawDataset::FlushCache();

    GDALRasterBand* band = (GetRasterCount() > 0) ? GetRasterBand(1) : NULL;

    if ( band == NULL || !(bHeaderDirty || band->GetCategoryNames() != NULL) )
        return;

    CPLLocaleC  oLocaleEnforcer;

    VSIFSeekL( fp, 0, SEEK_SET );
/* -------------------------------------------------------------------- */
/*      Rewrite out the header.                                           */
/* -------------------------------------------------------------------- */
    int		iBigEndian;

    const char	*pszInterleaving;
    char** catNames;

#ifdef CPL_LSB
    iBigEndian = 0;
#else
    iBigEndian = 1;
#endif

    VSIFPrintfL( fp, "ENVI\n" );
    if ("" != sDescription)
        VSIFPrintfL( fp, "description = {\n%s}\n", sDescription.c_str());
    VSIFPrintfL( fp, "samples = %d\nlines   = %d\nbands   = %d\n",
		nRasterXSize, nRasterYSize, nBands );

    catNames = band->GetCategoryNames();

    VSIFPrintfL( fp, "header offset = 0\n");
    if (0 == catNames)
        VSIFPrintfL( fp, "file type = ENVI Standard\n" );
    else
        VSIFPrintfL( fp, "file type = ENVI Classification\n" );

    int iENVIType = GetEnviType(band->GetRasterDataType());
    VSIFPrintfL( fp, "data type = %d\n", iENVIType );
    switch (interleave)
    {
      case BIP:
        pszInterleaving = "bip";		    // interleaved by pixel
        break;
      case BIL:
        pszInterleaving = "bil";		    // interleaved by line
        break;
      case BSQ:
        pszInterleaving = "bsq";		// band sequental by default
        break;
      default:
    	pszInterleaving = "bsq";
        break;
    }
    VSIFPrintfL( fp, "interleave = %s\n", pszInterleaving);
    VSIFPrintfL( fp, "byte order = %d\n", iBigEndian );

/* -------------------------------------------------------------------- */
/*      Write class and color information                               */
/* -------------------------------------------------------------------- */
    catNames = band->GetCategoryNames();
    if (0 != catNames)
    {
        int nrClasses = 0;
        while (*catNames++)
            ++nrClasses;

        if (nrClasses > 0)
        {
            VSIFPrintfL( fp, "classes = %d\n", nrClasses );

            GDALColorTable* colorTable = band->GetColorTable();
            if (0 != colorTable)
            {
                int nrColors = colorTable->GetColorEntryCount();
                if (nrColors > nrClasses)
                    nrColors = nrClasses;
                VSIFPrintfL( fp, "class lookup = {\n");
                for (int i = 0; i < nrColors; ++i)
                {
                    const GDALColorEntry* color = colorTable->GetColorEntry(i);
                    VSIFPrintfL(fp, "%d, %d, %d", color->c1, color->c2, color->c3);
                    if (i < nrColors - 1)
                    {
                        VSIFPrintfL(fp, ", ");
                        if (0 == (i+1) % 5)
                            VSIFPrintfL(fp, "\n");
                    }
                }
                VSIFPrintfL(fp, "}\n");
            }

            catNames = band->GetCategoryNames();
            if (0 != *catNames)
            {
                VSIFPrintfL( fp, "class names = {\n%s", *catNames++);
                int i = 0;
                while (*catNames) {
                    VSIFPrintfL( fp, ",");
                    if (0 == (++i) % 5)
                        VSIFPrintfL(fp, "\n");
                    VSIFPrintfL( fp, " %s", *catNames++);
                }
                VSIFPrintfL( fp, "}\n");
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the rest of header.                                       */
/* -------------------------------------------------------------------- */
    
    // only one map info type should be set
    //     - rpc
    //     - pseudo/gcp 
    //     - standard
    if ( !WriteRpcInfo() ) // are rpcs in the metadata
    {
        if ( !WritePseudoGcpInfo() ) // are gcps in the metadata
        {
            WriteProjectionInfo(); // standard - affine xform/coord sys str
        }
    }


    VSIFPrintfL( fp, "band names = {\n" );
    for ( int i = 1; i <= nBands; i++ )
    {
        CPLString sBandDesc = GetRasterBand( i )->GetDescription();

        if ( sBandDesc == "" )
            sBandDesc = CPLSPrintf( "Band %d", i );
        VSIFPrintfL( fp, "%s", sBandDesc.c_str() );
        if ( i != nBands )
            VSIFPrintfL( fp, ",\n" );
    }
    VSIFPrintfL( fp, "}\n" );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/
	 	 
char **ENVIDataset::GetFileList() 
    
{ 
    char **papszFileList = NULL; 
    
    // Main data file, etc.  
    papszFileList = GDALPamDataset::GetFileList(); 
    
    // Header file. 
    papszFileList = CSLAddString( papszFileList, pszHDRFilename );

    // Statistics file
    if (osStaFilename.size() != 0)
        papszFileList = CSLAddString( papszFileList, osStaFilename );
    
    return papszFileList; 
}

/************************************************************************/
/*                           GetEPSGGeogCS()                            */
/*                                                                      */
/*      Try to establish what the EPSG code for this coordinate         */
/*      systems GEOGCS might be.  Returns -1 if no reasonable guess     */
/*      can be made.                                                    */
/*                                                                      */
/*      TODO: We really need to do some name lookups.                   */
/************************************************************************/

static int ENVIGetEPSGGeogCS( OGRSpatialReference *poThis )

{
    const char *pszAuthName = poThis->GetAuthorityName( "GEOGCS" );

/* -------------------------------------------------------------------- */
/*      Do we already have it?                                          */
/* -------------------------------------------------------------------- */
    if( pszAuthName != NULL && EQUAL(pszAuthName,"epsg") )
        return atoi(poThis->GetAuthorityCode( "GEOGCS" ));

/* -------------------------------------------------------------------- */
/*      Get the datum and geogcs names.                                 */
/* -------------------------------------------------------------------- */
    const char *pszGEOGCS = poThis->GetAttrValue( "GEOGCS" );
    const char *pszDatum = poThis->GetAttrValue( "DATUM" );

    // We can only operate on coordinate systems with a geogcs.
    if( pszGEOGCS == NULL || pszDatum == NULL )
        return -1;

/* -------------------------------------------------------------------- */
/*      Is this a "well known" geographic coordinate system?            */
/* -------------------------------------------------------------------- */
    int bWGS, bNAD;

    bWGS = strstr(pszGEOGCS,"WGS") != NULL
        || strstr(pszDatum, "WGS")
        || strstr(pszGEOGCS,"World Geodetic System")
        || strstr(pszGEOGCS,"World_Geodetic_System")
        || strstr(pszDatum, "World Geodetic System")
        || strstr(pszDatum, "World_Geodetic_System"); 

    bNAD = strstr(pszGEOGCS,"NAD") != NULL
        || strstr(pszDatum, "NAD")
        || strstr(pszGEOGCS,"North American")
        || strstr(pszGEOGCS,"North_American")
        || strstr(pszDatum, "North American")
        || strstr(pszDatum, "North_American"); 

    if( bWGS && (strstr(pszGEOGCS,"84") || strstr(pszDatum,"84")) )
        return 4326;

    if( bWGS && (strstr(pszGEOGCS,"72") || strstr(pszDatum,"72")) )
        return 4322;

    if( bNAD && (strstr(pszGEOGCS,"83") || strstr(pszDatum,"83")) )
        return 4269;

    if( bNAD && (strstr(pszGEOGCS,"27") || strstr(pszDatum,"27")) )
        return 4267;

/* -------------------------------------------------------------------- */
/*      If we know the datum, associate the most likely GCS with        */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    pszAuthName = poThis->GetAuthorityName( "GEOGCS|DATUM" );

    if( pszAuthName != NULL 
        && EQUAL(pszAuthName,"epsg") 
        && poThis->GetPrimeMeridian() == 0.0 )
    {
        int nDatum = atoi(poThis->GetAuthorityCode("GEOGCS|DATUM"));
        
        if( nDatum >= 6000 && nDatum <= 6999 )
            return nDatum - 2000;
    }

    return -1;
}

/************************************************************************/
/*                        WriteProjectionInfo()                         */
/************************************************************************/

void ENVIDataset::WriteProjectionInfo()

{
/* -------------------------------------------------------------------- */
/*      Format the location (geotransform) portion of the map info      */
/*      line.                                                           */
/* -------------------------------------------------------------------- */
    CPLString   osLocation;

    osLocation.Printf( "1, 1, %.15g, %.15g, %.15g, %.15g", 
                       adfGeoTransform[0], adfGeoTransform[3], 
                       adfGeoTransform[1], fabs(adfGeoTransform[5]) );
                       
/* -------------------------------------------------------------------- */
/*      Minimal case - write out simple geotransform if we have a       */
/*      non-default geotransform.                                       */
/* -------------------------------------------------------------------- */
    if( pszProjection == NULL || strlen(pszProjection) == 0 )
    {
        if( adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0 || adfGeoTransform[5] != 1.0 )
        {
            const char* pszHemisphere = "North";
            VSIFPrintfL( fp, "map info = {Unknown, %s, %d, %s}\n",
                         osLocation.c_str(), 0, pszHemisphere);
        }
        return;
    }

/* -------------------------------------------------------------------- */
/*      Ingest WKT.                                                     */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    
    char	*pszProj = pszProjection;
    
    if( oSRS.importFromWkt( &pszProj ) != OGRERR_NONE )
        return;

/* -------------------------------------------------------------------- */
/*      Try to translate the datum and get major/minor ellipsoid        */
/*      values.                                                         */
/* -------------------------------------------------------------------- */
    int nEPSG_GCS = ENVIGetEPSGGeogCS( &oSRS );
    CPLString osDatum, osCommaDatum;
    double dfA, dfB;

    if( nEPSG_GCS == 4326 )
        osDatum = "WGS-84";
    else if( nEPSG_GCS == 4322 )
        osDatum = "WGS-72";
    else if( nEPSG_GCS == 4269 )
        osDatum = "North America 1983";
    else if( nEPSG_GCS == 4267 )
        osDatum = "North America 1927";
    else if( nEPSG_GCS == 4230 )
        osDatum = "European 1950";
    else if( nEPSG_GCS == 4277 )
        osDatum = "Ordnance Survey of Great Britain '36";
    else if( nEPSG_GCS == 4291 )
        osDatum = "SAD-69/Brazil";
    else if( nEPSG_GCS == 4283 )
        osDatum = "Geocentric Datum of Australia 1994";
    else if( nEPSG_GCS == 4275 )
        osDatum = "Nouvelle Triangulation Francaise IGN";

    if( osDatum != "" )
        osCommaDatum.Printf( ",%s", osDatum.c_str() );

    dfA = oSRS.GetSemiMajor();
    dfB = oSRS.GetSemiMinor();

/* -------------------------------------------------------------------- */
/*      Do we have unusual linear units?                                */
/* -------------------------------------------------------------------- */
    CPLString osOptionalUnits;
    if( fabs(oSRS.GetLinearUnits()-0.3048) < 0.0001 )
        osOptionalUnits = ", units=Feet";

/* -------------------------------------------------------------------- */
/*      Handle UTM case.                                                */
/* -------------------------------------------------------------------- */
    const char	*pszHemisphere;
    const char  *pszProjName = oSRS.GetAttrValue("PROJECTION");
    int		bNorth;
    int		iUTMZone;

    iUTMZone = oSRS.GetUTMZone( &bNorth );
    if ( iUTMZone )
    {
        if ( bNorth )
            pszHemisphere = "North";
        else
            pszHemisphere = "South";

        VSIFPrintfL( fp, "map info = {UTM, %s, %d, %s%s%s}\n",
                     osLocation.c_str(), iUTMZone, pszHemisphere,
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );
    }
    else if( oSRS.IsGeographic() )
    {
        VSIFPrintfL( fp, "map info = {Geographic Lat/Lon, %s%s}\n",
                     osLocation.c_str(), osCommaDatum.c_str());
    }
    else if( pszProjName == NULL )
    {
        // what to do? 
    }
    else if( EQUAL(pszProjName,SRS_PT_NEW_ZEALAND_MAP_GRID) )
    {
        VSIFPrintfL( fp, "map info = {New Zealand Map Grid, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {39, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, New Zealand Map Grid}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        VSIFPrintfL( fp, "map info = {Transverse Mercator, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {3, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Transverse Mercator}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)
             || EQUAL(pszProjName,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM) )
    {
        VSIFPrintfL( fp, "map info = {Lambert Conformal Conic, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {4, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Lambert Conformal Conic}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                     oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,
                   SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        VSIFPrintfL( fp, "map info = {Hotine Oblique Mercator A, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {5, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Hotine Oblique Mercator A}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_1,0.0),
                     oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_1,0.0),
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_2,0.0),
                     oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_2,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        VSIFPrintfL( fp, "map info = {Hotine Oblique Mercator B, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {6, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Hotine Oblique Mercator B}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_AZIMUTH,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_STEREOGRAPHIC) 
             || EQUAL(pszProjName,SRS_PT_OBLIQUE_STEREOGRAPHIC) )
    {
        VSIFPrintfL( fp, "map info = {Stereographic (ellipsoid), %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {7, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %s, Stereographic (ellipsoid)}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        VSIFPrintfL( fp, "map info = {Albers Conical Equal Area, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {9, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Albers Conical Equal Area}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0),
                     oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_POLYCONIC) )
    {
        VSIFPrintfL( fp, "map info = {Polyconic, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {10, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Polyconic}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        VSIFPrintfL( fp, "map info = {Lambert Azimuthal Equal Area, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {11, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Lambert Azimuthal Equal Area}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        VSIFPrintfL( fp, "map info = {Azimuthal Equadistant, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {12, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Azimuthal Equadistant}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     osCommaDatum.c_str() );
    }
    else if( EQUAL(pszProjName,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        VSIFPrintfL( fp, "map info = {Polar Stereographic, %s%s%s}\n",
                     osLocation.c_str(), 
                     osCommaDatum.c_str(), osOptionalUnits.c_str() );

        VSIFPrintfL( fp, "projection info = {31, %.16g, %.16g, %.16g, %.16g, %.16g, %.16g%s, Polar Stereographic}\n",
                     dfA, dfB, 
                     oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,90.0),
                     oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING,0.0),
                     oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0),
                     osCommaDatum.c_str() );
    }
    else
    {
        VSIFPrintfL( fp, "map info = {%s, %s}\n",
                     pszProjName, osLocation.c_str());
    }

    // write out coordinate system string
    if ( oSRS.morphToESRI() == OGRERR_NONE )
    {
        char *pszProjESRI = NULL;
        if ( oSRS.exportToWkt(&pszProjESRI) == OGRERR_NONE )
        {
            if ( strlen(pszProjESRI) )
                VSIFPrintfL( fp, "coordinate system string = {%s}\n", pszProjESRI);
        }
        CPLFree(pszProjESRI);
        pszProjESRI = NULL;
    }
}

/************************************************************************/
/*                ParseRpcCoeffsMetaDataString()                        */
/************************************************************************/

int ENVIDataset::ParseRpcCoeffsMetaDataString(const char *psName, char **papszVal,
                                              int& idx)
{
    // separate one string with 20 coefficients into an array of 20 strings.
    const char *psz20Vals = GetMetadataItem(psName, "RPC");
    if (!psz20Vals)
        return FALSE;

    char** papszArr = CSLTokenizeString2(psz20Vals, " ", 0);
    if (!papszArr)
        return FALSE;

    int x = 0;
    while ((papszArr[x] != NULL) && (x < 20))
    {
        papszVal[idx++] = CPLStrdup(papszArr[x]);
        x++;
    }

    CSLDestroy(papszArr);

    return (x == 20);
}

#define CPLStrdupIfNotNull(x) ((x) ? CPLStrdup(x) : NULL)

/************************************************************************/
/*                          WriteRpcInfo()                              */
/************************************************************************/

int ENVIDataset::WriteRpcInfo()
{
    // write out 90 rpc coeffs into the envi header plus 3 envi specific rpc values
    // returns 0 if the coeffs are not present or not valid
    int bRet = FALSE;
    int x, idx = 0;
    char* papszVal[93];

    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LINE_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("SAMP_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LAT_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LONG_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("HEIGHT_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LINE_SCALE", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("SAMP_SCALE", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LAT_SCALE", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LONG_SCALE", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("HEIGHT_SCALE", "RPC"));

    for (x=0; x<10; x++) // if we do not have 10 values we return 0
    {
        if (!papszVal[x])
            goto end;
    }

    if (!ParseRpcCoeffsMetaDataString("LINE_NUM_COEFF", papszVal, idx))
        goto end;

    if (!ParseRpcCoeffsMetaDataString("LINE_DEN_COEFF", papszVal, idx))
        goto end;

    if (!ParseRpcCoeffsMetaDataString("SAMP_NUM_COEFF", papszVal, idx))
        goto end;

    if (!ParseRpcCoeffsMetaDataString("SAMP_DEN_COEFF", papszVal, idx))
        goto end;

    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("TILE_ROW_OFFSET", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("TILE_COL_OFFSET", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("ENVI_RPC_EMULATION", "RPC"));
    CPLAssert(idx == 93);
    for (x=90; x<93; x++)
    {
        if (!papszVal[x])
            goto end;
    }

    // ok all the needed 93 values are present so write the rpcs into the envi header
    x = 1;
    VSIFPrintfL(fp, "rpc info = {\n");
    for (int iR=0; iR<93; iR++)
    {
      if (papszVal[iR][0] == '-')
        VSIFPrintfL(fp, " %s", papszVal[iR]);
      else
        VSIFPrintfL(fp, "  %s", papszVal[iR]);
      
      if (iR<92)
        VSIFPrintfL(fp, ",");

      if ((x % 4) == 0)
          VSIFPrintfL(fp, "\n");
     
      x++;
      if (x > 4) 
        x = 1;
    }

    VSIFPrintfL(fp, "}\n" );

    bRet = TRUE;

end:
    for (x=0;x<idx;x++)
        CPLFree(papszVal[x]);

    return bRet;
}

/************************************************************************/
/*                        WritePseudoGcpInfo()                          */
/************************************************************************/

int ENVIDataset::WritePseudoGcpInfo()
{
    // write out gcps into the envi header
    // returns 0 if the gcps are not present

    int iNum = GetGCPCount();
    if (iNum == 0)
      return FALSE;

    const GDAL_GCP *pGcpStructs = GetGCPs();

    //    double      dfGCPPixel; /** Pixel (x) location of GCP on raster */
    //    double      dfGCPLine;  /** Line (y) location of GCP on raster */
    //    double      dfGCPX;     /** X position of GCP in georeferenced space */
    //    double      dfGCPY;     /** Y position of GCP in georeferenced space */

    VSIFPrintfL(fp, "geo points = {\n");
    for (int iR=0; iR<iNum; iR++)
    {
      VSIFPrintfL(fp, " %#0.4f, %#0.4f, %#0.8f, %#0.8f",
                  pGcpStructs[iR].dfGCPPixel, pGcpStructs[iR].dfGCPLine,
                  pGcpStructs[iR].dfGCPY, pGcpStructs[iR].dfGCPX);
      if (iR<iNum-1)
        VSIFPrintfL(fp, ",\n");
    }

    VSIFPrintfL(fp, "}\n" );

    return TRUE;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ENVIDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr ENVIDataset::SetProjection( const char *pszNewProjection )

{
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    bHeaderDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ENVIDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    
    if( bFoundMapinfo )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ENVIDataset::SetGeoTransform( double * padfTransform )
{
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    bHeaderDirty = TRUE;
    bFoundMapinfo = TRUE;
    
    return CE_None;
}

/************************************************************************/
/*                             SetMetadata()                            */
/************************************************************************/

CPLErr ENVIDataset::SetMetadata( char ** papszMetadata,
                                 const char * pszDomain )
{
    if (pszDomain && EQUAL(pszDomain, "RPC"))
        bHeaderDirty = TRUE;

    return RawDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                             SetMetadataItem()                        */
/************************************************************************/

CPLErr ENVIDataset::SetMetadataItem( const char * pszName,
                                     const char * pszValue,
                                     const char * pszDomain )
{
    if (pszDomain && EQUAL(pszDomain, "RPC"))
        bHeaderDirty = TRUE;

    return RawDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr ENVIDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                             const char *pszGCPProjection )
{
    bHeaderDirty = TRUE;

    return RawDataset::SetGCPs(nGCPCount, pasGCPList, pszGCPProjection);
}

/************************************************************************/
/*                             SplitList()                              */
/*                                                                      */
/*      Split an ENVI value list into component fields, and strip       */
/*      white space.                                                    */
/************************************************************************/

char **ENVIDataset::SplitList( const char *pszCleanInput )

{
    char	**papszReturn = NULL;
    char	*pszInput = CPLStrdup(pszCleanInput);

    if( pszInput[0] != '{' )
    {
        CPLFree(pszInput);
        return NULL;
    }

    int iChar=1;


    while( pszInput[iChar] != '}' && pszInput[iChar] != '\0' )
    {
        int iFStart=-1, iFEnd=-1;

        // Find start of token.
        iFStart = iChar;
        while( pszInput[iFStart] == ' ' )
            iFStart++;

        iFEnd = iFStart;
        while( pszInput[iFEnd] != ',' 
               && pszInput[iFEnd] != '}'
               && pszInput[iFEnd] != '\0' )
            iFEnd++;

        if( pszInput[iFEnd] == '\0' )
            break;

        iChar = iFEnd + 1;
        iFEnd = iFEnd - 1;

        while( iFEnd > iFStart && pszInput[iFEnd] == ' ' )
            iFEnd--;

        pszInput[iFEnd + 1] = '\0';
        papszReturn = CSLAddString( papszReturn, pszInput + iFStart );
    }

    CPLFree( pszInput );

    return papszReturn;
}

/************************************************************************/
/*                            SetENVIDatum()                            */
/************************************************************************/

void ENVIDataset::SetENVIDatum( OGRSpatialReference *poSRS, 
                                const char *pszENVIDatumName )

{
    // datums
    if( EQUAL(pszENVIDatumName, "WGS-84") )
        poSRS->SetWellKnownGeogCS( "WGS84" );
    else if( EQUAL(pszENVIDatumName, "WGS-72") )
        poSRS->SetWellKnownGeogCS( "WGS72" );
    else if( EQUAL(pszENVIDatumName, "North America 1983") )
        poSRS->SetWellKnownGeogCS( "NAD83" );
    else if( EQUAL(pszENVIDatumName, "North America 1927") 
             || strstr(pszENVIDatumName,"NAD27") 
             || strstr(pszENVIDatumName,"NAD-27") )
        poSRS->SetWellKnownGeogCS( "NAD27" );
    else if( EQUALN(pszENVIDatumName, "European 1950",13) )
        poSRS->SetWellKnownGeogCS( "EPSG:4230" );
    else if( EQUAL(pszENVIDatumName, "Ordnance Survey of Great Britain '36") )
        poSRS->SetWellKnownGeogCS( "EPSG:4277" );
    else if( EQUAL(pszENVIDatumName, "SAD-69/Brazil") )
        poSRS->SetWellKnownGeogCS( "EPSG:4291" );
    else if( EQUAL(pszENVIDatumName, "Geocentric Datum of Australia 1994") )
        poSRS->SetWellKnownGeogCS( "EPSG:4283" );
    else if( EQUAL(pszENVIDatumName, "Australian Geodetic 1984") )
        poSRS->SetWellKnownGeogCS( "EPSG:4203" );
    else if( EQUAL(pszENVIDatumName, "Nouvelle Triangulation Francaise IGN") )
        poSRS->SetWellKnownGeogCS( "EPSG:4275" );

    // Ellipsoids
    else if( EQUAL(pszENVIDatumName, "GRS 80") )
        poSRS->SetWellKnownGeogCS( "NAD83" );
    else if( EQUAL(pszENVIDatumName, "Airy") )
        poSRS->SetWellKnownGeogCS( "EPSG:4001" );
    else if( EQUAL(pszENVIDatumName, "Australian National") )
        poSRS->SetWellKnownGeogCS( "EPSG:4003" );
    else if( EQUAL(pszENVIDatumName, "Bessel 1841") )
        poSRS->SetWellKnownGeogCS( "EPSG:4004" );
    else if( EQUAL(pszENVIDatumName, "Clark 1866") )
        poSRS->SetWellKnownGeogCS( "EPSG:4008" );
    else 
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unrecognised datum '%s', defaulting to WGS84.", pszENVIDatumName);
        poSRS->SetWellKnownGeogCS( "WGS84" );
    }
}

/************************************************************************/
/*                           SetENVIEllipse()                           */
/************************************************************************/

void ENVIDataset::SetENVIEllipse( OGRSpatialReference *poSRS, 
                                  char **papszPI_EI )

{
    double dfA = CPLAtofM(papszPI_EI[0]);
    double dfB = CPLAtofM(papszPI_EI[1]);
    double dfInvF;

    if( fabs(dfA-dfB) < 0.1 )
        dfInvF = 0.0; // sphere
    else
        dfInvF = dfA / (dfA - dfB);
    
    
    poSRS->SetGeogCS( "Ellipse Based", "Ellipse Based", "Unnamed", 
                      dfA, dfInvF );
}

/************************************************************************/
/*                           ProcessMapinfo()                           */
/*                                                                      */
/*      Extract projection, and geotransform from a mapinfo value in    */
/*      the header.                                                     */
/************************************************************************/

int ENVIDataset::ProcessMapinfo( const char *pszMapinfo )

{
    char	**papszFields;		
    int         nCount;
    OGRSpatialReference oSRS;

    papszFields = SplitList( pszMapinfo );
    nCount = CSLCount(papszFields);

    if( nCount < 7 )
    {
        CSLDestroy( papszFields );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Check if we have coordinate system string, and if so parse it.  */
/* -------------------------------------------------------------------- */
    char **papszCSS = NULL;
    if( CSLFetchNameValue( papszHeader, "coordinate_system_string" ) != NULL )
    {
        papszCSS = CSLTokenizeString2(
            CSLFetchNameValue( papszHeader, "coordinate_system_string" ), 
            "{}", CSLT_PRESERVEQUOTES );
    }

/* -------------------------------------------------------------------- */
/*      Check if we have projection info, and if so parse it.           */
/* -------------------------------------------------------------------- */
    char        **papszPI = NULL;
    int         nPICount = 0;
    if( CSLFetchNameValue( papszHeader, "projection_info" ) != NULL )
    {
        papszPI = SplitList( 
            CSLFetchNameValue( papszHeader, "projection_info" ) );
        nPICount = CSLCount(papszPI);
    }

/* -------------------------------------------------------------------- */
/*      Capture geotransform.                                           */
/* -------------------------------------------------------------------- */
    adfGeoTransform[1] = atof(papszFields[5]);	    // Pixel width
    adfGeoTransform[5] = -atof(papszFields[6]);	    // Pixel height
    adfGeoTransform[0] =			    // Upper left X coordinate
	atof(papszFields[3]) - (atof(papszFields[1]) - 1) * adfGeoTransform[1];
    adfGeoTransform[3] =			    // Upper left Y coordinate
	atof(papszFields[4]) - (atof(papszFields[2]) - 1) * adfGeoTransform[5];
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[4] = 0.0;

/* -------------------------------------------------------------------- */
/*      Capture projection.                                             */
/* -------------------------------------------------------------------- */
    if ( oSRS.importFromESRI( papszCSS ) != OGRERR_NONE )
    {
        oSRS.Clear();

        if( EQUALN(papszFields[0],"UTM",3) && nCount >= 9 )
        {
            oSRS.SetUTM( atoi(papszFields[7]), 
                         !EQUAL(papszFields[8],"South") );
            if( nCount >= 10 && strstr(papszFields[9],"=") == NULL )
                SetENVIDatum( &oSRS, papszFields[9] );
            else
                oSRS.SetWellKnownGeogCS( "NAD27" );
        }
        else if( EQUALN(papszFields[0],"State Plane (NAD 27)",19)
                 && nCount >= 7 )
        {
            oSRS.SetStatePlane( ITTVISToUSGSZone(atoi(papszFields[7])), FALSE );
        }
        else if( EQUALN(papszFields[0],"State Plane (NAD 83)",19)
                 && nCount >= 7 )
        {
            oSRS.SetStatePlane( ITTVISToUSGSZone(atoi(papszFields[7])), TRUE );
        }
        else if( EQUALN(papszFields[0],"Geographic Lat",14) 
                 && nCount >= 8 )
        {
            if( nCount >= 8 && strstr(papszFields[7],"=") == NULL )
                SetENVIDatum( &oSRS, papszFields[7] );
            else
                oSRS.SetWellKnownGeogCS( "WGS84" );
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 3 ) // TM
        {
            oSRS.SetTM( CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                        CPLAtofM(papszPI[7]),
                        CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 4 ) // Lambert Conformal Conic
        {
            oSRS.SetLCC( CPLAtofM(papszPI[7]), CPLAtofM(papszPI[8]),
                         CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                         CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 10 && atoi(papszPI[0]) == 5 ) // Oblique Merc (2 point)
        {
            oSRS.SetHOM2PNO( CPLAtofM(papszPI[3]), 
                             CPLAtofM(papszPI[4]), CPLAtofM(papszPI[5]), 
                             CPLAtofM(papszPI[6]), CPLAtofM(papszPI[7]), 
                             CPLAtofM(papszPI[10]), 
                             CPLAtofM(papszPI[8]), CPLAtofM(papszPI[9]) );
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 6 ) // Oblique Merc 
        {
            oSRS.SetHOM(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]), 
                        CPLAtofM(papszPI[5]), 0.0,
                        CPLAtofM(papszPI[8]), 
                        CPLAtofM(papszPI[6]), CPLAtofM(papszPI[7]) );
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 7 ) // Stereographic
        {
            oSRS.SetStereographic( CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                                   CPLAtofM(papszPI[7]),
                                   CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 9 ) // Albers Equal Area
        {
            oSRS.SetACEA( CPLAtofM(papszPI[7]), CPLAtofM(papszPI[8]),
                          CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                          CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 6 && atoi(papszPI[0]) == 10 ) // Polyconic
        {
            oSRS.SetPolyconic(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]), 
                              CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 6 && atoi(papszPI[0]) == 11 ) // LAEA
        {
            oSRS.SetLAEA(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]), 
                         CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 6 && atoi(papszPI[0]) == 12 ) // Azimuthal Equid.
        {
            oSRS.SetAE(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]), 
                       CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 6 && atoi(papszPI[0]) == 31 ) // Polar Stereographic
        {
            oSRS.SetPS(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]), 
                       1.0,
                       CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
    }

    CSLDestroy( papszCSS );

    // Still lots more that could be added for someone with the patience.

/* -------------------------------------------------------------------- */
/*      fallback to localcs if we don't recognise things.               */
/* -------------------------------------------------------------------- */
    if( oSRS.GetRoot() == NULL )
        oSRS.SetLocalCS( papszFields[0] );

/* -------------------------------------------------------------------- */
/*      Try to set datum from projection info line if we have a         */
/*      projected coordinate system without a GEOGCS.                   */
/* -------------------------------------------------------------------- */
    if( oSRS.IsProjected() && oSRS.GetAttrNode("GEOGCS") == NULL 
        && nPICount > 3 )
    {
        // Do we have a datum on the projection info line?
        int iDatum = nPICount-1;

        // Ignore units= items.
        if( strstr(papszPI[iDatum],"=") != NULL )
            iDatum--;

        // Skip past the name. 
        iDatum--;
    
        CPLString osDatumName = papszPI[iDatum];
        if( osDatumName.find_first_of("abcdefghijklmnopqrstuvwxyz"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ") 
            != CPLString::npos )
        {
            SetENVIDatum( &oSRS, osDatumName );
        }
        else
        {
            SetENVIEllipse( &oSRS, papszPI + 1 );
        }
    }


/* -------------------------------------------------------------------- */
/*      Try to process specialized units.                               */
/* -------------------------------------------------------------------- */
    if( EQUALN( papszFields[nCount-1],"units",5))
    {
        /* Handle linear units first. */
        if (EQUAL(papszFields[nCount-1],"units=Feet") )
            oSRS.SetLinearUnitsAndUpdateParameters( SRS_UL_FOOT, atof(SRS_UL_FOOT_CONV) );
        else if (EQUAL(papszFields[nCount-1],"units=Meters") )
            oSRS.SetLinearUnitsAndUpdateParameters( SRS_UL_METER, 1. );
        else if (EQUAL(papszFields[nCount-1],"units=Km") )
            oSRS.SetLinearUnitsAndUpdateParameters( "Kilometer", 1000.  );
        else if (EQUAL(papszFields[nCount-1],"units=Yards") )
            oSRS.SetLinearUnitsAndUpdateParameters( "Yard", .9144 );
        else if (EQUAL(papszFields[nCount-1],"units=Miles") )
            oSRS.SetLinearUnitsAndUpdateParameters( "Mile", 1609.344 );
        else if (EQUAL(papszFields[nCount-1],"units=Nautical Miles") )
            oSRS.SetLinearUnitsAndUpdateParameters( SRS_UL_NAUTICAL_MILE, atof(SRS_UL_NAUTICAL_MILE_CONV) );
	    
        /* Only handle angular units if we know the projection is geographic. */
        if (oSRS.IsGeographic()) 
        {
            if (EQUAL(papszFields[nCount-1],"units=Radians") )
                oSRS.SetAngularUnits( SRS_UA_RADIAN, 1. );
            else 
            {
                /* Degrees, minutes and seconds will all be represented as degrees. */
                oSRS.SetAngularUnits( SRS_UA_DEGREE,  atof(SRS_UA_DEGREE_CONV));

                double conversionFactor = 1.;
                if (EQUAL(papszFields[nCount-1],"units=Minutes") )
                    conversionFactor = 60.;
                else if( EQUAL(papszFields[nCount-1],"units=Seconds") )
                    conversionFactor = 3600.;
                adfGeoTransform[0] /= conversionFactor;
                adfGeoTransform[1] /= conversionFactor;
                adfGeoTransform[2] /= conversionFactor;
                adfGeoTransform[3] /= conversionFactor;
                adfGeoTransform[4] /= conversionFactor;
                adfGeoTransform[5] /= conversionFactor;
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Turn back into WKT.                                             */
/* -------------------------------------------------------------------- */
    if( oSRS.GetRoot() != NULL )
    {
        oSRS.Fixup();
	if ( pszProjection )
	{
	    CPLFree( pszProjection );
	    pszProjection = NULL;
	}
        oSRS.exportToWkt( &pszProjection );
    }

    CSLDestroy( papszFields );
    CSLDestroy( papszPI );
    return TRUE;
}

/************************************************************************/
/*                           ProcessRPCinfo()                           */
/*                                                                      */
/*      Extract RPC transformation coefficients if they are present     */
/*      and sets into the standard metadata fields for RPC.             */
/************************************************************************/

void ENVIDataset::ProcessRPCinfo( const char *pszRPCinfo, 
                                 int numCols, int numRows)
{
    char	**papszFields;
    char    sVal[1280];
    int         nCount;

    papszFields = SplitList( pszRPCinfo );
    nCount = CSLCount(papszFields);

    if( nCount < 90 )
    {
        CSLDestroy( papszFields );
        return;
    }
	
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[0]));
    SetMetadataItem("LINE_OFF",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[5]));
    SetMetadataItem("LINE_SCALE",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[1]));
    SetMetadataItem("SAMP_OFF",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[6]));
    SetMetadataItem("SAMP_SCALE",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[2]));
    SetMetadataItem("LAT_OFF",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[7]));
    SetMetadataItem("LAT_SCALE",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[3]));
    SetMetadataItem("LONG_OFF",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[8]));
    SetMetadataItem("LONG_SCALE",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[4]));
    SetMetadataItem("HEIGHT_OFF",sVal,"RPC");
    snprintf(sVal, sizeof(sVal),  "%.16g",atof(papszFields[9]));
    SetMetadataItem("HEIGHT_SCALE",sVal,"RPC");

    sVal[0] = '\0'; 
    int i;
    for(i = 0; i < 20; i++ )
       snprintf(sVal+strlen(sVal), sizeof(sVal),  "%.16g ", 
           atof(papszFields[10+i]));
    SetMetadataItem("LINE_NUM_COEFF",sVal,"RPC");

    sVal[0] = '\0'; 
    for(i = 0; i < 20; i++ )
       snprintf(sVal+strlen(sVal), sizeof(sVal),  "%.16g ",
           atof(papszFields[30+i]));
    SetMetadataItem("LINE_DEN_COEFF",sVal,"RPC");
      
    sVal[0] = '\0'; 
    for(i = 0; i < 20; i++ )
       snprintf(sVal+strlen(sVal), sizeof(sVal),  "%.16g ",
           atof(papszFields[50+i]));
    SetMetadataItem("SAMP_NUM_COEFF",sVal,"RPC");
      
    sVal[0] = '\0'; 
    for(i = 0; i < 20; i++ )
       snprintf(sVal+strlen(sVal), sizeof(sVal),  "%.16g ",
           atof(papszFields[70+i]));
    SetMetadataItem("SAMP_DEN_COEFF",sVal,"RPC");
	
    snprintf(sVal, sizeof(sVal), "%.16g", 
        atof(papszFields[3]) - atof(papszFields[8]));
    SetMetadataItem("MIN_LONG",sVal,"RPC");

    snprintf(sVal, sizeof(sVal), "%.16g", 
        atof(papszFields[3]) + atof(papszFields[8]) );
    SetMetadataItem("MAX_LONG",sVal,"RPC");

    snprintf(sVal, sizeof(sVal), "%.16g",
        atof(papszFields[2]) - atof(papszFields[7]));
    SetMetadataItem("MIN_LAT",sVal,"RPC");

    snprintf(sVal, sizeof(sVal), "%.16g",
        atof(papszFields[2]) + atof(papszFields[7]));
    SetMetadataItem("MAX_LAT",sVal,"RPC");

    if (nCount == 93)
    {
        SetMetadataItem("TILE_ROW_OFFSET",papszFields[90],"RPC");
        SetMetadataItem("TILE_COL_OFFSET",papszFields[91],"RPC");
        SetMetadataItem("ENVI_RPC_EMULATION",papszFields[92],"RPC");
    }

    /*   Handle the chipping case where the image is a subset. */
    double rowOffset, colOffset;
    rowOffset = (nCount == 93) ? atof(papszFields[90]) : 0;
    colOffset = (nCount == 93) ? atof(papszFields[91]) : 0;
    if (rowOffset || colOffset)
    {
        SetMetadataItem("ICHIP_SCALE_FACTOR", "1");
        SetMetadataItem("ICHIP_ANAMORPH_CORR", "0");
        SetMetadataItem("ICHIP_SCANBLK_NUM", "0");

        SetMetadataItem("ICHIP_OP_ROW_11", "0.5");
        SetMetadataItem("ICHIP_OP_COL_11", "0.5");
        SetMetadataItem("ICHIP_OP_ROW_12", "0.5");
        SetMetadataItem("ICHIP_OP_COL_21", "0.5");
        snprintf(sVal, sizeof(sVal), "%.16g", numCols - 0.5);
        SetMetadataItem("ICHIP_OP_COL_12", sVal);
        SetMetadataItem("ICHIP_OP_COL_22", sVal);
        snprintf(sVal, sizeof(sVal), "%.16g", numRows - 0.5);
        SetMetadataItem("ICHIP_OP_ROW_21", sVal);
        SetMetadataItem("ICHIP_OP_ROW_22", sVal);

        snprintf(sVal, sizeof(sVal), "%.16g", rowOffset + 0.5);
        SetMetadataItem("ICHIP_FI_ROW_11", sVal);
        SetMetadataItem("ICHIP_FI_ROW_12", sVal);
        snprintf(sVal, sizeof(sVal), "%.16g", colOffset + 0.5);
        SetMetadataItem("ICHIP_FI_COL_11", sVal);
        SetMetadataItem("ICHIP_FI_COL_21", sVal);
        snprintf(sVal, sizeof(sVal), "%.16g", colOffset + numCols - 0.5);
        SetMetadataItem("ICHIP_FI_COL_12", sVal);
        SetMetadataItem("ICHIP_FI_COL_22", sVal);
        snprintf(sVal, sizeof(sVal), "%.16g", rowOffset + numRows - 0.5);
        SetMetadataItem("ICHIP_FI_ROW_21", sVal);
        SetMetadataItem("ICHIP_FI_ROW_22", sVal);
    }
    CSLDestroy(papszFields);
}

void ENVIDataset::ProcessStatsFile()
{
    VSILFILE	*fpStaFile;

    osStaFilename = CPLResetExtension( pszHDRFilename, "sta" );
    fpStaFile = VSIFOpenL( osStaFilename, "rb" );

    if (!fpStaFile)
    {
        osStaFilename = "";
        return;
    }

    int lTestHeader[10],lOffset;

    if( VSIFReadL( lTestHeader, sizeof(int), 10, fpStaFile ) != 10 )
    {
        VSIFCloseL( fpStaFile );
        osStaFilename = "";
        return;
    }

    int isFloat;
    isFloat = (byteSwapInt(lTestHeader[0]) == 1111838282);

    int nb,i;
    float * fStats;
    double * dStats, dMin, dMax, dMean, dStd;
        
    nb=byteSwapInt(lTestHeader[3]);
    
    if (nb < 0 || nb > nBands)
    {
        CPLDebug("ENVI", ".sta file has statistics for %d bands, "
                         "whereas the dataset has only %d bands", nb, nBands);
        nb = nBands;
    }
    
    VSIFSeekL(fpStaFile,40+(nb+1)*4,SEEK_SET);

    if (VSIFReadL(&lOffset,sizeof(int),1,fpStaFile) == 1)
    {
        VSIFSeekL(fpStaFile,40+(nb+1)*8+byteSwapInt(lOffset)+nb,SEEK_SET);
        // This should be the beginning of the statistics
        if (isFloat)
        {
            fStats = (float*)CPLCalloc(nb*4,4);
            if ((int)VSIFReadL(fStats,4,nb*4,fpStaFile) == nb*4)
            {
                for (i=0;i<nb;i++)
                {
                    GetRasterBand(i+1)->SetStatistics(
                        byteSwapFloat(fStats[i]),
                        byteSwapFloat(fStats[nb+i]),
                        byteSwapFloat(fStats[2*nb+i]), 
                        byteSwapFloat(fStats[3*nb+i]));
                }
            }
            CPLFree(fStats);
        }
        else
        {
            dStats = (double*)CPLCalloc(nb*4,8);
            if ((int)VSIFReadL(dStats,8,nb*4,fpStaFile) == nb*4)
            {
                for (i=0;i<nb;i++)
                {
                    dMin = byteSwapDouble(dStats[i]);
                    dMax = byteSwapDouble(dStats[nb+i]);
                    dMean = byteSwapDouble(dStats[2*nb+i]);
                    dStd = byteSwapDouble(dStats[3*nb+i]);
                    if (dMin != dMax && dStd != 0) 
                        GetRasterBand(i+1)->SetStatistics(dMin,dMax,dMean,dStd);
                }
            }
            CPLFree(dStats);
        }
    }
    VSIFCloseL( fpStaFile );
}

int ENVIDataset::byteSwapInt(int swapMe)
{
    CPL_MSBPTR32(&swapMe);
    return swapMe;
}

float ENVIDataset::byteSwapFloat(float swapMe)
{
    CPL_MSBPTR32(&swapMe);
    return swapMe;
}

double ENVIDataset::byteSwapDouble(double swapMe)
{
    CPL_MSBPTR64(&swapMe);
    return swapMe;
}


/************************************************************************/
/*                             ReadHeader()                             */
/************************************************************************/

int ENVIDataset::ReadHeader( VSILFILE * fpHdr )

{

    CPLReadLineL( fpHdr );

/* -------------------------------------------------------------------- */
/*      Now start forming sets of name/value pairs.                     */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        const char *pszNewLine;
        char       *pszWorkingLine;

        pszNewLine = CPLReadLineL( fpHdr );
        if( pszNewLine == NULL )
            break;

        if( strstr(pszNewLine,"=") == NULL )
            continue;

        pszWorkingLine = CPLStrdup(pszNewLine);

        // Collect additional lines if we have open sqiggly bracket.
        if( strstr(pszWorkingLine,"{") != NULL 
            && strstr(pszWorkingLine,"}") == NULL )
        {
            do { 
                pszNewLine = CPLReadLineL( fpHdr );
                if( pszNewLine )
                {
                    pszWorkingLine = (char *) 
                        CPLRealloc(pszWorkingLine, 
                                 strlen(pszWorkingLine)+strlen(pszNewLine)+1);
                    strcat( pszWorkingLine, pszNewLine );
                }
            } while( pszNewLine != NULL && strstr(pszNewLine,"}") == NULL );
        }

        // Try to break input into name and value portions.  Trim whitespace.
        const char *pszValue;
        int         iEqual;

        for( iEqual = 0; 
             pszWorkingLine[iEqual] != '\0' && pszWorkingLine[iEqual] != '=';
             iEqual++ ) {}

        if( pszWorkingLine[iEqual] == '=' )
        {
            int		i;

            pszValue = pszWorkingLine + iEqual + 1;
            while( *pszValue == ' ' || *pszValue == '\t' )
                pszValue++;
            
            pszWorkingLine[iEqual--] = '\0';
            while( iEqual > 0 
                   && (pszWorkingLine[iEqual] == ' '
                       || pszWorkingLine[iEqual] == '\t') )
                pszWorkingLine[iEqual--] = '\0';

            // Convert spaces in the name to underscores.
            for( i = 0; pszWorkingLine[i] != '\0'; i++ )
            {
                if( pszWorkingLine[i] == ' ' )
                    pszWorkingLine[i] = '_';
            }

            papszHeader = CSLSetNameValue( papszHeader, 
                                           pszWorkingLine, pszValue );
        }

        CPLFree( pszWorkingLine );
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ENVIDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    
/* -------------------------------------------------------------------- */
/*	We assume the user is pointing to the binary (ie. .bil) file.	*/
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 2 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do we have a .hdr file?  Try upper and lower case, and          */
/*      replacing the extension as well as appending the extension      */
/*      to whatever we currently have.                                  */
/* -------------------------------------------------------------------- */
    const char	*pszMode;
    CPLString   osHdrFilename;
    VSILFILE	*fpHeader = NULL;

    if( poOpenInfo->eAccess == GA_Update )
	pszMode = "r+";
    else
	pszMode = "r";
    
    if (poOpenInfo->papszSiblingFiles == NULL)
    {
        osHdrFilename = CPLResetExtension( poOpenInfo->pszFilename, "hdr" );
        fpHeader = VSIFOpenL( osHdrFilename, pszMode );
    
        if( fpHeader == NULL && VSIIsCaseSensitiveFS(osHdrFilename) )
        {
            osHdrFilename = CPLResetExtension( poOpenInfo->pszFilename, "HDR" );
            fpHeader = VSIFOpenL( osHdrFilename, pszMode );
        }

        if( fpHeader == NULL )
        {
            osHdrFilename = CPLFormFilename( NULL, poOpenInfo->pszFilename, 
                                            "hdr" );
            fpHeader = VSIFOpenL( osHdrFilename, pszMode );
        }

        if( fpHeader == NULL && VSIIsCaseSensitiveFS(osHdrFilename) )
        {
            osHdrFilename = CPLFormFilename( NULL, poOpenInfo->pszFilename, 
                                            "HDR" );
            fpHeader = VSIFOpenL( osHdrFilename, pszMode );
        }

    }
    else
    {
        /* -------------------------------------------------------------------- */
        /*      Now we need to tear apart the filename to form a .HDR           */
        /*      filename.                                                       */
        /* -------------------------------------------------------------------- */
        CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
        CPLString osName = CPLGetFilename( poOpenInfo->pszFilename );

        int iFile = CSLFindString(poOpenInfo->papszSiblingFiles, 
                                  CPLResetExtension( osName, "hdr" ) );
        if( iFile >= 0 )
        {
            osHdrFilename = CPLFormFilename( osPath, poOpenInfo->papszSiblingFiles[iFile], 
                                             NULL );
            fpHeader = VSIFOpenL( osHdrFilename, pszMode );
        }
        else
        {
            iFile = CSLFindString(poOpenInfo->papszSiblingFiles,
                                  CPLFormFilename( NULL, osName, "hdr" ));
            if( iFile >= 0 )
            {
                osHdrFilename = CPLFormFilename( osPath, poOpenInfo->papszSiblingFiles[iFile], 
                                                 NULL );
                fpHeader = VSIFOpenL( osHdrFilename, pszMode );
            }
        }
    }

    if( fpHeader == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Check that the first line says "ENVI".                          */
/* -------------------------------------------------------------------- */
    char	szTestHdr[4];

    if( VSIFReadL( szTestHdr, 4, 1, fpHeader ) != 1 )
    {
        VSIFCloseL( fpHeader );
        return NULL;
    }
    if( strncmp(szTestHdr,"ENVI",4) != 0 )
    {
        VSIFCloseL( fpHeader );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ENVIDataset 	*poDS;

    poDS = new ENVIDataset();
    poDS->pszHDRFilename = CPLStrdup(osHdrFilename);
    poDS->fp = fpHeader;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    if( !poDS->ReadHeader( fpHeader ) )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Has the user selected the .hdr file to open?                    */
/* -------------------------------------------------------------------- */
    if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "hdr") )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The selected file is an ENVI header file, but to\n"
                  "open ENVI datasets, the data file should be selected\n"
                  "instead of the .hdr file.  Please try again selecting\n"
                  "the data file corresponding to the header file:\n"
                  "  %s\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Has the user selected the .sta (stats) file to open?            */
/* -------------------------------------------------------------------- */
    if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "sta") )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The selected file is an ENVI statistics file.\n"
                  "To open ENVI datasets, the data file should be selected\n"
                  "instead of the .sta file.  Please try again selecting\n"
                  "the data file corresponding to the statistics file:\n"
                  "  %s\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract required values from the .hdr.                          */
/* -------------------------------------------------------------------- */
    int	nLines = 0, nSamples = 0, nBands = 0, nHeaderSize = 0;
    const char   *pszInterleave = NULL;

    if( CSLFetchNameValue(poDS->papszHeader,"lines") )
        nLines = atoi(CSLFetchNameValue(poDS->papszHeader,"lines"));

    if( CSLFetchNameValue(poDS->papszHeader,"samples") )
        nSamples = atoi(CSLFetchNameValue(poDS->papszHeader,"samples"));

    if( CSLFetchNameValue(poDS->papszHeader,"bands") )
        nBands = atoi(CSLFetchNameValue(poDS->papszHeader,"bands"));

    pszInterleave = CSLFetchNameValue(poDS->papszHeader,"interleave");

    /* In case, there is no interleave keyword, we try to derive it from the */
    /* file extension. */
    if( pszInterleave == NULL )
    {
        const char* pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
        pszInterleave = pszExtension;
    }

    if ( !EQUALN(pszInterleave, "BSQ",3) &&
         !EQUALN(pszInterleave, "BIP",3) &&
         !EQUALN(pszInterleave, "BIL",3) )
    {
        CPLDebug("ENVI", "Unset or unknown value for 'interleave' keyword --> assuming BSQ interleaving");
        pszInterleave = "bsq";
    }

    if (!GDALCheckDatasetDimensions(nSamples, nLines) || !GDALCheckBandCount(nBands, FALSE))
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The file appears to have an associated ENVI header, but\n"
                  "one or more of the samples, lines and bands\n"
                  "keywords appears to be missing or invalid." );
        return NULL;
    }

    if( CSLFetchNameValue(poDS->papszHeader,"header_offset") )
        nHeaderSize = atoi(CSLFetchNameValue(poDS->papszHeader,"header_offset"));

/* -------------------------------------------------------------------- */
/*      Translate the datatype.                                         */
/* -------------------------------------------------------------------- */
    GDALDataType	eType = GDT_Byte;

    if( CSLFetchNameValue(poDS->papszHeader,"data_type" ) != NULL )
    {
        switch( atoi(CSLFetchNameValue(poDS->papszHeader,"data_type" )) )
        {
          case 1:
            eType = GDT_Byte;
            break;

          case 2:
            eType = GDT_Int16;
            break;

          case 3:
            eType = GDT_Int32;
            break;

          case 4:
            eType = GDT_Float32;
            break;

          case 5:
            eType = GDT_Float64;
            break;

          case 6:
            eType = GDT_CFloat32;
            break;

          case 9:
            eType = GDT_CFloat64;
            break;

          case 12:
            eType = GDT_UInt16;
            break;

          case 13:
            eType = GDT_UInt32;
            break;

            /* 14=Int64, 15=UInt64 */

          default:
            delete poDS;
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "The file does not have a value for the data_type\n"
                      "that is recognised by the GDAL ENVI driver.");
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the byte order.					*/
/* -------------------------------------------------------------------- */
    int		bNativeOrder = TRUE;

    if( CSLFetchNameValue(poDS->papszHeader,"byte_order" ) != NULL )
    {
#ifdef CPL_LSB                               
        bNativeOrder = atoi(CSLFetchNameValue(poDS->papszHeader,
                                              "byte_order" )) == 0;
#else
        bNativeOrder = atoi(CSLFetchNameValue(poDS->papszHeader,
                                              "byte_order" )) != 0;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Warn about unsupported file types virtual mosaic and meta file.*/
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(poDS->papszHeader,"file_type" ) != NULL )
    {
        // when the file type is one of these we return an invalid file type err
            //'envi meta file'
            //'envi virtual mosaic'
            //'envi spectral library'
            //'envi fft result'

        // when the file type is one of these we open it 
            //'envi standard'
            //'envi classification' 

        // when the file type is anything else we attempt to open it as a raster.

        const char * pszEnviFileType;
        pszEnviFileType = CSLFetchNameValue(poDS->papszHeader,"file_type");

        // envi gdal does not support any of these
        // all others we will attempt to open
        if(EQUAL(pszEnviFileType, "envi meta file") ||
           EQUAL(pszEnviFileType, "envi virtual mosaic") ||
           EQUAL(pszEnviFileType, "envi spectral library"))
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "File %s contains an invalid file type in the ENVI .hdr\n"
                      "GDAL does not support '%s' type files.",
                      poOpenInfo->pszFilename, pszEnviFileType );
            delete poDS;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Detect (gzipped) compressed datasets.                           */
/* -------------------------------------------------------------------- */
    int bIsCompressed = FALSE;
    if( CSLFetchNameValue(poDS->papszHeader,"file_compression" ) != NULL )
    {
        if( atoi(CSLFetchNameValue(poDS->papszHeader,"file_compression" )) 
            != 0 )
        {
            bIsCompressed = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nSamples;
    poDS->nRasterYSize = nLines;
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Reopen file in update mode if necessary.                        */
/* -------------------------------------------------------------------- */
    CPLString osImageFilename(poOpenInfo->pszFilename);
    if (bIsCompressed)
        osImageFilename = "/vsigzip/" + osImageFilename;
    if( poOpenInfo->eAccess == GA_Update )
    {
        if (bIsCompressed)
        {
            delete poDS;
            CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Cannot open compressed file in update mode.\n");
            return NULL;
        }
        poDS->fpImage = VSIFOpenL( osImageFilename, "rb+" );
    }
    else
        poDS->fpImage = VSIFOpenL( osImageFilename, "rb" );

    if( poDS->fpImage == NULL )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to re-open %s within ENVI driver.\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    int	nDataSize = GDALGetDataTypeSize(eType)/8;
    int nPixelOffset, nLineOffset;
    vsi_l_offset nBandOffset;
    int bIntOverflow = FALSE;
    
    if( EQUALN(pszInterleave, "bil", 3) )
    {
        poDS->interleave = BIL;
        poDS->SetMetadataItem( "INTERLEAVE", "LINE", "IMAGE_STRUCTURE" );
        if (nSamples > INT_MAX / (nDataSize * nBands)) bIntOverflow = TRUE;
        nLineOffset = nDataSize * nSamples * nBands;
        nPixelOffset = nDataSize;
        nBandOffset = (vsi_l_offset)nDataSize * nSamples;
    }
    else if( EQUALN(pszInterleave, "bip", 3) )
    {
        poDS->interleave = BIP;
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
        if (nSamples > INT_MAX / (nDataSize * nBands)) bIntOverflow = TRUE;
        nLineOffset = nDataSize * nSamples * nBands;
        nPixelOffset = nDataSize * nBands;
        nBandOffset = nDataSize;
    }
    else /* bsq */
    {
        poDS->interleave = BSQ;
        poDS->SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );
        if (nSamples > INT_MAX / nDataSize) bIntOverflow = TRUE;
        nLineOffset = nDataSize * nSamples;
        nPixelOffset = nDataSize;
        nBandOffset = (vsi_l_offset)nLineOffset * nLines;
    }

    if (bIntOverflow)
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Int overflow occured.");
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = nBands;
    for( i = 0; i < poDS->nBands; i++ )
    {
        poDS->SetBand( i + 1,
                       new RawRasterBand(poDS, i + 1, poDS->fpImage,
                                         nHeaderSize + nBandOffset * i,
                                         nPixelOffset, nLineOffset, eType,
                                         bNativeOrder, TRUE) );
    }

/* -------------------------------------------------------------------- */
/*      Apply band names if we have them.                               */
/*      Use wavelength for more descriptive information if possible     */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( poDS->papszHeader, "band_names" ) != NULL ||
        CSLFetchNameValue( poDS->papszHeader, "wavelength" ) != NULL)
    {
        char	**papszBandNames = 
            poDS->SplitList( CSLFetchNameValue( poDS->papszHeader, 
                                                "band_names" ) );
        char	**papszWL = 
            poDS->SplitList( CSLFetchNameValue( poDS->papszHeader, 
                                                "wavelength" ) );

        const char *pszWLUnits = NULL;
        int nWLCount = CSLCount(papszWL);
        if (papszWL) 
        {
            /* If WL information is present, process wavelength units */
            pszWLUnits = CSLFetchNameValue( poDS->papszHeader, 
                                            "wavelength_units" );
            if (pszWLUnits)
            {
                /* Don't show unknown or index units */
                if (EQUAL(pszWLUnits,"Unknown") ||
                    EQUAL(pszWLUnits,"Index") )
                    pszWLUnits=0;
            }
            if (pszWLUnits)
            {
                /* set wavelength units to dataset metadata */
                poDS->SetMetadataItem("wavelength_units", pszWLUnits);
            }
        }

        for( i = 0; i < nBands; i++ )
        {
            CPLString osBandId, osBandName, osWavelength;

            /* First set up the wavelength names and units if available */
            if (papszWL && nWLCount > i) 
            {
                osWavelength = papszWL[i];
                if (pszWLUnits) 
                {
                    osWavelength += " ";
                    osWavelength += pszWLUnits;
                }
            }

            /* Build the final name for this band */
            if (papszBandNames && CSLCount(papszBandNames) > i)
            {
                osBandName = papszBandNames[i];
                if (strlen(osWavelength) > 0) 
                {
                    osBandName += " (";
                    osBandName += osWavelength;
                    osBandName += ")";
                }
            }
            else   /* WL but no band names */
                osBandName = osWavelength;

            /* Description is for internal GDAL usage */
            poDS->GetRasterBand(i + 1)->SetDescription( osBandName );

            /* Metadata field named Band_1, etc. needed for ArcGIS integration */
            osBandId = CPLSPrintf("Band_%i", i+1);
            poDS->SetMetadataItem(osBandId, osBandName);

            /* Set wavelength metadata to band */
            if (papszWL && nWLCount > i)
            {
                poDS->GetRasterBand(i+1)->SetMetadataItem(
                    "wavelength", papszWL[i]);

                if (pszWLUnits)
                {
                    poDS->GetRasterBand(i+1)->SetMetadataItem(
                        "wavelength_units", pszWLUnits);
                }
            }

        }
        CSLDestroy( papszWL );
        CSLDestroy( papszBandNames );
    }
/* -------------------------------------------------------------------- */
/*      Apply class names if we have them.                              */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( poDS->papszHeader, "class_names" ) != NULL )
    {
        char	**papszClassNames = 
            poDS->SplitList( CSLFetchNameValue( poDS->papszHeader, 
                                                "class_names" ) );

        poDS->GetRasterBand(1)->SetCategoryNames( papszClassNames );
        CSLDestroy( papszClassNames );
    }
    
/* -------------------------------------------------------------------- */
/*      Apply colormap if we have one.					*/
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( poDS->papszHeader, "class_lookup" ) != NULL )
    {
        char	**papszClassColors = 
            poDS->SplitList( CSLFetchNameValue( poDS->papszHeader, 
                                                "class_lookup" ) );
        int nColorValueCount = CSLCount(papszClassColors);
        GDALColorTable oCT;
		
        for( i = 0; i*3+2 < nColorValueCount; i++ )
        {
            GDALColorEntry sEntry;

            sEntry.c1 = (short) atoi(papszClassColors[i*3+0]);
            sEntry.c2 = (short) atoi(papszClassColors[i*3+1]);
            sEntry.c3 = (short) atoi(papszClassColors[i*3+2]);
            sEntry.c4 = 255;
            oCT.SetColorEntry( i, &sEntry );
        }

        CSLDestroy( papszClassColors );

        poDS->GetRasterBand(1)->SetColorTable( &oCT );
        poDS->GetRasterBand(1)->SetColorInterpretation( GCI_PaletteIndex );
    }

/* -------------------------------------------------------------------- */
/*      Set the nodata value if it is present                           */
/* -------------------------------------------------------------------- */	
    if( CSLFetchNameValue(poDS->papszHeader,"data_ignore_value" ) != NULL )
    {
        for( i = 0; i < poDS->nBands; i++ )
            ((RawRasterBand*)poDS->GetRasterBand(i+1))->SetNoDataValue(atof(
                                                                             CSLFetchNameValue(poDS->papszHeader,"data_ignore_value")));
    }

/* -------------------------------------------------------------------- */
/*      Set all the header metadata into the ENVI domain                */
/* -------------------------------------------------------------------- */	
    {
        char** pTmp = poDS->papszHeader;
        while (*pTmp != NULL)
        {
            char** pTokens = CSLTokenizeString2(*pTmp, "=", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
            if (pTokens[0] != NULL && pTokens[1] != NULL && pTokens[2] == NULL)
            {
                poDS->SetMetadataItem(pTokens[0], pTokens[1], "ENVI");
            }
            CSLDestroy(pTokens);
            pTmp++;
        }
    } 

/* -------------------------------------------------------------------- */
/*      Read the stats file if it is present                            */
/* -------------------------------------------------------------------- */	
    poDS->ProcessStatsFile();

/* -------------------------------------------------------------------- */
/*      Look for mapinfo                                                */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( poDS->papszHeader, "map_info" ) != NULL )
    {
        poDS->bFoundMapinfo = 
            poDS->ProcessMapinfo( 
                CSLFetchNameValue(poDS->papszHeader,"map_info") );
    }

/* -------------------------------------------------------------------- */
/*      Look for RPC mapinfo                                            */
/* -------------------------------------------------------------------- */
    if( !poDS->bFoundMapinfo &&
        CSLFetchNameValue( poDS->papszHeader, "rpc_info" ) != NULL )
    {
        poDS->ProcessRPCinfo( 
            CSLFetchNameValue(poDS->papszHeader,"rpc_info"), 
            poDS->nRasterXSize, poDS->nRasterYSize);
    }
    
/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

int ENVIDataset::GetEnviType(GDALDataType eType)
{
  int iENVIType;
  switch( eType )
  {
      case GDT_Byte:
	      iENVIType = 1;
	      break;
      case GDT_Int16:
	      iENVIType = 2;
	      break;
      case GDT_Int32:
	      iENVIType = 3;
	      break;
      case GDT_Float32:
	      iENVIType = 4;
	      break;
      case GDT_Float64:
	      iENVIType = 5;
	      break;
      case GDT_CFloat32:
	      iENVIType = 6;
	      break;
      case GDT_CFloat64:
	      iENVIType = 9;
	      break;
      case GDT_UInt16:
	      iENVIType = 12;
	      break;
      case GDT_UInt32:
	      iENVIType = 13;
	      break;

	/* 14=Int64, 15=UInt64 */

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create ENVI .hdr labelled dataset with an illegal\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );
      	return 1;
  }
  return iENVIType;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *ENVIDataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    int	iENVIType = GetEnviType(eType);
    if (0 == iENVIType)
      return 0;

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE	*fp;

    fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Just write out a couple of bytes to establish the binary        */
/*      file, and then close it.                                        */
/* -------------------------------------------------------------------- */
    VSIFWriteL( (void *) "\0\0", 2, 1, fp );
    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Create the .hdr filename.                                       */
/* -------------------------------------------------------------------- */
    const char	*pszHDRFilename;
    const char	*pszSuffix;

    pszSuffix = CSLFetchNameValue( papszOptions, "SUFFIX" );
    if ( pszSuffix && EQUALN( pszSuffix, "ADD", 3 ))
	pszHDRFilename = CPLFormFilename( NULL, pszFilename, "hdr" );
    else
	pszHDRFilename = CPLResetExtension(pszFilename, "hdr" );

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszHDRFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszHDRFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out the header.                                           */
/* -------------------------------------------------------------------- */
    int		iBigEndian;
    const char	*pszInterleaving;
    
#ifdef CPL_LSB
    iBigEndian = 0;
#else
    iBigEndian = 1;
#endif

    VSIFPrintfL( fp, "ENVI\n" );
    VSIFPrintfL( fp, "samples = %d\nlines   = %d\nbands   = %d\n",
		nXSize, nYSize, nBands );
    VSIFPrintfL( fp, "header offset = 0\nfile type = ENVI Standard\n" );
    VSIFPrintfL( fp, "data type = %d\n", iENVIType );
    pszInterleaving = CSLFetchNameValue( papszOptions, "INTERLEAVE" );
    if ( pszInterleaving )
    {
	if ( EQUALN( pszInterleaving, "bip", 3 ) )
	    pszInterleaving = "bip";		    // interleaved by pixel
	else if ( EQUALN( pszInterleaving, "bil", 3 ) )
	    pszInterleaving = "bil";		    // interleaved by line
	else
	    pszInterleaving = "bsq";		// band sequental by default
    }
    else
	pszInterleaving = "bsq";
    VSIFPrintfL( fp, "interleave = %s\n", pszInterleaving);
    VSIFPrintfL( fp, "byte order = %d\n", iBigEndian );

    VSIFCloseL( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_ENVI()                          */
/************************************************************************/

void GDALRegister_ENVI()
{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ENVI" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ENVI" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ENVI .hdr Labelled" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#ENVI" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 "
                                   "Float32 Float64 CFloat32 CFloat64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='SUFFIX' type='string-select'>"
"       <Value>ADD</Value>"
"   </Option>"
"   <Option name='INTERLEAVE' type='string-select'>"
"       <Value>BIP</Value>"
"       <Value>BIL</Value>"
"       <Value>BSQ</Value>"
"   </Option>"
"</CreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->pfnOpen = ENVIDataset::Open;
        poDriver->pfnCreate = ENVIDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
