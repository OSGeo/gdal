/******************************************************************************
 * $Id$
 *
 * Name:     hfadataset.cpp
 * Project:  Erdas Imagine Driver
 * Purpose:  Main driver for Erdas Imagine format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.78  2006/04/20 19:14:22  fwarmerdam
 * Don't return null transform as if it were a valid geotransform.
 *
 * Revision 1.77  2006/04/19 14:07:43  fwarmerdam
 * do not create a dataset for a zero band or zero pixel file
 *
 * Revision 1.76  2006/04/10 14:34:06  fwarmerdam
 * Make sure that overviews are treated according to their own intrinsic type,
 * not just the type of the base band which may differ in some circumstances.
 *
 * Revision 1.75  2006/04/03 04:34:19  fwarmerdam
 * added support for reading affine polynomial transforms as geotransform
 *
 * Revision 1.74  2006/03/29 14:24:04  fwarmerdam
 * added preliminary nodata support (readonly)
 *
 * Revision 1.73  2006/02/13 22:42:35  fwarmerdam
 * Fixed metadata related memory leak.
 *
 * Revision 1.72  2006/01/12 22:15:38  fwarmerdam
 * Fix name of albers conical equal area when writing, bug 1035.
 *
 * Revision 1.71  2005/12/21 05:30:45  fwarmerdam
 * return compression type as metadata
 *
 * Revision 1.70  2005/12/20 17:22:51  fwarmerdam
 * Negative proZone values are apparently the FIPS code, instead of
 * the ESRI zone numbers.  Adjust ESRIToUSGSZone() accordingly.
 *
 * Revision 1.69  2005/10/24 14:29:58  fwarmerdam
 * Ensure that metadata dirty flags are cleared after finisheding
 * opening the dataset.
 *
 * Revision 1.68  2005/10/13 01:28:53  fwarmerdam
 * Changed to use underlying multidomain metadata.
 *
 * Revision 1.67  2005/10/13 01:22:03  fwarmerdam
 * Clear old cruft.
 *
 * Revision 1.66  2005/10/12 18:22:39  fwarmerdam
 * ensure bNoRegen is initialized
 *
 * Revision 1.65  2005/10/05 20:39:10  fwarmerdam
 * ensure HFADataset::GetMetadataItem() overridden even if PAM active
 *
 * Revision 1.64  2005/09/28 19:55:04  fwarmerdam
 * Managed the HFA domain ourselves to avoid problems when PAM disabled.
 *
 * Revision 1.63  2005/09/27 22:11:48  fwarmerdam
 * Added String column support for RAT.
 *
 * Revision 1.62  2005/09/27 18:00:59  fwarmerdam
 * Cleanup memory leaks in RAT code.
 *
 * Revision 1.61  2005/09/24 19:04:48  fwarmerdam
 * added preliminary RAT reading code
 *
 * Revision 1.60  2005/09/17 03:47:16  fwarmerdam
 * added dependent overview creation
 *
 * Revision 1.59  2005/09/16 20:30:33  fwarmerdam
 * return HFA_DEPENDENT_FILE in secret metadata, drop .ovr support
 *
 * Revision 1.58  2005/08/19 02:14:11  fwarmerdam
 * bug 857: add ability to set layer names
 *
 * Revision 1.57  2005/07/06 23:12:44  fwarmerdam
 * Fixed up "ds" (digital seconds) units on mapinfo
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=883
 *
 * Revision 1.56  2005/05/23 06:56:48  fwarmerdam
 * delay metadata on band
 *
 * Revision 1.55  2005/05/22 16:32:07  fwarmerdam
 * Fixed so that when we write .img files we translate the false easting
 * and northing into meters if the source SRS is not in meters.
 *
 * Revision 1.54  2005/05/13 13:59:54  fwarmerdam
 * Added GetDefaultHistogram() method.
 *
 * Revision 1.53  2005/05/10 00:58:22  fwarmerdam
 * added preliminary overview within .img support
 *
 * Revision 1.52  2005/05/05 15:54:48  fwarmerdam
 * PAM Enabled
 *
 * Revision 1.51  2005/03/06 19:54:12  fwarmerdam
 * Make sure a fuller UTM projcs name gets set.
 *
 * Revision 1.50  2005/02/17 22:21:27  fwarmerdam
 * avoid memory leak of bin values
 *
 * Revision 1.49  2005/01/29 00:58:38  fwarmerdam
 * Fixed to use 0 for inv flattening of spherical ellipsoid.  Bug 751.
 *
 * Revision 1.48  2005/01/28 03:42:05  fwarmerdam
 * Fixed spelling of Azimuthal, per bug 751.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=751
 *
 * Revision 1.47  2005/01/10 18:25:06  fwarmerdam
 * added support for getting/setting LAYER_TYPE metadata
 *
 * Revision 1.46  2005/01/10 17:41:27  fwarmerdam
 * added HFA compression support: bug 664
 *
 * Revision 1.45  2004/11/05 04:08:01  fwarmerdam
 * Don't crash if access to the histogram table files for some reason.
 *
 * Revision 1.44  2004/10/27 18:06:45  fwarmerdam
 * Avoid use of auto-sized arrays ... not really C++ standard.
 *
 * Revision 1.43  2004/10/26 22:47:23  fwarmerdam
 * Fixed at least one botch in SetColorTable().
 *
 * Revision 1.42  2004/10/26 17:42:02  fwarmerdam
 * support writing color tables with other than 256 entries
 *
 * Revision 1.41  2004/08/27 03:22:28  warmerda
 * hack to support IGNOREUTM option: bug 597
 *
 * Revision 1.40  2004/07/16 20:40:32  warmerda
 * Added a series of patches from Andreas Wimmer which:
 *  o Add lots of improved support for metadata.
 *  o Use USE_SPILL only, instead of SPILL_FILE extra creation option.
 *  o Added ability to control block sizes.
 *
 * Revision 1.39  2004/05/17 14:28:28  warmerda
 * Added min/max support, and harvesting of other auxilary metadata
 * from statistics node.
 *
 * Revision 1.38  2004/05/11 21:38:34  warmerda
 * Handle NAD27 better.
 *
 * Revision 1.37  2004/05/10 16:59:54  warmerda
 * improve EPSG info in coordinate system
 *
 * Revision 1.36  2003/06/10 16:58:57  warmerda
 * Added check on failiure of Create in CreateCopy()
 *
 * Revision 1.35  2003/05/30 17:30:46  warmerda
 * Avoid use of goto.
 *
 * Revision 1.34  2003/05/30 15:40:35  warmerda
 * improved state plane handling with unusual units
 *
 * Revision 1.33  2003/05/21 17:04:57  warmerda
 * significant improvements to reading and writing mapinfo units
 *
 * Revision 1.32  2003/05/13 19:32:10  warmerda
 * support for reading and writing opacity provided by Diana Esch-Mosher
 *
 * Revision 1.30  2003/04/28 20:50:18  warmerda
 * implement dataset level IO, and attempt to optimization createcopy()
 *
 * Revision 1.29  2003/04/22 19:40:36  warmerda
 * fixed email address
 *
 * Revision 1.28  2003/04/21 15:50:29  warmerda
 * fixup generic overview reading support
 *
 * Revision 1.27  2003/04/14 19:06:02  warmerda
 * Don't override a meaningful SRS name with the psPro->proName.
 *
 * Revision 1.26  2003/03/18 21:07:02  dron
 * Added HFADataset::Delete() method.
 *
 * Revision 1.25  2003/02/20 14:43:14  warmerda
 * fixed quirks in handling ungeoreferenced images
 */

#include "gdal_pam.h"
#include "gdal_rat.h"
#include "hfa_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

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
    "WGS 84", "WGS_1984",
    NULL, NULL
};

static const char *apszUnitMap[] = {
    "meters", "1.0",
    "meter", "1.0",
    "m", "1.0",
    "centimeters", "0.01",
    "centimeter", "0.01",
    "cm", "0.01", 
    "millimeters", "0.001",
    "millimeter", "0.001",
    "mm", "0.001",
    "kilometers", "1000.0",
    "kilometer", "1000.0",
    "km", "1000.0",
    "us_survey_feet", "0.3048006096012192",
    "us_survey_foot", "0.3048006096012192",
    "feet", "0.3048006096012192", 
    "foot", "0.3048006096012192",
    "ft", "0.3048006096012192",
    "international_feet", "0.3048",
    "international_foot", "0.3048",
    "inches", "0.0254000508001",
    "inch", "0.0254000508001",
    "in", "0.0254000508001",
    "yards", "0.9144",
    "yard", "0.9144",
    "yd", "0.9144",
    "miles", "1304.544",
    "mile", "1304.544",
    "mi", "1304.544",
    "modified_american_feet", "0.3048122530",
    "modified_american_foot", "0.3048122530",
    "clarke_feet", "0.3047972651",
    "clarke_foot", "0.3047972651",
    "indian_feet", "0.3047995142",
    "indian_foot", "0.3047995142",
    NULL, NULL
};


/* ==================================================================== */
/*      Table relating USGS and ESRI state plane zones.                 */
/* ==================================================================== */
int anUsgsEsriZones[] =
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
/* ==================================================================== */
/*				HFADataset				*/
/* ==================================================================== */
/************************************************************************/

class HFARasterBand;

class CPL_DLL HFADataset : public GDALPamDataset
{
    friend class HFARasterBand;

    HFAHandle	hHFA;

    int         bMetadataDirty;

    int         bGeoDirty;
    double      adfGeoTransform[6];
    char	*pszProjection;

    int         bIgnoreUTM;

    CPLErr      ReadProjection();
    CPLErr      WriteProjection();

  protected:
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

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
    static CPLErr       Delete( const char *pszFilename );


    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual CPLErr SetMetadata( char **, const char * = "" );
    virtual CPLErr SetMetadataItem( const char *, const char *, const char * = "" );

    virtual void   FlushCache( void );
    virtual CPLErr IBuildOverviews( const char *pszResampling, 
                                    int nOverviews, int *panOverviewList, 
                                    int nListBands, int *panBandList,
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                            HFARasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HFARasterBand : public GDALPamRasterBand
{
    friend class HFADataset;

    GDALColorTable *poCT;

    int		nHFADataType;

    int         nOverviews;
    int		nThisOverview;
    HFARasterBand **papoOverviewBands;

    HFAHandle	hHFA;

    int         bMetadataDirty;

    GDALRasterAttributeTable *poDefaultRAT; 

    void        ReadAuxMetadata();

    GDALRasterAttributeTable* ReadNamedRAT( const char *pszName );

  public:

                   HFARasterBand( HFADataset *, int, int );
    virtual        ~HFARasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual const char *GetDescription() const;
    virtual void        SetDescription( const char * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr          SetColorTable( GDALColorTable * );
    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );

    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum(int *pbSuccess = NULL );
    virtual double GetNoDataValue( int *pbSuccess = NULL );

    virtual CPLErr SetMetadata( char **, const char * = "" );
    virtual CPLErr SetMetadataItem( const char *, const char *, const char * = "" );
    virtual CPLErr BuildOverviews( const char *, int, int *,
                                   GDALProgressFunc, void * );

    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets, int ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData);

    virtual const GDALRasterAttributeTable *GetDefaultRAT();
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * );
};

/************************************************************************/
/*                           HFARasterBand()                            */
/************************************************************************/

HFARasterBand::HFARasterBand( HFADataset *poDS, int nBand, int iOverview )

{
    int nCompression;

    if( iOverview == -1 )
        this->poDS = poDS;
    else
        this->poDS = NULL;

    this->hHFA = poDS->hHFA;
    this->nBand = nBand;
    this->poCT = NULL;
    this->nThisOverview = iOverview;
    this->papoOverviewBands = NULL;
    this->bMetadataDirty = FALSE;
    this->poDefaultRAT = NULL;

    HFAGetBandInfo( hHFA, nBand, &nHFADataType,
                    &nBlockXSize, &nBlockYSize, &nOverviews, &nCompression );
    
    if( nCompression != 0 )
        GDALMajorObject::SetMetadataItem( "COMPRESSION", "RLC", 
                                          "IMAGE_STRUCTURE" );

    switch( nHFADataType )
    {
      case EPT_u1:
      case EPT_u4:
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

      case EPT_u32:
        eDataType = GDT_UInt32;
        break;

      case EPT_s32:
        eDataType = GDT_Int32;
        break;

      case EPT_f32:
        eDataType = GDT_Float32;
        break;

      case EPT_f64:
        eDataType = GDT_Float64;
        break;

      case EPT_c64:
        eDataType = GDT_CFloat32;
        break;

      case EPT_c128:
        eDataType = GDT_CFloat64;
        break;

      default:
        eDataType = GDT_Byte;
        /* notdef: this should really report an error, but this isn't
           so easy from within constructors. */
        CPLDebug( "GDAL", "Unsupported pixel type in HFARasterBand: %d.",
                  (int) nHFADataType );
        break;
    }

/* -------------------------------------------------------------------- */
/*      If this is an overview, we need to fetch the actual size,       */
/*      and block size.                                                 */
/* -------------------------------------------------------------------- */
    if( iOverview > -1 )
    {
        nOverviews = 0;
        HFAGetOverviewInfo( hHFA, nBand, iOverview,
                            &nRasterXSize, &nRasterYSize,
                            &nBlockXSize, &nBlockYSize );
    }

/* -------------------------------------------------------------------- */
/*      Collect color table if present.                                 */
/* -------------------------------------------------------------------- */
    double    *padfRed, *padfGreen, *padfBlue, *padfAlpha;
    int       nColors;

    if( iOverview == -1
        && HFAGetPCT( hHFA, nBand, &nColors,
                      &padfRed, &padfGreen, &padfBlue, &padfAlpha ) == CE_None
        && nColors > 0 )
    {
        poCT = new GDALColorTable();
        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            GDALColorEntry   sEntry;

            sEntry.c1 = (short) (padfRed[iColor]   * 255);
            sEntry.c2 = (short) (padfGreen[iColor] * 255);
            sEntry.c3 = (short) (padfBlue[iColor]  * 255);
            sEntry.c4 = (short) (padfAlpha[iColor]  * 255);
            poCT->SetColorEntry( iColor, &sEntry );
        }
    }

/* -------------------------------------------------------------------- */
/*      Setup overviews if present                                      */
/* -------------------------------------------------------------------- */
    if( nThisOverview == -1 && nOverviews > 0 )
    {
        papoOverviewBands = (HFARasterBand **)
            CPLMalloc(sizeof(void*)*nOverviews);

        for( int iOvIndex = 0; iOvIndex < nOverviews; iOvIndex++ )
        {
            papoOverviewBands[iOvIndex] =
                new HFARasterBand( poDS, nBand, iOvIndex );
        }
    }

    poDefaultRAT = ReadNamedRAT( "Descriptor_Table" );
}

/************************************************************************/
/*                           ~HFARasterBand()                           */
/************************************************************************/

HFARasterBand::~HFARasterBand()

{
    FlushCache();

    for( int iOvIndex = 0; iOvIndex < nOverviews; iOvIndex++ )
    {
        delete papoOverviewBands[iOvIndex];
    }
    CPLFree( papoOverviewBands );

    if( poCT != NULL )
        delete poCT;

    if( poDefaultRAT )
        delete poDefaultRAT;
}

/************************************************************************/
/*                          ReadAuxMetadata()                           */
/************************************************************************/

void HFARasterBand::ReadAuxMetadata()

{
    int i;
    HFABand *poBand = hHFA->papoBand[nBand-1];

    // only load metadata for full resolution layer.
    if( nThisOverview != -1 )
        return;

    char ** pszAuxMetaData = GetHFAAuxMetaDataList();
    for( i = 0; pszAuxMetaData[i] != NULL; i += 4 )
    {
        HFAEntry *poEntry;
        
        if( strlen(pszAuxMetaData[i]) > 0 )
            poEntry = poBand->poNode->GetNamedChild( pszAuxMetaData[i] );
        else
            poEntry = poBand->poNode;

        const char *pszFieldName = pszAuxMetaData[i+1] + 1;
        CPLErr eErr = CE_None;

        if( poEntry == NULL )
            continue;

        switch( pszAuxMetaData[i+1][0] )
        {
          case 'd':
          {
              double dfValue;

              dfValue = poEntry->GetDoubleField( pszFieldName, &eErr );
              if( eErr == CE_None )
              {
                  char szValueAsString[100];

                  sprintf( szValueAsString, "%.14g", dfValue );
                  SetMetadataItem( pszAuxMetaData[i+2],
                                   szValueAsString );
              }
          }
          break;
          case 'i':
          case 'l':
          {
              int nValue;
              nValue = poEntry->GetIntField( pszFieldName, &eErr );
              if( eErr == CE_None )
              {
                  char szValueAsString[100];

                  sprintf( szValueAsString, "%d", nValue );
                  SetMetadataItem( pszAuxMetaData[i+2], szValueAsString );
              }
          }
          break;
          case 's':
          case 'e':
          {
              const char *pszValue;
              pszValue = poEntry->GetStringField( pszFieldName, &eErr );
              if( eErr == CE_None )
                  SetMetadataItem( pszAuxMetaData[i+2], pszValue );
          }
          break;
          default:
            CPLAssert( FALSE );
        }
    }
    // now try to read the histogram
    HFAEntry *poEntry = poBand->poNode->GetNamedChild( "Descriptor_Table.Histogram" );
    if ( poEntry != NULL )
    {
        int nNumBins = poEntry->GetIntField( "numRows" );
        int nOffset =  poEntry->GetIntField( "columnDataPtr" );
        const char * pszType =  poEntry->GetStringField( "dataType" );
        int nBinSize = 4;
        
        if( pszType != NULL && EQUALN( "real", pszType, 4 ) )
        {
            nBinSize = 8;
        }
        unsigned int nBufSize = 1024;
        char * pszBinValues = (char *)CPLMalloc( nBufSize );
        pszBinValues[0] = 0;
        for ( int nBin = 0; nBin < nNumBins; ++nBin )
        {
            VSIFSeekL( hHFA->fp, nOffset + nBin*nBinSize, SEEK_SET );
            char szBuf[32];
            if ( nBinSize == 8 )
            {
                double dfValue;
                VSIFReadL( &dfValue, nBinSize, 1, hHFA->fp );
                HFAStandard( nBinSize, &dfValue );
                snprintf( szBuf, 31, "%.14g", dfValue );
            }
            else
            {
                int nValue;
                VSIFReadL( &nValue, nBinSize, 1, hHFA->fp );
                HFAStandard( nBinSize, &nValue );
                snprintf( szBuf, 31, "%d", nValue );
            }
            if ( ( strlen( pszBinValues ) + strlen( szBuf ) + 2 ) > nBufSize )
            {
                nBufSize *= 2;
                pszBinValues = (char *)realloc( pszBinValues, nBufSize );
            }
            strcat( pszBinValues, szBuf );
            strcat( pszBinValues, "|" );
        }
        SetMetadataItem( "STATISTICS_HISTOBINVALUES", pszBinValues );
        CPLFree( pszBinValues );
    }
}

/************************************************************************/
/*                             GetNoData()                              */
/************************************************************************/

double HFARasterBand::GetNoDataValue( int *pbSuccess )

{
    double dfNoData;

    if( HFAGetBandNoData( hHFA, nBand, &dfNoData ) )
    {
        *pbSuccess = TRUE;
        return dfNoData;
    }
    else
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double HFARasterBand::GetMinimum( int *pbSuccess )

{
    const char *pszValue = GetMetadataItem( "STATISTICS_MINIMUM" );
    
    if( pszValue != NULL )
    {
        *pbSuccess = TRUE;
        return atof(pszValue);
    }
    else
    {
        return GDALRasterBand::GetMinimum( pbSuccess );
    }
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double HFARasterBand::GetMaximum( int *pbSuccess )

{
    const char *pszValue = GetMetadataItem( "STATISTICS_MAXIMUM" );
    
    if( pszValue != NULL )
    {
        *pbSuccess = TRUE;
        return atof(pszValue);
    }
    else
    {
        return GDALRasterBand::GetMaximum( pbSuccess );
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int HFARasterBand::GetOverviewCount()

{
    if( nOverviews == 0 )
        return GDALRasterBand::GetOverviewCount();
    else
        return nOverviews;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *HFARasterBand::GetOverview( int i )

{
    if( nOverviews == 0 )
        return GDALRasterBand::GetOverview( i );
    else if( i < 0 || i >= nOverviews )
        return NULL;
    else
        return papoOverviewBands[i];
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HFARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    CPLErr	eErr;
    int         nThisDataType = nHFADataType; // overview may differ.

    if( nThisOverview == -1 )
        eErr = HFAGetRasterBlock( hHFA, nBand, nBlockXOff, nBlockYOff,
                                  pImage );
    else
    {
        eErr =  HFAGetOverviewRasterBlock( hHFA, nBand, nThisOverview,
                                           nBlockXOff, nBlockYOff,
                                           pImage );
        nThisDataType = 
            hHFA->papoBand[nBand-1]->papoOverviews[nThisOverview]->nDataType;
    }

    if( eErr == CE_None && nThisDataType == EPT_u4 )
    {
        GByte	*pabyData = (GByte *) pImage;

        for( int ii = nBlockXSize * nBlockYSize - 2; ii >= 0; ii -= 2 )
        {
            pabyData[ii] = pabyData[ii>>1] & 0x0f;
            pabyData[ii+1] = (pabyData[ii>>1] & 0xf0) >> 4;
        }
    }
    if( eErr == CE_None && nThisDataType == EPT_u1)
    {
        GByte	*pabyData = (GByte *) pImage;

        for( int ii = nBlockXSize * nBlockYSize - 1; ii >= 0; ii-- )
        {
            if( (pabyData[ii>>3] & (1 << (ii & 0x7))) )
                pabyData[ii] = 1;
            else
                pabyData[ii] = 0;
        }
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr HFARasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    if( nThisOverview == -1 )
        return( HFASetRasterBlock( hHFA, nBand, nBlockXOff, nBlockYOff,
                                   pImage ) );
    else
        return( HFASetOverviewRasterBlock( hHFA, nBand, nThisOverview,
                                           nBlockXOff, nBlockYOff,
                                           pImage ) );
}

/************************************************************************/
/*                         GetDescription()                             */
/************************************************************************/

const char * HFARasterBand::GetDescription() const
{
    const char *pszName = HFAGetBandName( hHFA, nBand );
    
    if( pszName == NULL )
        return GDALPamRasterBand::GetDescription();
    else
        return pszName;
}
 
/************************************************************************/
/*                         SetDescription()                             */
/************************************************************************/
void HFARasterBand::SetDescription( const char *pszName )
{
    HFASetBandName( hHFA, nBand, pszName );
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
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr HFARasterBand::SetColorTable( GDALColorTable * poCTable )

{
    int nColors = poCTable->GetColorEntryCount();

/* -------------------------------------------------------------------- */
/*      Write out the colortable, and update the configuration.         */
/* -------------------------------------------------------------------- */
    double *padfRed, *padfGreen, *padfBlue, *padfAlpha;

    padfRed   = (double *) CPLMalloc(sizeof(double) * nColors);
    padfGreen = (double *) CPLMalloc(sizeof(double) * nColors);
    padfBlue  = (double *) CPLMalloc(sizeof(double) * nColors);
    padfAlpha = (double *) CPLMalloc(sizeof(double) * nColors);

    for( int iColor = 0; iColor < nColors; iColor++ )
    {
        GDALColorEntry  sRGB;
	    
        poCTable->GetColorEntryAsRGB( iColor, &sRGB );
        
        padfRed[iColor] = sRGB.c1 / 255.0;
        padfGreen[iColor] = sRGB.c2 / 255.0;
        padfBlue[iColor] = sRGB.c3 / 255.0;
        padfAlpha[iColor] = sRGB.c4 / 255.0;
    }

    HFASetPCT( hHFA, nBand, nColors,
	       padfRed, padfGreen, padfBlue, padfAlpha);

    CPLFree( padfRed );
    CPLFree( padfGreen );
    CPLFree( padfBlue );
    CPLFree( padfAlpha );

    if( poCT )
      delete poCT;
    
    poCT = poCTable->Clone();

    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr HFARasterBand::SetMetadata( char **papszMDIn, const char *pszDomain )

{
    bMetadataDirty = TRUE;

    return GDALPamRasterBand::SetMetadata( papszMDIn, pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr HFARasterBand::SetMetadataItem( const char *pszTag, const char *pszValue,
                                       const char *pszDomain )

{
    bMetadataDirty = TRUE;

    return GDALPamRasterBand::SetMetadataItem( pszTag, pszValue, pszDomain );
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

CPLErr HFARasterBand::BuildOverviews( const char *pszResampling, 
                                      int nReqOverviews, int *panOverviewList, 
                                      GDALProgressFunc pfnProgress, 
                                      void *pProgressData )

{
    int iOverview;
    GDALRasterBand **papoOvBands;
    int bNoRegen = FALSE;
    
    if( nThisOverview != -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to build overviews on an overview layer." );

        return CE_Failure;
    }

    papoOvBands = (GDALRasterBand **) CPLCalloc(sizeof(void*),nReqOverviews);

    if( EQUALN(pszResampling,"NO_REGEN:",9) )
    {
        pszResampling += 9;
        bNoRegen = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Loop over overview levels requested.                            */
/* -------------------------------------------------------------------- */
    for( iOverview = 0; iOverview < nReqOverviews; iOverview++ )
    {
/* -------------------------------------------------------------------- */
/*      Find this overview level.                                       */
/* -------------------------------------------------------------------- */
        int i, iResult = -1, nReqOvLevel;

        nReqOvLevel = 
            GDALOvLevelAdjust(panOverviewList[iOverview],nRasterXSize);

        for( i = 0; i < nOverviews && papoOvBands[iOverview] == NULL; i++ )
        {
            int nThisOvLevel;

            nThisOvLevel = (int) (0.5 + GetXSize() 
                    / (double) papoOverviewBands[i]->GetXSize());

            if( nReqOvLevel == nThisOvLevel )
                papoOvBands[iOverview] = papoOverviewBands[i];
        }

/* -------------------------------------------------------------------- */
/*      If this overview level does not yet exist, create it now.       */
/* -------------------------------------------------------------------- */
        if( papoOvBands[iOverview] == NULL )
        {
            iResult = HFACreateOverview( hHFA, nBand, panOverviewList[iOverview] );
            if( iResult < 0 )
                return CE_Failure;
            
            nOverviews = iResult + 1;
            papoOverviewBands = (HFARasterBand **) 
                CPLRealloc( papoOverviewBands, sizeof(void*) * nOverviews);
            papoOverviewBands[iResult] = new HFARasterBand( 
                (HFADataset *) poDS, nBand, iResult );

            papoOvBands[iOverview] = papoOverviewBands[iResult];
        }

    }

/* -------------------------------------------------------------------- */
/*      Regenerate the overviews.                                       */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( !bNoRegen )
        eErr = GDALRegenerateOverviews( this, nReqOverviews, papoOvBands,
                                        pszResampling, 
                                        pfnProgress, pProgressData );
    
    CPLFree( papoOvBands );
    
    return CE_None;
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr 
HFARasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                    int *pnBuckets, int ** ppanHistogram,
                                    int bForce,
                                    GDALProgressFunc pfnProgress, 
                                    void *pProgressData)

{
    if( GetMetadataItem( "STATISTICS_HISTOBINVALUES" ) != NULL 
        && GetMetadataItem( "STATISTICS_MINIMUM" ) != NULL 
        && GetMetadataItem( "STATISTICS_MAXIMUM" ) != NULL )
    {
        int i;
        const char *pszNextBin;
        const char *pszBinValues = 
            GetMetadataItem( "STATISTICS_HISTOBINVALUES" );

        *pdfMin = atof(GetMetadataItem("STATISTICS_MINIMUM"));
        *pdfMax = atof(GetMetadataItem("STATISTICS_MAXIMUM"));

        *pnBuckets = 0;
        for( i = 0; pszBinValues[i] != '\0'; i++ )
        {
            if( pszBinValues[i] == '|' )
                (*pnBuckets)++;
        }

        *ppanHistogram = (int *) CPLCalloc(sizeof(int),*pnBuckets);

        pszNextBin = pszBinValues;
        for( i = 0; i < *pnBuckets; i++ )
        {
            (*ppanHistogram)[i] = atoi(pszNextBin);

            while( *pszNextBin != '|' && *pszNextBin != '\0' )
                pszNextBin++;
            if( *pszNextBin == '|' )
                pszNextBin++;
        }

        // Adjust min/max to reflect outer edges of buckets.
        double dfBucketWidth = (*pdfMax - *pdfMin) / (*pnBuckets-1);
        *pdfMax += 0.5 * dfBucketWidth;
        *pdfMin -= 0.5 * dfBucketWidth;

        return CE_None;
    }
    else
        return GDALPamRasterBand::GetDefaultHistogram( pdfMin, pdfMax, 
                                                       pnBuckets,ppanHistogram,
                                                       bForce, 
                                                       pfnProgress,
                                                       pProgressData );
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr HFARasterBand::SetDefaultRAT( const GDALRasterAttributeTable * poRAT )

{
    return GDALPamRasterBand::SetDefaultRAT( poRAT );
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

const GDALRasterAttributeTable *HFARasterBand::GetDefaultRAT()

{		
    return poDefaultRAT;
}

/************************************************************************/
/*                            ReadNamedRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *HFARasterBand::ReadNamedRAT( const char *pszName )

{
/* -------------------------------------------------------------------- */
/*      Find the requested table.                                       */
/* -------------------------------------------------------------------- */
    HFAEntry *poDT = hHFA->papoBand[nBand-1]->poNode->GetNamedChild(pszName);

    if( poDT == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding RAT.                                     */
/* -------------------------------------------------------------------- */
    GDALRasterAttributeTable *poRAT = NULL;
    int nRowCount = poDT->GetIntField( "numRows" );

    poRAT = new GDALRasterAttributeTable();

/* -------------------------------------------------------------------- */
/*      Scan under table for columns.                                   */
/* -------------------------------------------------------------------- */
    HFAEntry *poDTChild;

    for( poDTChild = poDT->GetChild(); 
         poDTChild != NULL; 
         poDTChild = poDTChild->GetNext() )
    {
        if( EQUAL(poDTChild->GetType(),"Edsc_BinFunction") )
        {
            double dfMax = poDTChild->GetDoubleField( "maxLimit" );
            double dfMin = poDTChild->GetDoubleField( "minLimit" );
            int    nBinCount = poDTChild->GetIntField( "numBins" );

            if( nBinCount == nRowCount 
                && dfMax != dfMin && nBinCount != 0 )
                poRAT->SetLinearBinning( dfMin, 
                                         (dfMax-dfMin) / (nBinCount-1) );
        }

        if( !EQUAL(poDTChild->GetType(),"Edsc_Column") )
            continue;

        int nOffset = poDTChild->GetIntField( "columnDataPtr" );
        const char * pszType = poDTChild->GetStringField( "dataType" );
        GDALRATFieldUsage eType = GFU_Generic;
        int i;

        if( pszType == NULL || nOffset == 0 )
            continue;
        
        if( EQUAL(poDTChild->GetName(),"Histogram") )
            eType = GFU_Generic;
        else if( EQUAL(poDTChild->GetName(),"Red") )
            eType = GFU_Red;
        else if( EQUAL(poDTChild->GetName(),"Green") )
            eType = GFU_Green;
        else if( EQUAL(poDTChild->GetName(),"Blue") )
            eType = GFU_Blue;
        else if( EQUAL(poDTChild->GetName(),"Alpha") )
            eType = GFU_Alpha;
        else if( EQUAL(poDTChild->GetName(),"Class_Names") )
            eType = GFU_Name;
            
        if( EQUAL(pszType,"real") )
        {
            double *padfColData = (double*)CPLMalloc(nRowCount*sizeof(double));

            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            VSIFReadL( padfColData, nRowCount, sizeof(double), hHFA->fp );
#ifdef CPL_MSB
            GDALSwapWords( padfColData, 8, nRowCount, 8 );
#endif
            poRAT->CreateColumn( poDTChild->GetName(), GFT_Real, eType );
            for( i = 0; i < nRowCount; i++ )
                poRAT->SetValue( i, poRAT->GetColumnCount()-1, padfColData[i]);

            CPLFree( padfColData );
        }
        else if( EQUAL(pszType,"string") )
        {
            int nMaxNumChars = poDTChild->GetIntField( "maxNumChars" );
            char *pachColData = (char*)CPLCalloc(nRowCount+1,nMaxNumChars);

            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            VSIFReadL( pachColData, nRowCount, nMaxNumChars, hHFA->fp );

            poRAT->CreateColumn(poDTChild->GetName(),GFT_String,eType);
            for( i = 0; i < nRowCount; i++ )
            {
                CPLString oRowVal;

                oRowVal.assign( pachColData+nMaxNumChars*i, nMaxNumChars );
                poRAT->SetValue( i, poRAT->GetColumnCount()-1, 
                                 oRowVal.c_str() );
            }

            CPLFree( pachColData );
        }
        else if( EQUALN(pszType,"int",3) )
        {
            GInt32 *panColData = (GInt32*)CPLMalloc(nRowCount*sizeof(GInt32));

            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            VSIFReadL( panColData, nRowCount, sizeof(GInt32), hHFA->fp );
#ifdef CPL_MSB
            GDALSwapWords( panColData, 4, nRowCount, 4 );
#endif
            poRAT->CreateColumn(poDTChild->GetName(),GFT_Integer,eType);
            for( i = 0; i < nRowCount; i++ )
                poRAT->SetValue( i, poRAT->GetColumnCount()-1, panColData[i] );

            CPLFree( panColData );
        }
    }

    return poRAT;
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
    pszProjection = CPLStrdup("");
    this->bMetadataDirty = FALSE;
    bIgnoreUTM = FALSE;
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
    GDALPamDataset::FlushCache();

    if( eAccess != GA_Update )
        return;

    if( bGeoDirty )
        WriteProjection();

    if( bMetadataDirty && GetMetadata() != NULL )
    {
        HFASetMetadata( hHFA, 0, GetMetadata() );
        bMetadataDirty = FALSE;
    }

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        HFARasterBand *poBand = (HFARasterBand *) GetRasterBand(iBand+1);
        if( poBand->bMetadataDirty && poBand->GetMetadata() != NULL )
        {
            HFASetMetadata( hHFA, iBand+1, poBand->GetMetadata() );
            poBand->bMetadataDirty = FALSE;
        }
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
            sPro.proName = "Geographic (Lat/Lon)";
        }
    }

    /* FIXME/NOTDEF/TODO: Add State Plane */
    else if( !bIgnoreUTM && oSRS.GetUTMZone( NULL ) != 0 )
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
        sPro.proName = "Albers Conical Equal Area";
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
        sPro.proName = "Lambert Azimuthal Equal-area";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        sPro.proNumber = EPRJ_AZIMUTHAL_EQUIDISTANT;
        sPro.proName = "Azimuthal Equidistant";
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
        sPro.proName = "Van der Grinten";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR;
        sPro.proName = "Oblique Mercator (Hotine)";
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
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Projection %s not supported for translation to Imagine.",
                  pszProjName );
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

/* -------------------------------------------------------------------- */
/*      Handle units.  Try to match up with a known name.               */
/* -------------------------------------------------------------------- */
    sMapInfo.units = "meters";

    if( bHaveSRS && oSRS.IsGeographic() )
        sMapInfo.units = "dd";
    else if( bHaveSRS && oSRS.GetLinearUnits() != 1.0 )
    {
        double dfClosestDiff = 100.0;
        int    iClosest=-1, iUnit;
        char *pszUnitName = NULL;
        double dfActualSize = oSRS.GetLinearUnits( &pszUnitName );

        for( iUnit = 0; apszUnitMap[iUnit] != NULL; iUnit += 2 )
        {
            if( fabs(atof(apszUnitMap[iUnit+1]) - dfActualSize) < dfClosestDiff )
            {
                iClosest = iUnit;
                dfClosestDiff = fabs(atof(apszUnitMap[iUnit+1])-dfActualSize);
            }
        }
        
        if( iClosest == -1 ||  fabs(dfClosestDiff/dfActualSize) > 0.0001 )
        {
            CPLError( CE_Warning, CPLE_NotSupported, 
                      "Unable to identify Erdas units matching %s/%gm,\n"
                      "output units will be wrong.", 
                      pszUnitName, dfActualSize );
        }
        else
            sMapInfo.units = (char *) apszUnitMap[iClosest];

        /* We need to convert false easting and northing to meters. */
        sPro.proParams[6] *= dfActualSize;
        sPro.proParams[7] *= dfActualSize;
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
/*                           ESRIToUSGSZone()                           */
/*                                                                      */
/*      Convert ESRI style state plane zones to USGS style state        */
/*      plane zones.                                                    */
/************************************************************************/

static int ESRIToUSGSZone( int nESRIZone )

{
    int		nPairs = sizeof(anUsgsEsriZones) / (2*sizeof(int));
    int		i;

    if( nESRIZone < 0 )
        return ABS(nESRIZone);

    for( i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i*2+1] == nESRIZone )
            return anUsgsEsriZones[i*2];
    }

    return 0;
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

    if( psPro == NULL && psMapInfo != NULL )
    {
        oSRS.SetLocalCS( psMapInfo->proName );
    }

    else if( psPro == NULL )
    {
        return CE_Failure;
    }

    else if( psPro->proType == EPRJ_EXTERNAL )
    {
        oSRS.SetLocalCS( psPro->proName );
    }

    else if( psPro->proNumber != EPRJ_LATLONG )
    {
        oSRS.SetProjCS( psPro->proName );
    }

/* -------------------------------------------------------------------- */
/*      Handle units.  It is important to deal with this first so       */
/*      that the projection Set methods will automatically do           */
/*      translation of linear values (like false easting) to PROJCS     */
/*      units from meters.  Erdas linear projection values are          */
/*      always in meters.                                               */
/* -------------------------------------------------------------------- */
    int iUnitIndex = 0;

    if( oSRS.IsProjected() || oSRS.IsLocal() )
    {
        if( psMapInfo )
        {
            for( iUnitIndex = 0; 
                 apszUnitMap[iUnitIndex] != NULL; 
                 iUnitIndex += 2 )
            {
                if( EQUAL(apszUnitMap[iUnitIndex], psMapInfo->units ) )
                    break;
            }
            
            if( apszUnitMap[iUnitIndex] == NULL )
                iUnitIndex = 0;
            
            oSRS.SetLinearUnits( psMapInfo->units, 
                                 atof(apszUnitMap[iUnitIndex+1]) );
        }
        else
            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
    }

    if( psPro == NULL )
    {
        if( oSRS.IsLocal() )
        {
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
        else
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Try to work out ellipsoid and datum information.                */
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

    if( psPro->proSpheroid.a == 0.0 )
        ((Eprj_ProParameters *) psPro)->proSpheroid.a = 6378137.0;
    if( psPro->proSpheroid.b == 0.0 )
        ((Eprj_ProParameters *) psPro)->proSpheroid.b = 6356752.3;

    if( fabs(psPro->proSpheroid.b - psPro->proSpheroid.a) < 0.001 )
        dfInvFlattening = 0.0; /* special value for sphere. */
    else
        dfInvFlattening = 1.0/(1.0-psPro->proSpheroid.b/psPro->proSpheroid.a);

/* -------------------------------------------------------------------- */
/*      Handle different projection methods.                            */
/* -------------------------------------------------------------------- */
    switch( psPro->proNumber )
    {
      case EPRJ_LATLONG:
        break;

      case EPRJ_UTM:
        // We change this to unnamed so that SetUTM will set the long
        // UTM description.
        oSRS.SetProjCS( "unnamed" );
        oSRS.SetUTM( psPro->proZone, psPro->proParams[3] >= 0.0 );
        break;

      case EPRJ_STATE_PLANE:
      {
          char *pszUnitsName = NULL;
          double dfLinearUnits = oSRS.GetLinearUnits( &pszUnitsName );
          
          pszUnitsName = CPLStrdup( pszUnitsName );

          /* Set state plane zone.  Set NAD83/27 on basis of spheroid */
          oSRS.SetStatePlane( ESRIToUSGSZone(psPro->proZone), 
                              fabs(psPro->proSpheroid.a - 6378137.0)< 1.0,
                              pszUnitsName, dfLinearUnits );

          CPLFree( pszUnitsName );
      }
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
        oSRS.SetLocalCS( psPro->proName );
        break;
    }

/* -------------------------------------------------------------------- */
/*      Try and set the GeogCS information.                             */
/* -------------------------------------------------------------------- */
    if( oSRS.GetAttrNode("GEOGCS") == NULL
        && oSRS.GetAttrNode("LOCAL_CS") == NULL )
    {
        if( EQUAL(pszDatumName,"WGS 84") )
            oSRS.SetWellKnownGeogCS( "WGS84" );
        else if( strstr(pszDatumName,"NAD27") != NULL 
                 || EQUAL(pszDatumName,"North_American_Datum_1927") )
            oSRS.SetWellKnownGeogCS( "NAD27" );
        else if( EQUAL(pszDatumName,"North_American_Datum_1983") 
                 || strstr(pszDatumName,"NAD83") != NULL )
            oSRS.SetWellKnownGeogCS( "NAD83" );
        else
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
    }

/* -------------------------------------------------------------------- */
/*      Try to insert authority information if possible.  Fixup any     */
/*      ordering oddities.                                              */
/* -------------------------------------------------------------------- */
    oSRS.AutoIdentifyEPSG();
    oSRS.Fixup();

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
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr HFADataset::IBuildOverviews( const char *pszResampling, 
                                    int nOverviews, int *panOverviewList, 
                                    int nListBands, int *panBandList,
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData )
    
{
    int i;

    if( GetAccess() == GA_ReadOnly )
        return GDALDataset::IBuildOverviews( pszResampling, 
                                             nOverviews, panOverviewList, 
                                             nListBands, panBandList, 
                                             pfnProgress, pProgressData );

    for( i = 0; i < nListBands; i++ )
    {
        CPLErr eErr;
        GDALRasterBand *poBand;

        // TODO: We ought to used scaled progress monitors so we would get 
        // 0 to 100 progress out of the whole process ... later.

        poBand = GetRasterBand( panBandList[i] );
        eErr = 
            poBand->BuildOverviews( pszResampling, nOverviews, panOverviewList,
                                    pfnProgress, pProgressData );
        if( eErr != CE_None )
            return eErr;
    }

    return CE_None;
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
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    HFAGetRasterInfo( hHFA, &poDS->nRasterXSize, &poDS->nRasterYSize,
                      &poDS->nBands );

    if( poDS->nBands == 0 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to open %s, it has zero usable bands.",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    if( poDS->nRasterXSize == 0 || poDS->nRasterYSize == 0 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to open %s, it has no pixels.",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get geotransform.                                               */
/* -------------------------------------------------------------------- */
    HFAGetGeoTransform( hHFA, poDS->adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Get the projection.                                             */
/* -------------------------------------------------------------------- */
    poDS->ReadProjection();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poDS->nBands; i++ )
    {
        poDS->SetBand( i+1, new HFARasterBand( poDS, i+1, -1 ) );
    }

/* -------------------------------------------------------------------- */
/*      Collect GDAL custom Metadata, and "auxilary" metadata from      */
/*      well known HFA structures for the bands.  We defer this till    */
/*      now to ensure that the bands are properly setup before          */
/*      interacting with PAM.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poDS->nBands; i++ )
    {
        HFARasterBand *poBand = (HFARasterBand *) poDS->GetRasterBand( i+1 );

        char **papszMD = HFAGetMetadata( hHFA, i+1 );
        if( papszMD != NULL )
        {
            poBand->SetMetadata( papszMD );
            CSLDestroy( papszMD );
        }
        
        poBand->ReadAuxMetadata();
    }

/* -------------------------------------------------------------------- */
/*      Check for GDAL style metadata.                                  */
/* -------------------------------------------------------------------- */
    char **papszMD = HFAGetMetadata( hHFA, 0 );
    if( papszMD != NULL )
    {
        poDS->SetMetadata( papszMD );
        CSLDestroy( papszMD );
    }

/* -------------------------------------------------------------------- */
/*      Check for dependent dataset value.                              */
/* -------------------------------------------------------------------- */
    HFAInfo_t *psInfo = (HFAInfo_t *) hHFA;
    HFAEntry  *poEntry = psInfo->poRoot->GetNamedChild("DependentFile");
    if( poEntry != NULL )
    {
        poDS->SetMetadataItem( "HFA_DEPENDENT_FILE", 
                               poEntry->GetStringField( "dependent.string" ),
                               "HFA" );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Clear dirty metadata flags.                                     */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poDS->nBands; i++ )
    {
        HFARasterBand *poBand = (HFARasterBand *) poDS->GetRasterBand( i+1 );
        poBand->bMetadataDirty = FALSE;
    }
    poDS->bMetadataDirty = FALSE;

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
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr HFADataset::SetMetadata( char **papszMDIn, const char *pszDomain )

{
    bMetadataDirty = TRUE;

    return GDALPamDataset::SetMetadata( papszMDIn, pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr HFADataset::SetMetadataItem( const char *pszTag, const char *pszValue,
                                    const char *pszDomain )

{
    bMetadataDirty = TRUE;

    return GDALPamDataset::SetMetadataItem( pszTag, pszValue, pszDomain );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HFADataset::GetGeoTransform( double * padfTransform )

{
    if( adfGeoTransform[0] != 0.0
        || adfGeoTransform[1] != 1.0
        || adfGeoTransform[2] != 0.0
        || adfGeoTransform[3] != 0.0
        || adfGeoTransform[4] != 0.0
        || adfGeoTransform[5] != 1.0 )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr HFADataset::SetGeoTransform( double * padfTransform )

{
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
    bGeoDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      Multi-band raster io handler.  Here we ensure that the block    */
/*      based loading is used for spill file rasters.  That is          */
/*      because they are effectively pixel interleaved, so              */
/*      processing all bands for a given block together avoid extra     */
/*      seeks.                                                          */
/************************************************************************/

CPLErr HFADataset::IRasterIO( GDALRWFlag eRWFlag, 
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize, 
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap, 
                              int nPixelSpace, int nLineSpace, int nBandSpace )

{
    if( hHFA->papoBand[panBandMap[0]-1]->fpExternal != NULL 
        && nBandCount > 1 )
        return GDALDataset::BlockBasedRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace );
    else
        return 
            GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize, eBufType, 
                                    nBandCount, panBandMap, 
                                    nPixelSpace, nLineSpace, nBandSpace );
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
    HFADataset *poDS = (HFADataset *) GDALOpen( pszFilenameIn, GA_Update );

/* -------------------------------------------------------------------- */
/*      Special creation option to disable checking for UTM             */
/*      parameters when writing the projection.  This is a special      */
/*      hack for sam.gillingham@nrm.qld.gov.au.                         */
/* -------------------------------------------------------------------- */
    if( poDS != NULL )
    {
        poDS->bIgnoreUTM = CSLFetchBoolean( papszParmList, "IGNOREUTM",
                                            FALSE );
    }

    return poDS;

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
    int          nBandCount = poSrcDS->GetRasterCount();

    (void) bStrict;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the basic dataset.                                       */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        eType = GDALDataTypeUnion( eType, poBand->GetRasterDataType() );
    }

    poDS = (HFADataset *) Create( pszFilename,
                                  poSrcDS->GetRasterXSize(),
                                  poSrcDS->GetRasterYSize(),
                                  nBandCount,
                                  eType, papszOptions );

    if( poDS == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Does the source have a PCT for any of the bands?  If so,        */
/*      copy it over.                                                   */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALColorTable *poCT;

        poCT = poBand->GetColorTable();
        if( poCT != NULL )
        {
            double	*padfRed, *padfGreen, *padfBlue, *padfAlpha;
            int         nColors = poCT->GetColorEntryCount(), iColor;

            padfRed   = (double *) CPLMalloc(sizeof(double) * nColors);
            padfGreen = (double *) CPLMalloc(sizeof(double) * nColors);
            padfBlue  = (double *) CPLMalloc(sizeof(double) * nColors);
            padfAlpha  = (double *) CPLMalloc(sizeof(double) * nColors);
            for( iColor = 0; iColor < nColors; iColor++ )
            {
                GDALColorEntry  sEntry;

                poCT->GetColorEntryAsRGB( iColor, &sEntry );
                padfRed[iColor]   = sEntry.c1 / 255.0;
                padfGreen[iColor] = sEntry.c2 / 255.0;
                padfBlue[iColor]  = sEntry.c3 / 255.0;
                padfAlpha[iColor]  = sEntry.c4 / 255.0;
            }

            HFASetPCT( poDS->hHFA, iBand+1, nColors,
                       padfRed, padfGreen, padfBlue, padfAlpha );

            CPLFree( padfRed );
            CPLFree( padfGreen );
            CPLFree( padfBlue );
            CPLFree( padfAlpha );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have metadata for any of the bands or the dataset as a    */
/*      whole?                                                          */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetMetadata() != NULL )
        poDS->SetMetadata( poSrcDS->GetMetadata() );

    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        poDS->GetRasterBand(iBand+1)->SetMetadata( poSrcBand->GetMetadata() );
    }

/* -------------------------------------------------------------------- */
/*      Copy projection information.                                    */
/* -------------------------------------------------------------------- */
    double	adfGeoTransform[6];
    const char  *pszProj;

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0 || fabs(adfGeoTransform[5]) != 1.0))
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
        * nBandCount;

    nBlocksDone = 0;
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDS->GetRasterBand( iBand+1 );
        int	       iYOffset;
        void           *pData;
        CPLErr  eErr;
        int            nMoveLines = nBlockYSize;

#define TRANSFER_BY_BLOCK
#ifdef TRANSFER_BY_BLOCK
        pData = CPLMalloc(nBlockXSize * nMoveLines
                          * GDALGetDataTypeSize(eType) / 8);
#else
xx
        pData = CPLMalloc(nXSize * nMoveLines
                          * GDALGetDataTypeSize(eType) / 8);
#endif

        for( iYOffset = 0; iYOffset < nYSize; iYOffset += nMoveLines )
        {
#ifdef TRANSFER_BY_BLOCK
            int iXOffset;

            for( iXOffset = 0; iXOffset < nXSize; iXOffset += nBlockXSize )
            {
                int	nTBXSize, nTBYSize;

                if( !pfnProgress( (nBlocksDone++) / (float) nBlockTotal,
                                  NULL, pProgressData ) )
                {
                    CPLError( CE_Failure, CPLE_UserInterrupt,
                              "User terminated" );
                    delete poDS;

                    GDALDriver *poHFADriver =
                        (GDALDriver *) GDALGetDriverByName( "HFA" );
                    poHFADriver->Delete( pszFilename );
                    return NULL;
                }

                nTBXSize = MIN(nBlockXSize,nXSize-iXOffset);
                nTBYSize = MIN(nMoveLines,nYSize-iYOffset);

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
#else
            if( !pfnProgress( (iYOffset + iBand*nYSize)  
                              / (float) (nYSize * nBandCount), 
                              NULL, pProgressData ))
            {
                CPLError( CE_Failure, CPLE_UserInterrupt,
                          "User terminated" );
                delete poDS;

                GDALDriver *poHFADriver =
                    (GDALDriver *) GDALGetDriverByName( "HFA" );
                poHFADriver->Delete( pszFilename );
                return NULL;
            }

            int	nTBYSize;

            nTBYSize = MIN(nMoveLines,nYSize-iYOffset);
            eErr = poSrcBand->RasterIO( GF_Read,
                                        0, iYOffset, nXSize, nTBYSize,
                                        pData, nXSize, nTBYSize,
                                        eType, 0, 0 );
            if( eErr != CE_None )
            {
                return NULL;
            }
            
            eErr = poDstBand->RasterIO( GF_Write,
                                        0, iYOffset, nXSize, nTBYSize,
                                        pData, nXSize, nTBYSize,
                                        eType, 0, 0 );

            if( eErr != CE_None )
            {
                return NULL;
            }

            poDstBand->FlushCache();
#endif
        }

        CPLFree( pData );
    }

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt,
                  "User terminated" );
        delete poDS;

        GDALDriver *poHFADriver =
            (GDALDriver *) GDALGetDriverByName( "HFA" );
        poHFADriver->Delete( pszFilename );
        return NULL;
    }

    poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

CPLErr HFADataset::Delete( const char *pszFilename )

{
    return HFADelete( pszFilename );
}


/************************************************************************/
/*                          GDALRegister_HFA()                          */
/************************************************************************/

void GDALRegister_HFA()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "HFA" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "HFA" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Erdas Imagine Images (.img)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_hfa.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "img" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64 CFloat32 CFloat64" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='BLOCKSIZE' type='integer' description='tile width/height (32-2048)'/>"
"   <Option name='USE_SPILL' type='boolean' description='Force use of spill file'/>"
"   <Option name='COMPRESSED' type='boolean' description='compress blocks, default NO'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = HFADataset::Open;
        poDriver->pfnCreate = HFADataset::Create;
        poDriver->pfnCreateCopy = HFADataset::CreateCopy;
        poDriver->pfnDelete = HFADataset::Delete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

