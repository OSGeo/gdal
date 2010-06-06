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
 ****************************************************************************/

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

OGRBoolean WritePeStringIfNeeded(OGRSpatialReference*	poSRS, HFAHandle hHFA);
void ClearSR(HFAHandle hHFA);

static const char *apszDatumMap[] = {
    /* Imagine name, WKT name */
    "NAD27", "North_American_Datum_1927",
    "NAD83", "North_American_Datum_1983",
    "WGS 84", "WGS_1984",
    "WGS 1972", "WGS_1972",
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
    int         bForceToPEString;

    int		nGCPCount;
    GDAL_GCP	asGCPList[36];

    void        UseXFormStack( int nStepCount,
                               Efga_Polynomial *pasPolyListForward,
                               Efga_Polynomial *pasPolyListReverse );

  protected:
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

  public:
                HFADataset();
                ~HFADataset();

    static int          Identify( GDALOpenInfo * );
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

    virtual char **GetFileList(void);

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

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

    CPLErr      CleanOverviews();

    HFAHandle	hHFA;

    int         bMetadataDirty;

    GDALRasterAttributeTable *poDefaultRAT; 

    void        ReadAuxMetadata();
    void        ReadHistogramMetadata();
    void        EstablishOverviews();

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
    virtual CPLErr SetNoDataValue( double dfValue );

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
    this->nOverviews = -1; 

    HFAGetBandInfo( hHFA, nBand, &nHFADataType,
                    &nBlockXSize, &nBlockYSize, &nCompression );
    
    if( nCompression != 0 )
        GDALMajorObject::SetMetadataItem( "COMPRESSION", "RLE", 
                                          "IMAGE_STRUCTURE" );

    switch( nHFADataType )
    {
      case EPT_u1:
      case EPT_u2:
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

    if( HFAGetDataTypeBits( nHFADataType ) < 8 )
    {
        GDALMajorObject::SetMetadataItem( 
            "NBITS", 
            CPLString().Printf( "%d", HFAGetDataTypeBits( nHFADataType ) ), "IMAGE_STRUCTURE" );
    }

    if( nHFADataType == EPT_s8 )
    {
        GDALMajorObject::SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE", 
                                          "IMAGE_STRUCTURE" );
    }

/* -------------------------------------------------------------------- */
/*      If this is an overview, we need to fetch the actual size,       */
/*      and block size.                                                 */
/* -------------------------------------------------------------------- */
    if( iOverview > -1 )
    {
        int nHFADataTypeO;

        nOverviews = 0;
        HFAGetOverviewInfo( hHFA, nBand, iOverview,
                            &nRasterXSize, &nRasterYSize,
                            &nBlockXSize, &nBlockYSize, &nHFADataTypeO  );

/* -------------------------------------------------------------------- */
/*      If we are an 8bit overview of a 1bit layer, we need to mark     */
/*      ourselves as being "resample: average_bit2grayscale".           */
/* -------------------------------------------------------------------- */
        if( nHFADataType == EPT_u1 && nHFADataTypeO == EPT_u8 )
        {
            GDALMajorObject::SetMetadataItem( "RESAMPLING", 
                                              "AVERAGE_BIT2GRAYSCALE" );
            GDALMajorObject::SetMetadataItem( "NBITS", "8" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect color table if present.                                 */
/* -------------------------------------------------------------------- */
    double    *padfRed, *padfGreen, *padfBlue, *padfAlpha, *padfBins;
    int       nColors;

    if( iOverview == -1
        && HFAGetPCT( hHFA, nBand, &nColors,
                      &padfRed, &padfGreen, &padfBlue, &padfAlpha,
                      &padfBins ) == CE_None
        && nColors > 0 )
    {
        poCT = new GDALColorTable();
        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            GDALColorEntry   sEntry;

            // The following mapping assigns "equal sized" section of 
            // the [0...1] range to each possible output value and avoid
            // rounding issues for the "normal" values generated using n/255.
            // See bug #1732 for some discussion.
            sEntry.c1 = MIN(255,(short) (padfRed[iColor]   * 256));
            sEntry.c2 = MIN(255,(short) (padfGreen[iColor] * 256));
            sEntry.c3 = MIN(255,(short) (padfBlue[iColor]  * 256));
            sEntry.c4 = MIN(255,(short) (padfAlpha[iColor] * 256));
            
            if( padfBins != NULL )
                poCT->SetColorEntry( (int) padfBins[iColor], &sEntry );
            else
                poCT->SetColorEntry( iColor, &sEntry );
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

    const char ** pszAuxMetaData = GetHFAAuxMetaDataList();
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
              int nCount, iValue;
              double dfValue;
              CPLString osValueList;

              nCount = poEntry->GetFieldCount( pszFieldName, &eErr );
              for( iValue = 0; eErr == CE_None && iValue < nCount; iValue++ )
              {
                  CPLString osSubFieldName;
                  osSubFieldName.Printf( "%s[%d]", pszFieldName, iValue );
                  dfValue = poEntry->GetDoubleField( osSubFieldName, &eErr );
                  if( eErr != CE_None )
                      break;

                  char szValueAsString[100];
                  sprintf( szValueAsString, "%.14g", dfValue );

                  if( iValue > 0 )
                      osValueList += ",";
                  osValueList += szValueAsString;
              }
              if( eErr == CE_None )
                  SetMetadataItem( pszAuxMetaData[i+2], osValueList );
          }
          break;
          case 'i':
          case 'l':
          {
              int nValue, nCount, iValue;
              CPLString osValueList;

              nCount = poEntry->GetFieldCount( pszFieldName, &eErr );
              for( iValue = 0; eErr == CE_None && iValue < nCount; iValue++ )
              {
                  CPLString osSubFieldName;
                  osSubFieldName.Printf( "%s[%d]", pszFieldName, iValue );
                  nValue = poEntry->GetIntField( osSubFieldName, &eErr );
                  if( eErr != CE_None )
                      break;

                  char szValueAsString[100];
                  sprintf( szValueAsString, "%d", nValue );

                  if( iValue > 0 )
                      osValueList += ",";
                  osValueList += szValueAsString;
              }
              if( eErr == CE_None )
                  SetMetadataItem( pszAuxMetaData[i+2], osValueList );
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
}

/************************************************************************/
/*                       ReadHistogramMetadata()                        */
/************************************************************************/

void HFARasterBand::ReadHistogramMetadata()

{
    int i;
    HFABand *poBand = hHFA->papoBand[nBand-1];

    // only load metadata for full resolution layer.
    if( nThisOverview != -1 )
        return;

    HFAEntry *poEntry = 
        poBand->poNode->GetNamedChild( "Descriptor_Table.Histogram" );
    if ( poEntry == NULL )
        return;

    int nNumBins = poEntry->GetIntField( "numRows" );
    if (nNumBins < 0)
        return;

/* -------------------------------------------------------------------- */
/*      Fetch the histogram values.                                     */
/* -------------------------------------------------------------------- */

    int nOffset =  poEntry->GetIntField( "columnDataPtr" );
    const char * pszType =  poEntry->GetStringField( "dataType" );
    int nBinSize = 4;
        
    if( pszType != NULL && EQUALN( "real", pszType, 4 ) )
        nBinSize = 8;

    int *panHistValues = (int *) VSIMalloc2(sizeof(int), nNumBins);
    GByte  *pabyWorkBuf = (GByte *) VSIMalloc2(nBinSize, nNumBins);
    
    if (panHistValues == NULL || pabyWorkBuf == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate memory");
        VSIFree(panHistValues);
        VSIFree(pabyWorkBuf);
        return;
    }

    VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );

    if( (int)VSIFReadL( pabyWorkBuf, nBinSize, nNumBins, hHFA->fp ) != nNumBins)
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Cannot read histogram values." );
        CPLFree( panHistValues );
        CPLFree( pabyWorkBuf );
        return;
    }

    // Swap into local order.
    for( i = 0; i < nNumBins; i++ )
        HFAStandard( nBinSize, pabyWorkBuf + i*nBinSize );

    if( nBinSize == 8 ) // source is doubles
    {
        for( i = 0; i < nNumBins; i++ )
            panHistValues[i] = (int) ((double *) pabyWorkBuf)[i];
    }
    else // source is 32bit integers
    {
        memcpy( panHistValues, pabyWorkBuf, 4 * nNumBins );
    }

    CPLFree( pabyWorkBuf );
    pabyWorkBuf = NULL;

/* -------------------------------------------------------------------- */
/*      Do we have unique values for the bins?                          */
/* -------------------------------------------------------------------- */
    double *padfBinValues = NULL;
    HFAEntry *poBinEntry = poBand->poNode->GetNamedChild( "Descriptor_Table.#Bin_Function840#" );

    if( poBinEntry != NULL
        && EQUAL(poBinEntry->GetType(),"Edsc_BinFunction840") 
        && EQUAL(poBinEntry->GetStringField( "binFunction.type.string" ),
                 "BFUnique") )
        padfBinValues = HFAReadBFUniqueBins( poBinEntry, nNumBins );

    if( padfBinValues )
    {
        int nMaxValue = 0;
        int nMinValue = 1000000;
        int bAllInteger = TRUE;
        
        for( i = 0; i < nNumBins; i++ )
        {
            if( padfBinValues[i] != floor(padfBinValues[i]) )
                bAllInteger = FALSE;
            
            nMaxValue = MAX(nMaxValue,(int)padfBinValues[i]);
            nMinValue = MIN(nMinValue,(int)padfBinValues[i]);
        }
        
        if( nMinValue < 0 || nMaxValue > 1000 || !bAllInteger )
        {
            CPLFree( padfBinValues );
            CPLFree( panHistValues );
            CPLDebug( "HFA", "Unable to offer histogram because unique values list is not convenient to reform as HISTOBINVALUES." );
            return;
        }

        int nNewBins = nMaxValue + 1;
        int *panNewHistValues = (int *) CPLCalloc(sizeof(int),nNewBins);

        for( i = 0; i < nNumBins; i++ )
            panNewHistValues[(int) padfBinValues[i]] = panHistValues[i];

        CPLFree( panHistValues );
        panHistValues = panNewHistValues;
        nNumBins = nNewBins;

        SetMetadataItem( "STATISTICS_HISTOMIN", "0" );
        SetMetadataItem( "STATISTICS_HISTOMAX", 
                         CPLString().Printf("%d", nMaxValue ) );
        SetMetadataItem( "STATISTICS_HISTONUMBINS", 
                         CPLString().Printf("%d", nMaxValue+1 ) );

        CPLFree(padfBinValues);
        padfBinValues = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Format into HISTOBINVALUES text format.                         */
/* -------------------------------------------------------------------- */
    unsigned int nBufSize = 1024;
    char * pszBinValues = (char *)CPLMalloc( nBufSize );
    int    nBinValuesLen = 0;
    pszBinValues[0] = 0;

    for ( int nBin = 0; nBin < nNumBins; ++nBin )
    {
        char szBuf[32];
        snprintf( szBuf, 31, "%d", panHistValues[nBin] );
        if ( ( nBinValuesLen + strlen( szBuf ) + 2 ) > nBufSize )
        {
            nBufSize *= 2;
            char* pszNewBinValues = (char *)VSIRealloc( pszBinValues, nBufSize );
            if (pszNewBinValues == NULL)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate memory");
                break;
            }
            pszBinValues = pszNewBinValues;
        }
        strcat( pszBinValues+nBinValuesLen, szBuf );
        strcat( pszBinValues+nBinValuesLen, "|" );
        nBinValuesLen += strlen(pszBinValues+nBinValuesLen);
    }

    SetMetadataItem( "STATISTICS_HISTOBINVALUES", pszBinValues );
    CPLFree( panHistValues );
    CPLFree( pszBinValues );
}

/************************************************************************/
/*                             GetNoDataValue()                         */
/************************************************************************/

double HFARasterBand::GetNoDataValue( int *pbSuccess )

{
    double dfNoData;

    if( HFAGetBandNoData( hHFA, nBand, &dfNoData ) )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return dfNoData;
    }
    else
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/ 
/*                             SetNoDataValue()                         */ 
/************************************************************************/ 

CPLErr HFARasterBand::SetNoDataValue( double dfValue ) 
{ 
    return HFASetBandNoData( hHFA, nBand, dfValue ); 
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double HFARasterBand::GetMinimum( int *pbSuccess )

{
    const char *pszValue = GetMetadataItem( "STATISTICS_MINIMUM" );
    
    if( pszValue != NULL )
    {
        if( pbSuccess )
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
        if( pbSuccess )
            *pbSuccess = TRUE;
        return atof(pszValue);
    }
    else
    {
        return GDALRasterBand::GetMaximum( pbSuccess );
    }
}

/************************************************************************/
/*                         EstablishOverviews()                         */
/*                                                                      */
/*      Delayed population of overview information.                     */
/************************************************************************/

void HFARasterBand::EstablishOverviews()

{
    if( nOverviews != -1 )
        return;

    nOverviews = HFAGetOverviewCount( hHFA, nBand );
    if( nOverviews > 0 )
    {
        papoOverviewBands = (HFARasterBand **)
            CPLMalloc(sizeof(void*)*nOverviews);

        for( int iOvIndex = 0; iOvIndex < nOverviews; iOvIndex++ )
        {
            papoOverviewBands[iOvIndex] =
                new HFARasterBand( (HFADataset *) poDS, nBand, iOvIndex );
        }
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int HFARasterBand::GetOverviewCount()

{
    EstablishOverviews();

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
    EstablishOverviews();

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
        eErr = HFAGetRasterBlockEx( hHFA, nBand, nBlockXOff, nBlockYOff,
                                    pImage,
                                    nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType) / 8) );
    else
    {
        eErr =  HFAGetOverviewRasterBlockEx( hHFA, nBand, nThisOverview,
                                           nBlockXOff, nBlockYOff,
                                           pImage,
                                           nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType) / 8));
        nThisDataType = 
            hHFA->papoBand[nBand-1]->papoOverviews[nThisOverview]->nDataType;
    }

    if( eErr == CE_None && nThisDataType == EPT_u4 )
    {
        GByte	*pabyData = (GByte *) pImage;

        for( int ii = nBlockXSize * nBlockYSize - 2; ii >= 0; ii -= 2 )
        {
            int k = ii>>1;
            pabyData[ii+1] = (pabyData[k]>>4) & 0xf;
            pabyData[ii]   = (pabyData[k]) & 0xf;
        }
    }
    if( eErr == CE_None && nThisDataType == EPT_u2 )
    {
        GByte	*pabyData = (GByte *) pImage;

        for( int ii = nBlockXSize * nBlockYSize - 4; ii >= 0; ii -= 4 )
        {
            int k = ii>>2;
            pabyData[ii+3] = (pabyData[k]>>6) & 0x3;
            pabyData[ii+2] = (pabyData[k]>>4) & 0x3;
            pabyData[ii+1] = (pabyData[k]>>2) & 0x3;
            pabyData[ii]   = (pabyData[k]) & 0x3;
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
    GByte *pabyOutBuf = (GByte *) pImage;

/* -------------------------------------------------------------------- */
/*      Do we need to pack 1/2/4 bit data?                              */
/* -------------------------------------------------------------------- */
    if( nHFADataType == EPT_u1 
        || nHFADataType == EPT_u2
        || nHFADataType == EPT_u4 )
    {
        int nPixCount =  nBlockXSize * nBlockYSize;
        pabyOutBuf = (GByte *) VSIMalloc2(nBlockXSize, nBlockYSize);
        if (pabyOutBuf == NULL)
            return CE_Failure;

        if( nHFADataType == EPT_u1 )
        {
            for( int ii = 0; ii < nPixCount - 7; ii += 8 )
            {
                int k = ii>>3;
                pabyOutBuf[k] = 
                    (((GByte *) pImage)[ii] & 0x1)
                    | ((((GByte *) pImage)[ii+1]&0x1) << 1)
                    | ((((GByte *) pImage)[ii+2]&0x1) << 2)
                    | ((((GByte *) pImage)[ii+3]&0x1) << 3)
                    | ((((GByte *) pImage)[ii+4]&0x1) << 4)
                    | ((((GByte *) pImage)[ii+5]&0x1) << 5)
                    | ((((GByte *) pImage)[ii+6]&0x1) << 6)
                    | ((((GByte *) pImage)[ii+7]&0x1) << 7);
            }
        }
        else if( nHFADataType == EPT_u2 )
        {
            for( int ii = 0; ii < nPixCount - 3; ii += 4 )
            {
                int k = ii>>2;
                pabyOutBuf[k] = 
                    (((GByte *) pImage)[ii] & 0x3)
                    | ((((GByte *) pImage)[ii+1]&0x3) << 2)
                    | ((((GByte *) pImage)[ii+2]&0x3) << 4)
                    | ((((GByte *) pImage)[ii+3]&0x3) << 6);
            }
        }
        else if( nHFADataType == EPT_u4 )
        {
            for( int ii = 0; ii < nPixCount - 1; ii += 2 )
            {
                int k = ii>>1;
                pabyOutBuf[k] = 
                    (((GByte *) pImage)[ii] & 0xf) 
                    | ((((GByte *) pImage)[ii+1]&0xf) << 4);
            }
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Actually write out.                                             */
/* -------------------------------------------------------------------- */
    CPLErr nRetCode;

    if( nThisOverview == -1 )
        nRetCode = HFASetRasterBlock( hHFA, nBand, nBlockXOff, nBlockYOff,
                                      pabyOutBuf );
    else
        nRetCode = HFASetOverviewRasterBlock( hHFA, nBand, nThisOverview,
                                              nBlockXOff, nBlockYOff,
                                              pabyOutBuf );

    if( pabyOutBuf != pImage  )
        CPLFree( pabyOutBuf );

    return nRetCode;
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
    if( strlen(pszName) > 0 )
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
    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess, 
                  "Unable to set color table on read-only file." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Special case if we are clearing the color table.                */
/* -------------------------------------------------------------------- */
    if( poCTable == NULL )
    {
        delete poCT;
        poCT = NULL;

        HFASetPCT( hHFA, nBand, 0, NULL, NULL, NULL, NULL );

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Write out the colortable, and update the configuration.         */
/* -------------------------------------------------------------------- */
    int nColors = poCTable->GetColorEntryCount();

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
/*                           CleanOverviews()                           */
/************************************************************************/

CPLErr HFARasterBand::CleanOverviews()

{
    if( nOverviews == 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Clear our reference to overviews as bands.                      */
/* -------------------------------------------------------------------- */
    int iOverview;

    for( iOverview = 0; iOverview < nOverviews; iOverview++ )
        delete papoOverviewBands[iOverview];

    CPLFree( papoOverviewBands );
    papoOverviewBands = NULL;
    nOverviews = 0;

/* -------------------------------------------------------------------- */
/*      Search for any RRDNamesList and destroy it.                     */
/* -------------------------------------------------------------------- */
    HFABand *poBand = hHFA->papoBand[nBand-1];
    HFAEntry *poEntry = poBand->poNode->GetNamedChild( "RRDNamesList" );
    if( poEntry != NULL )
    {
        poEntry->RemoveAndDestroy();
    }

/* -------------------------------------------------------------------- */
/*      Destroy and subsample layers under our band.                    */
/* -------------------------------------------------------------------- */
    HFAEntry *poChild;
    for( poChild = poBand->poNode->GetChild(); 
         poChild != NULL; ) 
    {
        HFAEntry *poNext = poChild->GetNext();

        if( EQUAL(poChild->GetType(),"Eimg_Layer_SubSample") )
            poChild->RemoveAndDestroy();

        poChild = poNext;
    }

/* -------------------------------------------------------------------- */
/*      Clean up dependent file if we are the last band under the       */
/*      assumption there will be nothing else referencing it after      */
/*      this.                                                           */
/* -------------------------------------------------------------------- */
    if( hHFA->psDependent != hHFA && hHFA->psDependent != NULL )
    {
        CPLString osFilename = 
            CPLFormFilename( hHFA->psDependent->pszPath, 
                             hHFA->psDependent->pszFilename, NULL );
        
        HFAClose( hHFA->psDependent );
        hHFA->psDependent = NULL;
        
        CPLDebug( "HFA", "Unlink(%s)", osFilename.c_str() );
        VSIUnlink( osFilename );
    }

    return CE_None;
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

    EstablishOverviews();
    
    if( nThisOverview != -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to build overviews on an overview layer." );

        return CE_Failure;
    }

    if( nReqOverviews == 0 )
        return CleanOverviews();

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
        eErr = GDALRegenerateOverviews( (GDALRasterBandH) this, 
                                        nReqOverviews, 
                                        (GDALRasterBandH *) papoOvBands,
                                        pszResampling, 
                                        pfnProgress, pProgressData );
    
    CPLFree( papoOvBands );
    
    return eErr;
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
        && GetMetadataItem( "STATISTICS_HISTOMIN" ) != NULL 
        && GetMetadataItem( "STATISTICS_HISTOMAX" ) != NULL )
    {
        int i;
        const char *pszNextBin;
        const char *pszBinValues = 
            GetMetadataItem( "STATISTICS_HISTOBINVALUES" );

        *pdfMin = atof(GetMetadataItem("STATISTICS_HISTOMIN"));
        *pdfMax = atof(GetMetadataItem("STATISTICS_HISTOMAX"));

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
        int i;

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

        if( EQUAL(poDTChild->GetType(),"Edsc_BinFunction840") 
            && EQUAL(poDTChild->GetStringField( "binFunction.type.string" ),
                     "BFUnique") )
        {
            double *padfBinValues = HFAReadBFUniqueBins( poDTChild, nRowCount );
            
            if( padfBinValues != NULL )
            {
                poRAT->CreateColumn( "Value", GFT_Integer, GFU_MinMax );
                for( i = 0; i < nRowCount; i++ )
                    poRAT->SetValue( i, poRAT->GetColumnCount()-1, 
                                     padfBinValues[i] );

                CPLFree( padfBinValues );
            }
        }

        if( !EQUAL(poDTChild->GetType(),"Edsc_Column") )
            continue;

        int nOffset = poDTChild->GetIntField( "columnDataPtr" );
        const char * pszType = poDTChild->GetStringField( "dataType" );
        GDALRATFieldUsage eType = GFU_Generic;

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
            double *padfColData = (double*)VSIMalloc2(nRowCount, sizeof(double));
            if (nRowCount != 0 && padfColData == NULL)
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                 "HFARasterBand::ReadNamedRAT : Out of memory");
                delete poRAT;
                return NULL;
            }

            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            if ((int)VSIFReadL( padfColData, sizeof(double), nRowCount, hHFA->fp ) != nRowCount)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                            "HFARasterBand::ReadNamedRAT : Cannot read values");
                CPLFree(padfColData);
                delete poRAT;
                return NULL;
            }
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
            char *pachColData = (char*)VSICalloc(nRowCount+1,nMaxNumChars);
            if (pachColData == NULL)
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                 "HFARasterBand::ReadNamedRAT : Out of memory");
                delete poRAT;
                return NULL;
            }

            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            if ((int)VSIFReadL( pachColData, nMaxNumChars, nRowCount, hHFA->fp ) != nRowCount)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                            "HFARasterBand::ReadNamedRAT : Cannot read values");
                CPLFree(pachColData);
                delete poRAT;
                return NULL;
            }

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
            GInt32 *panColData = (GInt32*)VSIMalloc2(nRowCount, sizeof(GInt32));
            if (nRowCount != 0 && panColData == NULL)
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                 "HFARasterBand::ReadNamedRAT : Out of memory");
                delete poRAT;
                return NULL;
            }

            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            if ((int)VSIFReadL( panColData, sizeof(GInt32), nRowCount, hHFA->fp ) != nRowCount)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                            "HFARasterBand::ReadNamedRAT : Cannot read values");
                CPLFree(panColData);
                delete poRAT;
                return NULL;
            }
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
    bMetadataDirty = FALSE;
    bIgnoreUTM = FALSE;
    bForceToPEString = FALSE;

    nGCPCount = 0;
}

/************************************************************************/
/*                           ~HFADataset()                            */
/************************************************************************/

HFADataset::~HFADataset()

{
    FlushCache();

/* -------------------------------------------------------------------- */
/*      Destroy the raster bands if they exist.  We forcably clean      */
/*      them up now to avoid any effort to write to them after the      */
/*      file is closed.                                                 */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; i < nBands && papoBands != NULL; i++ )
    {
        if( papoBands[i] != NULL )
            delete papoBands[i];
    }

    CPLFree( papoBands );
    papoBands = NULL;

/* -------------------------------------------------------------------- */
/*      Close the file                                                  */
/* -------------------------------------------------------------------- */
    if( hHFA != NULL )
    {
        HFAClose( hHFA );
        hHFA = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pszProjection );
    if( nGCPCount > 0 )
        GDALDeinitGCPs( 36, asGCPList );
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

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, asGCPList );
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
    OGRBoolean peStrStored = FALSE;

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

        /* Map some EPSG datum codes directly to Imagine names */
        int nGCS = poGeogSRS->GetEPSGGeogCS();

        if( nGCS == 4326 )
            sDatum.datumname = (char*) "WGS 84";
        if( nGCS == 4322 )
            sDatum.datumname = (char*) "WGS 1972";
        if( nGCS == 4267 )
            sDatum.datumname = (char*) "NAD27";
        if( nGCS == 4269 )
            sDatum.datumname = (char*) "NAD83";
            
        if( poGeogSRS->GetTOWGS84( sDatum.params ) == OGRERR_NONE )
            sDatum.type = EPRJ_DATUM_PARAMETRIC;
        else if( EQUAL(sDatum.datumname,"NAD27") )
        {
            sDatum.type = EPRJ_DATUM_GRID;
            sDatum.gridname = (char*) "nadcon.dat";
        }
        else
        {
            /* we will default to this (effectively WGS84) for now */
            sDatum.type = EPRJ_DATUM_PARAMETRIC;
        }

        /* Verify if we need to write a ESRI PE string */
        peStrStored = WritePeStringIfNeeded(&oSRS, hHFA);

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

    if( bForceToPEString )
    {
        char *pszPEString = NULL;
        oSRS.morphToESRI();
        oSRS.exportToWkt( &pszPEString );
        // need to transform this into ESRI format.
        HFASetPEString( hHFA, pszPEString );
        CPLFree( pszPEString );
    }
    else if( pszProjName == NULL )
    {
        if( bHaveSRS && oSRS.IsGeographic() )
        {
            sPro.proNumber = EPRJ_LATLONG;
            sPro.proName = (char*) "Geographic (Lat/Lon)";
        }
    }

    /* FIXME/NOTDEF/TODO: Add State Plane */
    else if( !bIgnoreUTM && oSRS.GetUTMZone( NULL ) != 0 )
    {
        int	bNorth, nZone;

        nZone = oSRS.GetUTMZone( &bNorth );
        sPro.proNumber = EPRJ_UTM;
        sPro.proName = (char*) "UTM";
        sPro.proZone = nZone;
        if( bNorth )
            sPro.proParams[3] = 1.0;
        else
            sPro.proParams[3] = -1.0;
    }

    else if( EQUAL(pszProjName,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        sPro.proNumber = EPRJ_ALBERS_CONIC_EQUAL_AREA;
        sPro.proName = (char*) "Albers Conical Equal Area";
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
        sPro.proName = (char*) "Lambert Conformal Conic";
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
        sPro.proName = (char*) "Mercator";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        /* hopefully the scale factor is 1.0! */
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_POLAR_STEREOGRAPHIC;
        sPro.proName = (char*) "Polar Stereographic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        /* hopefully the scale factor is 1.0! */
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_POLYCONIC) )
    {
        sPro.proNumber = EPRJ_POLYCONIC;
        sPro.proName = (char*) "Polyconic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_EQUIDISTANT_CONIC) )
    {
        sPro.proNumber = EPRJ_EQUIDISTANT_CONIC;
        sPro.proName = (char*) "Equidistant Conic";
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
        sPro.proName = (char*) "Transverse Mercator";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_STEREOGRAPHIC_EXTENDED;
        sPro.proName = (char*) "Stereographic (Extended)";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        sPro.proNumber = EPRJ_LAMBERT_AZIMUTHAL_EQUAL_AREA;
        sPro.proName = (char*) "Lambert Azimuthal Equal-area";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        sPro.proNumber = EPRJ_AZIMUTHAL_EQUIDISTANT;
        sPro.proName = (char*) "Azimuthal Equidistant";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_GNOMONIC) )
    {
        sPro.proNumber = EPRJ_GNOMONIC;
        sPro.proName = (char*) "Gnomonic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ORTHOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_ORTHOGRAPHIC;
        sPro.proName = (char*) "Orthographic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_SINUSOIDAL) )
    {
        sPro.proNumber = EPRJ_SINUSOIDAL;
        sPro.proName = (char*) "Sinusoidal";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_EQUIRECTANGULAR) )
    {
        sPro.proNumber = EPRJ_EQUIRECTANGULAR;
        sPro.proName = (char*) "Equirectangular";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_MILLER_CYLINDRICAL) )
    {
        sPro.proNumber = EPRJ_MILLER_CYLINDRICAL;
        sPro.proName = (char*) "Miller Cylindrical";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        /* hopefully the latitude is zero! */
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_VANDERGRINTEN) )
    {
        sPro.proNumber = EPRJ_VANDERGRINTEN;
        sPro.proName = (char*) "Van der Grinten";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR;
        sPro.proName = (char*) "Oblique Mercator (Hotine)";
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
        sPro.proName = (char*) "Robinson";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_MOLLWEIDE) )
    {
        sPro.proNumber = EPRJ_MOLLWEIDE;
        sPro.proName = (char*) "Mollweide";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ECKERT_I) )
    {
        sPro.proNumber = EPRJ_ECKERT_I;
        sPro.proName = (char*) "Eckert I";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ECKERT_II) )
    {
        sPro.proNumber = EPRJ_ECKERT_II;
        sPro.proName = (char*) "Eckert II";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ECKERT_III) )
    {
        sPro.proNumber = EPRJ_ECKERT_III;
        sPro.proName = (char*) "Eckert III";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ECKERT_IV) )
    {
        sPro.proNumber = EPRJ_ECKERT_IV;
        sPro.proName = (char*) "Eckert IV";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ECKERT_V) )
    {
        sPro.proNumber = EPRJ_ECKERT_V;
        sPro.proName = (char*) "Eckert V";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_ECKERT_VI) )
    {
        sPro.proNumber = EPRJ_ECKERT_VI;
        sPro.proName = (char*) "Eckert VI";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_GALL_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_GALL_STEREOGRAPHIC;
        sPro.proName = (char*) "Gall Stereographic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_CASSINI_SOLDNER) )
    {
        sPro.proNumber = EPRJ_CASSINI;
        sPro.proName = (char*) "Cassini";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_BONNE) )
    {
        sPro.proNumber = EPRJ_BONNE;
        sPro.proName = (char*) "Bonne";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,"Loximuthal") )
    {
        sPro.proNumber = EPRJ_LOXIMUTHAL;
        sPro.proName = (char*) "Loximuthal";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm("central_parallel")*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,"Quartic_Authalic") )
    {
        sPro.proNumber = EPRJ_QUARTIC_AUTHALIC;
        sPro.proName = (char*) "Quartic Authalic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,"Winkel_I") )
    {
        sPro.proNumber = EPRJ_WINKEL_I;
        sPro.proName = (char*) "Winkel I";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,"Winkel_II") )
    {
        sPro.proNumber = EPRJ_WINKEL_II;
        sPro.proName = (char*) "Winkel II";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,"Winkel_II") )
    {
        sPro.proNumber = EPRJ_WINKEL_II;
        sPro.proName = (char*) "Winkel II";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,"Behrmann") )
    {
        sPro.proNumber = EPRJ_BEHRMANN;
        sPro.proName = (char*) "Behrmann";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,"New_Zealand_Map_Grid") )
    {
        sPro.proType = EPRJ_EXTERNAL;
        sPro.proNumber = 0;
        sPro.proExeName = (char*) EPRJ_EXTERNAL_NZMG;
        sPro.proName = (char*) "New Zealand Map Grid";
        sPro.proZone = 0;
        sPro.proParams[0] = 0;  // false easting etc not stored in .img it seems 
        sPro.proParams[1] = 0;  // always fixed by definition. 
        sPro.proParams[2] = 0;
        sPro.proParams[3] = 0;
        sPro.proParams[4] = 0;
        sPro.proParams[5] = 0;
        sPro.proParams[6] = 0;
        sPro.proParams[7] = 0;
    }
    // Anything we can't map, we store as an ESRI PE_STRING 
    else if( oSRS.IsProjected() || oSRS.IsGeographic() )
    {
      if(!peStrStored)
      {
        char *pszPEString = NULL;
        oSRS.morphToESRI();
        oSRS.exportToWkt( &pszPEString );
        // need to transform this into ESRI format.
        HFASetPEString( hHFA, pszPEString );
        CPLFree( pszPEString );
        peStrStored = TRUE;
      }
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
    const char *pszPROJCS = oSRS.GetAttrValue( "PROJCS" );

    if( pszPROJCS )
        sMapInfo.proName = (char *) pszPROJCS;
    else if( bHaveSRS && sPro.proName != NULL )
        sMapInfo.proName = sPro.proName;
    else
        sMapInfo.proName = (char*) "Unknown";

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
    sMapInfo.units = (char*) "meters";

    if( bHaveSRS && oSRS.IsGeographic() )
        sMapInfo.units = (char*) "dd";
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
    if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 )
    {
        HFASetMapInfo( hHFA, &sMapInfo );
    }
    else
    {
        HFASetGeoTransform( hHFA, 
                            sMapInfo.proName, sMapInfo.units,
                            adfGeoTransform );
    }

    if( bHaveSRS && sPro.proName != NULL)
    {
        HFASetProParameters( hHFA, &sPro );
        HFASetDatum( hHFA, &sDatum );
    }
    else if( !peStrStored )
      ClearSR(hHFA);

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( poGeogSRS != NULL )
        delete poGeogSRS;

    return CE_None;
}

/************************************************************************/
/*                       WritePeStringIfNeeded()                        */
/************************************************************************/
OGRBoolean WritePeStringIfNeeded(OGRSpatialReference*	poSRS, HFAHandle hHFA)
{
  OGRBoolean ret = FALSE;
  if(!poSRS || !hHFA)
    return ret;

  const char *pszGEOGCS = poSRS->GetAttrValue( "GEOGCS" );
  const char *pszDatum = poSRS->GetAttrValue( "DATUM" );
  int gcsNameOffset = 0;
  int datumNameOffset = 0;
  if(strstr(pszGEOGCS, "GCS_"))
    gcsNameOffset = strlen("GCS_");
  if(strstr(pszDatum, "D_"))
    datumNameOffset = strlen("D_");

  if(!EQUAL(pszGEOGCS+gcsNameOffset, pszDatum+datumNameOffset))
    ret = TRUE;
  else
  {
    const char* name = poSRS->GetAttrValue("PRIMEM");
    if(name && !EQUAL(name,"Greenwich"))
      ret = TRUE;
    if(!ret)
    {
      OGR_SRSNode * poAUnits = poSRS->GetAttrNode( "GEOGCS|UNIT" );
      name = poAUnits->GetChild(0)->GetValue();
      if(name && !EQUAL(name,"Degree"))
        ret = TRUE;
    }
    if(!ret)
    {
      name = poSRS->GetAttrValue("UNIT");
      if(name)
      {
        ret = TRUE;
        for(int i=0; apszUnitMap[i] != NULL; i+=2)
          if(EQUAL(name, apszUnitMap[i]))
            ret = FALSE;
      }
    }
    if(!ret)
    {
        int nGCS = poSRS->GetEPSGGeogCS();
        switch(nGCS)
        {
          case 4326:
            if(!EQUAL(pszDatum+datumNameOffset, "WGS_84"))
              ret = TRUE;
            break;
          case 4322:
            if(!EQUAL(pszDatum+datumNameOffset, "WGS_72"))
              ret = TRUE;
            break;
          case 4267:
            if(!EQUAL(pszDatum+datumNameOffset, "North_America_1927"))
              ret = TRUE;
            break;
          case 4269:
            if(!EQUAL(pszDatum+datumNameOffset, "North_America_1983"))
              ret = TRUE;
            break;
        }
    }
  }
  if(ret)
  {
    char *pszPEString = NULL;
    poSRS->morphToESRI();
    poSRS->exportToWkt( &pszPEString );
    HFASetPEString( hHFA, pszPEString );
    CPLFree( pszPEString );
  }

  return ret;
}

/************************************************************************/
/*                              ClearSR()                               */
/************************************************************************/
void ClearSR(HFAHandle hHFA)
{
    for( int iBand = 0; iBand < hHFA->nBands; iBand++ )
    {
        HFAEntry	*poMIEntry;
        if( hHFA->papoBand[iBand]->poNode && (poMIEntry = hHFA->papoBand[iBand]->poNode->GetNamedChild("Projection")) != NULL )
        {
            poMIEntry->MarkDirty();
            poMIEntry->SetIntField( "proType", 0 );
            poMIEntry->SetIntField( "proNumber", 0 );
            poMIEntry->SetStringField( "proExeName", "");
            poMIEntry->SetStringField( "proName", "");
            poMIEntry->SetIntField( "proZone", 0 );
            poMIEntry->SetDoubleField( "proParams[0]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[1]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[2]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[3]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[4]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[5]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[6]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[7]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[8]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[9]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[10]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[11]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[12]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[13]", 0.0 );
            poMIEntry->SetDoubleField( "proParams[14]", 0.0 );
            poMIEntry->SetStringField( "proSpheroid.sphereName", "" );
            poMIEntry->SetDoubleField( "proSpheroid.a", 0.0 );
            poMIEntry->SetDoubleField( "proSpheroid.b", 0.0 );
            poMIEntry->SetDoubleField( "proSpheroid.eSquared", 0.0 );
            poMIEntry->SetDoubleField( "proSpheroid.radius", 0.0 );
            HFAEntry* poDatumEntry = poMIEntry->GetNamedChild("Datum");
            if( poDatumEntry != NULL )
            {
                poDatumEntry->MarkDirty();
                poDatumEntry->SetStringField( "datumname", "" );
                poDatumEntry->SetIntField( "type", 0 );
                poDatumEntry->SetDoubleField( "params[0]", 0.0 );
                poDatumEntry->SetDoubleField( "params[1]", 0.0 );
                poDatumEntry->SetDoubleField( "params[2]", 0.0 );
                poDatumEntry->SetDoubleField( "params[3]", 0.0 );
                poDatumEntry->SetDoubleField( "params[4]", 0.0 );
                poDatumEntry->SetDoubleField( "params[5]", 0.0 );
                poDatumEntry->SetDoubleField( "params[6]", 0.0 );
                poDatumEntry->SetStringField( "gridname", "" );
            }
            poMIEntry->FlushToDisk();
            char* peStr = HFAGetPEString( hHFA );
            if( peStr != NULL && strlen(peStr) > 0 )           
                HFASetPEString( hHFA, "" );
        }  
    }
    return;
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
/*                           PCSStructToWKT()                           */
/*                                                                      */
/*      Convert the datum, proparameters and mapinfo structures into    */
/*      WKT format.                                                     */
/************************************************************************/

char *
HFAPCSStructToWKT( const Eprj_Datum *psDatum,
                   const Eprj_ProParameters *psPro,
                   const Eprj_MapInfo *psMapInfo,
                   HFAEntry *poMapInformation )

{
    OGRSpatialReference oSRS;
    char *pszNewProj = NULL;

/* -------------------------------------------------------------------- */
/*      General case for Erdas style projections.                       */
/*                                                                      */
/*      We make a particular effort to adapt the mapinfo->proname as    */
/*      the PROJCS[] name per #2422.                                    */
/* -------------------------------------------------------------------- */

    if( psPro == NULL && psMapInfo != NULL )
    {
        oSRS.SetLocalCS( psMapInfo->proName );
    }

    else if( psPro == NULL )
    {
        return NULL;
    }

    else if( psPro->proType == EPRJ_EXTERNAL )
    {
        if( EQUALN(psPro->proExeName,EPRJ_EXTERNAL_NZMG,4) )
        {
            /* -------------------------------------------------------------------- */
            /*         handle NZMG which is an external projection see              */
            /*         http://www.linz.govt.nz/core/surveysystem/geodeticinfo\      */
            /*                /datums-projections/projections/nzmg/index.html       */
            /* -------------------------------------------------------------------- */
            /* Is there a better way that doesn't require hardcoding of these numbers? */
            oSRS.SetNZMG(-41.0,173.0,2510000,6023150);
        }
        else
        {
            oSRS.SetLocalCS( psPro->proName );
        }
    }

    else if( psPro->proNumber != EPRJ_LATLONG
             && psMapInfo != NULL )
    {
        oSRS.SetProjCS( psMapInfo->proName );
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
        const char  *pszUnits = NULL;

        if( psMapInfo )
            pszUnits = psMapInfo->units; 
        else if( poMapInformation != NULL )
            pszUnits = poMapInformation->GetStringField( "units.string" );
        
        if( pszUnits != NULL )
        {
            for( iUnitIndex = 0; 
                 apszUnitMap[iUnitIndex] != NULL; 
                 iUnitIndex += 2 )
            {
                if( EQUAL(apszUnitMap[iUnitIndex], pszUnits ) )
                    break;
            }
            
            if( apszUnitMap[iUnitIndex] == NULL )
                iUnitIndex = 0;
            
            oSRS.SetLinearUnits( pszUnits, 
                                 atof(apszUnitMap[iUnitIndex+1]) );
        }
        else
            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
    }

    if( psPro == NULL )
    {
        if( oSRS.IsLocal() )
        {
            if( oSRS.exportToWkt( &pszNewProj ) == OGRERR_NONE )
                return pszNewProj;
            else
            {
                pszNewProj = NULL;
                return NULL;
            }
        }
        else
            return NULL;
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
        for( i = 0; pszDatumName != NULL && apszDatumMap[i] != NULL; i += 2 )
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

      case EPRJ_PLATE_CARREE:
      case EPRJ_EQUIRECTANGULAR:
        oSRS.SetEquirectangular2( 0.0, 
                                  psPro->proParams[4]*R2D,
                                  psPro->proParams[5]*R2D, 
                                  psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_EQUIDISTANT_CYLINDRICAL:
        oSRS.SetEquirectangular2( 0.0,
                                  psPro->proParams[4]*R2D,
                                  psPro->proParams[2]*R2D,
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

      case EPRJ_GALL_STEREOGRAPHIC:
        oSRS.SetGS( psPro->proParams[4]*R2D,
                    psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ECKERT_I:
        oSRS.SetEckert( 1, psPro->proParams[4]*R2D,
                        psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ECKERT_II:
        oSRS.SetEckert( 2, psPro->proParams[4]*R2D,
                        psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ECKERT_III:
        oSRS.SetEckert( 3, psPro->proParams[4]*R2D,
                        psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ECKERT_IV:
        oSRS.SetEckert( 4, psPro->proParams[4]*R2D,
                        psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ECKERT_V:
        oSRS.SetEckert( 5, psPro->proParams[4]*R2D,
                        psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_ECKERT_VI:
        oSRS.SetEckert( 6, psPro->proParams[4]*R2D,
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

      case EPRJ_BONNE:
        oSRS.SetBonne( psPro->proParams[2]*R2D, psPro->proParams[4]*R2D,
                       psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_LOXIMUTHAL:
      {
          oSRS.SetProjection( "Loximuthal" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( "central_parallel", 
                           psPro->proParams[5] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[6] );
      }
      break;

      case EPRJ_QUARTIC_AUTHALIC:
      {
          oSRS.SetProjection( "Quartic_Authalic" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[6] );
      }
      break;

      case EPRJ_WINKEL_I:
      {
          oSRS.SetProjection( "Winkel_I" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 
                           psPro->proParams[2] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[6] );
      }
      break;

      case EPRJ_WINKEL_II:
      {
          oSRS.SetProjection( "Winkel_II" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 
                           psPro->proParams[2] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[6] );
      }
      break;

      case EPRJ_BEHRMANN:
      {
          oSRS.SetProjection( "Behrmann" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[6] );
      }
      break;

      default:
        if( oSRS.IsProjected() )
            oSRS.GetRoot()->SetValue( "LOCAL_CS" );
        else
            oSRS.SetLocalCS( psPro->proName );
        break;
    }

/* -------------------------------------------------------------------- */
/*      Try and set the GeogCS information.                             */
/* -------------------------------------------------------------------- */
    if( oSRS.GetAttrNode("GEOGCS") == NULL
        && oSRS.GetAttrNode("LOCAL_CS") == NULL )
    {
        if( pszDatumName == NULL)
            oSRS.SetGeogCS( pszDatumName, pszDatumName, pszEllipsoidName,
                            psPro->proSpheroid.a, dfInvFlattening );
        else if( EQUAL(pszDatumName,"WGS 84") 
            ||  EQUAL(pszDatumName,"WGS_1984") )
            oSRS.SetWellKnownGeogCS( "WGS84" );
        else if( strstr(pszDatumName,"NAD27") != NULL 
                 || EQUAL(pszDatumName,"North_American_Datum_1927") )
            oSRS.SetWellKnownGeogCS( "NAD27" );
        else if( strstr(pszDatumName,"NAD83") != NULL
                 || EQUAL(pszDatumName,"North_American_Datum_1983"))
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
    if( oSRS.exportToWkt( &pszNewProj ) == OGRERR_NONE )
        return pszNewProj;
    else
    {
        return NULL;
    }
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
    char *pszPE_COORDSYS;

/* -------------------------------------------------------------------- */
/*      Special logic for PE string in ProjectionX node.                */
/* -------------------------------------------------------------------- */
    pszPE_COORDSYS = HFAGetPEString( hHFA );
    if( pszPE_COORDSYS != NULL
        && oSRS.SetFromUserInput( pszPE_COORDSYS ) == OGRERR_NONE )
    {
        CPLFree( pszPE_COORDSYS );

        oSRS.morphFromESRI();
        oSRS.Fixup();

        CPLFree( pszProjection );
        pszProjection = NULL;
        oSRS.exportToWkt( &pszProjection );
        
        return CE_None;
    }
    
/* -------------------------------------------------------------------- */
/*      General case for Erdas style projections.                       */
/*                                                                      */
/*      We make a particular effort to adapt the mapinfo->proname as    */
/*      the PROJCS[] name per #2422.                                    */
/* -------------------------------------------------------------------- */
    psDatum = HFAGetDatum( hHFA );
    psPro = HFAGetProParameters( hHFA );
    psMapInfo = HFAGetMapInfo( hHFA );

    HFAEntry *poMapInformation = NULL;
    if( psMapInfo == NULL ) 
    {
        HFABand *poBand = hHFA->papoBand[0];
        poMapInformation = poBand->poNode->GetNamedChild("MapInformation");
    }

    CPLFree( pszProjection );

    if( !psDatum || !psPro ||
        (psMapInfo == NULL && poMapInformation == NULL) ||
        ((strlen(psDatum->datumname) == 0 || EQUAL(psDatum->datumname, "Unknown")) && 
        (strlen(psPro->proName) == 0 || EQUAL(psPro->proName, "Unknown")) &&
        (psMapInfo && (strlen(psMapInfo->proName) == 0 || EQUAL(psMapInfo->proName, "Unknown"))) && 
        psPro->proZone == 0) )
    {
        pszProjection = CPLStrdup("");
        return CE_None;
    }

    pszProjection = HFAPCSStructToWKT( psDatum, psPro, psMapInfo, 
                                       poMapInformation );

    if( pszProjection != NULL )
        return CE_None;
    else
    {
        pszProjection = CPLStrdup("");
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

        void* pScaledProgressData = GDALCreateScaledProgress(
                i * 1.0 / nListBands, (i + 1) * 1.0 / nListBands,
                pfnProgress, pProgressData);

        poBand = GetRasterBand( panBandList[i] );
        eErr = 
            poBand->BuildOverviews( pszResampling, nOverviews, panOverviewList,
                                    GDALScaledProgress, pScaledProgressData );

        GDALDestroyScaledProgress(pScaledProgressData);

        if( eErr != CE_None )
            return eErr;
    }

    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int HFADataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Verify that this is a HFA file.                                 */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 15
        || !EQUALN((char *) poOpenInfo->pabyHeader,"EHFA_HEADER_TAG",15) )
        return FALSE;
    else
        return TRUE;
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
    if( !Identify( poOpenInfo ) )
        return NULL;

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
/*      Get geotransform, or if that fails, try to find XForms to 	*/
/*	build gcps, and metadata.					*/
/* -------------------------------------------------------------------- */
    if( !HFAGetGeoTransform( hHFA, poDS->adfGeoTransform ) )
    {
        Efga_Polynomial *pasPolyListForward = NULL;
        Efga_Polynomial *pasPolyListReverse = NULL;
        int nStepCount = 
            HFAReadXFormStack( hHFA, &pasPolyListForward, 
                               &pasPolyListReverse );

        if( nStepCount > 0 )
        {
            poDS->UseXFormStack( nStepCount, 
                                 pasPolyListForward, 
                                 pasPolyListReverse );
            CPLFree( pasPolyListForward );
            CPLFree( pasPolyListReverse );
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the projection.                                             */
/* -------------------------------------------------------------------- */
    poDS->ReadProjection();

/* -------------------------------------------------------------------- */
/*      Read the camera model as metadata, if present.                  */
/* -------------------------------------------------------------------- */
    char **papszCM = HFAReadCameraModel( hHFA );

    if( papszCM != NULL )
    {
        poDS->SetMetadata( papszCM, "CAMERA_MODEL" );
        CSLDestroy( papszCM );
    }

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
        poBand->ReadHistogramMetadata();
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
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
    
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
/*                           UseXFormStack()                            */
/************************************************************************/

void HFADataset::UseXFormStack( int nStepCount,
                                Efga_Polynomial *pasPLForward,
                                Efga_Polynomial *pasPLReverse )

{
/* -------------------------------------------------------------------- */
/*      Generate GCPs using the transform.                              */
/* -------------------------------------------------------------------- */
    double dfXRatio, dfYRatio;

    nGCPCount = 0;
    GDALInitGCPs( 36, asGCPList );

    for( dfYRatio = 0.0; dfYRatio < 1.001; dfYRatio += 0.2 )
    {
        for( dfXRatio = 0.0; dfXRatio < 1.001; dfXRatio += 0.2 )
        {
            double dfLine = 0.5 + (GetRasterYSize()-1) * dfYRatio;
            double dfPixel = 0.5 + (GetRasterXSize()-1) * dfXRatio;
            int iGCP = nGCPCount;

            asGCPList[iGCP].dfGCPPixel = dfPixel;
            asGCPList[iGCP].dfGCPLine = dfLine;

            asGCPList[iGCP].dfGCPX = dfPixel;
            asGCPList[iGCP].dfGCPY = dfLine;
            asGCPList[iGCP].dfGCPZ = 0.0;

            if( HFAEvaluateXFormStack( nStepCount, FALSE, pasPLReverse,
                                       &(asGCPList[iGCP].dfGCPX),
                                       &(asGCPList[iGCP].dfGCPY) ) )
                nGCPCount++;
        }
    }

/* -------------------------------------------------------------------- */
/*      Store the transform as metadata.                                */
/* -------------------------------------------------------------------- */
    int iStep, i;

    GDALMajorObject::SetMetadataItem( 
        "XFORM_STEPS", 
        CPLString().Printf("%d",nStepCount),
        "XFORMS" );

    for( iStep = 0; iStep < nStepCount; iStep++ )
    {
        GDALMajorObject::SetMetadataItem( 
            CPLString().Printf("XFORM%d_ORDER", iStep),
            CPLString().Printf("%d",pasPLForward[iStep].order),
            "XFORMS" );

        if( pasPLForward[iStep].order == 1 )
        {
            for( i = 0; i < 4; i++ )
                GDALMajorObject::SetMetadataItem( 
                    CPLString().Printf("XFORM%d_POLYCOEFMTX[%d]", iStep, i),
                    CPLString().Printf("%.15g",
                                       pasPLForward[iStep].polycoefmtx[i]),
                    "XFORMS" );
            
            for( i = 0; i < 2; i++ )
                GDALMajorObject::SetMetadataItem( 
                    CPLString().Printf("XFORM%d_POLYCOEFVECTOR[%d]", iStep, i),
                    CPLString().Printf("%.15g",
                                       pasPLForward[iStep].polycoefvector[i]),
                    "XFORMS" );
            
            continue;
        }

        int nCoefCount;

        if( pasPLForward[iStep].order == 2 )
            nCoefCount = 10;
        else
        {
            CPLAssert( pasPLForward[iStep].order == 3 );
            nCoefCount = 18;
        }

        for( i = 0; i < nCoefCount; i++ )
            GDALMajorObject::SetMetadataItem( 
                CPLString().Printf("XFORM%d_FWD_POLYCOEFMTX[%d]", iStep, i),
                CPLString().Printf("%.15g",
                                   pasPLForward[iStep].polycoefmtx[i]),
                "XFORMS" );
            
        for( i = 0; i < 2; i++ )
            GDALMajorObject::SetMetadataItem( 
                CPLString().Printf("XFORM%d_FWD_POLYCOEFVECTOR[%d]", iStep, i),
                CPLString().Printf("%.15g",
                                   pasPLForward[iStep].polycoefvector[i]),
                "XFORMS" );
            
        for( i = 0; i < nCoefCount; i++ )
            GDALMajorObject::SetMetadataItem( 
                CPLString().Printf("XFORM%d_REV_POLYCOEFMTX[%d]", iStep, i),
                CPLString().Printf("%.15g",
                                   pasPLReverse[iStep].polycoefmtx[i]),
                "XFORMS" );
            
        for( i = 0; i < 2; i++ )
            GDALMajorObject::SetMetadataItem( 
                CPLString().Printf("XFORM%d_REV_POLYCOEFVECTOR[%d]", iStep, i),
                CPLString().Printf("%.15g",
                                   pasPLReverse[iStep].polycoefvector[i]),
                "XFORMS" );
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HFADataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *HFADataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *HFADataset::GetGCPs()

{
    return asGCPList;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **HFADataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if( hHFA->pszIGEFilename != NULL )
    {
        papszFileList = CSLAddString( papszFileList, 
                                      CPLFormFilename( hHFA->pszPath, 
                                                       hHFA->pszIGEFilename,
                                                       NULL ));
    }

    if( hHFA->psDependent != NULL )
    {
        HFAInfo_t *psDep = hHFA->psDependent;

        papszFileList = 
            CSLAddString( papszFileList, 
                          CPLFormFilename( psDep->pszPath, 
                                           psDep->pszFilename, NULL ));
        if( psDep->pszIGEFilename != NULL )
        {
            papszFileList = 
                CSLAddString( papszFileList, 
                              CPLFormFilename( psDep->pszPath, 
                                               psDep->pszIGEFilename, NULL ));
        }
    }
    
    return papszFileList;
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
    int         nBits = 0;
    const char *pszPixelType;


    if( CSLFetchNameValue( papszParmList, "NBITS" ) != NULL )
        nBits = atoi(CSLFetchNameValue(papszParmList,"NBITS"));

    pszPixelType = 
        CSLFetchNameValue( papszParmList, "PIXELTYPE" );
    if( pszPixelType == NULL )
        pszPixelType = "";

/* -------------------------------------------------------------------- */
/*      Translate the data type.                                        */
/* -------------------------------------------------------------------- */
    switch( eType )
    {
      case GDT_Byte:
        if( nBits == 1 )
            nHfaDataType = EPT_u1;
        else if( nBits == 2 )
            nHfaDataType = EPT_u2;
        else if( nBits == 4 )
            nHfaDataType = EPT_u4;
        else if( EQUAL(pszPixelType,"SIGNEDBYTE") )
            nHfaDataType = EPT_s8;
        else
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

/* -------------------------------------------------------------------- */
/*      Sometimes we can improve ArcGIS compatability by forcing        */
/*      generation of a PEString instead of traditional Imagine         */
/*      coordinate system descriptions.                                 */
/* -------------------------------------------------------------------- */
    if( poDS != NULL )
    {
        poDS->bForceToPEString = 
            CSLFetchBoolean( papszParmList, "FORCETOPESTRING", FALSE );
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
    char         **papszModOptions = CSLDuplicate( papszOptions );

/* -------------------------------------------------------------------- */
/*      Do we really just want to create an .aux file?                  */
/* -------------------------------------------------------------------- */
    int bCreateAux = CSLFetchBoolean( papszOptions, "AUX", FALSE );

/* -------------------------------------------------------------------- */
/*      Establish a representative data type to use.                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        eType = GDALDataTypeUnion( eType, poBand->GetRasterDataType() );
    }

/* -------------------------------------------------------------------- */
/*      If we have PIXELTYPE metadadata in the source, pass it          */
/*      through as a creation option.                                   */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "PIXELTYPE" ) == NULL
        && nBandCount > 0 
        && eType == GDT_Byte
        && poSrcDS->GetRasterBand(1)->GetMetadataItem( "PIXELTYPE", 
                                                       "IMAGE_STRUCTURE" ) )
    {
        papszModOptions = 
            CSLSetNameValue( papszModOptions, "PIXELTYPE", 
                             poSrcDS->GetRasterBand(1)->GetMetadataItem( 
                                 "PIXELTYPE", "IMAGE_STRUCTURE" ) );
    }

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    poDS = (HFADataset *) Create( pszFilename,
                                  poSrcDS->GetRasterXSize(),
                                  poSrcDS->GetRasterYSize(),
                                  nBandCount,
                                  eType, papszModOptions );

    CSLDestroy( papszModOptions );

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
            poDS->GetRasterBand(iBand+1)->SetColorTable(poCT);
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
        int bSuccess;
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDS->GetRasterBand(iBand+1);

        if( poSrcBand->GetMetadata() != NULL )
            poDstBand->SetMetadata( poSrcBand->GetMetadata() );

        if( strlen(poSrcBand->GetDescription()) > 0 )
            poDstBand->SetDescription( poSrcBand->GetDescription() );

        double dfNoDataValue = poSrcBand->GetNoDataValue( &bSuccess );
        if( bSuccess )
            poDstBand->SetNoDataValue( dfNoDataValue );
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
/*      Copy the imagery.                                               */
/* -------------------------------------------------------------------- */
    if( !bCreateAux )
    {
        CPLErr eErr;

        eErr = GDALDatasetCopyWholeRaster( (GDALDatasetH) poSrcDS, 
                                           (GDALDatasetH) poDS,
                                           NULL, pfnProgress, pProgressData );
        
        if( eErr != CE_None )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we want to generate statistics and a histogram?              */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "STATISTICS", FALSE ) )
    {
        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
            double dfMin, dfMax, dfMean, dfStdDev;
            char **papszStatsMD = NULL;

            // -----------------------------------------------------------
            // Statistics
            // -----------------------------------------------------------

            if( poSrcBand->GetStatistics( TRUE, FALSE, &dfMin, &dfMax, 
                                          &dfMean, &dfStdDev ) == CE_None
                || poSrcBand->ComputeStatistics( TRUE, &dfMin, &dfMax, 
                                                 &dfMean, &dfStdDev, 
                                                 pfnProgress, pProgressData )
                == CE_None )
            {
                CPLString osValue;
                
                papszStatsMD = 
                    CSLSetNameValue( papszStatsMD, "STATISTICS_MINIMUM", 
                                     osValue.Printf( "%.15g", dfMin ) );
                papszStatsMD = 
                    CSLSetNameValue( papszStatsMD, "STATISTICS_MAXIMUM", 
                                     osValue.Printf( "%.15g", dfMax ) );
                papszStatsMD = 
                    CSLSetNameValue( papszStatsMD, "STATISTICS_MEAN", 
                                     osValue.Printf( "%.15g", dfMean ) );
                papszStatsMD = 
                    CSLSetNameValue( papszStatsMD, "STATISTICS_STDDEV", 
                                     osValue.Printf( "%.15g", dfStdDev ) );
            }

            // -----------------------------------------------------------
            // Histogram
            // -----------------------------------------------------------

            int nBuckets, *panHistogram = NULL;

            if( poSrcBand->GetDefaultHistogram( &dfMin, &dfMax, 
                                                &nBuckets, &panHistogram, 
                                                TRUE, 
                                                pfnProgress, pProgressData )
                == CE_None )
            {
                CPLString osValue;
                char *pszBinValues = (char *) CPLCalloc(12,nBuckets+1);
                int iBin, nBinValuesLen = 0;
                double dfBinWidth = (dfMax - dfMin) / nBuckets;
                
                papszStatsMD = CSLSetNameValue( 
                    papszStatsMD, "STATISTICS_HISTOMIN", 
                    osValue.Printf( "%.15g", dfMin+dfBinWidth*0.5 ) );
                papszStatsMD = CSLSetNameValue( 
                    papszStatsMD, "STATISTICS_HISTOMAX", 
                    osValue.Printf( "%.15g", dfMax-dfBinWidth*0.5 ) );
                papszStatsMD = 
                    CSLSetNameValue( papszStatsMD, "STATISTICS_HISTONUMBINS", 
                                     osValue.Printf( "%d", nBuckets ) );

                for( iBin = 0; iBin < nBuckets; iBin++ )
                {
                    
                    strcat( pszBinValues+nBinValuesLen, 
                            osValue.Printf( "%d", panHistogram[iBin]) );
                    strcat( pszBinValues+nBinValuesLen, "|" );
                    nBinValuesLen += strlen(pszBinValues+nBinValuesLen);
                }
                papszStatsMD = 
                    CSLSetNameValue( papszStatsMD, "STATISTICS_HISTOBINVALUES",
                                     pszBinValues );
                CPLFree( pszBinValues );
            }

            if( CSLCount(papszStatsMD) > 0 )
                HFASetMetadata( poDS->hHFA, iBand+1, papszStatsMD );

            CSLDestroy( papszStatsMD );
        }
    }

/* -------------------------------------------------------------------- */
/*      All report completion.                                          */
/* -------------------------------------------------------------------- */
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
"   <Option name='BLOCKSIZE' type='integer' description='tile width/height (32-2048)' default='64'/>"
"   <Option name='USE_SPILL' type='boolean' description='Force use of spill file'/>"
"   <Option name='COMPRESSED' alias='COMPRESS' type='boolean' description='compress blocks'/>"
"   <Option name='PIXELTYPE' type='string' description='By setting this to SIGNEDBYTE, a new Byte file can be forced to be written as signed byte'/>"
"   <Option name='AUX' type='boolean' description='Create an .aux file'/>"
"   <Option name='IGNOREUTM' type='boolean' description='Ignore UTM when selecting coordinate system - will use Transverse Mercator. Only used for Create() method'/>"
"   <Option name='NBITS' type='integer' description='Create file with special sub-byte data type (1/2/4)'/>"
"   <Option name='STATISTICS' type='boolean' description='Generate statistics and a histogram'/>"
"   <Option name='DEPENDENT_FILE' type='string' description='Name of dependent file (must not have absolute path)'/>"
"   <Option name='FORCETOPESTRING' type='boolean' description='Force use of ArcGIS PE String in file instead of Imagine coordinate system format'/>"
"</CreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = HFADataset::Open;
        poDriver->pfnCreate = HFADataset::Create;
        poDriver->pfnCreateCopy = HFADataset::CreateCopy;
        poDriver->pfnIdentify = HFADataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

