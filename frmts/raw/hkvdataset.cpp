/******************************************************************************
 * $Id$
 *
 * Project:  GView
 * Purpose:  Implementation of Atlantis HKV labelled blob support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.25  2003/03/03 20:10:05  gwalter
 * Updated MFF and HKV (MFF2) georeferencing support.
 *
 * Revision 1.24  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.23  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.22  2002/06/19 18:21:08  warmerda
 * removed stat buf from GDALOpenInfo
 *
 * Revision 1.21  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.20  2002/04/24 19:27:00  warmerda
 * Added HKV nodata read support (pixel.no_data).
 *
 * Revision 1.19  2001/12/12 18:15:46  warmerda
 * preliminary update for large raw file support
 *
 * Revision 1.18  2001/12/12 17:19:06  warmerda
 * Use CPLStat for directories.
 *
 * Revision 1.17  2001/12/08 04:45:59  warmerda
 * fixed south setting for UTM
 *
 * Revision 1.16  2001/11/11 23:51:00  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.15  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.14  2001/07/11 18:09:06  warmerda
 * Correct corner GCPs. BOTTOM_RIGHT actually refers to the top left corner of
 * the bottom right pixel!
 *
 * Revision 1.13  2001/06/20 16:10:02  warmerda
 * overhauled to TIFF style overviews
 *
 * Revision 1.12  2001/06/13 19:42:10  warmerda
 * Changed blob->image_data, and HKV to MFF2.
 *
 * Revision 1.11  2000/12/14 17:34:13  warmerda
 * Added dataset delete method.
 *
 * Revision 1.10  2000/12/05 22:40:09  warmerda
 * Added very limited SetProjection support, includes Bessel
 *
 * Revision 1.9  2000/11/29 21:31:28  warmerda
 * added support for writing georef on SetGeoTransform
 *
 * Revision 1.8  2000/08/16 15:51:39  warmerda
 * added support for reading overviews
 *
 * Revision 1.7  2000/08/15 19:28:26  warmerda
 * added help topic
 *
 * Revision 1.6  2000/07/12 19:21:59  warmerda
 * added support for _reading_ georef file
 *
 * Revision 1.5  2000/06/05 17:24:06  warmerda
 * added real complex support
 *
 * Revision 1.4  2000/05/15 14:18:27  warmerda
 * added COMPLEX_INTERPRETATION metadata
 *
 * Revision 1.3  2000/04/05 19:28:48  warmerda
 * Fixed MSB case.
 *
 * Revision 1.2  2000/03/13 14:34:42  warmerda
 * avoid const problem on write
 *
 * Revision 1.1  2000/03/07 21:33:42  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"
#include <ctype.h>
#include "ogr_spatialref.h"
#include "atlsci_spheroid.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_HKV(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                            HKVRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HKVDataset;

class HKVRasterBand : public RawRasterBand
{
    friend class HKVDataset;

  public:
    		HKVRasterBand( HKVDataset *poDS, int nBand, FILE * fpRaw, 
                               unsigned int nImgOffset, int nPixelOffset,
                               int nLineOffset,
                               GDALDataType eDataType, int bNativeOrder );
    virtual     ~HKVRasterBand();
};

/************************************************************************/
/*                      HKV Spheroids                                   */
/************************************************************************/

class HKVSpheroidList : public SpheroidList
{

public:

  HKVSpheroidList();
  ~HKVSpheroidList();

};

HKVSpheroidList :: HKVSpheroidList()
{
  num_spheroids = 29;
  epsilonR = 0.1;
  epsilonI = 0.000001;

  spheroids[0].SetValuesByEqRadiusAndInvFlattening("airy-1830",6377563.396,299.3249646);
  spheroids[1].SetValuesByEqRadiusAndInvFlattening("modified-airy",6377340.189,299.3249646);
  spheroids[2].SetValuesByEqRadiusAndInvFlattening("australian-national",6378160,298.25);
  spheroids[3].SetValuesByEqRadiusAndInvFlattening("bessel-1841-namibia",6377483.865,299.1528128);
  spheroids[4].SetValuesByEqRadiusAndInvFlattening("bessel-1841",6377397.155,299.1528128);
  spheroids[5].SetValuesByEqRadiusAndInvFlattening("clarke-1858",6378294.0,294.297);
  spheroids[6].SetValuesByEqRadiusAndInvFlattening("clarke-1866",6378206.4,294.9786982);
  spheroids[7].SetValuesByEqRadiusAndInvFlattening("clarke-1880",6378249.145,293.465);
  spheroids[8].SetValuesByEqRadiusAndInvFlattening("everest-india-1830",6377276.345,300.8017);
  spheroids[9].SetValuesByEqRadiusAndInvFlattening("everest-sabah-sarawak",6377298.556,300.8017);
  spheroids[10].SetValuesByEqRadiusAndInvFlattening("everest-india-1956",6377301.243,300.8017);
  spheroids[11].SetValuesByEqRadiusAndInvFlattening("everest-malaysia-1969",6377295.664,300.8017);
  spheroids[12].SetValuesByEqRadiusAndInvFlattening("everest-malay-sing",6377304.063,300.8017);
  spheroids[13].SetValuesByEqRadiusAndInvFlattening("everest-pakistan",6377309.613,300.8017);
  spheroids[14].SetValuesByEqRadiusAndInvFlattening("modified-fisher-1960",6378155,298.3);
  spheroids[15].SetValuesByEqRadiusAndInvFlattening("helmert-1906",6378200,298.3);
  spheroids[16].SetValuesByEqRadiusAndInvFlattening("hough-1960",6378270,297);
  spheroids[17].SetValuesByEqRadiusAndInvFlattening("hughes",6378273.0,298.279);
  spheroids[18].SetValuesByEqRadiusAndInvFlattening("indonesian-1974",6378160,298.247);
  spheroids[19].SetValuesByEqRadiusAndInvFlattening("international-1924",6378388,297);
  spheroids[20].SetValuesByEqRadiusAndInvFlattening("iugc-67",6378160.0,298.254);
  spheroids[21].SetValuesByEqRadiusAndInvFlattening("iugc-75",6378140.0,298.25298);
  spheroids[22].SetValuesByEqRadiusAndInvFlattening("krassovsky-1940",6378245,298.3);
  spheroids[23].SetValuesByEqRadiusAndInvFlattening("kaula",6378165.0,292.308);
  spheroids[24].SetValuesByEqRadiusAndInvFlattening("grs-80",6378137,298.257222101);
  spheroids[25].SetValuesByEqRadiusAndInvFlattening("south-american-1969",6378160,298.25);
  spheroids[26].SetValuesByEqRadiusAndInvFlattening("wgs-72",6378135,298.26);
  spheroids[27].SetValuesByEqRadiusAndInvFlattening("wgs-84",6378137,298.257223563);
  spheroids[28].SetValuesByEqRadiusAndInvFlattening("ev-wgs-84",6378137.0,298.252841); 
  spheroids[29].SetValuesByEqRadiusAndInvFlattening("ev-bessel",6377397.0,299.1976073);

}

HKVSpheroidList::~HKVSpheroidList()

{
}

/************************************************************************/
/* ==================================================================== */
/*				HKVDataset				*/
/* ==================================================================== */
/************************************************************************/

class HKVDataset : public RawDataset
{
    friend class HKVRasterBand;

    char	*pszPath;
    FILE	*fpBlob;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    void        ProcessGeoref(const char *);
    void        ProcessGeorefGCP(char **, const char *, double, double);
    CPLErr      SetGCPProjection(const char *); /* for use in CreateCopy */

    char        *pszProjection;
    char        *pszGCPProjection;
    double      adfGeoTransform[6];

    char	**papszAttrib;

    int		bGeorefChanged;
    char	**papszGeoref;
    
  public:
    		HKVDataset();
    virtual     ~HKVDataset();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

    virtual CPLErr SetGeoTransform( double * );
    virtual CPLErr SetProjection( const char * );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    static GDALDataset *CreateCopy( const char * pszFilename, 
                                    GDALDataset *poSrcDS, 
                                    int bStrict, char ** papszOptions, 
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData );

    static CPLErr Delete( const char * pszName );
};

/************************************************************************/
/* ==================================================================== */
/*                            HKVRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HKVRasterBand()                            */
/************************************************************************/

HKVRasterBand::HKVRasterBand( HKVDataset *poDS, int nBand, FILE * fpRaw, 
                              unsigned int nImgOffset, int nPixelOffset,
                              int nLineOffset,
                              GDALDataType eDataType, int bNativeOrder )
        : RawRasterBand( (GDALDataset *) poDS, nBand, 
                         fpRaw, nImgOffset, nPixelOffset, 
                         nLineOffset, eDataType, bNativeOrder, TRUE )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                           ~HKVRasterBand()                           */
/************************************************************************/

HKVRasterBand::~HKVRasterBand()

{
}

/************************************************************************/
/* ==================================================================== */
/*				HKVDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            HKVDataset()                             */
/************************************************************************/

HKVDataset::HKVDataset()
{
    pszPath = NULL;
    papszAttrib = NULL;
    papszGeoref = NULL;
    bGeorefChanged = FALSE;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszProjection = CPLStrdup("");
    pszGCPProjection = CPLStrdup("");
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~HKVDataset()                            */
/************************************************************************/

HKVDataset::~HKVDataset()

{
    FlushCache();
    if( bGeorefChanged )
    {
        const char	*pszFilename;

        pszFilename = CPLFormFilename(pszPath, "georef", NULL );

        CSLSave( papszGeoref, pszFilename );
    }

    if( fpBlob != NULL )
        VSIFCloseL( fpBlob );
 
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CPLFree( pszProjection );
    CPLFree( pszGCPProjection );
    CPLFree( pszPath );
    CSLDestroy( papszGeoref );
    CSLDestroy( papszAttrib );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HKVDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HKVDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 ); 
    return( CE_None );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr HKVDataset::SetGeoTransform( double * padfTransform )

{
    char	szValue[128];

    /* NOTE:  Geotransform coordinates must match the current projection   */
    /* of the dataset being changed (not the geotransform source).         */
    /* ie. be in lat/longs for LL projected; UTM for UTM projected.        */
    /* SET PROJECTION BEFORE SETTING GEOTRANSFORM TO AVOID SYNCHRONIZATION */
    /* PROBLEMS!                                                           */

    /* Update the geotransform itself */
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );


    /* Update georef GCPs for saving later */
    if(( CSLFetchNameValue( papszGeoref, "projection.name" ) != NULL ) &&
       ( EQUAL(CSLFetchNameValue( papszGeoref, "projection.name" ),"UTM" )))

    {
        double temp_lat, temp_long;
        OGRSpatialReference oUTM;
        OGRSpatialReference oLL;
        OGRCoordinateTransformation *poTransform = NULL;
        int bSuccess=TRUE;
        char *pszPtemp;
        char *pszGCPtemp;

        /* pass copies of projection info, not originals (pointers */
        /* get updated by importFromWkt)                           */
        pszPtemp = CPLStrdup(pszProjection);
        pszGCPtemp = CPLStrdup(pszGCPProjection);
        oUTM.importFromWkt(&pszPtemp);
        oLL.importFromWkt(&pszGCPtemp);

        poTransform = OGRCreateCoordinateTransformation( &oUTM, &oLL );
        if( poTransform == NULL )
            bSuccess = FALSE;

    /* Projection is utm- coordinates need to be transformed */
    /* -------------------------------------------------------------------- */
    /*      top left                                                        */
    /* -------------------------------------------------------------------- */

        temp_lat = padfTransform[3];
        temp_long = padfTransform[0]; 
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = FALSE;

        if (bSuccess)
        {
            sprintf( szValue, "%.10f", temp_lat );
            papszGeoref = CSLSetNameValue( papszGeoref, "top_left.latitude", 
                                           szValue );

            sprintf( szValue, "%.10f", temp_long );
            papszGeoref = CSLSetNameValue( papszGeoref, "top_left.longitude", 
                                           szValue );
        }

    /* -------------------------------------------------------------------- */
    /*      top_right                                                       */
    /* -------------------------------------------------------------------- */

        temp_lat = padfTransform[3] + (GetRasterXSize()-1) * padfTransform[4];
        temp_long = padfTransform[0] + (GetRasterXSize()-1) * padfTransform[1];
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = FALSE;

        if (bSuccess)
        {
            sprintf( szValue, "%.10f", temp_lat );
            papszGeoref = CSLSetNameValue( papszGeoref, "top_right.latitude", 
                                           szValue );

            sprintf( szValue, "%.10f", temp_long );
            papszGeoref = CSLSetNameValue( papszGeoref, "top_right.longitude", 
                                           szValue );
        }

    /* -------------------------------------------------------------------- */
    /*      bottom_left                                                     */
    /* -------------------------------------------------------------------- */
        temp_lat = padfTransform[3] + (GetRasterYSize()-1) * padfTransform[5];
        temp_long = padfTransform[0] + (GetRasterYSize()-1) * padfTransform[2]; 
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = FALSE;

        if (bSuccess)
        {
            sprintf( szValue, "%.10f", temp_lat );
            papszGeoref = CSLSetNameValue( papszGeoref, "bottom_left.latitude", 
                                           szValue );

            sprintf( szValue, "%.10f", temp_long );
            papszGeoref = CSLSetNameValue( papszGeoref, "bottom_left.longitude", 
                                           szValue );
        }

    /* -------------------------------------------------------------------- */
    /*      bottom_right                                                    */
    /* -------------------------------------------------------------------- */
        temp_lat = padfTransform[3] + (GetRasterXSize()-1) * padfTransform[4] + 
          (GetRasterYSize()-1) * padfTransform[5];
        temp_long = padfTransform[0] + (GetRasterXSize()-1) * padfTransform[1] + 
          (GetRasterYSize()-1) * padfTransform[2];
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = FALSE;

        if (bSuccess)
        {
            sprintf( szValue, "%.10f", temp_lat );
            papszGeoref = CSLSetNameValue( papszGeoref, "bottom_right.latitude", 
                                           szValue );

            sprintf( szValue, "%.10f", temp_long );
            papszGeoref = CSLSetNameValue( papszGeoref, "bottom_right.longitude", 
                                           szValue );
        }

    /* -------------------------------------------------------------------- */
    /*      Center                                                          */
    /* -------------------------------------------------------------------- */
        temp_lat = padfTransform[3] + (GetRasterXSize()-1) * padfTransform[4] * 0.5 +
          (GetRasterYSize()-1) * padfTransform[5] * 0.5;
        temp_long = padfTransform[0] + (GetRasterXSize()-1) * padfTransform[1] * 0.5 +
                 (GetRasterYSize()-1) * padfTransform[2] * 0.5; 
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = FALSE;

        if (bSuccess)
        {
            sprintf( szValue, "%.10f", temp_lat );
            papszGeoref = CSLSetNameValue( papszGeoref, "centre.latitude", 
                                           szValue );

            sprintf( szValue, "%.10f", temp_long );
            papszGeoref = CSLSetNameValue( papszGeoref, "centre.longitude", 
                                           szValue );
        }
    
        if (!bSuccess)
        {
          CPLError(CE_Warning,CPLE_AppDefined,
                   "Warning- error setting header info in SetGeoTransform. Changes may not be saved properly.\n"); 
        }
     
        if (poTransform != NULL)
            delete poTransform;
    }
    else
    {
    /* Projection is lat/long- coordinates don't need to be transformed */
    /* -------------------------------------------------------------------- */
    /*      top left                                                        */
    /* -------------------------------------------------------------------- */
        sprintf( szValue, "%.10f", padfTransform[3] );
        papszGeoref = CSLSetNameValue( papszGeoref, "top_left.latitude", 
                                       szValue );

        sprintf( szValue, "%.10f", padfTransform[0] );
        papszGeoref = CSLSetNameValue( papszGeoref, "top_left.longitude", 
                                       szValue );

    /* -------------------------------------------------------------------- */
    /*      top_right                                                       */
    /* -------------------------------------------------------------------- */
        sprintf( szValue, "%.10f", 
                 padfTransform[3] + (GetRasterXSize()-1) * padfTransform[4] );
        papszGeoref = CSLSetNameValue( papszGeoref, "top_right.latitude", 
                                       szValue );

        sprintf( szValue, "%.10f", 
                 padfTransform[0] + (GetRasterXSize()-1) * padfTransform[1] );
        papszGeoref = CSLSetNameValue( papszGeoref, "top_right.longitude", 
                                       szValue );

    /* -------------------------------------------------------------------- */
    /*      bottom_left                                                     */
    /* -------------------------------------------------------------------- */
        sprintf( szValue, "%.10f", 
                 padfTransform[3] + (GetRasterYSize()-1) * padfTransform[5] );
        papszGeoref = CSLSetNameValue( papszGeoref, "bottom_left.latitude", 
                                       szValue );

        sprintf( szValue, "%.10f", 
                 padfTransform[0] + (GetRasterYSize()-1) * padfTransform[2] );
        papszGeoref = CSLSetNameValue( papszGeoref, "bottom_left.longitude", 
                                       szValue );

    /* -------------------------------------------------------------------- */
    /*      bottom_right                                                    */
    /* -------------------------------------------------------------------- */
        sprintf( szValue, "%.10f", 
                 padfTransform[3] + (GetRasterXSize()-1) * padfTransform[4] + 
                 (GetRasterYSize()-1) * padfTransform[5] );
        papszGeoref = CSLSetNameValue( papszGeoref, "bottom_right.latitude", 
                                       szValue );

        sprintf( szValue, "%.10f", 
                 padfTransform[0] + (GetRasterXSize()-1) * padfTransform[1] + 
                 (GetRasterYSize()-1) * padfTransform[2] );
        papszGeoref = CSLSetNameValue( papszGeoref, "bottom_right.longitude", 
                                       szValue );

    /* -------------------------------------------------------------------- */
    /*      Center                                                          */
    /* -------------------------------------------------------------------- */
        sprintf( szValue, "%.10f", 
                 padfTransform[3] + (GetRasterXSize()-1) * padfTransform[4] * 0.5 +
                 (GetRasterYSize()-1) * padfTransform[5] * 0.5);
        papszGeoref = CSLSetNameValue( papszGeoref, "centre.latitude", 
                                       szValue );

        sprintf( szValue, "%.10f", 
                 padfTransform[0] + (GetRasterXSize()-1) * padfTransform[1] * 0.5 +
                 (GetRasterYSize()-1) * padfTransform[2] * 0.5);
        papszGeoref = CSLSetNameValue( papszGeoref, "centre.longitude", 
                                       szValue );
    
    /* -------------------------------------------------------------------- */
    /*      Set projection to LL if not previously set.                     */
    /* -------------------------------------------------------------------- */
        if( CSLFetchNameValue( papszGeoref, "projection.name" ) == NULL )
        {
            papszGeoref = CSLSetNameValue( papszGeoref, "projection.name", "LL" );
            papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                           "ev-wgs-84" );
        }
    }


    bGeorefChanged = TRUE;

    return( CE_None );
}

CPLErr HKVDataset::SetGCPProjection( const char *pszNewProjection )
{
    if( !EQUALN(pszNewProjection,"GEOGCS",6)
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "GCPs must be in lat/long coordinates for MFF2 (hkv).",
                  pszNewProjection );
        
        return CE_Failure;
    }
    
    CPLFree( pszGCPProjection );
    this->pszGCPProjection = CPLStrdup(pszNewProjection);

    return CE_None;
}

/************************************************************************/
/*                           SetProjection()                            */
/*                                                                      */
/*      We provide very limited support for setting the projection.     */
/************************************************************************/

CPLErr HKVDataset::SetProjection( const char * pszNewProjection )

{
    OGRSpatialReference *oSRS;
    HKVSpheroidList *hkvEllipsoids;
    double eq_radius, inv_flattening;
    OGRErr ogrerrorEq=OGRERR_NONE;
    OGRErr ogrerrorInvf=OGRERR_NONE;
    OGRErr ogrerrorOl=OGRERR_NONE;
    char *modifiableProjection = NULL;

    char *spheroid_name = NULL;

    /* This function is used to create a georef file */


    printf( "HKVDataset::SetProjection(%s)\n", pszNewProjection );

    if( !EQUALN(pszNewProjection,"GEOGCS",6)
        && !EQUALN(pszNewProjection,"PROJCS",6)
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to HKV.\n"
                "%s not supported.",
                  pszNewProjection );
        
        return CE_Failure;
    }
    else if (EQUAL(pszNewProjection,""))
    {
      CPLFree( pszProjection );
      pszProjection = (char *) CPLStrdup(pszNewProjection); 

      return CE_None;
    }
    CPLFree( pszProjection );
    pszProjection = (char *) CPLStrdup(pszNewProjection);
   

    /* importFromWkt updates the pointer, so don't use pszNewProjection directly */
    modifiableProjection=CPLStrdup(pszNewProjection);

    oSRS = new OGRSpatialReference;
    oSRS->importFromWkt(&modifiableProjection);

    if ((oSRS->GetAttrValue("PROJECTION") != NULL) && 
        (EQUAL(oSRS->GetAttrValue("PROJECTION"),SRS_PT_TRANSVERSE_MERCATOR)))
    {
      char *ol_txt;
    
        ol_txt=(char *) CPLMalloc(255);
        papszGeoref = CSLSetNameValue( papszGeoref, "projection.name", "utm" );
        sprintf(ol_txt,"%f",oSRS->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0,&ogrerrorOl));
        papszGeoref = CSLSetNameValue( papszGeoref, "projection.origin_longitude",
        ol_txt );
        CPLFree(ol_txt);
    }
    else if ((oSRS->GetAttrValue("PROJECTION") == NULL) && (oSRS->IsGeographic()))
    {
        papszGeoref = CSLSetNameValue( papszGeoref, "projection.name", "LL" );
    }
    else
    {
      CPLError( CE_Warning, CPLE_AppDefined,
                "Unrecognized projection- assuming lat/long.");
        papszGeoref = CSLSetNameValue( papszGeoref, "projection.name", "LL" );
    }
    eq_radius = oSRS->GetSemiMajor(&ogrerrorEq);
    inv_flattening = oSRS->GetInvFlattening(&ogrerrorInvf);
    if ((ogrerrorEq == OGRERR_NONE) && (ogrerrorInvf == OGRERR_NONE)) 
    {
        hkvEllipsoids = new HKVSpheroidList;
        spheroid_name = hkvEllipsoids->GetSpheroidNameByEqRadiusAndInvFlattening(eq_radius,inv_flattening);
        if (spheroid_name != NULL)
        {
            papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                           spheroid_name );
        }
        delete hkvEllipsoids;
    }
    else
    {
      /* default to previous behaviour if spheroid not found by */
      /* radius and inverse flattening */

        if( strstr(pszNewProjection,"Bessel") != NULL )
        {
            papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                       "ev-bessel" );
        }
        else
        {
            papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                       "ev-wgs-84" );
        }                                   
    }
    bGeorefChanged = TRUE;
    delete oSRS;
    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HKVDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *HKVDataset::GetGCPProjection()

{
  return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *HKVDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          ProcessGeorefGCP()                          */
/************************************************************************/

void HKVDataset::ProcessGeorefGCP( char **papszGeoref, const char *pszBase,
                                   double dfRasterX, double dfRasterY )

{
    char      szFieldName[128];
    double    dfLat, dfLong;

/* -------------------------------------------------------------------- */
/*      Fetch the GCP from the string list.                             */
/* -------------------------------------------------------------------- */
    sprintf( szFieldName, "%s.latitude", pszBase );
    if( CSLFetchNameValue(papszGeoref, szFieldName) == NULL )
        return;
    else
        dfLat = atof(CSLFetchNameValue(papszGeoref, szFieldName));

    sprintf( szFieldName, "%s.longitude", pszBase );
    if( CSLFetchNameValue(papszGeoref, szFieldName) == NULL )
        return;
    else
        dfLong = atof(CSLFetchNameValue(papszGeoref, szFieldName));

/* -------------------------------------------------------------------- */
/*      Add the gcp to the internal list.                               */
/* -------------------------------------------------------------------- */
    GDALInitGCPs( 1, pasGCPList + nGCPCount );
            
    CPLFree( pasGCPList[nGCPCount].pszId );

    pasGCPList[nGCPCount].pszId = CPLStrdup( pszBase );
                
    pasGCPList[nGCPCount].dfGCPX = dfLong;
    pasGCPList[nGCPCount].dfGCPY = dfLat;
    pasGCPList[nGCPCount].dfGCPZ = 0.0;

    pasGCPList[nGCPCount].dfGCPPixel = dfRasterX;
    pasGCPList[nGCPCount].dfGCPLine = dfRasterY;

    nGCPCount++;
}

/************************************************************************/
/*                           ProcessGeoref()                            */
/************************************************************************/

void HKVDataset::ProcessGeoref( const char * pszFilename )

{
    int   i;
    HKVSpheroidList *hkvEllipsoids = NULL;

/* -------------------------------------------------------------------- */
/*      Load the georef file, and boil white space away from around     */
/*      the equal sign.                                                 */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszGeoref );
    papszGeoref = CSLLoad( pszFilename );
    if( papszGeoref == NULL )
        return;

    hkvEllipsoids = new HKVSpheroidList;

    for( i = 0; papszGeoref[i] != NULL; i++ )
    {
        int       bAfterEqual = FALSE;
        int       iSrc, iDst;
        char     *pszLine = papszGeoref[i];

        for( iSrc=0, iDst=0; pszLine[iSrc] != '\0'; iSrc++ )
        {
            if( bAfterEqual || pszLine[iSrc] != ' ' )
            {
                pszLine[iDst++] = pszLine[iSrc];
            }

            if( iDst > 0 && pszLine[iDst-1] == '=' )
                bAfterEqual = FALSE;
        }
        pszLine[iDst] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Try to get GCPs.                                                */
/* -------------------------------------------------------------------- */
    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),5);

    ProcessGeorefGCP( papszGeoref, "top_left", 
                      0, 0 );
    ProcessGeorefGCP( papszGeoref, "top_right", 
                      GetRasterXSize()-1, 0 );
    ProcessGeorefGCP( papszGeoref, "bottom_left", 
                      0, GetRasterYSize()-1 );
    ProcessGeorefGCP( papszGeoref, "bottom_right", 
                      GetRasterXSize()-1, GetRasterYSize()-1 );
    ProcessGeorefGCP( papszGeoref, "centre", 
                      (GetRasterXSize()-1)/2.0, (GetRasterYSize()-1)/2.0 );

/* -------------------------------------------------------------------- */
/*      Do we have a recognised projection?                             */
/* -------------------------------------------------------------------- */
    const char *pszProjName, *pszOriginLong, *pszSpheroidName;
    double eq_radius, inv_flattening;

    pszProjName = CSLFetchNameValue(papszGeoref, 
                                    "projection.name");
    pszOriginLong = CSLFetchNameValue(papszGeoref, 
                                      "projection.origin_longitude");
    pszSpheroidName = CSLFetchNameValue(papszGeoref, 
                                      "spheroid.name");


    if ((pszSpheroidName != NULL) && (hkvEllipsoids->SpheroidInList(pszSpheroidName)))
    {
      eq_radius=hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName);
      inv_flattening=hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName);
    }
    else if (pszProjName != NULL)
    {
      CPLError(CE_Warning,CPLE_AppDefined,"Warning- unrecognized ellipsoid.  Using wgs-84 parameters.\n");
      eq_radius=hkvEllipsoids->GetSpheroidEqRadius("wgs-84");
      inv_flattening=hkvEllipsoids->GetSpheroidInverseFlattening("wgs-84");
    }

    if( (pszProjName != NULL) && EQUAL(pszProjName,"utm") && (nGCPCount == 5) )
    {
      /*int nZone = (int)((atof(pszOriginLong)+184.5) / 6.0); */
        int nZone;

        if (pszOriginLong == NULL)
        {
            /* If origin not specified, assume 0.0 */
            CPLError(CE_Warning,CPLE_AppDefined,
                   "Warning- no projection origin longitude specified.  Assuming 0.0.");
            nZone = 31;
        }
        else
            nZone = 31 + (int) floor(atof(pszOriginLong)/6.0);

        OGRSpatialReference oUTM;
        OGRSpatialReference oLL;
        OGRCoordinateTransformation *poTransform = NULL;
        double dfUtmX[5], dfUtmY[5]; 
        int gcp_index;

        GDAL_GCP *utm_gcps;
        int    bSuccess = TRUE;

        if( pasGCPList[4].dfGCPY < 0 )
            oUTM.SetUTM( nZone, 0 );
        else
            oUTM.SetUTM( nZone, 1 );
     
        if (pszOriginLong != NULL)
        {
            oUTM.SetProjParm(SRS_PP_CENTRAL_MERIDIAN,atof(pszOriginLong));
        }
        if ((pszSpheroidName == NULL) || (EQUAL(pszSpheroidName,"wgs-84")))
          {
            oUTM.SetWellKnownGeogCS( "WGS84" );
            oLL.SetWellKnownGeogCS( "WGS84" );
          }
        else
        {
          if (hkvEllipsoids->SpheroidInList(pszSpheroidName))
          { 
            oUTM.SetGeogCS( "unknown","unknown",pszSpheroidName,
                            hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName), 
                            hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName)
                          );
            oLL.SetGeogCS( "unknown","unknown",pszSpheroidName,
                            hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName), 
                            hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName)
                           );
          }
          else
          {
            CPLError(CE_Warning,CPLE_AppDefined,"Warning- unrecognized ellipsoid.  Using wgs-84 parameters.\n");
            oUTM.SetWellKnownGeogCS( "WGS84" );
            oLL.SetWellKnownGeogCS( "WGS84" );
          }
        }  
  
        poTransform = OGRCreateCoordinateTransformation( &oLL, &oUTM );
        if( poTransform == NULL )
            bSuccess = FALSE;

        for(gcp_index=0;gcp_index<5;gcp_index++)
        {
            dfUtmX[gcp_index] = pasGCPList[gcp_index].dfGCPX;
            dfUtmY[gcp_index] = pasGCPList[gcp_index].dfGCPY;

            if( bSuccess && !poTransform->Transform( 1, &(dfUtmX[gcp_index]), &(dfUtmY[gcp_index]) ) )
                bSuccess = FALSE;
 
        }

        if( bSuccess )
        {
            int transform_ok = FALSE;

            utm_gcps = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),5);
            GDALInitGCPs(5,utm_gcps);

            for(gcp_index=0;gcp_index<5;gcp_index++)
            {
                utm_gcps[gcp_index].dfGCPX = dfUtmX[gcp_index];
                utm_gcps[gcp_index].dfGCPY = dfUtmY[gcp_index];
                utm_gcps[gcp_index].dfGCPZ = 0.0;
                utm_gcps[gcp_index].dfGCPPixel = pasGCPList[gcp_index].dfGCPPixel;
                utm_gcps[gcp_index].dfGCPLine = pasGCPList[gcp_index].dfGCPLine;

            }
            transform_ok = GDALGCPsToGeoTransform(5,utm_gcps,adfGeoTransform,0);
        
            /*printf("Geotransform: 0: %f 1: %f 2: %f\n3: %f 4: %f 5: %f\n\n",adfGeoTransform[0],adfGeoTransform[1],adfGeoTransform[2],adfGeoTransform[3],adfGeoTransform[4],adfGeoTransform[5]);*/
            if (transform_ok == FALSE)
            {
              /* transform may not be sufficient in all cases (slant range projection) */
                adfGeoTransform[0] = 0.0;
                adfGeoTransform[1] = 1.0;
                adfGeoTransform[2] = 0.0;
                adfGeoTransform[3] = 0.0;
                adfGeoTransform[4] = 0.0;
                adfGeoTransform[5] = 1.0;
            }
            GDALDeinitGCPs(5,utm_gcps);

            CPLFree( pszProjection );
            CPLFree( pszGCPProjection );
            pszProjection = NULL;
            pszGCPProjection = NULL;
            oUTM.exportToWkt( &pszProjection );
            oLL.exportToWkt( &pszGCPProjection );
            
        }

        if( poTransform != NULL )
            delete poTransform;
    }
    else if ((pszProjName != NULL) && (nGCPCount == 5))
    {
        OGRSpatialReference oLL;
        int transform_ok = FALSE;


        if ((pszSpheroidName == NULL) || (EQUAL(pszSpheroidName,"wgs-84")))
          {
            oLL.SetWellKnownGeogCS( "WGS84" );
          }
        else
        {
          if (hkvEllipsoids->SpheroidInList(pszSpheroidName))
          { 
            oLL.SetGeogCS( "","",pszSpheroidName,
                            hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName), 
                            hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName)
                           );
          }
          else
          {
            CPLError(CE_Warning,CPLE_AppDefined,"Warning- unrecognized ellipsoid.  Using wgs-84 parameters.\n");
            oLL.SetWellKnownGeogCS( "WGS84" );
          }

            transform_ok = GDALGCPsToGeoTransform(5,pasGCPList,adfGeoTransform,0);
            if (transform_ok == FALSE)
            {
                adfGeoTransform[0] = 0.0;
                adfGeoTransform[1] = 1.0;
                adfGeoTransform[2] = 0.0;
                adfGeoTransform[3] = 0.0;
                adfGeoTransform[4] = 0.0;
                adfGeoTransform[5] = 1.0;
            }
            oLL.exportToWkt( &pszGCPProjection );
            oLL.exportToWkt( &pszProjection );
          
        }  
    }

    delete hkvEllipsoids;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HKVDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bNoDataSet = FALSE;
    double      dfNoDataValue = 0.0;
    char        **papszAttrib;
    const char  *pszFilename, *pszValue;
    VSIStatBuf  sStat;
    
/* -------------------------------------------------------------------- */
/*      We assume the dataset is passed as a directory.  Check for      */
/*      an attrib and blob file as a minimum.                           */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bIsDirectory )
        return NULL;
    
    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "image_data", NULL);
    if( VSIStat(pszFilename,&sStat) != 0 )
        pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "blob", NULL );
    if( VSIStat(pszFilename,&sStat) != 0 )
        return NULL;

    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "attrib", NULL );
    if( VSIStat(pszFilename,&sStat) != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Load the attrib file, and boil white space away from around     */
/*      the equal sign.                                                 */
/* -------------------------------------------------------------------- */
    papszAttrib = CSLLoad( pszFilename );
    if( papszAttrib == NULL )
        return NULL;

    for( i = 0; papszAttrib[i] != NULL; i++ )
    {
        int       bAfterEqual = FALSE;
        int       iSrc, iDst;
        char     *pszLine = papszAttrib[i];

        for( iSrc=0, iDst=0; pszLine[iSrc] != '\0'; iSrc++ )
        {
            if( bAfterEqual || pszLine[iSrc] != ' ' )
            {
                pszLine[iDst++] = pszLine[iSrc];
            }

            if( iDst > 0 && pszLine[iDst-1] == '=' )
                bAfterEqual = FALSE;
        }
        pszLine[iDst] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HKVDataset 	*poDS;

    poDS = new HKVDataset();

    poDS->pszPath = CPLStrdup( poOpenInfo->pszFilename );
    poDS->papszAttrib = papszAttrib;
    
/* -------------------------------------------------------------------- */
/*      Set some dataset wide information.                              */
/* -------------------------------------------------------------------- */
    int bNative, bComplex;
    int nRawBands = 0;

    if( CSLFetchNameValue( papszAttrib, "extent.cols" ) == NULL 
        || CSLFetchNameValue( papszAttrib, "extent.rows" ) == NULL )
        return NULL;

    poDS->RasterInitialize( 
        atoi(CSLFetchNameValue(papszAttrib,"extent.cols")),
        atoi(CSLFetchNameValue(papszAttrib,"extent.rows")) );

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.order");
    if( pszValue == NULL )
        bNative = TRUE;
    else
    {
#ifdef CPL_MSB
        bNative = (strstr(pszValue,"*msbf") != NULL);
#else
        bNative = (strstr(pszValue,"*lsbf") != NULL);
#endif
    }

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.no_data");
    if( pszValue != NULL )
    {
        bNoDataSet = TRUE;
        dfNoDataValue = atof(pszValue);
    }

    pszValue = CSLFetchNameValue(papszAttrib,"channel.enumeration");
    if( pszValue != NULL )
        nRawBands = atoi(pszValue);
    else
        nRawBands = 1;

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.field");
    if( pszValue != NULL && strstr(pszValue,"*complex") != NULL )
        bComplex = TRUE;
    else
        bComplex = FALSE;

/* -------------------------------------------------------------------- */
/*      Figure out the datatype                                         */
/* -------------------------------------------------------------------- */
    const char * pszEncoding;
    int          nSize = 1;
    int          nPseudoBands;
    GDALDataType eType;

    pszEncoding = CSLFetchNameValue(papszAttrib,"pixel.encoding");
    if( pszEncoding == NULL )
        pszEncoding = "{ *unsigned }";

    if( CSLFetchNameValue(papszAttrib,"pixel.size") != NULL )
        nSize = atoi(CSLFetchNameValue(papszAttrib,"pixel.size"))/8;
        
    if( bComplex )
        nPseudoBands = 2;
    else 
        nPseudoBands = 1;

    if( nSize == 1 )
        eType = GDT_Byte;
    else if( nSize == 2 && strstr(pszEncoding,"*unsigned") != NULL )
        eType = GDT_UInt16;
    else if( nSize == 4 && bComplex )
        eType = GDT_CInt16;
    else if( nSize == 2 )
        eType = GDT_Int16;
    else if( nSize == 4 && strstr(pszEncoding,"*unsigned") != NULL )
        eType = GDT_UInt32;
    else if( nSize == 8 && strstr(pszEncoding,"*two") != NULL && bComplex )
        eType = GDT_CInt32;
    else if( nSize == 4 && strstr(pszEncoding,"*two") != NULL )
        eType = GDT_Int32;
    else if( nSize == 8 && bComplex )
        eType = GDT_CFloat32;
    else if( nSize == 4 )
        eType = GDT_Float32;
    else if( nSize == 16 && bComplex )
        eType = GDT_CFloat64;
    else if( nSize == 8 )
        eType = GDT_Float64;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported pixel data type in %s.\n"
                  "pixel.size=%d pixel.encoding=%s\n", 
                  poDS->pszPath, nSize, pszEncoding );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the blob file.                                             */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename(poDS->pszPath, "image_data", NULL );
    if( VSIStat(pszFilename,&sStat) != 0 )
        pszFilename = CPLFormFilename(poDS->pszPath, "blob", NULL );
    if( poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fpBlob = VSIFOpenL( pszFilename, "rb" );
        if( poDS->fpBlob == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to open file %s for read access.\n",
                      pszFilename );
            delete poDS;
            return NULL;
        }
    }
    else
    {
        poDS->fpBlob = VSIFOpenL( pszFilename, "rb+" );
        if( poDS->fpBlob == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to open file %s for update access.\n",
                      pszFilename );
            delete poDS;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Build the overview filename, as blob file = "_ovr".             */
/* -------------------------------------------------------------------- */
    char	*pszOvrFilename = (char *) CPLMalloc(strlen(pszFilename)+5);

    sprintf( pszOvrFilename, "%s_ovr", pszFilename );

/* -------------------------------------------------------------------- */
/*      Define the bands.                                               */
/* -------------------------------------------------------------------- */
    int    nPixelOffset, nLineOffset, nOffset;

    nPixelOffset = nRawBands * nSize;
    nLineOffset = nPixelOffset * poDS->GetRasterXSize();
    nOffset = 0;

    for( int iRawBand=0; iRawBand < nRawBands; iRawBand++ )
    {
        HKVRasterBand *poBand;

        poBand = 
            new HKVRasterBand( poDS, poDS->GetRasterCount()+1, poDS->fpBlob,
                               nOffset, nPixelOffset, nLineOffset, 
                               eType, bNative );
        poDS->SetBand( poDS->GetRasterCount()+1, poBand );
        nOffset += GDALGetDataTypeSize( eType ) / 8;

        if( bNoDataSet )
            poBand->StoreNoDataValue( dfNoDataValue );
    }

/* -------------------------------------------------------------------- */
/*      Process the georef file if there is one.                        */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename(poDS->pszPath, "georef", NULL );
    if( VSIStat(pszFilename,&sStat) == 0 )
        poDS->ProcessGeoref(pszFilename);

/* -------------------------------------------------------------------- */
/*      Handle overviews.                                               */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszOvrFilename, TRUE );

    CPLFree( pszOvrFilename );

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *HKVDataset::Create( const char * pszFilenameIn,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte && eType != GDT_Float32 
        && eType != GDT_UInt16 && eType != GDT_Int16 
        && eType != GDT_CInt16 && eType != GDT_CInt32
        && eType != GDT_CFloat32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create HKV file with currently unsupported\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish the name of the directory we will be creating the     */
/*      new HKV directory in.  Verify that this is a directory.         */
/* -------------------------------------------------------------------- */
    char	*pszBaseDir;
    VSIStatBuf  sStat;

    if( strlen(CPLGetPath(pszFilenameIn)) == 0 )
        pszBaseDir = CPLStrdup(".");
    else
        pszBaseDir = CPLStrdup(CPLGetPath(pszFilenameIn));

    if( CPLStat( pszBaseDir, &sStat ) != 0 || !VSI_ISDIR( sStat.st_mode ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create HKV dataset under %s,\n"
                  "but this is not a valid directory.\n", 
                  pszBaseDir);
        CPLFree( pszBaseDir );
        return NULL;
    }

    if( VSIMkdir( pszFilenameIn, 0755 ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create directory %s.\n", 
                  pszFilenameIn );
        return NULL;
    }

    CPLFree( pszBaseDir );

/* -------------------------------------------------------------------- */
/*      Create the header file.                                         */
/* -------------------------------------------------------------------- */
    FILE       *fp;
    const char *pszFilename;

    pszFilename = CPLFormFilename( pszFilenameIn, "attrib", NULL );

    fp = VSIFOpen( pszFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
    }
    
    fprintf( fp, "channel.enumeration = %d\n", nBands );
    fprintf( fp, "channel.interleave = { *pixel tile sequential }\n" );
    fprintf( fp, "extent.cols = %d\n", nXSize );
    fprintf( fp, "extent.rows = %d\n", nYSize );
    
    switch( eType )
    {
      case GDT_Byte:
        fprintf( fp, "pixel.encoding = "
                 "{ *unsigned twos-complement ieee-754 }\n" );
        break;

      case GDT_UInt16:
        fprintf( fp, "pixel.encoding = "
                 "{ *unsigned twos-complement ieee-754 }\n" );
        break;

      case GDT_CInt16:
      case GDT_Int16:
        fprintf( fp, "pixel.encoding = "
                 "{ unsigned *twos-complement ieee-754 }\n" );
        break;

      case GDT_CFloat32:
      case GDT_Float32:
        fprintf( fp, "pixel.encoding = "
                 "{ unsigned twos-complement *ieee-754 }\n" );
        break;

      default:
        CPLAssert( FALSE );
    }

    fprintf( fp, "pixel.size = %d\n", GDALGetDataTypeSize(eType) );
    if( GDALDataTypeIsComplex( eType ) )
        fprintf( fp, "pixel.field = { real *complex }\n" );
    else
        fprintf( fp, "pixel.field = { *real complex }\n" );

#ifdef CPL_MSB     
    fprintf( fp, "pixel.order = { lsbf *msbf }\n" );
#else
    fprintf( fp, "pixel.order = { *lsbf msbf }\n" );
#endif
    VSIFClose( fp );
   
/* -------------------------------------------------------------------- */
/*      Create the blob file.                                           */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename( pszFilenameIn, "image_data", NULL );
    fp = VSIFOpen( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
    }
    
    VSIFWrite( (void*)"", 1, 1, fp );
    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Open the dataset normally.                                      */
/* -------------------------------------------------------------------- */
    return (GDALDataset *) GDALOpen( pszFilenameIn, GA_Update );
}

/************************************************************************/
/*                               Delete()                               */
/*                                                                      */
/*      An HKV Blob dataset consists of a bunch of files in a           */
/*      directory.  Try to delete all the files, then the               */
/*      directory.                                                      */
/************************************************************************/

CPLErr HKVDataset::Delete( const char * pszName )

{
    VSIStatBuf	sStat;
    char        **papszFiles;
    int         i;

    if( CPLStat( pszName, &sStat ) != 0 
        || !VSI_ISDIR(sStat.st_mode) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s does not appear to be an HKV Dataset, as it is not\n"
                  "a path to a directory.", 
                  pszName );
        return CE_Failure;
    }

    papszFiles = CPLReadDir( pszName );
    for( i = 0; i < CSLCount(papszFiles); i++ )
    {
        const char *pszTarget;

        if( EQUAL(papszFiles[i],".") || EQUAL(papszFiles[i],"..") )
            continue;

        pszTarget = CPLFormFilename(pszName, papszFiles[i], NULL );
        if( VSIUnlink(pszTarget) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to delete file %s,\n"
                      "HKVDataset Delete(%s) failed.\n", 
                      pszTarget, 
                      pszName );
            CSLDestroy( papszFiles );
            return CE_Failure;
        }
    }

    CSLDestroy( papszFiles );

    if( VSIRmdir( pszName ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to delete directory %s,\n"
                  "HKVDataset Delete() failed.\n", 
                  pszName );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
HKVDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                        int bStrict, char ** papszOptions, 
                        GDALProgressFunc pfnProgress, void * pProgressData )

{
    HKVDataset	*poDS;
    GDALDataType eType = GDT_Byte;
    int          iBand;
    char szValue[128];

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;
    for( iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        eType = GDALDataTypeUnion( eType, poBand->GetRasterDataType() );
    }    

    poDS = (HKVDataset *) Create( pszFilename, 
                                  poSrcDS->GetRasterXSize(), 
                                  poSrcDS->GetRasterYSize(), 
                                  poSrcDS->GetRasterCount(), 
                                  eType, papszOptions );

/* -------------------------------------------------------------------- */
/*      Copy the image data.                                            */
/* -------------------------------------------------------------------- */
    int         nXSize = poDS->GetRasterXSize();
    int         nYSize = poDS->GetRasterYSize();
    int  	nBlockXSize, nBlockYSize, nBlockTotal, nBlocksDone;

    poDS->GetRasterBand(1)->GetBlockSize( &nBlockXSize, &nBlockYSize );

    nBlockTotal = ((nXSize + nBlockXSize - 1) / nBlockXSize)
        * ((nYSize + nBlockYSize - 1) / nBlockYSize)
        * poSrcDS->GetRasterCount();

    nBlocksDone = 0;
    for( iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDS->GetRasterBand( iBand+1 );
        int	       iYOffset, iXOffset;
        void           *pData;
        CPLErr  eErr;


        pData = CPLMalloc(nBlockXSize * nBlockYSize
                          * GDALGetDataTypeSize(eType) / 8);

        for( iYOffset = 0; iYOffset < nYSize; iYOffset += nBlockYSize )
        {
            for( iXOffset = 0; iXOffset < nXSize; iXOffset += nBlockXSize )
            {
                int	nTBXSize, nTBYSize;

                if( !pfnProgress( (nBlocksDone++) / (float) nBlockTotal,
                                  NULL, pProgressData ) )
                {
                    CPLError( CE_Failure, CPLE_UserInterrupt, 
                              "User terminated" );
                    delete poDS;

                    GDALDriver *poHKVDriver = 
                        (GDALDriver *) GDALGetDriverByName( "MFF2" );
                    poHKVDriver->Delete( pszFilename );
                    return NULL;
                }

                nTBXSize = MIN(nBlockXSize,nXSize-iXOffset);
                nTBYSize = MIN(nBlockYSize,nYSize-iYOffset);

                eErr = poSrcBand->RasterIO( GF_Read, 
                                            iXOffset, iYOffset, 
                                            nTBXSize, nTBYSize,
                                            pData, nTBXSize, nTBYSize,
                                            eType, 0, 0 );
                if( eErr != CE_None )
                {
                    return NULL;
                }
            
                eErr = poDstBand->RasterIO( GF_Write, 
                                            iXOffset, iYOffset, 
                                            nTBXSize, nTBYSize,
                                            pData, nTBXSize, nTBYSize,
                                            eType, 0, 0 );

                if( eErr != CE_None )
                {
                    return NULL;
                }
            }
        }

        CPLFree( pData );
    }
/* -------------------------------------------------------------------- */
/*      Copy georeferencing information, if enough is available.        */
/* -------------------------------------------------------------------- */

    int georef_created = FALSE;
    /* Try to locate the corner and center GCP's- want to copy MFF */
    /* GCPs directly if present.  Check that the coordinates are   */
    /* in lat/long first though (GEOGCS as opposed to PROJCS at    */ 
    /* the start of the projection string).                        */

    if (( poSrcDS->GetGCPCount() > 4 ) && 
        ( poSrcDS->GetGCPProjection() != NULL ) &&
        ( EQUALN(poSrcDS->GetGCPProjection(),"GEOGCS",6)))
    {
        const GDAL_GCP *pasGCPs = poSrcDS->GetGCPs();
        double	*padfTiepoints, gcppix, gcpline;
        char foundinfo[10]="00000\n";


        /* MFF2 requires corner and center gcps */
        padfTiepoints = (double *) 
            CPLMalloc(2*sizeof(double)*5);

        for( int iGCP = 0; iGCP < poSrcDS->GetGCPCount(); iGCP++ )
        {
          gcppix=pasGCPs[iGCP].dfGCPPixel;
          gcpline=pasGCPs[iGCP].dfGCPLine;
          
          if ((gcppix == 0.0) && (gcpline == 0.0))
          {
            padfTiepoints[0]=pasGCPs[iGCP].dfGCPX;
            padfTiepoints[1]=pasGCPs[iGCP].dfGCPY;
            foundinfo[0]='1';
          } 
          else if ((gcppix == poSrcDS->GetRasterXSize()-1) && (gcpline == 0.0))
          {
            padfTiepoints[2]=pasGCPs[iGCP].dfGCPX;
            padfTiepoints[3]=pasGCPs[iGCP].dfGCPY;
            foundinfo[1]='1';
          } 
          else if ((gcppix == 0.0) && (gcpline == poSrcDS->GetRasterYSize()-1))
          {
            padfTiepoints[4]=pasGCPs[iGCP].dfGCPX;
            padfTiepoints[5]=pasGCPs[iGCP].dfGCPY;
            foundinfo[2]='1';
          } 
          else if ((gcppix == poSrcDS->GetRasterXSize()-1) && (gcpline == poSrcDS->GetRasterYSize()-1))
          {
            padfTiepoints[6]=pasGCPs[iGCP].dfGCPX;
            padfTiepoints[7]=pasGCPs[iGCP].dfGCPY;
            foundinfo[3]='1';
          } 
          else if ((gcppix == (poSrcDS->GetRasterXSize()-1)/2.0) && (gcpline == (poSrcDS->GetRasterYSize()-1)/2.0))
          {
            padfTiepoints[8]=pasGCPs[iGCP].dfGCPX;
            padfTiepoints[9]=pasGCPs[iGCP].dfGCPY;
            foundinfo[4]='1';
          } 
       }
       if EQUAL(foundinfo,"11111\n")
       {
          /* -------------------------------------------------------------------- */
          /*      top left                                                        */
          /* -------------------------------------------------------------------- */
              sprintf( szValue, "%.10f", padfTiepoints[1] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "top_left.latitude", 
                                             szValue );

              sprintf( szValue, "%.10f", padfTiepoints[0] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "top_left.longitude", 
                                   szValue );
             
          /* -------------------------------------------------------------------- */
          /*      top_right                                                       */
          /* -------------------------------------------------------------------- */
              sprintf( szValue, "%.10f", padfTiepoints[3] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "top_right.latitude", 
                                   szValue );

              sprintf( szValue, "%.10f", padfTiepoints[2] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "top_right.longitude", 
                                   szValue );

          /* -------------------------------------------------------------------- */
          /*      bottom_left                                                     */
          /* -------------------------------------------------------------------- */
              sprintf( szValue, "%.10f", padfTiepoints[5] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "bottom_left.latitude", 
                                             szValue );

              sprintf( szValue, "%.10f", padfTiepoints[4] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "bottom_left.longitude", 
                                             szValue );

          /* -------------------------------------------------------------------- */
          /*      bottom_right                                                    */
          /* -------------------------------------------------------------------- */
              sprintf( szValue, "%.10f", padfTiepoints[7] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "bottom_right.latitude", 
                                             szValue );

              sprintf( szValue, "%.10f", padfTiepoints[6] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "bottom_right.longitude", 
                                             szValue );

          /* -------------------------------------------------------------------- */
          /*      Center                                                          */
          /* -------------------------------------------------------------------- */
              sprintf( szValue, "%.10f", padfTiepoints[9] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "centre.latitude", 
                                             szValue );

              sprintf( szValue, "%.10f", padfTiepoints[8] );
              poDS->papszGeoref = CSLSetNameValue( poDS->papszGeoref, "centre.longitude", 
                                             szValue );
         
              poDS->SetProjection(CPLStrdup(poSrcDS->GetProjectionRef()));

              /* georef file will be saved automatically when dataset is deleted */
              /* because SetProjection sets a flag to indicate it's necessary.   */

              georef_created = TRUE;
       }
       CPLFree( padfTiepoints );
    }

    if (georef_created != TRUE)
    {
      double *tempGeoTransform=NULL; 

      tempGeoTransform = (double *) CPLMalloc(6*sizeof(double));

      if (( poSrcDS->GetGeoTransform( tempGeoTransform ) == CE_None)
          && (tempGeoTransform[0] != 0.0 || tempGeoTransform[1] != 1.0
          || tempGeoTransform[2] != 0.0 || tempGeoTransform[3] != 0.0
              || tempGeoTransform[4] != 0.0 || ABS(tempGeoTransform[5]) != 1.0 ))
      {
          OGRSpatialReference oUTMorLL;
          char *srcProjection=NULL;
          char *newGCPProjection=NULL;

          srcProjection = CPLStrdup(poSrcDS->GetProjectionRef());
          oUTMorLL.importFromWkt(&srcProjection);
          (oUTMorLL.GetAttrNode("GEOGCS"))->exportToWkt(&newGCPProjection);

          poDS->SetGCPProjection(newGCPProjection);
          poDS->SetProjection(poSrcDS->GetProjectionRef());
          poDS->SetGeoTransform(tempGeoTransform);

          CPLFree(tempGeoTransform);

          /* georef file will be saved automatically when dataset is deleted */
          /* because SetProjection sets a flag to indicate it's necessary.   */

  
          georef_created = TRUE;
      }
      else
      {
        CPLFree(tempGeoTransform);
      }
    }    
   
    if( !pfnProgress( 1.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, 
                  "User terminated" );
        delete poDS;

        GDALDriver *poHKVDriver = 
            (GDALDriver *) GDALGetDriverByName( "MFF2" );
        poHKVDriver->Delete( pszFilename );
        return NULL;
    }

    return poDS;
}


/************************************************************************/
/*                         GDALRegister_HKV()                          */
/************************************************************************/

void GDALRegister_HKV()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "MFF2" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "MFF2" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Atlantis MFF2 (HKV) Raster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#MFF2" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 CInt16 CInt32 Float32 Float64 CFloat32 CFloat64" );
        
        poDriver->pfnOpen = HKVDataset::Open;
        poDriver->pfnCreate = HKVDataset::Create;
        poDriver->pfnDelete = HKVDataset::Delete;
        poDriver->pfnCreateCopy = HKVDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
