/******************************************************************************
 * $Id$
 *
 * Name:     hfadataset.cpp
 * Project:  Erdas Imagine Driver
 * Purpose:  Main driver for Erdas Imagine format.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.12  2000/10/13 20:58:37  warmerda
 * added to projections list
 *
 * Revision 1.11  2000/10/13 18:09:40  warmerda
 * fixed degree/radian translation
 *
 * Revision 1.10  2000/10/12 19:30:31  warmerda
 * substantially improved write support
 *
 * Revision 1.9  2000/09/29 21:42:38  warmerda
 * preliminary write support implemented
 *
 * Revision 1.8  2000/09/01 19:39:48  warmerda
 * added support for f64, and returning geotransform
 *
 * Revision 1.7  2000/08/25 21:31:34  warmerda
 * added overview support
 *
 * Revision 1.6  2000/08/18 16:24:06  warmerda
 * Added color table support
 *
 * Revision 1.5  2000/08/15 19:28:26  warmerda
 * added help topic
 *
 * Revision 1.4  2000/02/28 16:32:20  warmerda
 * use SetBand method
 *
 * Revision 1.3  1999/01/27 18:48:12  warmerda
 * Declare constructor and destructor for HFADataset.
 *
 * Revision 1.2  1999/01/27 18:32:46  warmerda
 * compiles OK
 *
 * Revision 1.1  1999/01/22 17:40:43  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "hfa.h"
#include "ogr_spatialref.h"

CPL_C_START
void	GDALRegister_HFA(void);
CPL_C_END

#ifndef PI
#  define PI 3.14159265358979323846
#endif

#ifndef R2D
#  define R2D	(180/PI)
#endif
#ifndef D2R
#  define D2R	(PI/180)
#endif

static const char *apszDatumMap[] = {
    /* Imagine name, WKT name */
    "NAD27", "North_American_Datum_1927",
    "NAD83", "North_American_Datum_1983",
    "WGS84", "WGS_1984",
    NULL, NULL 
};


/************************************************************************/
/* ==================================================================== */
/*				HFADataset				*/
/* ==================================================================== */
/************************************************************************/

class HFARasterBand;

class CPL_DLL HFADataset : public GDALDataset
{
    friend	HFARasterBand;
    
    HFAHandle	hHFA;

    int         bGeoDirty;
    double      adfGeoTransform[6];
    char	*pszProjection;

    CPLErr      ReadProjection();
    CPLErr      WriteProjection();

  public:
                HFADataset();
                ~HFADataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    static GDALDataset *CreateCopy( const char * pszFilename, 
                                    GDALDataset *poSrcDS, 
                                    int bStrict, char ** papszOptions, 
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData );


    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual void   FlushCache( void );
};

/************************************************************************/
/* ==================================================================== */
/*                            HFARasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HFARasterBand : public GDALRasterBand
{
    friend	HFADataset;

    GDALColorTable *poCT;

  public:

                   HFARasterBand( HFADataset *, int );
    virtual        ~HFARasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};

static GDALDriver	*poHFADriver = NULL;

/************************************************************************/
/*                           HFARasterBand()                            */
/************************************************************************/

HFARasterBand::HFARasterBand( HFADataset *poDS, int nBand )

{
    int		nHFADataType;
    
    this->poDS = poDS;
    this->nBand = nBand;
    this->poCT = NULL;

    HFAGetBandInfo( poDS->hHFA, nBand, &nHFADataType,
                    &nBlockXSize, &nBlockYSize );

    switch( nHFADataType )
    {
      case EPT_u8:
      case EPT_s8:
        eDataType = GDT_Byte;
        break;

      case EPT_u16:
        eDataType = GDT_UInt16;
        break;

      case EPT_s16:
        eDataType = GDT_Int16;
        break;

      case EPT_f32:
        eDataType = GDT_Float32;
        break;

      case EPT_f64:
        eDataType = GDT_Float64;
        break;

      default:
        eDataType = GDT_Byte;
        /* notdef: this should really report an error, but this isn't
           so easy from within constructors. */
        break;
    }

/* -------------------------------------------------------------------- */
/*      Collect color table if present.                                 */
/* -------------------------------------------------------------------- */
    double    *padfRed, *padfGreen, *padfBlue;
    int       nColors;

    if( HFAGetPCT( poDS->hHFA, nBand, &nColors, 
                   &padfRed, &padfGreen, &padfBlue ) == CE_None
        && nColors > 0 )
    {
        poCT = new GDALColorTable();
        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            GDALColorEntry   sEntry;

            sEntry.c1 = (int) (padfRed[iColor]   * 255);
            sEntry.c2 = (int) (padfGreen[iColor] * 255);
            sEntry.c3 = (int) (padfBlue[iColor]  * 255);
            sEntry.c4 = 255;
            poCT->SetColorEntry( iColor, &sEntry );
        }
    }
}

/************************************************************************/
/*                           ~HFARasterBand()                           */
/************************************************************************/

HFARasterBand::~HFARasterBand()

{
    FlushCache();
    if( poCT != NULL )
        delete poCT;
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HFARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    HFADataset	*poODS = (HFADataset *) poDS;

    return( HFAGetRasterBlock( poODS->hHFA, nBand, nBlockXOff, nBlockYOff,
                               pImage ) );
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr HFARasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    HFADataset	*poODS = (HFADataset *) poDS;

    return( HFASetRasterBlock( poODS->hHFA, nBand, nBlockXOff, nBlockYOff,
                               pImage ) );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp HFARasterBand::GetColorInterpretation()

{
    if( poCT != NULL )
        return GCI_PaletteIndex;
    else
        return GCI_Undefined;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *HFARasterBand::GetColorTable()

{
    return poCT;
}

/************************************************************************/
/* ==================================================================== */
/*                            HFADataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            HFADataset()                            */
/************************************************************************/

HFADataset::HFADataset()

{
    hHFA = NULL;
    bGeoDirty = FALSE;
    pszProjection = NULL;
}

/************************************************************************/
/*                           ~HFADataset()                            */
/************************************************************************/

HFADataset::~HFADataset()

{
    FlushCache();

    if( hHFA != NULL )
        HFAClose( hHFA );
    
    CPLFree( pszProjection );
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void HFADataset::FlushCache()

{
    GDALDataset::FlushCache();

/* -------------------------------------------------------------------- */
/*      If necessary write various georef structures.                   */
/* -------------------------------------------------------------------- */
    if( bGeoDirty )
    {
/* -------------------------------------------------------------------- */
/*      Set the projection.                                             */
/* -------------------------------------------------------------------- */
        WriteProjection();

    }
}

/************************************************************************/
/*                          WriteProjection()                           */
/************************************************************************/

CPLErr HFADataset::WriteProjection()

{
    Eprj_Datum	        sDatum;
    Eprj_ProParameters  sPro;
    Eprj_MapInfo	sMapInfo;
    OGRSpatialReference	oSRS;
    OGRSpatialReference *poGeogSRS = NULL;
    int                 bHaveSRS;
    char		*pszP = pszProjection;

    bGeoDirty = FALSE;

    if( pszProjection != NULL && strlen(pszProjection) > 0 
        && oSRS.importFromWkt( &pszP ) == OGRERR_NONE )
        bHaveSRS = TRUE;
    else
        bHaveSRS = FALSE;

/* -------------------------------------------------------------------- */
/*      Initialize projection and datum.                                */
/* -------------------------------------------------------------------- */
    memset( &sPro, 0, sizeof(sPro) );
    memset( &sDatum, 0, sizeof(sDatum) );
    memset( &sMapInfo, 0, sizeof(sMapInfo) );

/* -------------------------------------------------------------------- */
/*      Collect datum information.                                      */
/* -------------------------------------------------------------------- */
    if( bHaveSRS )
    {
        poGeogSRS = oSRS.CloneGeogCS();
    }

    if( poGeogSRS )
    {
        int	i;

        sDatum.datumname = (char *) poGeogSRS->GetAttrValue( "GEOGCS|DATUM" );
        
        /* WKT to Imagine translation */
        for( i = 0; apszDatumMap[i] != NULL; i += 2 )
        {
            if( EQUAL(sDatum.datumname,apszDatumMap[i+1]) )
            {
                sDatum.datumname = (char *) apszDatumMap[i];
                break;
            }
        }
    
        if( poGeogSRS->GetTOWGS84( sDatum.params ) == OGRERR_NONE )
            sDatum.type = EPRJ_DATUM_PARAMETRIC;
        else if( EQUAL(sDatum.datumname,"NAD27") )
        {
            sDatum.type = EPRJ_DATUM_GRID;
            sDatum.gridname = "nadcon.dat";
        }
        else
        {
            /* we will default to this (effectively WGS84) for now */
            sDatum.type = EPRJ_DATUM_PARAMETRIC;
        }

        sPro.proSpheroid.sphereName = (char *) 
            poGeogSRS->GetAttrValue( "GEOGCS|DATUM|SPHEROID" );
        sPro.proSpheroid.a = poGeogSRS->GetSemiMajor();
        sPro.proSpheroid.b = poGeogSRS->GetSemiMinor();
        sPro.proSpheroid.radius = sPro.proSpheroid.a;

        double a2 = sPro.proSpheroid.a*sPro.proSpheroid.a;
        double b2 = sPro.proSpheroid.b*sPro.proSpheroid.b;

        sPro.proSpheroid.eSquared = (a2-b2)/a2;
    }

/* -------------------------------------------------------------------- */
/*      Recognise various projections.                                  */
/* -------------------------------------------------------------------- */
    const char * pszProjName = NULL;

    if( bHaveSRS )
        pszProjName = oSRS.GetAttrValue( "PROJCS|PROJECTION" );

    if( pszProjName == NULL )
    {
        if( bHaveSRS && oSRS.IsGeographic() )
        {
            sPro.proNumber = EPRJ_LATLONG;
            sPro.proName = "Geographic(Latitude/Longitude)";
        }
    }

    /* FIXME/NOTDEF/TODO: Add State Plane */
    else if( oSRS.GetUTMZone( NULL ) != 0 )
    {
        int	bNorth, nZone;

        nZone = oSRS.GetUTMZone( &bNorth );
        sPro.proNumber = EPRJ_UTM;
        sPro.proName = "UTM";
        sPro.proZone = nZone;
        if( bNorth )
            sPro.proParams[3] = 1.0;
        else
            sPro.proParams[3] = -1.0;
    }

    else if( EQUAL(pszProjName,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        sPro.proNumber = EPRJ_ALBERS_CONIC_EQUAL_AREA;
        sPro.proName = "Albers Conic Equal Area";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_2)*D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        sPro.proNumber = EPRJ_LAMBERT_CONFORMAL_CONIC;
        sPro.proName = "Lambert Conformal Conic";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_2)*D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_MERCATOR_1SP) )
    {
        sPro.proNumber = EPRJ_MERCATOR;
        sPro.proName = "Mercator";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        /* hopefully the scale factor is 1.0! */
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_POLAR_STEREOGRAPHIC;
        sPro.proName = "Polar Stereographic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        /* hopefully the scale factor is 1.0! */
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_POLYCONIC) )
    {
        sPro.proNumber = EPRJ_POLYCONIC;
        sPro.proName = "Polyconic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_EQUIDISTANT_CONIC) )
    {
        sPro.proNumber = EPRJ_EQUIDISTANT_CONIC;
        sPro.proName = "Equidistant Conic";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_2)*D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] = 1.0;
    }
    else if( EQUAL(pszProjName,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        sPro.proNumber = EPRJ_TRANSVERSE_MERCATOR;
        sPro.proName = "Transverse Mercator";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_STEREOGRAPHIC_EXTENDED;
        sPro.proName = "Stereographic (Extended)";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        sPro.proNumber = EPRJ_LAMBERT_AZIMUTHAL_EQUAL_AREA;
        sPro.proName = "Lambert Azitmuthal Equal Area";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        sPro.proNumber = EPRJ_AZIMUTHAL_EQUIDISTANT;
        sPro.proName = "Azitmuthal Equidistant";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_GNOMONIC) )
    {
        sPro.proNumber = EPRJ_GNOMONIC;
        sPro.proName = "Gnomonic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ORTHOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_ORTHOGRAPHIC;
        sPro.proName = "Orthographic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_SINUSOIDAL) )
    {
        sPro.proNumber = EPRJ_SINUSOIDAL;
        sPro.proName = "Sinusoidal";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_EQUIRECTANGULAR) )
    {
        sPro.proNumber = EPRJ_EQUIRECTANGULAR;
        sPro.proName = "Equirectangular";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_MILLER_CYLINDRICAL) )
    {
        sPro.proNumber = EPRJ_MILLER_CYLINDRICAL;
        sPro.proName = "Miller Cylindrical";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        /* hopefully the latitude is zero! */
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_VANDERGRINTEN) )
    {
        sPro.proNumber = EPRJ_VANDERGRINTEN;
        sPro.proName = "VanDerGrinten";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR;
        sPro.proName = "Hotine Oblique Mercator";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH)*D2R;
        /* hopefully the rectified grid angle is zero */
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[12] = 1.0;
    }
    else if( EQUAL(pszProjName,SRS_PT_ROBINSON) )
    {
        sPro.proNumber = EPRJ_ROBINSON;
        sPro.proName = "Robinson";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_MOLLWEIDE) )
    {
        sPro.proNumber = EPRJ_MOLLWEIDE;
        sPro.proName = "Mollweide";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ECKERT_IV) )
    {
        sPro.proNumber = EPRJ_ECKERT_IV;
        sPro.proName = "Eckert IV";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ECKERT_VI) )
    {
        sPro.proNumber = EPRJ_ECKERT_VI;
        sPro.proName = "Eckert VI";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_GALL_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_GALL_STEREOGRAPHIC;
        sPro.proName = "Gall Stereographic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_CASSINI_SOLDNER) )
    {
        sPro.proNumber = EPRJ_CASSINI;
        sPro.proName = "Cassini";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else
    {
    }

/* -------------------------------------------------------------------- */
/*      MapInfo                                                         */
/* -------------------------------------------------------------------- */

    if( bHaveSRS && sPro.proName != NULL )
        sMapInfo.proName = sPro.proName;
    else
        sMapInfo.proName = "Unknown";

    sMapInfo.upperLeftCenter.x = 
        adfGeoTransform[0] + adfGeoTransform[1]*0.5;
    sMapInfo.upperLeftCenter.y = 
        adfGeoTransform[3] + adfGeoTransform[5]*0.5;

    sMapInfo.lowerRightCenter.x = 
        adfGeoTransform[0] + adfGeoTransform[1] * (GetRasterXSize()-0.5);
    sMapInfo.lowerRightCenter.y = 
        adfGeoTransform[3] + adfGeoTransform[5] * (GetRasterYSize()-0.5);

    sMapInfo.pixelSize.width = ABS(adfGeoTransform[1]);
    sMapInfo.pixelSize.height = ABS(adfGeoTransform[5]);

    sMapInfo.units = "meters";

    if( bHaveSRS )
    {
        if( oSRS.IsGeographic() )
            sMapInfo.units = "dd";
        else if( oSRS.GetLinearUnits() == 1.0 )
            sMapInfo.units = "meters";
        else
            sMapInfo.units = "feet";
    }

/* -------------------------------------------------------------------- */
/*      Write out definitions.                                          */
/* -------------------------------------------------------------------- */
    HFASetMapInfo( hHFA, &sMapInfo );

    if( bHaveSRS && sPro.proName != NULL )
    {
        HFASetProParameters( hHFA, &sPro );
        HFASetDatum( hHFA, &sDatum );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( poGeogSRS != NULL )
        delete poGeogSRS;

    return CE_None;
}

/************************************************************************/
/*                           ReadProjection()                           */
/************************************************************************/

CPLErr HFADataset::ReadProjection()

{
    const Eprj_Datum	      *psDatum;
    const Eprj_ProParameters  *psPro;
    const Eprj_MapInfo        *psMapInfo;
    OGRSpatialReference        oSRS;

    psDatum = HFAGetDatum( hHFA );
    psPro = HFAGetProParameters( hHFA );
    psMapInfo = HFAGetMapInfo( hHFA );

    if( psPro == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Handle different projection methods.                            */
/* -------------------------------------------------------------------- */
    switch( psPro->proNumber )
    {
      case EPRJ_LATLONG:
        break;
       
      case EPRJ_UTM:
        oSRS.SetUTM( psPro->proZone, psPro->proParams[3] >= 0.0 );
        break;

      case EPRJ_STATE_PLANE:
        /* We don't currently handle state plane */
        break;

      case EPRJ_ALBERS_CONIC_EQUAL_AREA:
        oSRS.SetACEA( psPro->proParams[2]*R2D, psPro->proParams[3]*R2D, 
                      psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                      psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_LAMBERT_CONFORMAL_CONIC:
        oSRS.SetLCC( psPro->proParams[2]*R2D, psPro->proParams[3]*R2D, 
                     psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                     psPro->proParams[6], psPro->proParams[7] );
        break;
        
      case EPRJ_MERCATOR:
        oSRS.SetMercator( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                          1.0,
                          psPro->proParams[6], psPro->proParams[7] );
        break;
        
      case EPRJ_POLAR_STEREOGRAPHIC:
        oSRS.SetPS( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                    1.0,
                    psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_POLYCONIC:
        oSRS.SetPolyconic( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                           psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_EQUIDISTANT_CONIC:
        double		dfStdParallel2;

        if( psPro->proParams[8] != 0.0 )
            dfStdParallel2 = psPro->proParams[3]*R2D;
        else
            dfStdParallel2 = psPro->proParams[2]*R2D;
        oSRS.SetEC( psPro->proParams[2]*R2D, dfStdParallel2,
                    psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                    psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_TRANSVERSE_MERCATOR:
        oSRS.SetTM( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                    psPro->proParams[2],
                    psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_STEREOGRAPHIC:
        oSRS.SetStereographic( psPro->proParams[5]*R2D,psPro->proParams[4]*R2D,
                               1.0,
                               psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_LAMBERT_AZIMUTHAL_EQUAL_AREA:
        oSRS.SetLAEA( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                      psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_AZIMUTHAL_EQUIDISTANT:
        oSRS.SetAE( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                    psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_GNOMONIC:
        oSRS.SetGnomonic( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                          psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ORTHOGRAPHIC:
        oSRS.SetOrthographic( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D,
                              psPro->proParams[6], psPro->proParams[7] );
        break;
        
      case EPRJ_SINUSOIDAL:
        oSRS.SetSinusoidal( psPro->proParams[4]*R2D, 
                            psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_EQUIRECTANGULAR:
        oSRS.SetEquirectangular( 
            psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
            psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_MILLER_CYLINDRICAL:
        oSRS.SetMC( 0.0, psPro->proParams[4]*R2D, 
                    psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_VANDERGRINTEN:
        oSRS.SetVDG( psPro->proParams[4]*R2D, 
                     psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_HOTINE_OBLIQUE_MERCATOR:
        if( psPro->proParams[12] > 0.0 )
            oSRS.SetHOM( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                         psPro->proParams[3]*R2D, 0.0, 
                         psPro->proParams[2], 
                         psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ROBINSON:
        oSRS.SetRobinson( psPro->proParams[4]*R2D, 
                          psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_MOLLWEIDE:
        oSRS.SetMollweide( psPro->proParams[4]*R2D, 
                           psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ECKERT_IV:
        oSRS.SetEckertIV( psPro->proParams[4]*R2D, 
                          psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ECKERT_VI:
        oSRS.SetEckertVI( psPro->proParams[4]*R2D, 
                          psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_GALL_STEREOGRAPHIC:
        oSRS.SetGS( psPro->proParams[4]*R2D, 
                    psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_CASSINI:
        oSRS.SetCS( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D, 
                    psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_STEREOGRAPHIC_EXTENDED:
        oSRS.SetStereographic( psPro->proParams[5]*R2D,psPro->proParams[4]*R2D,
                               psPro->proParams[2],
                               psPro->proParams[6], psPro->proParams[7] );
        break;


      default:
        /* we do nothing for unsupported projections for now */
        break;
    }

    if( oSRS.IsProjected() )
    {
        oSRS.SetProjCS( psPro->proName );

        if( psMapInfo && EQUAL(psMapInfo->units,"feet") )
        {
            oSRS.SetLinearUnits( SRS_UL_US_FOOT, 
                                 atof(SRS_UL_US_FOOT_CONV) );
        }
        else
        {
            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try and set the GeogCS information.                             */
/* -------------------------------------------------------------------- */
    const char *pszDatumName = psPro->proSpheroid.sphereName;
    const char *pszEllipsoidName = psPro->proSpheroid.sphereName;
    double	dfInvFlattening;
    
    if( psDatum != NULL )
    {
        int	i;

        pszDatumName = psDatum->datumname;

        /* Imagine to WKT translation */
        for( i = 0; apszDatumMap[i] != NULL; i += 2 )
        {
            if( EQUAL(pszDatumName,apszDatumMap[i]) )
            {
                pszDatumName = apszDatumMap[i+1];
                break;
            }
        }
    }

    dfInvFlattening = 1.0/(1.0 - psPro->proSpheroid.b/psPro->proSpheroid.a);

    oSRS.SetGeogCS( pszDatumName, pszDatumName, pszEllipsoidName, 
                    psPro->proSpheroid.a, dfInvFlattening );

    if( psDatum != NULL && psDatum->type == EPRJ_DATUM_PARAMETRIC )
    {
        oSRS.SetTOWGS84( psDatum->params[0], 
                         psDatum->params[1], 
                         psDatum->params[2], 
                         psDatum->params[3], 
                         psDatum->params[4], 
                         psDatum->params[5], 
                         psDatum->params[6] );
    }

/* -------------------------------------------------------------------- */
/*      Get the WKT representation of the coordinate system.            */
/* -------------------------------------------------------------------- */
    CPLFree( pszProjection );
    pszProjection = NULL;
    
    if( oSRS.exportToWkt( &pszProjection ) == OGRERR_NONE )
        return CE_None;
    else
    {
        pszProjection = NULL;
        return CE_Failure;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HFADataset::Open( GDALOpenInfo * poOpenInfo )

{
    HFAHandle	hHFA;
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Verify that this is a HFA file.                                 */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bStatOK || poOpenInfo->nHeaderBytes < 15
        || !EQUALN((char *) poOpenInfo->pabyHeader,"EHFA_HEADER_TAG",15) )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
        hHFA = HFAOpen( poOpenInfo->pszFilename, "r+" );
    else
        hHFA = HFAOpen( poOpenInfo->pszFilename, "r" );

    if( hHFA == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HFADataset 	*poDS;

    poDS = new HFADataset();

    poDS->hHFA = hHFA;
    poDS->poDriver = poHFADriver;

/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    HFAGetRasterInfo( hHFA, &poDS->nRasterXSize, &poDS->nRasterYSize,
                      &poDS->nBands );

/* -------------------------------------------------------------------- */
/*      Get geotransform.                                               */
/* -------------------------------------------------------------------- */
    const Eprj_MapInfo  *psMapinfo = HFAGetMapInfo( hHFA );
    
    if( psMapinfo == NULL )
    {
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = 1.0;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 1.0;
    }
    else
    {
        poDS->adfGeoTransform[0] = psMapinfo->upperLeftCenter.x 
            - psMapinfo->pixelSize.width*0.5;
        poDS->adfGeoTransform[1] = psMapinfo->pixelSize.width;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = psMapinfo->upperLeftCenter.y
            + psMapinfo->pixelSize.height*0.5;
        poDS->adfGeoTransform[4] = 0.0;

        if( psMapinfo->upperLeftCenter.y > psMapinfo->lowerRightCenter.y )
            poDS->adfGeoTransform[5] = - psMapinfo->pixelSize.height;
        else
            poDS->adfGeoTransform[5] = psMapinfo->pixelSize.height;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the projection.                                             */
/* -------------------------------------------------------------------- */
    poDS->ReadProjection();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poDS->nBands; i++ )
        poDS->SetBand( i+1, new HFARasterBand( poDS, i+1 ) );

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HFADataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr HFADataset::SetProjection( const char * pszNewProjection )

{
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );
    bGeoDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HFADataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    return CE_None;
}

/************************************************************************/
/*                          GetSeoTransform()                           */
/************************************************************************/

CPLErr HFADataset::SetGeoTransform( double * padfTransform )

{
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
    bGeoDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *HFADataset::Create( const char * pszFilenameIn,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** papszParmList )

{
    int		nHfaDataType;

/* -------------------------------------------------------------------- */
/*      Translate the data type.                                        */
/* -------------------------------------------------------------------- */
    switch( eType )
    {
      case GDT_Byte:
        nHfaDataType = EPT_u8;
        break;
        
      case GDT_UInt16:
        nHfaDataType = EPT_u16;
        break;
        
      case GDT_Int16:
        nHfaDataType = EPT_s16;
        break;
        
      case GDT_Int32:
        nHfaDataType = EPT_s32;
        break;
        
      case GDT_UInt32:
        nHfaDataType = EPT_u32;
        break;
        
      case GDT_Float32:
        nHfaDataType = EPT_f32;
        break;

      case GDT_Float64:
        nHfaDataType = EPT_f64;
        break;
        
      case GDT_CFloat32:
        nHfaDataType = EPT_c64;
        break;
        
      case GDT_CFloat64:
        nHfaDataType = EPT_c128;
        break;

      default:
        CPLError( CE_Failure, CPLE_NotSupported, 
                 "Data type %s not supported by Erdas Imagine (HFA) format.\n",
                  GDALGetDataTypeName( eType ) );
        return NULL;
        
    }

/* -------------------------------------------------------------------- */
/*      Create the new file.                                            */
/* -------------------------------------------------------------------- */
    HFAHandle hHFA;

    hHFA = HFACreate( pszFilenameIn, nXSize, nYSize, nBands, 
                      nHfaDataType, papszParmList );
    if( hHFA == NULL )
        return NULL;

    HFAClose( hHFA );

/* -------------------------------------------------------------------- */
/*      Open the dataset normally.                                      */
/* -------------------------------------------------------------------- */
    return (GDALDataset *) GDALOpen( pszFilenameIn, GA_Update );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
HFADataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                        int bStrict, char ** papszOptions, 
                        GDALProgressFunc pfnProgress, void * pProgressData )

{
    HFADataset	*poDS;
    GDALDataType eType = GDT_Byte;
    int          iBand;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the basic dataset.                                       */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        eType = GDALDataTypeUnion( eType, poBand->GetRasterDataType() );
    }    

    poDS = (HFADataset *) Create( pszFilename, 
                                  poSrcDS->GetRasterXSize(), 
                                  poSrcDS->GetRasterYSize(), 
                                  poSrcDS->GetRasterCount(), 
                                  eType, papszOptions );

/* -------------------------------------------------------------------- */
/*      Does the source have a PCT for any of the bands?  If so,        */
/*      copy it over.                                                   */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALColorTable *poCT;

        poCT = poBand->GetColorTable();
        if( poCT != NULL )
        {
            double	*padfRed, *padfGreen, *padfBlue;
            int         nColors = poCT->GetColorEntryCount(), iColor;

            padfRed   = (double *) CPLMalloc(sizeof(double) * nColors);
            padfGreen = (double *) CPLMalloc(sizeof(double) * nColors);
            padfBlue  = (double *) CPLMalloc(sizeof(double) * nColors);
            for( iColor = 0; iColor < nColors; iColor++ )
            {
                GDALColorEntry  sEntry;

                poCT->GetColorEntryAsRGB( iColor, &sEntry );
                padfRed[iColor]   = sEntry.c1 / 255.0;
                padfGreen[iColor] = sEntry.c2 / 255.0;
                padfBlue[iColor]  = sEntry.c3 / 255.0;
            }

            HFASetPCT( poDS->hHFA, iBand+1, nColors, 
                       padfRed, padfGreen, padfBlue );

            CPLFree( padfRed );
            CPLFree( padfGreen );
            CPLFree( padfBlue );
        }
    }    

/* -------------------------------------------------------------------- */
/*      Copy projection information.                                    */
/* -------------------------------------------------------------------- */
    double	adfGeoTransform[6];
    const char  *pszProj;

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
        poDS->SetGeoTransform( adfGeoTransform );

    pszProj = poSrcDS->GetProjectionRef();
    if( pszProj != NULL && strlen(pszProj) > 0 )
        poDS->SetProjection( pszProj );

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
                    poHFADriver->Delete( pszFilename );
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

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, 
                  "User terminated" );
        delete poDS;
        poHFADriver->Delete( pszFilename );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_HFA()                        */
/************************************************************************/

void GDALRegister_HFA()

{
    GDALDriver	*poDriver;

    if( poHFADriver == NULL )
    {
        poHFADriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "HFA";
        poDriver->pszLongName = "Erdas Imagine Images (.img)";
        poDriver->pszHelpTopic = "frmt_various.html#HFA";
        
        poDriver->pfnOpen = HFADataset::Open;
        poDriver->pfnCreate = HFADataset::Create;
        poDriver->pfnCreateCopy = HFADataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

