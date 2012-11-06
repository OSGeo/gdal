/*****************************************************************************
* $Id$
*
* Project:  Idrisi Raster Image File Driver
* Purpose:  Read/write Idrisi Raster Image Format RST
* Author:   Ivan Lucena, ivan.lucena@pmldnet.com
*
******************************************************************************
* Copyright( c ) 2006, Ivan Lucena
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files( the "Software" ),
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

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "ogr_spatialref.h"
#include "gdal_pam.h"
#include "gdal_alg.h"
#include "gdal_rat.h"
#include "idrisi.h"

CPL_CVSID( "$Id$" );

CPL_C_START
void GDALRegister_IDRISI( void);
CPL_C_END

#ifdef WIN32        
#define PATHDELIM       '\\'
#else
#define PATHDELIM       '/'
#endif 

//----- Safe numeric conversion, NULL as zero
#define atoi_nz(s) (s == NULL ? (int)      0 : atoi(s))
#define atof_nz(s) (s == NULL ? (double) 0.0 : atof(s))

//----- file extensions:
#define extRST          "rst"
#define extRDC          "rdc"
#define extSMP          "smp"
#define extREF          "ref"
#define extRSTu         "RST"
#define extRDCu         "RDC"
#define extSMPu         "SMP"
#define extREFu         "REF"

//----- field names on rdc file:
#define rdcFILE_FORMAT  "file format "
#define rdcFILE_TITLE   "file title  "
#define rdcDATA_TYPE    "data type   "
#define rdcFILE_TYPE    "file type   "
#define rdcCOLUMNS      "columns     "
#define rdcROWS         "rows        "
#define rdcREF_SYSTEM   "ref. system "
#define rdcREF_UNITS    "ref. units  "
#define rdcUNIT_DIST    "unit dist.  "
#define rdcMIN_X        "min. X      "
#define rdcMAX_X        "max. X      "
#define rdcMIN_Y        "min. Y      "
#define rdcMAX_Y        "max. Y      "
#define rdcPOSN_ERROR   "pos'n error "
#define rdcRESOLUTION   "resolution  "
#define rdcMIN_VALUE    "min. value  "
#define rdcMAX_VALUE    "max. value  "
#define rdcDISPLAY_MIN  "display min "
#define rdcDISPLAY_MAX  "display max "
#define rdcVALUE_UNITS  "value units "
#define rdcVALUE_ERROR  "value error "
#define rdcFLAG_VALUE   "flag value  "
#define rdcFLAG_DEFN    "flag def'n  "
#define rdcFLAG_DEFN2   "flag def`n  "
#define rdcLEGEND_CATS  "legend cats "
#define rdcLINEAGES     "lineage     "
#define rdcCOMMENTS     "comment     "
#define rdcCODE_N       "code %6d "

//----- ".ref" file field names:
#define refREF_SYSTEM   "ref. system "
#define refREF_SYSTEM2  "ref.system  "
#define refPROJECTION   "projection  "
#define refDATUM        "datum       " 
#define refDELTA_WGS84  "delta WGS84 " 
#define refELLIPSOID    "ellipsoid   "
#define refMAJOR_SAX    "major s-ax  " 
#define refMINOR_SAX    "minor s-ax  "
#define refORIGIN_LONG  "origin long "
#define refORIGIN_LAT   "origin lat  "
#define refORIGIN_X     "origin X    "
#define refORIGIN_Y     "origin Y    "
#define refSCALE_FAC    "scale fac   "
#define refUNITS        "units       "
#define refPARAMETERS   "parameters  "
#define refSTANDL_1     "stand ln 1  "
#define refSTANDL_2     "stand ln 2  "

//----- standard values:
#define rstVERSION      "Idrisi Raster A.1"
#define rstBYTE         "byte"
#define rstINTEGER      "integer"
#define rstREAL         "real"
#define rstRGB24        "rgb24"
#define rstDEGREE       "deg"
#define rstMETER        "m"
#define rstLATLONG      "latlong"
#define rstLATLONG2     "lat/long"
#define rstPLANE        "plane"
#define rstUTM          "utm-%d%c"
#define rstSPC          "spc%2d%2s%d"

//----- palette file( .smp ) header size:
#define smpHEADERSIZE    18

//----- check if file exists:
bool FileExists( const char *pszPath );

//----- Reference Table
struct ReferenceTab {
    int nCode;
    const char *pszName;
};

//----- USA State's reference table to USGS PCS Code
static const ReferenceTab aoUSStateTable[] = {
    {101,     "al"},
    {201,     "az"},
    {301,     "ar"},
    {401,     "ca"},
    {501,     "co"},
    {600,     "ct"},
    {700,     "de"},
    {901,     "fl"},
    {1001,    "ga"},
    {1101,    "id"},
    {1201,    "il"},
    {1301,    "in"},
    {1401,    "ia"},
    {1501,    "ks"},
    {1601,    "ky"},
    {1701,    "la"},
    {1801,    "me"},
    {1900,    "md"},
    {2001,    "ma"},
    {2111,    "mi"},
    {2201,    "mn"},
    {2301,    "ms"},
    {2401,    "mo"},
    {2500,    "mt"},
    {2600,    "ne"},
    {2701,    "nv"},
    {2800,    "nh"},
    {2900,    "nj"},
    {3001,    "nm"},
    {3101,    "ny"},
    {3200,    "nc"},
    {3301,    "nd"},
    {3401,    "oh"},
    {3501,    "ok"},
    {3601,    "or"},
    {3701,    "pa"},
    {3800,    "ri"},
    {3900,    "sc"},
    {4001,    "sd"},
    {4100,    "tn"},
    {4201,    "tx"},
    {4301,    "ut"},
    {4400,    "vt"},
    {4501,    "va"},
    {4601,    "wa"},
    {4701,    "wv"},
    {4801,    "wv"},
    {4901,    "wy"},
    {5001,    "ak"},
    {5101,    "hi"},
    {5200,    "pr"}
};
#define US_STATE_COUNT ( sizeof( aoUSStateTable ) / sizeof( ReferenceTab ) )

//----- Get the Code of a US State
int GetStateCode ( const char *pszState );

//----- Get the state name of a Code
const char *GetStateName( int nCode );

//----- Conversion Table definition
struct ConvertionTab {
    const char *pszName;
    int nDefaultI;
    int nDefaultG;
    double dfConv;
};

//----- Linear Unit Conversion Table
static const ConvertionTab aoLinearUnitsConv[] = {
    {"m",            /*  0 */  0,   1,  1.0},
    {SRS_UL_METER,   /*  1 */  0,   1,  1.0},
    {"meters",       /*  2 */  0,   1,  1.0},
    {"metre",        /*  3 */  0,   1,  1.0},

    {"ft",           /*  4 */  4,   5,  0.3048},
    {SRS_UL_FOOT,    /*  5 */  4,   5,  0.3048},
    {"feet",         /*  6 */  4,   5,  0.3048},
    {"foot_us",      /*  7 */  4,   5,  0.3048006},
    {"u.s. foot",    /*  8 */  4,   5,  0.3048006},

    {"mi",           /*  9 */  9,  10,  1612.9},
    {"mile",         /* 10 */  9,  10,  1612.9},
    {"miles",        /* 11 */  9,  10,  1612.9},

    {"km",           /* 12 */ 12,  13,  1000.0},
    {"kilometers",   /* 13 */ 12,  13,  1000.0},
    {"kilometer",    /* 14 */ 12,  13,  1000.0},
    {"kilometre",    /* 15 */ 12,  13,  1000.0},

    {"deg",          /* 16 */ 16,  17,  0.0},
    {SRS_UA_DEGREE,  /* 17 */ 16,  17,  0.0},
    {"degrees",      /* 18 */ 16,  17,  0.0},

    {"rad",          /* 19 */ 19,  20,  0.0},
    {SRS_UA_RADIAN,  /* 20 */ 19,  20,  0.0},
    {"radians",      /* 21 */ 19,  20,  0.0}
};
#define LINEAR_UNITS_COUNT (sizeof(aoLinearUnitsConv) / sizeof(ConvertionTab))

//----- Get the index of a given linear unit
int GetUnitIndex( const char *pszUnitName );

//----- Get the defaut name
char *GetUnitDefault( const char *pszUnitName, const char *pszToMeter = NULL );

//----- Get the "to meter"
int GetToMeterIndex( const char *pszToMeter );

//----- CSLSaveCRLF
int  SaveAsCRLF(char **papszStrList, const char *pszFname);

//----- Classes pre-definition:
class IdrisiDataset;
class IdrisiRasterBand;

//  ----------------------------------------------------------------------------
//        Idrisi GDALDataset
//  ----------------------------------------------------------------------------

class IdrisiDataset : public GDALPamDataset
{
    friend class IdrisiRasterBand;

private:
    VSILFILE *fp;

    char *pszFilename;
    char *pszDocFilename;
    char **papszRDC;
    double adfGeoTransform[6];

    char *pszProjection;
    char **papszCategories;
    char *pszUnitType;

    CPLErr Wkt2GeoReference( const char *pszProjString,
        char **pszRefSystem, 
        char **pszRefUnit );

protected:
    GDALColorTable *poColorTable;

public:
    IdrisiDataset();
    ~IdrisiDataset();

    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static GDALDataset *Create( const char *pszFilename,
        int nXSize,
        int nYSize,
        int nBands, 
        GDALDataType eType,
        char **papszOptions );
    static GDALDataset *CreateCopy( const char *pszFilename, 
        GDALDataset *poSrcDS,
        int bStrict,
        char **papszOptions,
        GDALProgressFunc pfnProgress, 
        void * pProgressData );
    virtual char **GetFileList(void);
    virtual CPLErr GetGeoTransform( double *padfTransform );
    virtual CPLErr SetGeoTransform( double *padfTransform );
    virtual const char *GetProjectionRef( void );
    virtual CPLErr SetProjection( const char *pszProjString );
};

//  ----------------------------------------------------------------------------
//        Idrisi GDALPamRasterBand
//  ----------------------------------------------------------------------------

class IdrisiRasterBand : public GDALPamRasterBand
{
    friend class IdrisiDataset;

    GDALRasterAttributeTable *poDefaultRAT; 

private:
    int     nRecordSize;
    GByte *pabyScanLine;

public:
    IdrisiRasterBand( IdrisiDataset *poDS, 
        int nBand, 
        GDALDataType eDataType );
    ~IdrisiRasterBand();

    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum( int *pbSuccess = NULL );
    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage );
    virtual CPLErr IWriteBlock( int nBlockXOff, int nBlockYOff, void *pImage );
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
    virtual char **GetCategoryNames();
    virtual const char *GetUnitType();

    virtual CPLErr SetCategoryNames( char **papszCategoryNames );
    virtual CPLErr SetNoDataValue( double dfNoDataValue );
    virtual CPLErr SetColorTable( GDALColorTable *poColorTable ); 
    virtual CPLErr SetUnitType( const char *pszUnitType );
    virtual CPLErr SetStatistics( double dfMin, double dfMax,
                                  double dfMean, double dfStdDev ); 
    CPLErr SetMinMax( double dfMin, double dfMax );
    virtual const GDALRasterAttributeTable *GetDefaultRAT();
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * );

    float  fMaximum;
    float  fMinimum;
    bool   bFirstVal;
};

//  ------------------------------------------------------------------------  //
//                        Implementation of IdrisiDataset                     //
//  ------------------------------------------------------------------------  //

/************************************************************************/
/*                           IdrisiDataset()                            */
/************************************************************************/

IdrisiDataset::IdrisiDataset()
{
    pszFilename = NULL;
    fp = NULL;
    papszRDC = NULL;
    pszDocFilename = NULL;
    pszProjection = NULL;
    poColorTable = new GDALColorTable();
    papszCategories = NULL;
    pszUnitType = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~IdrisiDataset()                           */
/************************************************************************/

IdrisiDataset::~IdrisiDataset()
{
    FlushCache();

    if( papszRDC != NULL )
    {
        if( eAccess == GA_Update )
        {
            int i;

            for( i = 0; i < nBands; i++ )
            {
                IdrisiRasterBand *poBand = (IdrisiRasterBand*) GetRasterBand( i + 1 );
                poBand->SetMinMax( poBand->fMinimum, poBand->fMaximum );
            }

            CSLSetNameValueSeparator( papszRDC, ": " );
            SaveAsCRLF( papszRDC, pszDocFilename );
        }
        CSLDestroy( papszRDC );
    }

    if( poColorTable )
    {
        delete poColorTable;
    }
    CPLFree( pszFilename );
    CPLFree( pszDocFilename );
    CPLFree( pszProjection );
    CSLDestroy( papszCategories );
    CPLFree( pszUnitType );

    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *IdrisiDataset::Open( GDALOpenInfo *poOpenInfo )
{
    if( EQUAL( CPLGetExtension( poOpenInfo->pszFilename ), extRST ) == FALSE )
        return NULL;

    // --------------------------------------------------------------------
    //      Check the documentation file .rdc
    // --------------------------------------------------------------------

    const char *pszLDocFilename = CPLResetExtension( poOpenInfo->pszFilename, extRDC );

    if( ! FileExists( pszLDocFilename ) )
    {
        pszLDocFilename = CPLResetExtension( poOpenInfo->pszFilename, extRDCu );

        if( ! FileExists( pszLDocFilename ) )
        {
            return NULL;
        }
    }

    char **papszLRDC = CSLLoad( pszLDocFilename );

    CSLSetNameValueSeparator( papszLRDC, ":" );

    const char *pszVersion = CSLFetchNameValue( papszLRDC, rdcFILE_FORMAT );

    if( pszVersion == NULL || !EQUAL( pszVersion, rstVERSION ) )
    {
        CSLDestroy( papszLRDC );
        return NULL;
    }

    // --------------------------------------------------------------------
    //      Create a corresponding GDALDataset                   
    // --------------------------------------------------------------------

    IdrisiDataset *poDS;

    poDS = new IdrisiDataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->pszFilename = CPLStrdup( poOpenInfo->pszFilename );

    if( poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fp = VSIFOpenL( poDS->pszFilename, "rb" );
    } 
    else 
    {
        poDS->fp = VSIFOpenL( poDS->pszFilename, "r+b" );
    }

    if( poDS->fp == NULL )
    {
        CSLDestroy( papszLRDC );
        delete poDS;
        return NULL;
    }

    poDS->pszDocFilename = CPLStrdup( pszLDocFilename );
    poDS->papszRDC = CSLDuplicate( papszLRDC );
    CSLDestroy( papszLRDC );

    // -------------------------------------------------------------------- 
    //      Load information from rdc
    // -------------------------------------------------------------------- 

    poDS->nRasterXSize = atoi_nz( CSLFetchNameValue( poDS->papszRDC, rdcCOLUMNS ) );
    poDS->nRasterYSize = atoi_nz( CSLFetchNameValue( poDS->papszRDC, rdcROWS ) );

    // -------------------------------------------------------------------- 
    //      Create band information
    // -------------------------------------------------------------------- 

    const char *pszDataType = CSLFetchNameValue( poDS->papszRDC, rdcDATA_TYPE );
    if( pszDataType == NULL )
    {
        delete poDS;
        return NULL;
    }

    if( EQUAL( pszDataType, rstBYTE ) )
    {
        poDS->nBands = 1;
        poDS->SetBand( 1, new IdrisiRasterBand( poDS, 1, GDT_Byte ) );
    }
    else if( EQUAL( pszDataType, rstINTEGER ) )
    {
        poDS->nBands = 1;
        poDS->SetBand( 1, new IdrisiRasterBand( poDS, 1, GDT_Int16 ) );
    }
    else if( EQUAL( pszDataType, rstREAL ) )
    {
        poDS->nBands = 1;
        poDS->SetBand( 1, new IdrisiRasterBand( poDS, 1, GDT_Float32 ) );
    }
    else if( EQUAL( pszDataType, rstRGB24 ) )
    {
        poDS->nBands = 3;
        poDS->SetBand( 1, new IdrisiRasterBand( poDS, 1, GDT_Byte ) );
        poDS->SetBand( 2, new IdrisiRasterBand( poDS, 2, GDT_Byte ) );
        poDS->SetBand( 3, new IdrisiRasterBand( poDS, 3, GDT_Byte ) );
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown data type : %s", pszDataType);
        delete poDS;
        return NULL;
    }

    int i;
    for(i=0;i<poDS->nBands;i++)
    {
        IdrisiRasterBand* band = (IdrisiRasterBand*) poDS->GetRasterBand(i+1);
        if (band->pabyScanLine == NULL)
        {
            delete poDS;
            return NULL;
        }
    }

    // -------------------------------------------------------------------- 
    //      Load the transformation matrix
    // -------------------------------------------------------------------- 

    const char *pszMinX = CSLFetchNameValue( poDS->papszRDC, rdcMIN_X );
    const char *pszMaxX = CSLFetchNameValue( poDS->papszRDC, rdcMAX_X );
    const char *pszMinY = CSLFetchNameValue( poDS->papszRDC, rdcMIN_Y );
    const char *pszMaxY = CSLFetchNameValue( poDS->papszRDC, rdcMAX_Y );
    const char *pszUnit = CSLFetchNameValue( poDS->papszRDC, rdcUNIT_DIST );

    if( pszMinX != NULL && strlen( pszMinX ) > 0 &&
        pszMaxX != NULL && strlen( pszMaxX ) > 0 &&
        pszMinY != NULL && strlen( pszMinY ) > 0 &&
        pszMaxY != NULL && strlen( pszMaxY ) > 0 &&
        pszUnit != NULL && strlen( pszUnit ) > 0 )
    {
        double dfMinX, dfMaxX, dfMinY, dfMaxY, dfUnit, dfXPixSz, dfYPixSz;

        dfMinX = atof_nz( pszMinX );
        dfMaxX = atof_nz( pszMaxX );
        dfMinY = atof_nz( pszMinY );
        dfMaxY = atof_nz( pszMaxY );
        dfUnit = atof_nz( pszUnit );

        dfMinX = dfMinX * dfUnit; 
        dfMaxX = dfMaxX * dfUnit; 
        dfMinY = dfMinY * dfUnit; 
        dfMaxY = dfMaxY * dfUnit;

        dfYPixSz = ( dfMinY - dfMaxY ) / poDS->nRasterYSize;
        dfXPixSz = ( dfMaxX - dfMinX ) / poDS->nRasterXSize;

        poDS->adfGeoTransform[0] = dfMinX;
        poDS->adfGeoTransform[1] = dfXPixSz;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = dfMaxY;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = dfYPixSz;
    }

    // -------------------------------------------------------------------- 
    //      Set Color Table in the presence of a smp file
    // -------------------------------------------------------------------- 

    if( poDS->nBands != 3 )
    {
        const char *pszSMPFilename = CPLResetExtension( poDS->pszFilename, extSMP );
        VSILFILE *fpSMP;
        if( ( fpSMP = VSIFOpenL( pszSMPFilename, "rb" ) ) != NULL )
        {
            int dfMaxValue = atoi_nz( CSLFetchNameValue( poDS->papszRDC, rdcMAX_VALUE ) );
            int nCatCount = atoi_nz( CSLFetchNameValue( poDS->papszRDC, rdcLEGEND_CATS ) );
            if( nCatCount == 0 )
                dfMaxValue = 255;
            VSIFSeekL( fpSMP, smpHEADERSIZE, SEEK_SET );
            GDALColorEntry oEntry;
            unsigned char aucRGB[3];
            int i = 0;
            while( ( VSIFReadL( &aucRGB, sizeof( aucRGB ), 1, fpSMP ) ) &&( i <= dfMaxValue ) )
            {
                oEntry.c1 = (short) aucRGB[0];
                oEntry.c2 = (short) aucRGB[1];
                oEntry.c3 = (short) aucRGB[2];
                oEntry.c4 = (short) 255;                      
                poDS->poColorTable->SetColorEntry( i, &oEntry );
                i++;
            }
            VSIFCloseL( fpSMP );
        }
    }

    // -------------------------------------------------------------------- 
    //      Check for Unit Type
    // -------------------------------------------------------------------- 

    const char *pszValueUnit = CSLFetchNameValue( poDS->papszRDC, rdcVALUE_UNITS );

    if( pszValueUnit == NULL )
        poDS->pszUnitType = CPLStrdup( "unspecified" );
    else
    {
        if( EQUALN( pszValueUnit, "meter", 4 ) )
        {
            poDS->pszUnitType = CPLStrdup( "m" );
        }
        else if( EQUALN( pszValueUnit, "feet", 4 ) )
        {
            poDS->pszUnitType = CPLStrdup( "ft" );
        }
        else
            poDS->pszUnitType = CPLStrdup( pszValueUnit );
    }

    // -------------------------------------------------------------------- 
    //      Check for category names.
    // -------------------------------------------------------------------- 

    int nCatCount = atoi_nz( CSLFetchNameValue( poDS->papszRDC, rdcLEGEND_CATS ) );

    if( nCatCount > 0 )
    {
        // ----------------------------------------------------------------
        //      Sequentialize categories names, from 0 to the last "code n"
        // ----------------------------------------------------------------

        int nCode = 0;
        int nCount = 0;
        int nLine = -1;
        for( int i = 0;( i < CSLCount( poDS->papszRDC ) ) &&( nLine == -1 ); i++ )
            if( EQUALN( poDS->papszRDC[i], rdcLEGEND_CATS, 11 ) )
                nLine = i;
        if( nLine > 0 )
        {
            sscanf( poDS->papszRDC[++nLine], rdcCODE_N, &nCode );
            for( int i = 0;( i < 255 ) &&( nCount < nCatCount ); i++ )
            {
                if( i == nCode )
                {
                    poDS->papszCategories = 
                        CSLAddString( poDS->papszCategories, 
                        CPLParseNameValue( poDS->papszRDC[nLine], NULL ) );
                    nCount++;
                    if( nCount < nCatCount )
                        sscanf( poDS->papszRDC[++nLine], rdcCODE_N, &nCode );
                }
                else
                    poDS->papszCategories = CSLAddString( poDS->papszCategories, "" );
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Automatic Generated Color Table                                 */
    /* -------------------------------------------------------------------- */

    if( poDS->papszCategories != NULL && 
      ( poDS->poColorTable->GetColorEntryCount() == 0 ) )
    {
        int nEntryCount = CSLCount(poDS->papszCategories);

        GDALColorEntry sFromColor;
        sFromColor.c1 = (short) ( 255 );
        sFromColor.c2 = (short) ( 0 );
        sFromColor.c3 = (short) ( 0 );
        sFromColor.c4 = (short) ( 255 );

        GDALColorEntry sToColor;
        sToColor.c1 = (short) ( 0 );
        sToColor.c2 = (short) ( 0 );
        sToColor.c3 = (short) ( 255 );
        sToColor.c4 = (short) ( 255 );

        poDS->poColorTable->CreateColorRamp( 
            0, &sFromColor, ( nEntryCount - 1 ), &sToColor );
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

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *IdrisiDataset::Create( const char *pszFilename,
                                   int nXSize, 
                                   int nYSize, 
                                   int nBands, 
                                   GDALDataType eType,
                                   char **papszOptions )
{
    (void) papszOptions;

    // -------------------------------------------------------------------- 
    //      Check input options
    // -------------------------------------------------------------------- 

    if( nBands != 1 ) 
    {
        if( !( nBands == 3 && eType == GDT_Byte ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                "Attempt to create IDRISI dataset with an illegal "
                "number of bands(%d) or data type(%s).\n",
                nBands, GDALGetDataTypeName( eType ) );
            return NULL;
        }
    }

    // ---------------------------------------------------------------- 
    //  Create the header file with minimun information
    // ---------------------------------------------------------------- 

    const char *pszLDataType;

    switch( eType )
    {    
    case GDT_Byte:
        if( nBands == 1 )
            pszLDataType = rstBYTE;
        else
            pszLDataType = rstRGB24;
        break;
    case GDT_Int16:
        pszLDataType = rstINTEGER;
        break;
    case GDT_Float32:                
        pszLDataType = rstREAL;
        break;
    default:
        CPLError( CE_Failure, CPLE_AppDefined,
            "Attempt to create IDRISI dataset with an illegal "
            "data type(%s).\n",
            GDALGetDataTypeName( eType ) );
        return NULL;

    };

    char **papszLRDC;

    papszLRDC = NULL;
    papszLRDC = CSLAddNameValue( papszLRDC, rdcFILE_FORMAT,   rstVERSION );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcFILE_TITLE,    "" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcDATA_TYPE,     pszLDataType );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcFILE_TYPE,     "binary" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcCOLUMNS,       CPLSPrintf( "%d", nXSize ) );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcROWS,          CPLSPrintf( "%d", nYSize ) );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcREF_SYSTEM,    "plane" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcREF_UNITS,     "m" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcUNIT_DIST,     "1" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcMIN_X,         "0" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcMAX_X,         CPLSPrintf( "%d", nXSize ) );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcMIN_Y,         "0" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcMAX_Y,         CPLSPrintf( "%d", nYSize ) );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcPOSN_ERROR,    "unspecified" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcRESOLUTION,    "1.0" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcMIN_VALUE,     "0" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcMAX_VALUE,     "0" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcDISPLAY_MIN,   "0" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcDISPLAY_MAX,   "0" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcVALUE_UNITS,   "unspecified" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcVALUE_ERROR,   "unspecified" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcFLAG_VALUE,    "none" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcFLAG_DEFN,     "none" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcLEGEND_CATS,   "0" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcLINEAGES,      "" );
    papszLRDC = CSLAddNameValue( papszLRDC, rdcCOMMENTS,      "" );

    const char *pszLDocFilename;
    pszLDocFilename = CPLResetExtension( pszFilename, extRDC );

    CSLSetNameValueSeparator( papszLRDC, ": " );
    SaveAsCRLF( papszLRDC, pszLDocFilename );
    CSLDestroy( papszLRDC );

    // ---------------------------------------------------------------- 
    //  Create an empty data file
    // ---------------------------------------------------------------- 

    VSILFILE *fp;

    fp = VSIFOpenL( pszFilename, "wb+" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
            "Attempt to create file %s' failed.\n", pszFilename );
        return NULL;
    }
    VSIFCloseL( fp );

    return (IdrisiDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *IdrisiDataset::CreateCopy( const char *pszFilename, 
                                       GDALDataset *poSrcDS,
                                       int bStrict,
                                       char **papszOptions,
                                       GDALProgressFunc pfnProgress, 
                                       void *pProgressData )
{
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    // ------------------------------------------------------------------------
    //      Check number of bands
    // ------------------------------------------------------------------------

    if ( ( ( poSrcDS->GetRasterCount() == 1 ) ||
         ( ( poSrcDS->GetRasterCount() == 3 ) &&
         ( ( poSrcDS->GetRasterBand( 1 )->GetRasterDataType() == GDT_Byte ) &&
           ( poSrcDS->GetRasterBand( 2 )->GetRasterDataType() == GDT_Byte ) &&
           ( poSrcDS->GetRasterBand( 3 )->GetRasterDataType() == GDT_Byte ) ) ) ) == FALSE )
    {

        CPLError( CE_Failure, CPLE_AppDefined,
            "Attempt to create IDRISI dataset with an illegal "
            "number of bands(%d).\n",
            poSrcDS->GetRasterCount() );
        return NULL;
    }

    // ------------------------------------------------------------------------
    //      Check Data types
    // ------------------------------------------------------------------------

    int i;

    for( i = 1; i <= poSrcDS->GetRasterCount(); i++ )
    {
        GDALDataType eType = poSrcDS->GetRasterBand( i )->GetRasterDataType();

        if( bStrict )
        {
            if( eType != GDT_Byte && 
                eType != GDT_Int16 && 
                eType != GDT_Float32 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                    "Attempt to create IDRISI dataset in strict mode "
                    "with an illegal data type(%s).\n",
                    GDALGetDataTypeName( eType ) );
                return NULL;
            }
        }
        else
        {
            if( eType != GDT_Byte && 
                eType != GDT_Int16 && 
                eType != GDT_UInt16 && 
                eType != GDT_UInt32 && 
                eType != GDT_Int32 && 
                eType != GDT_Float32 &&
                eType != GDT_Float64 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                    "Attempt to create IDRISI dataset with an illegal "
                    "data type(%s).\n",
                    GDALGetDataTypeName( eType ) );
                return NULL;
            }
        }
    }

    // --------------------------------------------------------------------
    //      Define data type 
    // --------------------------------------------------------------------

    GDALRasterBand *poBand = poSrcDS->GetRasterBand( 1 );
    GDALDataType eType = poBand->GetRasterDataType();

    int bSuccessMin = FALSE;
    int bSuccessMax = FALSE;

    double dfMin;
    double dfMax;

    dfMin = poBand->GetMinimum( &bSuccessMin );
    dfMax = poBand->GetMaximum( &bSuccessMax );

    if( ! ( bSuccessMin && bSuccessMax ) )
    {
      poBand->GetStatistics( false, false, &dfMin, &dfMax, NULL, NULL );
    }

    if(!( ( eType == GDT_Byte ) || 
          ( eType == GDT_Int16 ) || 
          ( eType == GDT_Float32 ) ) )
    {
        if( eType == GDT_Float64 )
        {
            eType = GDT_Float32;
        }
        else
        {
            if( ( dfMin < (double) SHRT_MIN ) || 
                ( dfMax > (double) SHRT_MAX ) )
            {
                eType = GDT_Float32; 
            }
            else
            {
                eType = GDT_Int16;
            }
        }
    }

    // --------------------------------------------------------------------
    //      Create the dataset
    // --------------------------------------------------------------------

    IdrisiDataset *poDS;

    poDS = (IdrisiDataset *) IdrisiDataset::Create( pszFilename, 
        poSrcDS->GetRasterXSize(), 
        poSrcDS->GetRasterYSize(), 
        poSrcDS->GetRasterCount(), 
        eType, 
        papszOptions );

    if( poDS == NULL )
        return NULL;

    // --------------------------------------------------------------------
    //      Copy information to the dataset
    // --------------------------------------------------------------------

    double adfGeoTransform[6];

    poDS->SetProjection( poSrcDS->GetProjectionRef() );

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        poDS->SetGeoTransform( adfGeoTransform );
    }
    else
    {
        poDS->papszRDC =
            CSLSetNameValue( poDS->papszRDC, rdcMIN_X, CPLSPrintf( "%.1f", -0.5 ));
        poDS->papszRDC =
            CSLSetNameValue( poDS->papszRDC, rdcMAX_X, CPLSPrintf( "%.1f",
            poSrcDS->GetRasterXSize() - 0.5 ) );
        poDS->papszRDC =
            CSLSetNameValue( poDS->papszRDC, rdcMIN_Y, CPLSPrintf( "%.1f",
            poSrcDS->GetRasterYSize() + 0.5 ) );
        poDS->papszRDC =
            CSLSetNameValue( poDS->papszRDC, rdcMAX_Y, CPLSPrintf( "%.1f", 0.5 ));
        poDS->papszRDC =
            CSLSetNameValue( poDS->papszRDC, rdcRESOLUTION, "1" );
    }

    // --------------------------------------------------------------------
    //      Copy information to the raster band
    // --------------------------------------------------------------------

    GDALRasterBand *poSrcBand;
    int bHasNoDataValue;
    double dfNoDataValue;

    for( i = 1; i <= poDS->nBands; i++ )
    {
        poSrcBand = poSrcDS->GetRasterBand( i );
        IdrisiRasterBand* poDstBand = (IdrisiRasterBand*) poDS->GetRasterBand( i );

        if( poDS->nBands == 1 )
        {
            poDstBand->SetUnitType( poSrcBand->GetUnitType() );
            poDstBand->SetColorTable( poSrcBand->GetColorTable() );
            poDstBand->SetCategoryNames( poSrcBand->GetCategoryNames() );

            const GDALRasterAttributeTable *poRAT = poSrcBand->GetDefaultRAT();

            if( poRAT != NULL )
            {
                poDstBand->SetDefaultRAT( poRAT );
            }
        }

        dfMin = poSrcBand->GetMinimum( NULL );
        dfMax = poSrcBand->GetMaximum( NULL );
        poDstBand->SetMinMax( dfMin, dfMax );
        dfNoDataValue = poSrcBand->GetNoDataValue( &bHasNoDataValue );
        if( bHasNoDataValue )
            poDstBand->SetNoDataValue( dfNoDataValue );
    }

    // --------------------------------------------------------------------
    //      Copy image data
    // --------------------------------------------------------------------

    GDALDatasetCopyWholeRaster( (GDALDatasetH) poSrcDS, 
                                (GDALDatasetH) poDS, NULL,
                                pfnProgress, pProgressData );

    // --------------------------------------------------------------------
    //      Finalize
    // --------------------------------------------------------------------

    poDS->FlushCache();

    return poDS;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **IdrisiDataset::GetFileList()
{
    char **papszFileList = GDALPamDataset::GetFileList();
    const char *pszAssociated;
        
    // --------------------------------------------------------------------
    //      Symbol table file
    // --------------------------------------------------------------------

    pszAssociated = CPLResetExtension( pszFilename, extSMP );

    if( FileExists( pszAssociated ) )
    {
        papszFileList = CSLAddString( papszFileList, pszAssociated );
    }
    else
    {
        pszAssociated = CPLResetExtension( pszFilename, extSMPu );

        if( FileExists( pszAssociated ) )
        {
            papszFileList = CSLAddString( papszFileList, pszAssociated );
        }
    }

    // --------------------------------------------------------------------
    //      Documentation file
    // --------------------------------------------------------------------

    pszAssociated = CPLResetExtension( pszFilename, extRDC );

    if( FileExists( pszAssociated ) )
    {
        papszFileList = CSLAddString( papszFileList, pszAssociated );
    }
    else
    {
        pszAssociated = CPLResetExtension( pszFilename, extRDCu );

        if( FileExists( pszAssociated ) )
        {
            papszFileList = CSLAddString( papszFileList, pszAssociated );
        }
    }

    // --------------------------------------------------------------------
    //      Reference file
    // --------------------------------------------------------------------

    pszAssociated = CPLResetExtension( pszFilename, extREF );

    if( FileExists( pszAssociated ) )
    {
        papszFileList = CSLAddString( papszFileList, pszAssociated );
    }
    else
    {
        pszAssociated = CPLResetExtension( pszFilename, extREFu );

        if( FileExists( pszAssociated ) )
        {
            papszFileList = CSLAddString( papszFileList, pszAssociated );
        }
    }

    return papszFileList;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr  IdrisiDataset::GetGeoTransform( double * padfTransform )
{
    if( GDALPamDataset::GetGeoTransform( padfTransform ) != CE_None )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof( double ) * 6 );

        if( adfGeoTransform[0] == 0.0
        &&  adfGeoTransform[1] == 1.0
        &&  adfGeoTransform[2] == 0.0
        &&  adfGeoTransform[3] == 0.0
        &&  adfGeoTransform[4] == 0.0
        &&  adfGeoTransform[5] == 1.0 )
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr  IdrisiDataset::SetGeoTransform( double * padfTransform )
{    
    if( padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Attempt to set rotated geotransform on Idrisi Raster file.\n"
            "Idrisi Raster does not support rotation.\n" );
        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Update the .rdc file
    // --------------------------------------------------------------------

    double dfMinX, dfMaxX, dfMinY, dfMaxY, dfXPixSz, dfYPixSz;

    dfXPixSz = padfTransform[1];
    dfYPixSz = padfTransform[5];
    dfMinX   = padfTransform[0];
    dfMaxX   = ( dfXPixSz * nRasterXSize ) + dfMinX;

    if( dfYPixSz < 0 )
    {
        dfMaxY   = padfTransform[3];
        dfMinY   = ( dfYPixSz * nRasterYSize ) + padfTransform[3];
    }
    else
    {
        dfMaxY   = ( dfYPixSz * nRasterYSize ) + padfTransform[3];
        dfMinY   = padfTransform[3];
    }

    papszRDC = CSLSetNameValue( papszRDC, rdcMIN_X,      CPLSPrintf( "%.7f", dfMinX ) );
    papszRDC = CSLSetNameValue( papszRDC, rdcMAX_X,      CPLSPrintf( "%.7f", dfMaxX ) );
    papszRDC = CSLSetNameValue( papszRDC, rdcMIN_Y,      CPLSPrintf( "%.7f", dfMinY ) );
    papszRDC = CSLSetNameValue( papszRDC, rdcMAX_Y,      CPLSPrintf( "%.7f", dfMaxY ) );
    papszRDC = CSLSetNameValue( papszRDC, rdcRESOLUTION, CPLSPrintf( "%.7f", fabs( dfYPixSz ) ) );

    // --------------------------------------------------------------------
    // Update the Dataset attribute
    // --------------------------------------------------------------------

    memcpy( adfGeoTransform, padfTransform, sizeof( double ) * 6 );

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *IdrisiDataset::GetProjectionRef( void )
{
    const char *pszPamSRS = GDALPamDataset::GetProjectionRef();

    if( pszPamSRS != NULL && strlen( pszPamSRS ) > 0 )
        return pszPamSRS;

    if( pszProjection == NULL )
    {
        const char *pszRefSystem = CSLFetchNameValue( papszRDC, rdcREF_SYSTEM );
        const char *pszRefUnit = CSLFetchNameValue( papszRDC, rdcREF_UNITS );

        if (pszRefSystem != NULL && pszRefUnit != NULL)
            IdrisiGeoReference2Wkt( pszFilename, pszRefSystem, pszRefUnit, &pszProjection );
        else
            pszProjection = CPLStrdup("");
    }
    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr IdrisiDataset::SetProjection( const char *pszProjString )
{   
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszProjString );
    CPLErr eResult = CE_None;

    char *pszRefSystem = NULL;
    char *pszRefUnit = NULL;

    eResult = Wkt2GeoReference( pszProjString, &pszRefSystem, &pszRefUnit );

    papszRDC = CSLSetNameValue( papszRDC, rdcREF_SYSTEM, pszRefSystem );
    papszRDC = CSLSetNameValue( papszRDC, rdcREF_UNITS,  pszRefUnit );

    CPLFree( pszRefSystem );
    CPLFree( pszRefUnit );

    return eResult;
}

/************************************************************************/
/*                          IdrisiRasterBand()                          */
/************************************************************************/

IdrisiRasterBand::IdrisiRasterBand( IdrisiDataset *poDS, 
                                    int nBand, 
                                    GDALDataType eDataType )
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = eDataType;
    this->poDefaultRAT = NULL;
    this->fMinimum = 0.0;
    this->fMaximum = 0.0;
    this->bFirstVal = true;

    // -------------------------------------------------------------------- 
    //      Set Dimension
    // -------------------------------------------------------------------- 

    nBlockYSize = 1;
    nBlockXSize = poDS->GetRasterXSize();

    // -------------------------------------------------------------------- 
    //      Get ready for reading and writing
    // -------------------------------------------------------------------- 

    nRecordSize  = poDS->GetRasterXSize() * GDALGetDataTypeSize( eDataType ) / 8;

    pabyScanLine = (GByte*) VSIMalloc2( nRecordSize, poDS->nBands );

    if( pabyScanLine == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "IdrisiRasterBand::IdrisiRasterBand : Out of memory (nRasterXSize = %d)",
                  poDS->GetRasterXSize());
    }

    nRecordSize *= poDS->nBands;
}

/************************************************************************/
/*                         ~IdrisiRasterBand()                          */
/************************************************************************/

IdrisiRasterBand::~IdrisiRasterBand()
{
    CPLFree( pabyScanLine );

    if( poDefaultRAT )
    {
        delete poDefaultRAT;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr IdrisiRasterBand::IReadBlock( int nBlockXOff, 
                                     int nBlockYOff,
                                     void *pImage )
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if( VSIFSeekL( poGDS->fp, 
        vsi_l_offset(nRecordSize) * nBlockYOff, SEEK_SET ) < 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
            "Can't seek(%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    if( (int) VSIFReadL( pabyScanLine, 1, nRecordSize, poGDS->fp ) < nRecordSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
            "Can't read(%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    if( poGDS->nBands == 3 ) 
    {
        int i, j;
        for( i = 0, j = ( 3 - nBand ); i < nBlockXSize; i++, j += 3 )
        {
            ( (GByte*) pImage )[i] = pabyScanLine[j];
        }
    }
    else
    {
        memcpy( pImage, pabyScanLine, nRecordSize );
    }

#ifdef CPL_MSB    
    if( eDataType == GDT_Float32 )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );
#endif

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr IdrisiRasterBand::IWriteBlock( int nBlockXOff, 
                                      int nBlockYOff,
                                      void *pImage )
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

#ifdef CPL_MSB    
    // Swap in input buffer if needed.
    if( eDataType == GDT_Float32 )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );
#endif

    if( poGDS->nBands == 1 )
    {
        memcpy( pabyScanLine, pImage, nRecordSize );
    }
    else
    {
        if( nBand > 1 ) 
        {
            VSIFSeekL( poGDS->fp, 
                vsi_l_offset(nRecordSize) * nBlockYOff, SEEK_SET );
            VSIFReadL( pabyScanLine, 1, nRecordSize, poGDS->fp );
        }
        int i, j;
        for( i = 0, j = ( 3 - nBand ); i < nBlockXSize; i++, j += 3 )
        {
            pabyScanLine[j] = ( (GByte *) pImage )[i];
        }
    }

#ifdef CPL_MSB    
    // Swap input buffer back to original form.
    if( eDataType == GDT_Float32 )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );
#endif

    VSIFSeekL( poGDS->fp, vsi_l_offset(nRecordSize) * nBlockYOff, SEEK_SET );

    if( (int) VSIFWriteL( pabyScanLine, 1, nRecordSize, poGDS->fp ) < nRecordSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
            "Can't write(%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    int bHasNoDataValue = FALSE;
    float fNoDataValue = (float) GetNoDataValue(&bHasNoDataValue);

    // -------------------------------------------------------------------- 
    //      Search for the minimum and maximum values
    // -------------------------------------------------------------------- 

    int i, j;

    if( eDataType == GDT_Float32 )
    {
        for( i = 0; i < nBlockXSize; i++ )
        {
            float fVal = ((float*) pabyScanLine)[i];
            if( !bHasNoDataValue || fVal != fNoDataValue )
            {
                if( bFirstVal )
                {
                    fMinimum = fMaximum = fVal;
                    bFirstVal = false;
                }
                else
                {
                    if( fVal < fMinimum) fMinimum = fVal;
                    if( fVal > fMaximum) fMaximum = fVal;
                }
            }
        }
    }
    else if( eDataType == GDT_Int16 )
    {
        for( i = 0; i < nBlockXSize; i++ )
        {
            float fVal = (float) ((GInt16*) pabyScanLine)[i];
            if( !bHasNoDataValue || fVal != fNoDataValue )
            {
                if( bFirstVal )
                {
                    fMinimum = fMaximum = fVal;
                    bFirstVal = false;
                }
                else
                {
                    if( fVal < fMinimum) fMinimum = fVal;
                    if( fVal > fMaximum) fMaximum = fVal;
                }
            }
        }
    }
    else if( poGDS->nBands == 1 )
    {
        for( i = 0; i < nBlockXSize; i++ )
        {
            float fVal = (float) ((GByte*) pabyScanLine)[i];
            if( !bHasNoDataValue || fVal != fNoDataValue )
            {
                if( bFirstVal )
                {
                    fMinimum = fMaximum = fVal;
                    bFirstVal = false;
                }
                else
                {
                    if( fVal < fMinimum) fMinimum = fVal;
                    if( fVal > fMaximum) fMaximum = fVal;
                }
            }
        }
    }
    else
    {
        for( i = 0, j = ( 3 - nBand ); i < nBlockXSize; i++, j += 3 )
        {
            float fVal = (float) ((GByte*) pabyScanLine)[j];
            if( !bHasNoDataValue || fVal != fNoDataValue )
            {
                if( bFirstVal )
                {
                    fMinimum = fMaximum = fVal;
                    bFirstVal = false;
                }
                else
                {
                    if( fVal < fMinimum) fMinimum = fVal;
                    if( fVal > fMaximum) fMaximum = fVal;
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double IdrisiRasterBand::GetMinimum( int *pbSuccess )
{      
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (CSLFetchNameValue( poGDS->papszRDC, rdcMIN_VALUE ) == NULL)
        return GDALPamRasterBand::GetMinimum(pbSuccess);

    double adfMinValue[3];
    sscanf( CSLFetchNameValue( poGDS->papszRDC, rdcMIN_VALUE ), "%lf %lf %lf", 
        &adfMinValue[0], &adfMinValue[1], &adfMinValue[2] );

    if( pbSuccess )
  {
        *pbSuccess = true;
  }

    return adfMinValue[this->nBand - 1];
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double IdrisiRasterBand::GetMaximum( int *pbSuccess )
{      
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (CSLFetchNameValue( poGDS->papszRDC, rdcMAX_VALUE ) == NULL)
        return GDALPamRasterBand::GetMinimum(pbSuccess);

    double adfMaxValue[3];
    sscanf( CSLFetchNameValue( poGDS->papszRDC, rdcMAX_VALUE ), "%lf %lf %lf", 
        &adfMaxValue[0], &adfMaxValue[1], &adfMaxValue[2] );

    if( pbSuccess )
  {
        *pbSuccess = true;
  }

    return adfMaxValue[this->nBand - 1];
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double IdrisiRasterBand::GetNoDataValue( int *pbSuccess )
{      
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    double dfNoData;
    const char *pszFlagDefn;

    if( CSLFetchNameValue( poGDS->papszRDC, rdcFLAG_DEFN ) != NULL )
        pszFlagDefn = CSLFetchNameValue( poGDS->papszRDC, rdcFLAG_DEFN );
    else if( CSLFetchNameValue( poGDS->papszRDC, rdcFLAG_DEFN2 ) != NULL )
        pszFlagDefn = CSLFetchNameValue( poGDS->papszRDC, rdcFLAG_DEFN2 );
    else
        pszFlagDefn = CPLStrdup( "none" );

    // ------------------------------------------------------------------------
    // If Flag_Def is not "none", Flag_Value means "background" 
    // or "missing data"
    // ------------------------------------------------------------------------

    if( ! EQUAL( pszFlagDefn, "none" ) )
    {
        dfNoData = atof_nz( CSLFetchNameValue( poGDS->papszRDC, rdcFLAG_VALUE ) );
        if( pbSuccess )
            *pbSuccess = TRUE;
    }
    else
    {
        dfNoData = -9999.0;    /* this value should be ignored */
        if( pbSuccess )
            *pbSuccess = FALSE;
    }

    return dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr IdrisiRasterBand::SetNoDataValue( double dfNoDataValue )
{      
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    poGDS->papszRDC = 
        CSLSetNameValue( poGDS->papszRDC, rdcFLAG_VALUE, CPLSPrintf( "%.7g", dfNoDataValue ) );
    poGDS->papszRDC = 
        CSLSetNameValue( poGDS->papszRDC, rdcFLAG_DEFN,  "missing data" );

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp IdrisiRasterBand::GetColorInterpretation()
{               
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if( poGDS->nBands == 3 )
    {
        switch( nBand )
        {
        case 1: return GCI_BlueBand;
        case 2: return GCI_GreenBand;
        case 3: return GCI_RedBand;
        }
    }
    else if( poGDS->poColorTable->GetColorEntryCount() > 0 )
    {
        return GCI_PaletteIndex;
    }
    return GCI_GrayIndex;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **IdrisiRasterBand::GetCategoryNames()
{      
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    return poGDS->papszCategories;
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr IdrisiRasterBand::SetCategoryNames( char **papszCategoryNames )
{      
    int nCatCount = CSLCount( papszCategoryNames );

    if( nCatCount == 0 )
        return CE_None;

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLDestroy( poGDS->papszCategories );
    poGDS->papszCategories = CSLDuplicate( papszCategoryNames );

    // ------------------------------------------------------
    //        Search for the "Legend cats  : N" line
    // ------------------------------------------------------

    int i, nLine = -1;
    for( i = 0;( i < CSLCount( poGDS->papszRDC ) ) &&( nLine == -1 ); i++ )
        if( EQUALN( poGDS->papszRDC[i], rdcLEGEND_CATS, 12 ) )
            nLine = i;

    if( nLine < 0 )
        return CE_None;

    int nCount = atoi_nz( CSLFetchNameValue( poGDS->papszRDC, rdcLEGEND_CATS ) );

    // ------------------------------------------------------
    //        Delte old instance of the categoty names
    // ------------------------------------------------------

    if( nCount > 0 )
        poGDS->papszRDC = CSLRemoveStrings( poGDS->papszRDC, nLine + 1, nCount, NULL );

    nCount = 0;

    for( i = 0; i < nCatCount; i++ )
    {
        if( ( strlen( papszCategoryNames[i] ) > 0 ) )
        {
            poGDS->papszRDC = CSLInsertString( poGDS->papszRDC,( nLine + nCount + 1 ), 
                CPLSPrintf( "%s:%s", CPLSPrintf( rdcCODE_N, i ), papszCategoryNames[i] ) );
            nCount++;
        }
    }

    poGDS->papszRDC = CSLSetNameValue( poGDS->papszRDC, rdcLEGEND_CATS, CPLSPrintf( "%d", nCount ) );

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *IdrisiRasterBand::GetColorTable()
{               
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if( poGDS->poColorTable->GetColorEntryCount() == 0 )
    {
        return NULL;
    }
    else
    {
        return poGDS->poColorTable;
    }
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr IdrisiRasterBand::SetColorTable( GDALColorTable *poColorTable )
{      
    if( poColorTable == NULL )
    {
        return CE_None;
    }

    if( poColorTable->GetColorEntryCount() == 0 )
    {
        return CE_None;
    }

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    delete poGDS->poColorTable;

    poGDS->poColorTable = poColorTable->Clone();

    const char *pszSMPFilename;
    pszSMPFilename = CPLResetExtension( poGDS->pszFilename, extSMP );
    VSILFILE *fpSMP;

    if( ( fpSMP = VSIFOpenL( pszSMPFilename, "w" ) ) != NULL )
    {
        VSIFWriteL( "[Idrisi]", 8, 1, fpSMP );
        GByte nPlatform = 1;    VSIFWriteL( &nPlatform, 1, 1, fpSMP );
        GByte nVersion = 11;    VSIFWriteL( &nVersion, 1, 1, fpSMP );
        GByte nDepth = 8;       VSIFWriteL( &nDepth, 1, 1, fpSMP );
        GByte nHeadSz = 18;     VSIFWriteL( &nHeadSz, 1, 1, fpSMP );
        GUInt16 nCount = 255;   VSIFWriteL( &nCount, 2, 1, fpSMP );
        GUInt16 nMix = 0;       VSIFWriteL( &nMix, 2, 1, fpSMP );
        GUInt16 nMax = 255;     VSIFWriteL( &nMax, 2, 1, fpSMP );

        GDALColorEntry oEntry;
        GByte aucRGB[3];
        int i;

        for( i = 0; i < poColorTable->GetColorEntryCount(); i++ )
        {
            poColorTable->GetColorEntryAsRGB( i, &oEntry );
            aucRGB[0] = (GByte) oEntry.c1;
            aucRGB[1] = (GByte) oEntry.c2;
            aucRGB[2] = (GByte) oEntry.c3;
            VSIFWriteL( &aucRGB, 3, 1, fpSMP );
        }
        /* smp files always have 256 occurences */
        for( i = poColorTable->GetColorEntryCount(); i <= 255; i++ )
        {
            poColorTable->GetColorEntryAsRGB( i, &oEntry );
            aucRGB[0] = (GByte) 0;
            aucRGB[1] = (GByte) 0;
            aucRGB[2] = (GByte) 0;
            VSIFWriteL( &aucRGB, 3, 1, fpSMP );
        }
        VSIFCloseL( fpSMP );
    }

    return CE_None;
}

/************************************************************************/
/*                           GetUnitType()                              */
/************************************************************************/

const char *IdrisiRasterBand::GetUnitType()
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    return poGDS->pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr IdrisiRasterBand::SetUnitType( const char *pszUnitType )
{      
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if( strlen( pszUnitType ) == 0 )
    {
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcVALUE_UNITS, "unspecified" );
    }
    else
    {
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcVALUE_UNITS, pszUnitType );
    }

    return CE_None;
}

/************************************************************************/
/*                             SetMinMax()                              */
/************************************************************************/

CPLErr IdrisiRasterBand::SetMinMax( double dfMin, double dfMax )
{      
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;
    
    fMinimum = (float)dfMin;
    fMaximum = (float)dfMax;

    double adfMin[3] = {0.0, 0.0, 0.0};
    double adfMax[3] = {0.0, 0.0, 0.0};

    if (CSLFetchNameValue( poGDS->papszRDC, rdcMIN_VALUE ) != NULL)
        sscanf( CSLFetchNameValue( poGDS->papszRDC, rdcMIN_VALUE ), "%lf %lf %lf", &adfMin[0], &adfMin[1], &adfMin[2] );
    if (CSLFetchNameValue( poGDS->papszRDC, rdcMAX_VALUE ) != NULL)
        sscanf( CSLFetchNameValue( poGDS->papszRDC, rdcMAX_VALUE ), "%lf %lf %lf", &adfMax[0], &adfMax[1], &adfMax[2] );

    adfMin[nBand - 1] = dfMin;
    adfMax[nBand - 1] = dfMax;

    if( poGDS->nBands == 3 )
    {
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcMIN_VALUE,   CPLSPrintf( "%.8g %.8g %.8g", adfMin[0], adfMin[1], adfMin[2] ) );
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcMAX_VALUE,   CPLSPrintf( "%.8g %.8g %.8g", adfMax[0], adfMax[1], adfMax[2] ) );
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcDISPLAY_MIN, CPLSPrintf( "%.8g %.8g %.8g", adfMin[0], adfMin[1], adfMin[2] ) );
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcDISPLAY_MAX, CPLSPrintf( "%.8g %.8g %.8g", adfMax[0], adfMax[1], adfMax[2] ) );
    }
    else
    {
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcMIN_VALUE,   CPLSPrintf( "%.8g", adfMin[0] ) );
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcMAX_VALUE,   CPLSPrintf( "%.8g", adfMax[0] ) );
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcDISPLAY_MIN, CPLSPrintf( "%.8g", adfMin[0] ) );
        poGDS->papszRDC = 
            CSLSetNameValue( poGDS->papszRDC, rdcDISPLAY_MAX, CPLSPrintf( "%.8g", adfMax[0] ) );
    }

    return CE_None;
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

CPLErr IdrisiRasterBand::SetStatistics( double dfMin, double dfMax, double dfMean, double dfStdDev )
{
    SetMinMax(dfMin, dfMax);

    return GDALPamRasterBand::SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr IdrisiRasterBand::SetDefaultRAT( const GDALRasterAttributeTable *poRAT )
{
    if( ! poRAT )
    {
        return CE_Failure;
    }

    // ----------------------------------------------------------
    // Get field indecies
    // ----------------------------------------------------------

    int iValue = -1;
    int iRed   = poRAT->GetColOfUsage( GFU_Red );
    int iGreen = poRAT->GetColOfUsage( GFU_Green );
    int iBlue  = poRAT->GetColOfUsage( GFU_Blue );
    int iName  = -1;
    int i;

    GDALColorTable *poCT = NULL;
    char **papszNames = NULL;

    int nFact  = 1;

    // ----------------------------------------------------------
    // Seek for "Value" field index (AGIS standards field name)
    // ----------------------------------------------------------

    if( GetColorTable() == 0 || GetColorTable()->GetColorEntryCount() == 0 )
    {
        for( i = 0; i < poRAT->GetColumnCount(); i++ )
        {
            if EQUALN( "Value", poRAT->GetNameOfCol( i ), 5 ) 
            {
                iValue = i;
                break;
            }
        }

        if( iRed != -1 && iGreen != -1 && iBlue != -1 )
        {
            poCT  = new GDALColorTable();
            nFact = poRAT->GetTypeOfCol( iRed ) == GFT_Real ? 255 : 1;
        }
    }

    // ----------------------------------------------------------
    // Seek for Name field index
    // ----------------------------------------------------------

    if( GetCategoryNames() == 0 || CSLCount( GetCategoryNames() ) == 0 )
    {
        iName  = poRAT->GetColOfUsage( GFU_Name );
        if( iName == -1 )
        {
            for( i = 0; i < poRAT->GetColumnCount(); i++ )
            {
                if EQUALN( "Class_Name", poRAT->GetNameOfCol( i ), 10 )
                {
                    iName = i;
                    break;
                } 
                else if EQUALN( "Categor", poRAT->GetNameOfCol( i ), 7 )
                {
                    iName = i;
                    break;
                } 
                else if EQUALN( "Name",  poRAT->GetNameOfCol( i ), 4 )
                {
                    iName = i;
                    break;
                }
            }
        }

        /* if still can't find it use the first String column */

        if( iName == -1 )
        {   
            for( i = 0; i < poRAT->GetColumnCount(); i++ )
            {
                if( poRAT->GetTypeOfCol( i ) == GFT_String )
                {
                    iName = i;
                    break;
                }
            }
        }

        // ----------------------------------------------------------
        // Incomplete Attribute Table;
        // ----------------------------------------------------------

        if( iName == -1 )
        {
            iName = iValue;
        }
    }

    // ----------------------------------------------------------
    // Initialization
    // ----------------------------------------------------------

    double dRed     = 0.0;
    double dGreen   = 0.0;
    double dBlue    = 0.0;

    // ----------------------------------------------------------
    // Load values
    // ----------------------------------------------------------

    GDALColorEntry  sColor;
    int iEntry      = 0;
    int iOut        = 0;
    int nEntryCount = poRAT->GetRowCount();
    int nValue      = 0;

    if( iValue != -1 )
    {
        nValue = poRAT->GetValueAsInt( iEntry, iValue );
    }

    for( iOut = 0; iOut < 65535 && ( iEntry < nEntryCount ); iOut++ )
    {
        if( iOut == nValue )
        {
            if( poCT )
            {
                dRed    = poRAT->GetValueAsDouble( iEntry, iRed );
                dGreen  = poRAT->GetValueAsDouble( iEntry, iGreen );
                dBlue   = poRAT->GetValueAsDouble( iEntry, iBlue );
                sColor.c1  = (short) ( dRed   * nFact );
                sColor.c2  = (short) ( dGreen * nFact );
                sColor.c3  = (short) ( dBlue  * nFact );
                sColor.c4  = (short) ( 255    / nFact );    
                poCT->SetColorEntry( iEntry, &sColor );
            }

            if( iName != -1 )
            {
                papszNames = CSLAddString( papszNames, 
                    poRAT->GetValueAsString( iEntry, iName ) );
            }

            /* Advance on the table */

            if( ( ++iEntry ) < nEntryCount )
            {
                if( iValue != -1 )
                    nValue = poRAT->GetValueAsInt( iEntry, iValue );
                else
                    nValue = iEntry;
            }
        }
        else if( iOut < nValue )
        {
            if( poCT )
            {
                sColor.c1  = (short) 0;
                sColor.c2  = (short) 0;
                sColor.c3  = (short) 0;
                sColor.c4  = (short) 255;    
                poCT->SetColorEntry( iEntry, &sColor );
            }

            if( iName != -1 )
                papszNames = CSLAddString( papszNames, "" );
        }
    }

    // ----------------------------------------------------------
    // Set Color Table
    // ----------------------------------------------------------

    if( poCT )
    {
        SetColorTable( poCT );
        delete poCT;
    }

    // ----------------------------------------------------------
    // Update Category Names
    // ----------------------------------------------------------

    if( papszNames )
    {
        SetCategoryNames( papszNames );
        CSLDestroy( papszNames );
    }

    // ----------------------------------------------------------
    // Update Attribute Table
    // ----------------------------------------------------------

    if( poDefaultRAT ) 
    {
        delete poDefaultRAT;
    }

    poDefaultRAT = poRAT->Clone();

    return CE_None;
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

const GDALRasterAttributeTable *IdrisiRasterBand::GetDefaultRAT()
{        
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if( poGDS->papszCategories == NULL )
    {
        return NULL;
    }

    bool bHasColorTable = poGDS->poColorTable->GetColorEntryCount() > 0;

    // ----------------------------------------------------------
    // Create the bands Attribute Table
    // ----------------------------------------------------------

    if( poDefaultRAT ) 
    {
        delete poDefaultRAT;
    }

    poDefaultRAT = new GDALRasterAttributeTable();

    // ----------------------------------------------------------
    // Create (Value, Red, Green, Blue, Alpha, Class_Name) fields
    // ----------------------------------------------------------

    poDefaultRAT->CreateColumn( "Value",      GFT_Integer, GFU_Generic );
    poDefaultRAT->CreateColumn( "Value_1",    GFT_Integer, GFU_MinMax );

    if( bHasColorTable )
    {
        poDefaultRAT->CreateColumn( "Red",    GFT_Integer, GFU_Red );
        poDefaultRAT->CreateColumn( "Green",  GFT_Integer, GFU_Green );
        poDefaultRAT->CreateColumn( "Blue",   GFT_Integer, GFU_Blue );
        poDefaultRAT->CreateColumn( "Alpha",  GFT_Integer, GFU_Alpha );
    }
    poDefaultRAT->CreateColumn( "Class_name", GFT_String,  GFU_Name );

    // ----------------------------------------------------------
    // Loop throught the Category Names
    // ----------------------------------------------------------

    GDALColorEntry sEntry;
    int iName = poDefaultRAT->GetColOfUsage( GFU_Name );
    int nEntryCount = CSLCount( poGDS->papszCategories );
    int iEntry = 0;
    int iRows = 0;

    for( iEntry = 0; iEntry < nEntryCount; iEntry++ )
    {
        if EQUAL( poGDS->papszCategories[iEntry], "" )
        {
            continue; // Eliminate the empty ones
        }
        poDefaultRAT->SetRowCount( poDefaultRAT->GetRowCount() + 1 );
        poDefaultRAT->SetValue( iRows, 0, iEntry );
        poDefaultRAT->SetValue( iRows, 1, iEntry );
        if( bHasColorTable )
        {
            poGDS->poColorTable->GetColorEntryAsRGB( iEntry, &sEntry );
            poDefaultRAT->SetValue( iRows, 2, sEntry.c1 );
            poDefaultRAT->SetValue( iRows, 3, sEntry.c2 );
            poDefaultRAT->SetValue( iRows, 4, sEntry.c3 );
            poDefaultRAT->SetValue( iRows, 5, sEntry.c4 );
        }
        poDefaultRAT->SetValue( iRows, iName, poGDS->papszCategories[iEntry] );
        iRows++;
    }
  
    return poDefaultRAT;
}

/************************************************************************/
/*                       IdrisiGeoReference2Wkt()                       */
/************************************************************************/

/***
* Converts Idrisi geographic reference information to OpenGIS WKT.
* 
* The Idrisi metadata file contain two fields that describe the  
* geographic reference, RefSystem and RefUnit. 
* 
* RefSystem can contains the world "plane" or the name of a georeference 
* file <refsystem>.ref that details the geographic reference
* system( coordinate system and projection parameters ). RefUnits 
* indicates the unit of the image bounds. 
* 
* The georeference files are generally located in the product installation 
* folder $IDRISIDIR\Georef, but they are first looked for in the same  
* folder as the data file. 
*  
* If a Reference system names can be recognized by a name convention 
* it will be interpreted without the need to read the georeference file. 
* That includes "latlong" and all the UTM and State Plane zones.  
* 
* RefSystem "latlong" means that the data is not project and the coordinate 
* system is WGS84. RefSystem "plane" means that the there is no coordinate 
* system but the it is possible to calculate areas and distance by looking
* at the RefUnits.
*  
* If the environment variable IDRISIDIR is not set and the georeference file  
* need to be read then the projection string will result as unknown. 
***/

CPLErr IdrisiGeoReference2Wkt( const char* pszFilename,
                               const char *pszRefSystem,
                               const char *pszRefUnits,
                               char **ppszProjString )
{
    OGRSpatialReference oSRS;

    *ppszProjString = NULL;

    // ---------------------------------------------------------
    //  Plane 
    // ---------------------------------------------------------

    if( EQUAL( pszRefSystem, rstPLANE ) )
    {
        oSRS.SetLocalCS( "Plane" );
        int nUnit = GetUnitIndex( pszRefUnits );
        if( nUnit > -1 )
        {
            int nDeft = aoLinearUnitsConv[nUnit].nDefaultG;
            oSRS.SetLinearUnits( aoLinearUnitsConv[nDeft].pszName,
                aoLinearUnitsConv[nDeft].dfConv );
        }
        oSRS.exportToWkt( ppszProjString );
        return CE_None;
    }

    // ---------------------------------------------------------
    //  Latlong
    // ---------------------------------------------------------

    if( EQUAL( pszRefSystem, rstLATLONG  ) || 
        EQUAL( pszRefSystem, rstLATLONG2 ) )
    {
        oSRS.SetWellKnownGeogCS( "WGS84" );
        oSRS.exportToWkt( ppszProjString );
        return CE_None;
    }

    // ---------------------------------------------------------
    //  Prepare for scanning in lower case
    // ---------------------------------------------------------

    char *pszRefSystemLower;
    pszRefSystemLower = CPLStrdup( pszRefSystem );
    CPLStrlwr( pszRefSystemLower );

    // ---------------------------------------------------------
    //  UTM naming convention( ex.: utm-30n )
    // ---------------------------------------------------------

    if( EQUALN( pszRefSystem, rstUTM, 3 ) )
    {
        int    nZone;
        char cNorth;
        sscanf( pszRefSystemLower, rstUTM, &nZone, &cNorth );
        oSRS.SetWellKnownGeogCS( "WGS84" );
        oSRS.SetUTM( nZone,( cNorth == 'n' ) );
        oSRS.exportToWkt( ppszProjString );
        CPLFree( pszRefSystemLower );
        return CE_None;
    }

    // ---------------------------------------------------------
    //  State Plane naming convention( ex.: spc83ma1 )
    // ---------------------------------------------------------

    if( EQUALN( pszRefSystem, rstSPC, 3 ) )
    {
        int nNAD;
        int nZone;
        char szState[3];
        sscanf( pszRefSystemLower, rstSPC, &nNAD, szState, &nZone );
        int nSPCode = GetStateCode( szState );
        if( nSPCode != -1 )
        {
            nZone = ( nZone == 1 ? nSPCode : nSPCode + nZone - 1 );

            if( oSRS.SetStatePlane( nZone, ( nNAD == 83 ) ) != OGRERR_FAILURE )
            {
                oSRS.exportToWkt( ppszProjString );
                CPLFree( pszRefSystemLower );
                return CE_None;
            }

            // ----------------------------------------------------------
            //  If SetStatePlane fails, set GeoCS as NAD Datum and let it
            //  try to read the projection info from georeference file( * )
            // ----------------------------------------------------------

            oSRS.SetWellKnownGeogCS( CPLSPrintf( "NAD%d", nNAD ) );
        }
    }
    
    CPLFree( pszRefSystemLower );
    pszRefSystemLower = NULL;

    // ------------------------------------------------------------------
    //  Search for georeference file <RefSystem>.ref
    // ------------------------------------------------------------------

    const char *pszFName = CPLSPrintf( "%s%c%s.ref", 
        CPLGetDirname( pszFilename ), PATHDELIM,  pszRefSystem );

    if( ! FileExists( pszFName ) )
    {
        // ------------------------------------------------------------------
        //  Look at $IDRISIDIR\Georef\<RefSystem>.ref
        // ------------------------------------------------------------------

        const char *pszIdrisiDir = CPLGetConfigOption( "IDRISIDIR", NULL );

        if( ( pszIdrisiDir ) != NULL )
        {
            pszFName = CPLSPrintf( "%s%cgeoref%c%s.ref", 
                pszIdrisiDir, PATHDELIM, PATHDELIM, pszRefSystem );
        }
    }

    // ------------------------------------------------------------------
    //  Cannot find georeference file
    // ------------------------------------------------------------------

    if( ! FileExists( pszFName ) )
    {
        CPLDebug( "RST", "Cannot find Idrisi georeference file %s",
            pszRefSystem );

        if( oSRS.IsGeographic() == FALSE ) /* see State Plane remarks( * ) */
        {
            oSRS.SetLocalCS( "Unknown" );
            int nUnit = GetUnitIndex( pszRefUnits );
            if( nUnit > -1 )
            {
                int nDeft = aoLinearUnitsConv[nUnit].nDefaultG;
                oSRS.SetLinearUnits( aoLinearUnitsConv[nDeft].pszName,
                    aoLinearUnitsConv[nDeft].dfConv );
            }
        }
        oSRS.exportToWkt( ppszProjString );
        return CE_Failure;
    }

    // ------------------------------------------------------------------
    //  Read values from georeference file 
    // ------------------------------------------------------------------

    char **papszRef = CSLLoad( pszFName );
    CSLSetNameValueSeparator( papszRef, ":" );

    char *pszGeorefName;

    const char* pszREF_SYSTEM = CSLFetchNameValue( papszRef, refREF_SYSTEM );
    if( pszREF_SYSTEM != NULL && EQUAL( pszREF_SYSTEM, "" ) == FALSE )
    {
        pszGeorefName           = CPLStrdup( pszREF_SYSTEM );
    }
    else
    {
        pszGeorefName           = CPLStrdup( CSLFetchNameValue( papszRef, refREF_SYSTEM2 ) );
    }
    char *pszProjName           = CPLStrdup( CSLFetchNameValue( papszRef, refPROJECTION ) );
    char *pszDatum              = CPLStrdup( CSLFetchNameValue( papszRef, refDATUM ) );
    char *pszEllipsoid          = CPLStrdup( CSLFetchNameValue( papszRef, refELLIPSOID ) );
    double dfCenterLat          = atof_nz( CSLFetchNameValue( papszRef, refORIGIN_LAT ) );
    double dfCenterLong         = atof_nz( CSLFetchNameValue( papszRef, refORIGIN_LONG ) );
    double dfSemiMajor          = atof_nz( CSLFetchNameValue( papszRef, refMAJOR_SAX ) );
    double dfSemiMinor          = atof_nz( CSLFetchNameValue( papszRef, refMINOR_SAX ) );
    double dfFalseEasting       = atof_nz( CSLFetchNameValue( papszRef, refORIGIN_X ) );
    double dfFalseNorthing      = atof_nz( CSLFetchNameValue( papszRef, refORIGIN_Y ) );
    double dfStdP1              = atof_nz( CSLFetchNameValue( papszRef, refSTANDL_1 ) );
    double dfStdP2              = atof_nz( CSLFetchNameValue( papszRef, refSTANDL_2 ) );
    double dfScale;
    double adfToWGS84[3] = { 0.0, 0.0, 0.0 };

    const char* pszToWGS84 = CSLFetchNameValue( papszRef, refDELTA_WGS84 );
    if (pszToWGS84)
        sscanf( pszToWGS84, "%lf %lf %lf",
            &adfToWGS84[0], &adfToWGS84[1], &adfToWGS84[2] );

    const char* pszSCALE_FAC = CSLFetchNameValue( papszRef, refSCALE_FAC );
    if( pszSCALE_FAC == NULL || EQUAL( pszSCALE_FAC, "na" ) )
        dfScale = 1.0;
    else
        dfScale = atof_nz( pszSCALE_FAC );

    CSLDestroy( papszRef );

    // ----------------------------------------------------------------------
    //  Set the Geographic Coordinate System
    // ----------------------------------------------------------------------

    if( oSRS.IsGeographic() == FALSE ) /* see State Plane remarks(*) */
    {
        int nEPSG = 0;

        // ----------------------------------------------------------------------
        //  Is it a WGS84 equivalent?
        // ----------------------------------------------------------------------

        if( ( EQUALN( pszEllipsoid, "WGS", 3 ) ) &&( strstr( pszEllipsoid, "84" ) ) &&
            ( EQUALN( pszDatum, "WGS", 3 ) )     &&( strstr( pszDatum, "84" ) ) &&
            ( adfToWGS84[0] == 0.0 ) &&( adfToWGS84[1] == 0.0 ) &&( adfToWGS84[2] == 0.0 ) )
        {
            nEPSG = 4326;
        }

        // ----------------------------------------------------------------------
        //  Match GCS's DATUM_NAME by using 'ApproxString' over Datum 
        // ----------------------------------------------------------------------

        if( nEPSG == 0 )
        {
            nEPSG = atoi_nz( CSVGetField( CSVFilename( "gcs.csv" ), 
                "DATUM_NAME", pszDatum, CC_ApproxString, "COORD_REF_SYS_CODE" ) );
        }

        // ----------------------------------------------------------------------
        //  Match GCS's COORD_REF_SYS_NAME by using 'ApproxString' over Datum 
        // ----------------------------------------------------------------------

        if( nEPSG == 0 )
        {
            nEPSG = atoi_nz( CSVGetField( CSVFilename( "gcs.csv" ), 
                "COORD_REF_SYS_NAME", pszDatum, CC_ApproxString, "COORD_REF_SYS_CODE" ) );
        }

        if( nEPSG != 0 )
        {
            oSRS.importFromEPSG( nEPSG );
        }
        else
        {
            // --------------------------------------------------
            //  Create GeogCS based on the georeference file info
            // --------------------------------------------------

            oSRS.SetGeogCS( pszRefSystem, 
                pszDatum, 
                pszEllipsoid, 
                dfSemiMajor, 
                (dfSemiMinor == dfSemiMajor) ? 0.0 : ( -1.0 /( dfSemiMinor / dfSemiMajor - 1.0 ) ) );
        }

        // ----------------------------------------------------------------------
        //  Note: That will override EPSG info:
        // ----------------------------------------------------------------------

        oSRS.SetTOWGS84( adfToWGS84[0], adfToWGS84[1], adfToWGS84[2] );
    }

    // ----------------------------------------------------------------------
    //  If the georeference file tells that it is a non project system:
    // ----------------------------------------------------------------------

    if( EQUAL( pszProjName, "none" ) )
    {
        oSRS.exportToWkt( ppszProjString );
        
        CPLFree( pszGeorefName );
        CPLFree( pszProjName );
        CPLFree( pszDatum );
        CPLFree( pszEllipsoid );
        
        return CE_None;
    }

    // ----------------------------------------------------------------------
    //  Create Projection information based on georeference file info
    // ----------------------------------------------------------------------

    //  Idrisi user's Manual,   Supported Projection:
    //
    //      Mercator
    //      Transverse Mercator
    //      Gauss-Kruger
    //      Lambert Conformal Conic
    //      Plate Carree
    //      Hammer Aitoff
    //      Lambert North Polar Azimuthal Equal Area
    //      Lambert South Polar Azimuthal Equal Area
    //      Lambert Transverse Azimuthal Equal Area
    //      Lambert Oblique Polar Azimuthal Equal Area
    //      North Polar Stereographic
    //      South Polar Stereographic
    //      Transverse Stereographic
    //      Oblique Stereographic
    //      Albers Equal Area Conic
    //      Sinusoidal
    //

    if( EQUAL( pszProjName, "Mercator" ) )
    {
        oSRS.SetMercator( dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing );
    }
    else if EQUAL( pszProjName, "Transverse Mercator" )
    {
        oSRS.SetTM( dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing );
    }
    else if EQUALN( pszProjName, "Gauss-Kruger", 9 )
    {
        oSRS.SetTM( dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing );
    }
    else if( EQUAL( pszProjName, "Lambert Conformal Conic" ) )
    {
        oSRS.SetLCC( dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing );
    }
    else if( EQUALN( pszProjName, "Plate Carr" "\xE9" "e", 10 ) ) /* 'eacute' in ISO-8859-1 */
    {
        oSRS.SetEquirectangular( dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing );
    }
    else if( EQUAL( pszProjName, "Hammer Aitoff" ) )
    {
        oSRS.SetProjection( pszProjName );
        oSRS.SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN,  dfCenterLat );
        oSRS.SetProjParm( SRS_PP_CENTRAL_MERIDIAN,    dfCenterLong );
        oSRS.SetProjParm( SRS_PP_FALSE_EASTING,       dfFalseEasting );
        oSRS.SetProjParm( SRS_PP_FALSE_NORTHING,      dfFalseNorthing );
    }
    else if( EQUALN( pszProjName, "Lambert North Polar Azimuthal Equal Area", 15 ) ||
        EQUALN( pszProjName, "Lambert South Polar Azimuthal Equal Area", 15 ) ||
        EQUALN( pszProjName, "Lambert Transverse Azimuthal Equal Area", 15 ) ||
        EQUALN( pszProjName, "Lambert Oblique Polar Azimuthal Equal Area", 15 ) )
    {
        oSRS.SetLAEA( dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing );
    }
    else if( EQUALN( pszProjName, "North Polar Stereographic", 15 ) ||
        EQUALN( pszProjName, "South Polar Stereographic", 15 ) )
    {
        oSRS.SetPS( dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing );
    }
    else if( EQUALN( pszProjName, "Transverse Stereographic", 15 ) )
    {
        oSRS.SetStereographic( dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing );
    }
    else if( EQUALN( pszProjName, "Oblique Stereographic", 15 ) )
    {
        oSRS.SetOS( dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing );
    }
    else if( EQUAL( pszProjName, "Alber's Equal Area Conic" ) ||
        EQUAL( pszProjName, "Albers Equal Area Conic" ) )
    {
        oSRS.SetACEA( dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing );
    }
    else if( EQUAL( pszProjName, "Sinusoidal" ) )
    {
        oSRS.SetSinusoidal( dfCenterLong, dfFalseEasting, dfFalseNorthing );
    }
    else
    {
        CPLError( CE_Warning, CPLE_NotSupported, 
            "Projection not listed on Idrisi User's Manual( v.15.0/2005 ).\n\t" 
            "[\"%s\" in georeference file \"%s\"]",
            pszProjName, pszFName );
        oSRS.Clear();
        oSRS.exportToWkt( ppszProjString );
        
        CPLFree( pszGeorefName );
        CPLFree( pszProjName );
        CPLFree( pszDatum );
        CPLFree( pszEllipsoid );
        
        return CE_Warning;
    }

    // ----------------------------------------------------------------------
    //  Set the Linear Units
    // ----------------------------------------------------------------------

    int nUnit = GetUnitIndex( pszRefUnits );
    if( nUnit > -1 )
    {
        int nDeft = aoLinearUnitsConv[nUnit].nDefaultG;
        oSRS.SetLinearUnits( aoLinearUnitsConv[nDeft].pszName,
            aoLinearUnitsConv[nDeft].dfConv );
    }
    else
    {
        oSRS.SetLinearUnits( "unknown",  1.0 );
    }

    // ----------------------------------------------------------------------
    //  Name ProjCS with the name on the georeference file
    // ----------------------------------------------------------------------

    oSRS.SetProjCS( pszGeorefName );

    oSRS.exportToWkt( ppszProjString );
        
    CPLFree( pszGeorefName );
    CPLFree( pszProjName );
    CPLFree( pszDatum );
    CPLFree( pszEllipsoid );
        
    return CE_None;
}

/************************************************************************/
/*                        Wkt2GeoReference()                            */
/************************************************************************/

/***
* Converts OpenGIS WKT to Idrisi geographic reference information.
* 
* That function will fill up the two parameters RefSystem and RefUnit
* that goes into the Idrisi metadata. But it could also create
* a companying georeference file to the output if necessary.
*
* First it will try to identify the ProjString as Local, WGS84 or
* one of the Idrisi name convention reference systems
* otherwise, if the projection system is supported by Idrisi,  
* it will create a companying georeference files.
***/

CPLErr IdrisiDataset::Wkt2GeoReference( const char *pszProjString,
                                        char **pszRefSystem, 
                                        char **pszRefUnit )
{
    // -----------------------------------------------------
    //  Plane with default "Meters"
    // -----------------------------------------------------

    if EQUAL( pszProjString, "" )
    {
        *pszRefSystem = CPLStrdup( rstPLANE );
        *pszRefUnit   = CPLStrdup( rstMETER );
        return CE_None;
    }

    OGRSpatialReference oSRS;
    oSRS.importFromWkt( (char **) &pszProjString );

    // -----------------------------------------------------
    //  Local => Plane + Linear Unit
    // -----------------------------------------------------

    if( oSRS.IsLocal() )
    {
        *pszRefSystem = CPLStrdup( rstPLANE );
        *pszRefUnit   = GetUnitDefault( oSRS.GetAttrValue( "UNIT" ), 
                        CPLSPrintf( "%f", oSRS.GetLinearUnits() ) );
        return CE_None;
    }

    // -----------------------------------------------------
    //  Test to identify WGS84 => Latlong + Angular Unit
    // -----------------------------------------------------

    if( oSRS.IsGeographic() )
    {
        char *pszSpheroid = CPLStrdup( oSRS.GetAttrValue( "SPHEROID" ) );
        char *pszAuthName = CPLStrdup( oSRS.GetAuthorityName( "GEOGCS" ) );
        char *pszDatum    = CPLStrdup( oSRS.GetAttrValue( "DATUM" ) );
        int nGCSCode = -1;
        if EQUAL( pszAuthName, "EPSG" )
        {
            nGCSCode = atoi( oSRS.GetAuthorityCode( "GEOGCS" ) );
        }
        if( ( nGCSCode == 4326 ) ||( 
            ( EQUALN( pszSpheroid, "WGS", 3 ) ) &&( strstr( pszSpheroid, "84" ) ) &&
            ( EQUALN( pszDatum, "WGS", 3 ) )    &&( strstr( pszDatum, "84" ) ) ) )
        {
            *pszRefSystem = CPLStrdup( rstLATLONG );
            *pszRefUnit   = CPLStrdup( rstDEGREE );

            CPLFree( pszSpheroid );
            CPLFree( pszAuthName );
            CPLFree( pszDatum );

            return CE_None;
        }

        CPLFree( pszSpheroid );
        CPLFree( pszAuthName );
        CPLFree( pszDatum );
    }

    // -----------------------------------------------------
    //  Prepare to match some projections 
    // -----------------------------------------------------

    const char *pszProjName = oSRS.GetAttrValue( "PROJECTION" );

    if( pszProjName == NULL )
    {
        pszProjName = "";
    }

    // -----------------------------------------------------
    //  Check for UTM zones
    // -----------------------------------------------------

    if( EQUAL( pszProjName, SRS_PT_TRANSVERSE_MERCATOR ) )
    {
        int nZone = oSRS.GetUTMZone();

        if( ( nZone != 0 ) && ( EQUAL( oSRS.GetAttrValue( "DATUM" ), SRS_DN_WGS84 ) ) )
        {
            double dfNorth = oSRS.GetProjParm( SRS_PP_FALSE_NORTHING );
            *pszRefSystem  = CPLStrdup( CPLSPrintf( rstUTM, nZone,( dfNorth == 0.0 ? 'n' : 's' ) ) );
            *pszRefUnit    = CPLStrdup( rstMETER );
            return CE_None;
        }
    }

    // -----------------------------------------------------
    //  Check for State Plane
    // -----------------------------------------------------

#ifndef GDAL_RST_PLUGIN

    if( EQUAL( pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP ) ||
        EQUAL( pszProjName, SRS_PT_TRANSVERSE_MERCATOR ) )
    {
        const char *pszPCSCode;
        const char *pszID = CPLStrdup( oSRS.GetAuthorityCode( "PROJCS" ) );
        if( strlen( pszID ) > 0 )
        {
            pszPCSCode = CPLStrdup( CSVGetField( CSVFilename( "stateplane.csv" ),
                "EPSG_PCS_CODE", pszID, CC_Integer, "ID" ) );
            if( strlen( pszPCSCode ) > 0 )
            {
                int nNADYear   = 83;
                int nZone      = pszPCSCode[strlen( pszPCSCode ) - 1] - '0';
                int nSPCode    = atoi_nz( pszPCSCode );

                if( nZone == 0 )
                    nZone = 1;
                else
                    nSPCode = nSPCode - nZone + 1;

                if( nSPCode > 10000 )
                {
                    nNADYear = 27;
                    nSPCode -= 10000;
                }
                char *pszState  = CPLStrdup( GetStateName( nSPCode ) );
                if( ! EQUAL( pszState, "" ) )
                {
                    *pszRefSystem   = CPLStrdup( CPLSPrintf( rstSPC, nNADYear, pszState, nZone ) );
                    *pszRefUnit     = GetUnitDefault( oSRS.GetAttrValue( "UNIT" ),
                                      CPLSPrintf( "%f", oSRS.GetLinearUnits() ) );
                    return CE_None;
                }
            }
        }
    }

#endif // GDAL_RST_PLUGIN

    const char *pszProjectionOut = NULL;

    if( oSRS.IsProjected() )
    {
        // ---------------------------------------------------------
        //  Check for supported projections
        // ---------------------------------------------------------

        if EQUAL( pszProjName, SRS_PT_MERCATOR_1SP )
        {
            pszProjectionOut =  "Mercator" ;
        }
        if EQUAL( pszProjName, SRS_PT_TRANSVERSE_MERCATOR )         
        {   
            pszProjectionOut =  "Transverse Mercator" ;
        }
        else if EQUAL( pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP )
        {
            pszProjectionOut =  "Lambert Conformal Conic" ;
        }
        else if EQUAL( pszProjName, SRS_PT_EQUIRECTANGULAR )
        {
            pszProjectionOut =  "Plate Carr" "\xE9" "e" ; /* 'eacute' in ISO-8859-1 */
        }
        else if EQUAL( pszProjName, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA )
        {   
            double dfCenterLat = oSRS.GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0, NULL );
            if( dfCenterLat == 0.0 )
                pszProjectionOut =  "Lambert Transverse Azimuthal Equal Area" ; 
            else if( fabs( dfCenterLat ) == 90.0 )
                pszProjectionOut =  "Lambert Oblique Polar Azimuthal Equal Area" ;
            else if( dfCenterLat > 0.0 )
                pszProjectionOut =  "Lambert North Oblique Azimuthal Equal Area" ; 
            else
                pszProjectionOut =  "Lambert South Oblique Azimuthal Equal Area" ;
        }
        else if EQUAL( pszProjName, SRS_PT_POLAR_STEREOGRAPHIC )
        {
            if( oSRS.GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0, NULL ) > 0 )
                pszProjectionOut =  "North Polar Stereographic" ;
            else
                pszProjectionOut =  "South Polar Stereographic" ;
        }
        else if EQUAL( pszProjName, SRS_PT_STEREOGRAPHIC )
        {
            pszProjectionOut =  "Transverse Stereographic" ;
        }
        else if EQUAL( pszProjName, SRS_PT_OBLIQUE_STEREOGRAPHIC )
        {
            pszProjectionOut =  "Oblique Stereographic" ;
        }
        else if EQUAL( pszProjName, SRS_PT_SINUSOIDAL )
        {
            pszProjectionOut =  "Sinusoidal" ;
        }
        else if EQUAL( pszProjName, SRS_PT_ALBERS_CONIC_EQUAL_AREA )
        {
            pszProjectionOut =  "Alber's Equal Area Conic" ;
        }

        // ---------------------------------------------------------
        //  Failure, Projection system not suppotted
        // ---------------------------------------------------------

        if( pszProjectionOut == NULL )
        {
            CPLDebug( "RST",
                      "Not supported by RST driver: PROJECTION[\"%s\"]",
                      pszProjName );

            *pszRefSystem = CPLStrdup( rstPLANE );
            *pszRefUnit   = CPLStrdup( rstMETER );
            return CE_Failure;
        }
    }
    else
    {
        pszProjectionOut =  "none" ;
    }

    // ---------------------------------------------------------
    //  Prepare to write ref file
    // ---------------------------------------------------------

    char *pszGeorefName         = CPLStrdup( "Unknown" );
    char *pszDatum              = CPLStrdup( oSRS.GetAttrValue( "DATUM" ) );
    char *pszEllipsoid          = CPLStrdup( oSRS.GetAttrValue( "SPHEROID" ) );
    double dfSemiMajor          = oSRS.GetSemiMajor();        
    double dfSemiMinor          = oSRS.GetSemiMinor();        
    double adfToWGS84[3];
    oSRS.GetTOWGS84( adfToWGS84, 3 );

    double dfCenterLat          = 0.0;
    double dfCenterLong         = 0.0;        
    double dfFalseNorthing      = 0.0;    
    double dfFalseEasting       = 0.0;    
    double dfScale              = 1.0;
    int nParameters             = 0;         
    double dfStdP1              = 0.0;            
    double dfStdP2              = 0.0;            
    char *pszAngularUnit        = CPLStrdup( oSRS.GetAttrValue( "GEOGCS|UNIT" ) );
    char *pszLinearUnit;

    if( oSRS.IsProjected() )
    {
        CPLFree( pszGeorefName );
        pszGeorefName   = CPLStrdup( oSRS.GetAttrValue( "PROJCS" ) );
        dfCenterLat     = oSRS.GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0, NULL );
        dfCenterLong    = oSRS.GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0, NULL );
        dfFalseNorthing = oSRS.GetProjParm( SRS_PP_FALSE_NORTHING, 0.0, NULL );
        dfFalseEasting  = oSRS.GetProjParm( SRS_PP_FALSE_EASTING, 0.0, NULL );
        dfScale         = oSRS.GetProjParm( SRS_PP_SCALE_FACTOR, 0.0, NULL );
        dfStdP1         = oSRS.GetProjParm( SRS_PP_STANDARD_PARALLEL_1, -0.1, NULL );
        dfStdP2         = oSRS.GetProjParm( SRS_PP_STANDARD_PARALLEL_2, -0.1, NULL );  
        if( dfStdP1 != -0.1 )
        {
            nParameters = 1;
            if( dfStdP2 != -0.1 )
                nParameters = 2;
        }
        pszLinearUnit   = GetUnitDefault( oSRS.GetAttrValue( "PROJCS|UNIT" ),
                                          CPLSPrintf( "%f", oSRS.GetLinearUnits() ) ) ;
    }
    else
    {
        pszLinearUnit   = GetUnitDefault( pszAngularUnit );
    }

    // ---------------------------------------------------------
    //  Create a compaining georeference file for this dataset
    // ---------------------------------------------------------

    char **papszRef = NULL;
    papszRef = CSLAddNameValue( papszRef, refREF_SYSTEM,   pszGeorefName );  
    papszRef = CSLAddNameValue( papszRef, refPROJECTION,   pszProjectionOut );  
    papszRef = CSLAddNameValue( papszRef, refDATUM,        pszDatum );     
    papszRef = CSLAddNameValue( papszRef, refDELTA_WGS84,  CPLSPrintf( "%.3g %.3g %.3g", 
        adfToWGS84[0], adfToWGS84[1], adfToWGS84[2] ) );
    papszRef = CSLAddNameValue( papszRef, refELLIPSOID,    pszEllipsoid );
    papszRef = CSLAddNameValue( papszRef, refMAJOR_SAX,    CPLSPrintf( "%.3f", dfSemiMajor ) );
    papszRef = CSLAddNameValue( papszRef, refMINOR_SAX,    CPLSPrintf( "%.3f", dfSemiMinor ) );
    papszRef = CSLAddNameValue( papszRef, refORIGIN_LONG,  CPLSPrintf( "%.9g", dfCenterLong ) );
    papszRef = CSLAddNameValue( papszRef, refORIGIN_LAT,   CPLSPrintf( "%.9g", dfCenterLat ) );
    papszRef = CSLAddNameValue( papszRef, refORIGIN_X,     CPLSPrintf( "%.9g", dfFalseEasting ) );
    papszRef = CSLAddNameValue( papszRef, refORIGIN_Y,     CPLSPrintf( "%.9g", dfFalseNorthing ) );
    papszRef = CSLAddNameValue( papszRef, refSCALE_FAC,    CPLSPrintf( "%.9g", dfScale ) );
    papszRef = CSLAddNameValue( papszRef, refUNITS,        pszLinearUnit );
    papszRef = CSLAddNameValue( papszRef, refPARAMETERS,   CPLSPrintf( "%1d",  nParameters ) );
    if( nParameters > 0 )
        papszRef = CSLAddNameValue( papszRef, refSTANDL_1, CPLSPrintf( "%.9g", dfStdP1 ) );
    if( nParameters > 1 )
        papszRef = CSLAddNameValue( papszRef, refSTANDL_2, CPLSPrintf( "%.9g", dfStdP2 ) );
    CSLSetNameValueSeparator( papszRef, ": " );
    SaveAsCRLF( papszRef, CPLResetExtension( pszFilename, extREF ) );
    CSLDestroy( papszRef );

    *pszRefSystem = CPLStrdup( CPLGetBasename( pszFilename ) );
    *pszRefUnit   = CPLStrdup( pszLinearUnit );

    CPLFree( pszGeorefName );
    CPLFree( pszDatum );
    CPLFree( pszEllipsoid );
    CPLFree( pszLinearUnit );
    CPLFree( pszAngularUnit );

    return CE_None;
}

/************************************************************************/
/*                             FileExists()                             */
/************************************************************************/

bool FileExists( const char *pszPath )
{
    VSIStatBufL  sStat;

    return (bool) ( VSIStatL( pszPath, &sStat ) == 0 );
}

/************************************************************************/
/*                            GetStateCode()                            */
/************************************************************************/

int GetStateCode( const char *pszState )
{
    unsigned int i;

    for( i = 0; i < US_STATE_COUNT; i++ )
    {
        if EQUAL( pszState, aoUSStateTable[i].pszName )
        {
            return aoUSStateTable[i].nCode;
        }
    }
    return -1;
}

/************************************************************************/
/*                            GetStateCode()                            */
/************************************************************************/

const char *GetStateName( int nCode )
{
    unsigned int i;

    for( i = 0; i < US_STATE_COUNT; i++ )
    {
        if( nCode == aoUSStateTable[i].nCode )
        {
            return aoUSStateTable[i].pszName;
        }
    }
    return NULL;
}

/************************************************************************/
/*                            GetUnitIndex()                            */
/************************************************************************/

int GetUnitIndex( const char *pszUnitName )
{
    int i;

    for( i = 0; i < (int) LINEAR_UNITS_COUNT; i++ )
    {
        if EQUAL( pszUnitName, aoLinearUnitsConv[i].pszName )
        {
            return i;
        }
    }
    return -1;
}

/************************************************************************/
/*                            GetToMeterIndex()                         */
/************************************************************************/

int GetToMeterIndex( const char *pszToMeter )
{
    double dfToMeter = atof_nz(pszToMeter);

    if( dfToMeter != 0.0 )
    {
        int i;

        for( i = 0; i < (int) LINEAR_UNITS_COUNT; i++ )
        {
            if ( ABS( aoLinearUnitsConv[i].dfConv - dfToMeter ) < 0.00001 )
            {
                return i;
            }
        }
    }

    return -1;
}

/************************************************************************/
/*                            GetUnitDefault()                          */
/************************************************************************/

char *GetUnitDefault( const char *pszUnitName, const char *pszToMeter )
{
    int nIndex = GetUnitIndex( pszUnitName );

    if( nIndex == -1 && pszToMeter != NULL )
    {
        nIndex = GetToMeterIndex( pszToMeter );
    }

    if( nIndex == -1 )
    {
        return CPLStrdup( "Unknown" );
    }

    return CPLStrdup( aoLinearUnitsConv[aoLinearUnitsConv[nIndex].nDefaultI].pszName );
}

/************************************************************************/
/*                               CSLSaveCRLF()                          */
/************************************************************************/

/***
 * Write a stringlist to a CR + LF terminated text file.
 *
 * Returns the number of lines written, or 0 if the file could not 
 * be written.
 */

int  SaveAsCRLF(char **papszStrList, const char *pszFname)
{
    VSILFILE    *fp;
    int     nLines = 0;

    if (papszStrList)
    {
        if ((fp = VSIFOpenL(pszFname, "wt")) != NULL)
        {
            while(*papszStrList != NULL)
            {
                if( VSIFPrintfL( fp, "%s\r\n", *papszStrList ) < 1 )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                    "CSLSaveCRLF(\"%s\") failed: unable to write to output file.",
                              pszFname );
                    break;
                }

                nLines++;
                papszStrList++;
            }

            VSIFCloseL(fp);
        }
        else
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "CSLSaveCRLF(\"%s\") failed: unable to open output file.",
                      pszFname );
        }
    }

    return nLines;
}

/************************************************************************/
/*                        GDALRegister_IDRISI()                         */
/************************************************************************/

void GDALRegister_IDRISI()
{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "RST" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "RST" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, rstVERSION );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_Idrisi.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, extRST );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte Int16 Float32" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = IdrisiDataset::Open;
        poDriver->pfnCreate = IdrisiDataset::Create;
        poDriver->pfnCreateCopy = IdrisiDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

