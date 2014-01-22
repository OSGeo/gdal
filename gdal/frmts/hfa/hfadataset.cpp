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

int WritePeStringIfNeeded(OGRSpatialReference* poSRS, HFAHandle hHFA);
void ClearSR(HFAHandle hHFA);

static const char *apszDatumMap[] = {
    /* Imagine name, WKT name */
    "NAD27", "North_American_Datum_1927",
    "NAD83", "North_American_Datum_1983",
    "WGS 84", "WGS_1984",
    "WGS 1972", "WGS_1972",
    "GDA94", "Geocentric_Datum_of_Australia_1994",
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
    static CPLErr       Rename( const char *pszNewName, const char *pszOldName);
    static CPLErr       CopyFiles( const char *pszNewName, const char *pszOldName);
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
    friend class HFARasterAttributeTable;

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
    CPLErr      WriteNamedRAT( const char *pszName, const GDALRasterAttributeTable *poRAT );


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

    virtual GDALRasterAttributeTable *GetDefaultRAT();
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * );
};

class HFAAttributeField
{
public:
    CPLString         sName;
    GDALRATFieldType  eType;
    GDALRATFieldUsage eUsage;
    int               nDataOffset;
    int               nElementSize;
    HFAEntry         *poColumn;
    int               bIsBinValues; // handled differently
    int               bConvertColors; // map 0-1 floats to 0-255 ints
};

class HFARasterAttributeTable : public GDALRasterAttributeTable
{
private:

    HFAHandle	hHFA;
    HFAEntry   *poDT;
    CPLString   osName;
    int         nBand;
    GDALAccess  eAccess;

    std::vector<HFAAttributeField>  aoFields;
    int         nRows;

    int bLinearBinning;
    double dfRow0Min;
    double dfBinSize;

    CPLString osWorkingResult;

    void AddColumn(const char *pszName, GDALRATFieldType eType, GDALRATFieldUsage eUsage, 
                int nDataOffset, int nElementSize, HFAEntry *poColumn, int bIsBinValues=FALSE,
                int bConvertColors=FALSE)
    {
        HFAAttributeField aField;
        aField.sName = pszName;
        aField.eType = eType;
        aField.eUsage = eUsage;
        aField.nDataOffset = nDataOffset;
        aField.nElementSize = nElementSize;
        aField.poColumn = poColumn;
        aField.bIsBinValues = bIsBinValues;
        aField.bConvertColors = bConvertColors;

        this->aoFields.push_back(aField);
    }

    void CreateDT()
    {
        this->poDT = new HFAEntry( this->hHFA->papoBand[this->nBand-1]->psInfo, 
                             this->osName, "Edsc_Table",
                             this->hHFA->papoBand[this->nBand-1]->poNode );
        this->poDT->SetIntField( "numrows", nRows );
    }

public:
    HFARasterAttributeTable(HFARasterBand *poBand, const char *pszName);
    ~HFARasterAttributeTable();

    GDALDefaultRasterAttributeTable *Clone() const;
    
    virtual int           GetColumnCount() const;

    virtual const char   *GetNameOfCol( int ) const;
    virtual GDALRATFieldUsage GetUsageOfCol( int ) const;
    virtual GDALRATFieldType GetTypeOfCol( int ) const;
    
    virtual int           GetColOfUsage( GDALRATFieldUsage ) const;

    virtual int           GetRowCount() const;

    virtual const char   *GetValueAsString( int iRow, int iField ) const;
    virtual int           GetValueAsInt( int iRow, int iField ) const;
    virtual double        GetValueAsDouble( int iRow, int iField ) const;

    virtual void          SetValue( int iRow, int iField, const char *pszValue );
    virtual void          SetValue( int iRow, int iField, double dfValue);
    virtual void          SetValue( int iRow, int iField, int nValue );

    virtual CPLErr        ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, double *pdfData);
    virtual CPLErr        ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, int *pnData);
    virtual CPLErr        ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, char **papszStrList);

    virtual int           ChangesAreWrittenToFile();
    virtual void          SetRowCount( int iCount );

    virtual int           GetRowOfValue( double dfValue ) const;
    virtual int           GetRowOfValue( int nValue ) const;

    virtual CPLErr        CreateColumn( const char *pszFieldName, 
                                GDALRATFieldType eFieldType, 
                                GDALRATFieldUsage eFieldUsage );
    virtual CPLErr        SetLinearBinning( double dfRow0Min, double dfBinSize );
    virtual int           GetLinearBinning( double *pdfRow0Min, double *pdfBinSize ) const;

    virtual CPLXMLNode   *Serialize() const;

protected:
    CPLErr                ColorsIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, int *pnData);
};

/************************************************************************/
/*                     HFARasterAttributeTable()                        */
/************************************************************************/

HFARasterAttributeTable::HFARasterAttributeTable(HFARasterBand *poBand, const char *pszName)
{
    this->hHFA = poBand->hHFA;
    this->poDT = poBand->hHFA->papoBand[poBand->nBand-1]->poNode->GetNamedChild(pszName);
    this->nBand = poBand->nBand;
    this->eAccess = poBand->GetAccess();
    this->osName = pszName;
    this->nRows = 0;
    this->bLinearBinning = FALSE;

    if( this->poDT != NULL )
    {
        this->nRows = this->poDT->GetIntField( "numRows" );

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

                if( nBinCount == this->nRows
                    && dfMax != dfMin && nBinCount != 0 )
                {
                    // can't call SetLinearBinning since it will re-write
                    // which we might not have permission to do
                    this->bLinearBinning = TRUE;
                    this->dfRow0Min = dfMin;
                    this->dfBinSize = (dfMax-dfMin) / (nBinCount-1);
                }
            }

            if( EQUAL(poDTChild->GetType(),"Edsc_BinFunction840") 
                && EQUAL(poDTChild->GetStringField( "binFunction.type.string" ),
                         "BFUnique") )
            {
                AddColumn( "BinValues", GFT_Real, GFU_MinMax, 0, 0, poDTChild, TRUE);
            }

            if( !EQUAL(poDTChild->GetType(),"Edsc_Column") )
                continue;

            int nOffset = poDTChild->GetIntField( "columnDataPtr" );
            const char * pszType = poDTChild->GetStringField( "dataType" );
            GDALRATFieldUsage eUsage = GFU_Generic;
            int bConvertColors = FALSE;

            if( pszType == NULL || nOffset == 0 )
                continue;

            GDALRATFieldType eType;
            if( EQUAL(pszType,"real") )
                eType = GFT_Real;
            else if( EQUAL(pszType,"string") )
                eType = GFT_String;
            else if( EQUALN(pszType,"int",3) )
                eType = GFT_Integer;
            else
                continue;
        
            if( EQUAL(poDTChild->GetName(),"Histogram") )
                eUsage = GFU_Generic;
            else if( EQUAL(poDTChild->GetName(),"Red") )
            {
                eUsage = GFU_Red;
                // treat color columns as ints regardless
                // of how they are stored
                bConvertColors = (eType == GFT_Real);
                eType = GFT_Integer;
            }
            else if( EQUAL(poDTChild->GetName(),"Green") )
            {
                eUsage = GFU_Green;
                bConvertColors = (eType == GFT_Real);
                eType = GFT_Integer;
            }
            else if( EQUAL(poDTChild->GetName(),"Blue") )
            {
                eUsage = GFU_Blue;
                bConvertColors = (eType == GFT_Real);
                eType = GFT_Integer;
            }
            else if( EQUAL(poDTChild->GetName(),"Opacity") )
            {
                eUsage = GFU_Alpha;
                bConvertColors = (eType == GFT_Real);
                eType = GFT_Integer;
            }
            else if( EQUAL(poDTChild->GetName(),"Class_Names") )
                eUsage = GFU_Name;

            if( eType == GFT_Real )
            {
                AddColumn(poDTChild->GetName(), GFT_Real, eUsage, nOffset, sizeof(double), poDTChild);
            }
            else if( eType == GFT_String )
            {
                int nMaxNumChars = poDTChild->GetIntField( "maxNumChars" );
                AddColumn(poDTChild->GetName(), GFT_String, eUsage, nOffset, nMaxNumChars, poDTChild);
            }
            else if( eType == GFT_Integer )
            {
                int nSize = sizeof(GInt32);
                if( bConvertColors )
                    nSize = sizeof(double);
                AddColumn(poDTChild->GetName(), GFT_Integer, eUsage, nOffset, nSize, poDTChild, 
                                        FALSE, bConvertColors);
            }
        }
    }
}

/************************************************************************/
/*                    ~HFARasterAttributeTable()                        */
/************************************************************************/

HFARasterAttributeTable::~HFARasterAttributeTable()
{

}

/************************************************************************/
/*                              Clone()                                 */
/************************************************************************/

GDALDefaultRasterAttributeTable *HFARasterAttributeTable::Clone() const
{
    if( ( GetRowCount() * GetColumnCount() ) > RAT_MAX_ELEM_FOR_CLONE )
        return NULL;

    GDALDefaultRasterAttributeTable *poRAT = new GDALDefaultRasterAttributeTable();

    for( int iCol = 0; iCol < (int)aoFields.size(); iCol++)
    {
        poRAT->CreateColumn(aoFields[iCol].sName, aoFields[iCol].eType, aoFields[iCol].eUsage);
        poRAT->SetRowCount(this->nRows);

        if( aoFields[iCol].eType == GFT_Integer )
        {
            int *panColData = (int*)VSIMalloc2(sizeof(int), this->nRows);
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::Clone");
                delete poRAT;
                return NULL;
            }

            if( ((GDALDefaultRasterAttributeTable*)this)->
                        ValuesIO(GF_Read, iCol, 0, this->nRows, panColData ) != CE_None )
            {
                CPLFree(panColData);
                delete poRAT;
                return NULL;
            }           

            for( int iRow = 0; iRow < this->nRows; iRow++ )
            {
                poRAT->SetValue(iRow, iCol, panColData[iRow]);            
            }
            CPLFree(panColData);
        }
        if( aoFields[iCol].eType == GFT_Real )
        {
            double *padfColData = (double*)VSIMalloc2(sizeof(double), this->nRows);
            if( padfColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::Clone");
                delete poRAT;
                return NULL;
            }

            if( ((GDALDefaultRasterAttributeTable*)this)->
                        ValuesIO(GF_Read, iCol, 0, this->nRows, padfColData ) != CE_None )
            {
                CPLFree(padfColData);
                delete poRAT;
                return NULL;
            }           

            for( int iRow = 0; iRow < this->nRows; iRow++ )
            {
                poRAT->SetValue(iRow, iCol, padfColData[iRow]);            
            }
            CPLFree(padfColData);
        }
        if( aoFields[iCol].eType == GFT_String )
        {
            char **papszColData = (char**)VSIMalloc2(sizeof(char*), this->nRows);
            if( papszColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::Clone");
                delete poRAT;
                return NULL;
            }

            if( ((GDALDefaultRasterAttributeTable*)this)->
                    ValuesIO(GF_Read, iCol, 0, this->nRows, papszColData ) != CE_None )
            {
                CPLFree(papszColData);
                delete poRAT;
                return NULL;
            }           

            for( int iRow = 0; iRow < this->nRows; iRow++ )
            {
                poRAT->SetValue(iRow, iCol, papszColData[iRow]);
                CPLFree(papszColData[iRow]);
            }
            CPLFree(papszColData);
        }
    }

    if( this->bLinearBinning )
        poRAT->SetLinearBinning( this->dfRow0Min, this->dfBinSize );

    return poRAT;
}
    
/************************************************************************/
/*                          GetColumnCount()                            */
/************************************************************************/

int HFARasterAttributeTable::GetColumnCount() const
{
    return this->aoFields.size();
}

/************************************************************************/
/*                          GetNameOfCol()                              */
/************************************************************************/

const char *HFARasterAttributeTable::GetNameOfCol( int nCol ) const
{
    if( ( nCol < 0 ) || ( nCol >= (int)this->aoFields.size() ) )
        return NULL;

    return this->aoFields[nCol].sName;
}

/************************************************************************/
/*                          GetUsageOfCol()                             */
/************************************************************************/

GDALRATFieldUsage HFARasterAttributeTable::GetUsageOfCol( int nCol ) const
{
    if( ( nCol < 0 ) || ( nCol >= (int)this->aoFields.size() ) )
        return GFU_Generic;

    return this->aoFields[nCol].eUsage;
}

/************************************************************************/
/*                          GetTypeOfCol()                              */
/************************************************************************/

GDALRATFieldType HFARasterAttributeTable::GetTypeOfCol( int nCol ) const
{
    if( ( nCol < 0 ) || ( nCol >= (int)this->aoFields.size() ) )
        return GFT_Integer;

    return this->aoFields[nCol].eType;
}
    
/************************************************************************/
/*                          GetColOfUsage()                             */
/************************************************************************/

int HFARasterAttributeTable::GetColOfUsage( GDALRATFieldUsage eUsage ) const
{
    unsigned int i;

    for( i = 0; i < this->aoFields.size(); i++ )
    {
        if( this->aoFields[i].eUsage == eUsage )
            return i;
    }

    return -1;

}
/************************************************************************/
/*                          GetRowCount()                               */
/************************************************************************/

int HFARasterAttributeTable::GetRowCount() const
{
    return this->nRows;
}

/************************************************************************/
/*                      GetValueAsString()                              */
/************************************************************************/

const char *HFARasterAttributeTable::GetValueAsString( int iRow, int iField ) const
{
    // Get ValuesIO do do the work
    char *apszStrList[1];
    if( ((HFARasterAttributeTable*)this)->
                ValuesIO(GF_Read, iField, iRow, 1, apszStrList ) != CPLE_None )
    {
        return "";
    }

    ((HFARasterAttributeTable *) this)->osWorkingResult = apszStrList[0];
    CPLFree(apszStrList[0]);

    return osWorkingResult;
}

/************************************************************************/
/*                        GetValueAsInt()                               */
/************************************************************************/

int HFARasterAttributeTable::GetValueAsInt( int iRow, int iField ) const
{
    // Get ValuesIO do do the work
    int nValue;
    if( ((HFARasterAttributeTable*)this)->
                ValuesIO(GF_Read, iField, iRow, 1, &nValue ) != CE_None )
    {
        return 0;
    }

    return nValue;
}

/************************************************************************/
/*                      GetValueAsDouble()                              */
/************************************************************************/

double HFARasterAttributeTable::GetValueAsDouble( int iRow, int iField ) const
{
    // Get ValuesIO do do the work
    double dfValue;
    if( ((HFARasterAttributeTable*)this)->
                ValuesIO(GF_Read, iField, iRow, 1, &dfValue ) != CE_None )
    {
        return 0;
    }

    return dfValue;
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void HFARasterAttributeTable::SetValue( int iRow, int iField, const char *pszValue )
{
    // Get ValuesIO do do the work
    ValuesIO(GF_Write, iField, iRow, 1, (char**)&pszValue );
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void HFARasterAttributeTable::SetValue( int iRow, int iField, double dfValue)
{
    // Get ValuesIO do do the work
    ValuesIO(GF_Write, iField, iRow, 1, &dfValue );
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void HFARasterAttributeTable::SetValue( int iRow, int iField, int nValue )
{
    // Get ValuesIO do do the work
    ValuesIO(GF_Write, iField, iRow, 1, &nValue );
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr HFARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, double *pdfData)
{
    if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }

    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return CE_Failure;
    }

    if( iStartRow < 0 || (iStartRow+iLength) > this->nRows )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iStartRow (%d) + iLength(%d) out of range.", iStartRow, iLength );

        return CE_Failure;
    }

    if( aoFields[iField].bConvertColors )
    {
        // convert to/from float color field
        int *panColData = (int*)VSIMalloc2(iLength, sizeof(int) );
        if( panColData == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
            CPLFree(panColData);
            return CE_Failure;
        }

        if( eRWFlag == GF_Write )
        {
            for( int i = 0; i < iLength; i++ )
                panColData[i] = pdfData[i];
        }

        CPLErr ret = ColorsIO(eRWFlag, iField, iStartRow, iLength, panColData);

        if( eRWFlag == GF_Read )
        {
            // copy them back to doubles
            for( int i = 0; i < iLength; i++ )
                pdfData[i] = panColData[i];
        }

        CPLFree(panColData);
        return ret;
    }

    switch( aoFields[iField].eType )
    {
        case GFT_Integer:
        {
            // allocate space for ints
            int *panColData = (int*)VSIMalloc2(iLength, sizeof(int) );
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                CPLFree(panColData);
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied doubles to ints
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = pdfData[i];
            }

            // do the ValuesIO as ints
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData );
            if( eVal != CE_None )
            {
                CPLFree(panColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // copy them back to doubles
                for( int i = 0; i < iLength; i++ )
                    pdfData[i] = panColData[i];
            }

            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            if( (eRWFlag == GF_Read ) && aoFields[iField].bIsBinValues )
            {
                // probably could change HFAReadBFUniqueBins to only read needed rows
                double *padfBinValues = HFAReadBFUniqueBins( aoFields[iField].poColumn, iStartRow+iLength );
                memcpy(pdfData, &padfBinValues[iStartRow], sizeof(double) * iLength);
                CPLFree(padfBinValues);
            }
            else
            {
                VSIFSeekL( hHFA->fp, aoFields[iField].nDataOffset + (iStartRow*aoFields[iField].nElementSize), SEEK_SET );

                if( eRWFlag == GF_Read )
                {
                    if ((int)VSIFReadL(pdfData, sizeof(double), iLength, hHFA->fp ) != iLength)
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                            "HFARasterAttributeTable::ValuesIO : Cannot read values");
                        return CE_Failure;
                    }
#ifdef CPL_MSB
                    GDALSwapWords( pdfData, 8, iLength, 8 );
#endif
                }
                else
                {
#ifdef CPL_MSB
                    GDALSwapWords( pdfData, 8, iLength, 8 );
#endif
                    // Note: HFAAllocateSpace now called by CreateColumn so space should exist
                    if((int)VSIFWriteL(pdfData, sizeof(double), iLength, hHFA->fp) != iLength)
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                            "HFARasterAttributeTable::ValuesIO : Cannot write values");
                        return CE_Failure;
                    }
#ifdef CPL_MSB
                    // swap back
                    GDALSwapWords( pdfData, 8, iLength, 8 );
#endif
                }
            }
        }
        break;
        case GFT_String:
        {
            // allocate space for string pointers
            char **papszColData = (char**)VSIMalloc2(iLength, sizeof(char*));
            if( papszColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied doubles to strings
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf( "%.16g", pdfData[i] );
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // do the ValuesIO as strings
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData );
            if( eVal != CE_None )
            {
                if( eRWFlag == GF_Write )
                {
                    for( int i = 0; i < iLength; i++ )
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // copy them back to doubles
                for( int i = 0; i < iLength; i++ )
                    pdfData[i] = atof(papszColData[i]);
            }

            // either we allocated them for write, or they were allocated
            // by ValuesIO on read
            for( int i = 0; i < iLength; i++ )
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr HFARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, int *pnData)
{
    if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }

    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return CE_Failure;
    }

    if( iStartRow < 0 || (iStartRow+iLength) > this->nRows )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iStartRow (%d) + iLength(%d) out of range.", iStartRow, iLength );

        return CE_Failure;
    }

    if( aoFields[iField].bConvertColors )
    {
        // convert to/from float color field
        return ColorsIO(eRWFlag, iField, iStartRow, iLength, pnData);
    }

    switch( aoFields[iField].eType )
    {
        case GFT_Integer:
        {
            VSIFSeekL( hHFA->fp, aoFields[iField].nDataOffset + (iStartRow*aoFields[iField].nElementSize), SEEK_SET );
            GInt32 *panColData = (GInt32*)VSIMalloc2(iLength, sizeof(GInt32));
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Read )
            {
                if ((int)VSIFReadL( panColData, sizeof(GInt32), iLength, hHFA->fp ) != iLength)
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                        "HFARasterAttributeTable::ValuesIO : Cannot read values");
                    CPLFree(panColData);
                    return CE_Failure;
                }
#ifdef CPL_MSB
                GDALSwapWords( panColData, 4, iLength, 4 );
#endif
                // now copy into application buffer. This extra step
                // may not be necessary if sizeof(int) == sizeof(GInt32)
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = panColData[i];
            }
            else
            {
                // copy from application buffer
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = pnData[i];

#ifdef CPL_MSB
                GDALSwapWords( panColData, 4, iLength, 4 );
#endif
                // Note: HFAAllocateSpace now called by CreateColumn so space should exist
                if((int)VSIFWriteL(panColData, sizeof(GInt32), iLength, hHFA->fp) != iLength)
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                        "HFARasterAttributeTable::ValuesIO : Cannot write values");
                    CPLFree(panColData);
                    return CE_Failure;
                }
            }
            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            // allocate space for doubles
            double *padfColData = (double*)VSIMalloc2(iLength, sizeof(double) );
            if( padfColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied ints to doubles
                for( int i = 0; i < iLength; i++ )
                    padfColData[i] = pnData[i];
            }

            // do the ValuesIO as doubles
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData );
            if( eVal != CE_None )
            {
                CPLFree(padfColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // copy them back to ints
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = padfColData[i];
            }

            CPLFree(padfColData);
        }
        break;
        case GFT_String:
        {
            // allocate space for string pointers
            char **papszColData = (char**)VSIMalloc2(iLength, sizeof(char*));
            if( papszColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied ints to strings
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf( "%d", pnData[i] );
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // do the ValuesIO as strings
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData );
            if( eVal != CE_None )
            {
                if( eRWFlag == GF_Write )
                {
                    for( int i = 0; i < iLength; i++ )
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // copy them back to ints
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = atol(papszColData[i]);
            }

            // either we allocated them for write, or they were allocated
            // by ValuesIO on read
            for( int i = 0; i < iLength; i++ )
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr HFARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, char **papszStrList)
{
    if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }

    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return CE_Failure;
    }

    if( iStartRow < 0 || (iStartRow+iLength) > this->nRows )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iStartRow (%d) + iLength(%d) out of range.", iStartRow, iLength );

        return CE_Failure;
    }

    if( aoFields[iField].bConvertColors )
    {
        // convert to/from float color field
        int *panColData = (int*)VSIMalloc2(iLength, sizeof(int) );
        if( panColData == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
            CPLFree(panColData);
            return CE_Failure;
        }

        if( eRWFlag == GF_Write )
        {
            for( int i = 0; i < iLength; i++ )
                panColData[i] = atol(papszStrList[i]);
        }

        CPLErr ret = ColorsIO(eRWFlag, iField, iStartRow, iLength, panColData);

        if( eRWFlag == GF_Read )
        {
            // copy them back to strings
            for( int i = 0; i < iLength; i++ )
            {
                osWorkingResult.Printf( "%d", panColData[i]);
                papszStrList[i] = CPLStrdup(osWorkingResult);
            }
        }

        CPLFree(panColData);
        return ret;
    }

    switch( aoFields[iField].eType )
    {
        case GFT_Integer:
        {
            // allocate space for ints
            int *panColData = (int*)VSIMalloc2(iLength, sizeof(int) );
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }
            
            if( eRWFlag == GF_Write )
            {
                // convert user supplied strings to ints
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = atol(papszStrList[i]);
            }

            // call values IO to read/write ints
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData);
            if( eVal != CE_None )
            {
                CPLFree(panColData);
                return eVal;
            }


            if( eRWFlag == GF_Read )
            {
                // convert ints back to strings
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf( "%d", panColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            // allocate space for doubles
            double *padfColData = (double*)VSIMalloc2(iLength, sizeof(double) );
            if( padfColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }
            
            if( eRWFlag == GF_Write )
            {
                // convert user supplied strings to doubles
                for( int i = 0; i < iLength; i++ )
                    padfColData[i] = atof(papszStrList[i]);
            }

            // call value IO to read/write doubles
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData);
            if( eVal != CE_None )
            {
                CPLFree(padfColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // convert doubles back to strings
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf( "%.16g", padfColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(padfColData);
        }
        break;
        case GFT_String:
        {
            VSIFSeekL( hHFA->fp, aoFields[iField].nDataOffset + (iStartRow*aoFields[iField].nElementSize), SEEK_SET );
            char *pachColData = (char*)VSIMalloc2(iLength, aoFields[iField].nElementSize);
            if( pachColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Read )
            {
                if ((int)VSIFReadL( pachColData, aoFields[iField].nElementSize, iLength, hHFA->fp ) != iLength)
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                        "HFARasterAttributeTable::ValuesIO : Cannot read values");
                    CPLFree(pachColData);
                    return CE_Failure;
                }

                // now copy into application buffer
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.assign(pachColData+aoFields[iField].nElementSize*i, aoFields[iField].nElementSize );
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            else
            {
                // we need to check that these strings will fit in the allocated space
                int nNewMaxChars = aoFields[iField].nElementSize, nStringSize;
                for( int i = 0; i < iLength; i++ )
                {
                    nStringSize = strlen(papszStrList[i]) + 1;
                    if( nStringSize > nNewMaxChars )
                        nNewMaxChars = nStringSize;
                }

                if( nNewMaxChars > aoFields[iField].nElementSize )
                {
                    // OK we have a problem - the allocated space is not big enough
                    // we need to re-allocate the space and update the pointers
                    // and copy accross the old data
                    int nNewOffset = HFAAllocateSpace( this->hHFA->papoBand[this->nBand-1]->psInfo,
                                            this->nRows * nNewMaxChars);
                    char *pszBuffer = (char*)VSIMalloc2(aoFields[iField].nElementSize, sizeof(char));
                    char cNullByte = '\0';
                    for( int i = 0; i < this->nRows; i++ )
                    {
                        // seek to the old place
                        VSIFSeekL( hHFA->fp, aoFields[iField].nDataOffset + (i*aoFields[iField].nElementSize), SEEK_SET );
                        // read in old data
                        VSIFReadL(pszBuffer, aoFields[iField].nElementSize, 1, hHFA->fp );
                        // seek to new place
                        VSIFSeekL( hHFA->fp, nNewOffset + (i*nNewMaxChars), SEEK_SET );
                        // write data to new place
                        VSIFWriteL(pszBuffer, aoFields[iField].nElementSize, 1, hHFA->fp);
                        // make sure there is a terminating null byte just to be safe
                        VSIFWriteL(&cNullByte, sizeof(char), 1, hHFA->fp);
                    }
                    // update our data structures
                    aoFields[iField].nElementSize = nNewMaxChars;
                    aoFields[iField].nDataOffset = nNewOffset;
                    // update file
                    aoFields[iField].poColumn->SetIntField( "columnDataPtr", nNewOffset );
                    aoFields[iField].poColumn->SetIntField( "maxNumChars", nNewMaxChars );

                    // Note: there isn't an HFAFreeSpace so we can't un-allocate the old space in the file
                    CPLFree(pszBuffer);

                    // re-allocate our buffer
                    CPLFree(pachColData);
                    pachColData = (char*)VSIMalloc2(iLength, nNewMaxChars);
                    if(pachColData == NULL )
                    {
                        CPLError( CE_Failure, CPLE_OutOfMemory,
                            "Memory Allocation failed in HFARasterAttributeTable::ValuesIO");
                        return CE_Failure;
                    }

                    // lastly seek to the right place in the new space ready to write
                    VSIFSeekL( hHFA->fp, nNewOffset + (iStartRow*nNewMaxChars), SEEK_SET );
                }

                // copy from application buffer
                for( int i = 0; i < iLength; i++ )
                    strcpy(&pachColData[nNewMaxChars*i], papszStrList[i]);

                // Note: HFAAllocateSpace now called by CreateColumn so space should exist
                if((int)VSIFWriteL(pachColData, aoFields[iField].nElementSize, iLength, hHFA->fp) != iLength)
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                        "HFARasterAttributeTable::ValuesIO : Cannot write values");
                    CPLFree(pachColData);
                    return CE_Failure;
                }
            }
            CPLFree(pachColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                               ColorsIO()                              */
/************************************************************************/

// Handle the fact that HFA stores colours as floats, but we need to 
// read them in as ints 0...255
CPLErr HFARasterAttributeTable::ColorsIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, int *pnData)
{
    // allocate space for doubles
    double *padfData = (double*)VSIMalloc2(iLength, sizeof(double) );
    if( padfData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
            "Memory Allocation failed in HFARasterAttributeTable::ColorsIO");
        return CE_Failure;
    }

    if( eRWFlag == GF_Write )
    {
        // copy the application supplied ints to doubles
        // and convert 0..255 to 0..1 in the same manner 
        // as the color table
        for( int i = 0; i < iLength; i++ )
            padfData[i] = pnData[i] / 255.0;
    }

    VSIFSeekL( hHFA->fp, aoFields[iField].nDataOffset + (iStartRow*aoFields[iField].nElementSize), SEEK_SET );

    if( eRWFlag == GF_Read )
    {
        if ((int)VSIFReadL(padfData, sizeof(double), iLength, hHFA->fp ) != iLength)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                "HFARasterAttributeTable::ColorsIO : Cannot read values");
            return CE_Failure;
        }
#ifdef CPL_MSB
        GDALSwapWords( padfData, 8, iLength, 8 );
#endif
    }
    else
    {
#ifdef CPL_MSB
        GDALSwapWords( padfData, 8, iLength, 8 );
#endif
        // Note: HFAAllocateSpace now called by CreateColumn so space should exist
        if((int)VSIFWriteL(padfData, sizeof(double), iLength, hHFA->fp) != iLength)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                "HFARasterAttributeTable::ColorsIO : Cannot write values");
            return CE_Failure;
        }
    }

    if( eRWFlag == GF_Read )
    {
        // copy them back to ints converting 0..1 to 0..255 in
        // the same manner as the color table 
        for( int i = 0; i < iLength; i++ )
            pnData[i] = MIN(255,(int) (padfData[i] * 256));
    }

    CPLFree(padfData);

    return CE_None;
}

/************************************************************************/
/*                       ChangesAreWrittenToFile()                      */
/************************************************************************/

int HFARasterAttributeTable::ChangesAreWrittenToFile()
{
    return TRUE;
}

/************************************************************************/
/*                          SetRowCount()                               */
/************************************************************************/

void HFARasterAttributeTable::SetRowCount( int iCount )
{
    if( this->eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return;
    }

    if( iCount > this->nRows )
    {
        // making the RAT larger - a bit hard
        // We need to re-allocate space on disc
        for( int iCol = 0; iCol < (int)this->aoFields.size(); iCol++ )
        {
            // new space 
            int nNewOffset = HFAAllocateSpace( this->hHFA->papoBand[this->nBand-1]->psInfo,
                                            iCount * aoFields[iCol].nElementSize);

            // only need to bother if there are actually rows
            if( this->nRows > 0 )
            {
                // temp buffer for this column
                void *pData = VSIMalloc2(this->nRows, aoFields[iCol].nElementSize);
                if( pData == NULL )
                {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                        "Memory Allocation failed in HFARasterAttributeTable::SetRowCount");
                    return;
                }
                // read old data
                VSIFSeekL( hHFA->fp, aoFields[iCol].nDataOffset, SEEK_SET );
                if((int)VSIFReadL(pData, aoFields[iCol].nElementSize, this->nRows, hHFA->fp) != this->nRows )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                        "HFARasterAttributeTable::SetRowCount : Cannot read values");
                    CPLFree(pData);
                    return;                
                }

                // write data - new space will be uninitialised
                VSIFSeekL( hHFA->fp, nNewOffset, SEEK_SET );
                if((int)VSIFWriteL(pData, aoFields[iCol].nElementSize, this->nRows, hHFA->fp) != this->nRows )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                            "HFARasterAttributeTable::SetRowCount : Cannot write values");
                    CPLFree(pData);
                    return;                
                }
                CPLFree(pData);
            }

            // update our data structures
            aoFields[iCol].nDataOffset = nNewOffset;
            // update file
            aoFields[iCol].poColumn->SetIntField( "columnDataPtr", nNewOffset );
            aoFields[iCol].poColumn->SetIntField( "numRows", iCount);
        }
    }
    else if( iCount < this->nRows )
    {
        // update the numRows
        for( int iCol = 0; iCol < (int)this->aoFields.size(); iCol++ )
        {
            aoFields[iCol].poColumn->SetIntField( "numRows", iCount);
        }
    }

    this->nRows = iCount;

    if( ( this->poDT != NULL ) && ( EQUAL(this->poDT->GetType(),"Edsc_Table")))
    {
        this->poDT->SetIntField( "numrows", iCount );
    }
}

/************************************************************************/
/*                          GetRowOfValue()                             */
/************************************************************************/

int HFARasterAttributeTable::GetRowOfValue( double dfValue ) const
{
/* -------------------------------------------------------------------- */
/*      Handle case of regular binning.                                 */
/* -------------------------------------------------------------------- */
    if( bLinearBinning )
    {
        int iBin = (int) floor((dfValue - dfRow0Min) / dfBinSize);
        if( iBin < 0 || iBin >= this->nRows )
            return -1;
        else
            return iBin;
    }

/* -------------------------------------------------------------------- */
/*      Do we have any information?                                     */
/* -------------------------------------------------------------------- */
    int nMinCol = GetColOfUsage( GFU_Min );
    if( nMinCol == -1 )
        nMinCol = GetColOfUsage( GFU_MinMax );

    int nMaxCol = GetColOfUsage( GFU_Max );
    if( nMaxCol == -1 )
        nMaxCol = GetColOfUsage( GFU_MinMax );

    if( nMinCol == -1 && nMaxCol == -1 )
        return -1;

/* -------------------------------------------------------------------- */
/*      Search through rows for match.                                  */
/* -------------------------------------------------------------------- */
    int   iRow;

    for( iRow = 0; iRow < this->nRows; iRow++ )
    {
        if( nMinCol != -1 )
        {
            while( iRow < this->nRows && dfValue < GetValueAsDouble(iRow, nMinCol) ) 
                    iRow++;

            if( iRow == this->nRows )
                break;
        }

        if( nMaxCol != -1 )
        {
            if( dfValue > GetValueAsDouble(iRow, nMaxCol) )
                continue;
        }

        return iRow;
    }

    return -1;
}

/************************************************************************/
/*                          GetRowOfValue()                             */
/*                                                                      */
/*      Int arg for now just converted to double.  Perhaps we will      */
/*      handle this in a special way some day?                          */
/************************************************************************/

int HFARasterAttributeTable::GetRowOfValue( int nValue ) const
{
    return GetRowOfValue( (double) nValue );
}

/************************************************************************/
/*                          CreateColumn()                              */
/************************************************************************/

CPLErr HFARasterAttributeTable::CreateColumn( const char *pszFieldName, 
                                GDALRATFieldType eFieldType, 
                                GDALRATFieldUsage eFieldUsage )
{
    if( this->eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }

    // do we have a descriptor table already?
    if( this->poDT == NULL || !EQUAL(this->poDT->GetType(),"Edsc_Table") )
        CreateDT();

    int bConvertColors = FALSE;

    // Imagine doesn't have a concept of usage - works of the names instead.
    // must make sure name matches use
    if( eFieldUsage == GFU_Red )
    {
        pszFieldName = "Red";
        // create a real column in the file, but make it
        // available as int to GDAL
        bConvertColors = TRUE;
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_Green )
    {
        pszFieldName = "Green";
        bConvertColors = TRUE;
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_Blue )
    {
        pszFieldName = "Blue";
        bConvertColors = TRUE;
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_Alpha )
    {
        pszFieldName = "Opacity";
        bConvertColors = TRUE;
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_PixelCount )
    {
        pszFieldName = "Histogram";
    }
    else if( eFieldUsage == GFU_Name )
    {
        pszFieldName = "Class_Names";
    }

/* -------------------------------------------------------------------- */
/*      Check to see if a column with pszFieldName exists and create it */
/*      if necessary.                                                   */
/* -------------------------------------------------------------------- */

    HFAEntry        *poColumn;
    poColumn = poDT->GetNamedChild(pszFieldName);
        
    if(poColumn == NULL || !EQUAL(poColumn->GetType(),"Edsc_Column"))
        poColumn = new HFAEntry( this->hHFA->papoBand[this->nBand-1]->psInfo,
                                     pszFieldName, "Edsc_Column",
                                     this->poDT );


    poColumn->SetIntField( "numRows", this->nRows );
    int nElementSize = 0;

    if( eFieldType == GFT_Integer )
    {
        nElementSize = sizeof(GInt32);
        poColumn->SetStringField( "dataType", "integer" );
    }
    else if( eFieldType == GFT_Real )
    {
        nElementSize = sizeof(double);
        poColumn->SetStringField( "dataType", "real" );
    }
    else if( eFieldType == GFT_String )
    {
        // just have to guess here since we don't have any
        // strings to check
        nElementSize = 10;
        poColumn->SetStringField( "dataType", "string" );
        poColumn->SetIntField( "maxNumChars", nElementSize);
    }
    else
    {
        /* can't deal with any of the others yet */
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Writing this data type in a column is not supported for this Raster Attribute Table.");
        return CE_Failure;
    }

    int nOffset = HFAAllocateSpace( this->hHFA->papoBand[this->nBand-1]->psInfo,
                                        this->nRows * nElementSize );
    poColumn->SetIntField( "columnDataPtr", nOffset );

    if( bConvertColors )
        // GDAL Int column 
        eFieldType = GFT_Integer;

    AddColumn(pszFieldName, eFieldType, eFieldUsage, nOffset, nElementSize, poColumn, FALSE, bConvertColors);

    return CE_None;
}

/************************************************************************/
/*                          SetLinearBinning()                          */
/************************************************************************/

CPLErr HFARasterAttributeTable::SetLinearBinning( double dfRow0MinIn, double dfBinSizeIn )
{
    if( this->eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }

    this->bLinearBinning = TRUE;
    this->dfRow0Min = dfRow0MinIn;
    this->dfBinSize = dfBinSizeIn;

    // do we have a descriptor table already?
    if( this->poDT == NULL || !EQUAL(this->poDT->GetType(),"Edsc_Table") )
        CreateDT();

    /* we should have an Edsc_BinFunction */
    HFAEntry *poBinFunction = this->poDT->GetNamedChild( "#Bin_Function#" );
    if( poBinFunction == NULL || !EQUAL(poBinFunction->GetType(),"Edsc_BinFunction") )
        poBinFunction = new HFAEntry( hHFA->papoBand[nBand-1]->psInfo,
                                      "#Bin_Function#", "Edsc_BinFunction",
                                      this->poDT );

    // Because of the BaseData we have to hardcode the size. 
    poBinFunction->MakeData( 30 );
       
    poBinFunction->SetStringField("binFunction", "direct");
    poBinFunction->SetDoubleField("minLimit",this->dfRow0Min);
    poBinFunction->SetDoubleField("maxLimit",(this->nRows -1)*this->dfBinSize+this->dfRow0Min);
    poBinFunction->SetIntField("numBins",this->nRows);

    return CE_None;
}

/************************************************************************/
/*                          GetLinearBinning()                          */
/************************************************************************/

int HFARasterAttributeTable::GetLinearBinning( double *pdfRow0Min, double *pdfBinSize ) const
{
    if( !bLinearBinning )
        return FALSE;

    *pdfRow0Min = dfRow0Min;
    *pdfBinSize = dfBinSize;

    return TRUE;
}

/************************************************************************/
/*                              Serialize()                             */
/************************************************************************/

CPLXMLNode *HFARasterAttributeTable::Serialize() const
{
    if( ( GetRowCount() * GetColumnCount() ) > RAT_MAX_ELEM_FOR_CLONE )
        return NULL;

    return GDALRasterAttributeTable::Serialize();
}

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
    
/* -------------------------------------------------------------------- */
/*      If this is an overview, we need to fetch the actual size,       */
/*      and block size.                                                 */
/* -------------------------------------------------------------------- */
    if( iOverview > -1 )
    {
        int nHFADataTypeO;

        nOverviews = 0;
        if (HFAGetOverviewInfo( hHFA, nBand, iOverview,
                                &nRasterXSize, &nRasterYSize,
                                &nBlockXSize, &nBlockYSize, &nHFADataTypeO ) != CE_None)
        {
            nRasterXSize = nRasterYSize = 0;
            return;
        }

/* -------------------------------------------------------------------- */
/*      If we are an 8bit overview of a 1bit layer, we need to mark     */
/*      ourselves as being "resample: average_bit2grayscale".           */
/* -------------------------------------------------------------------- */
        if( nHFADataType == EPT_u1 && nHFADataTypeO == EPT_u8 )
        {
            GDALMajorObject::SetMetadataItem( "RESAMPLING", 
                                              "AVERAGE_BIT2GRAYSCALE" );
            GDALMajorObject::SetMetadataItem( "NBITS", "8" );
            nHFADataType = nHFADataTypeO;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set some other information.                                     */
/* -------------------------------------------------------------------- */
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
        return CPLAtofM(pszValue);
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
        return CPLAtofM(pszValue);
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
            if (papoOverviewBands[iOvIndex]->GetXSize() == 0)
            {
                delete papoOverviewBands[iOvIndex];
                papoOverviewBands[iOvIndex] = NULL;
            }
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
    }

    if( eErr == CE_None && nHFADataType == EPT_u4 )
    {
        GByte	*pabyData = (GByte *) pImage;

        for( int ii = nBlockXSize * nBlockYSize - 2; ii >= 0; ii -= 2 )
        {
            int k = ii>>1;
            pabyData[ii+1] = (pabyData[k]>>4) & 0xf;
            pabyData[ii]   = (pabyData[k]) & 0xf;
        }
    }
    if( eErr == CE_None && nHFADataType == EPT_u2 )
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
    if( eErr == CE_None && nHFADataType == EPT_u1)
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
            
            if( papoOverviewBands[i] == NULL )
            {
                CPLDebug("HFA", "Shouldn't happen happened at line %d", __LINE__);
                continue;
            }

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
            iResult = HFACreateOverview( hHFA, nBand, 
                                         panOverviewList[iOverview],
                                         pszResampling );
            if( iResult < 0 )
                return CE_Failure;

            if( papoOverviewBands == NULL && nOverviews == 0 && iResult > 0)
            {
                CPLDebug("HFA", "Shouldn't happen happened at line %d", __LINE__);
                papoOverviewBands = (HFARasterBand **) 
                    CPLCalloc( sizeof(void*), iResult );
            }

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
    if( poRAT == NULL )
        return CE_Failure;

    return WriteNamedRAT( "Descriptor_Table", poRAT );
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *HFARasterBand::GetDefaultRAT()

{		
    if( poDefaultRAT == NULL )
        poDefaultRAT = new HFARasterAttributeTable(this, "Descriptor_Table" );
 
    return poDefaultRAT;
}

/************************************************************************/
/*                            WriteNamedRAT()                            */
/************************************************************************/
 
CPLErr HFARasterBand::WriteNamedRAT( const char *pszName, const GDALRasterAttributeTable *poRAT )
{
/* -------------------------------------------------------------------- */
/*      Find the requested table.                                       */
/* -------------------------------------------------------------------- */
    HFAEntry * poDT = hHFA->papoBand[nBand-1]->poNode->GetNamedChild( "Descriptor_Table" );
    if( poDT == NULL || !EQUAL(poDT->GetType(),"Edsc_Table") )
        poDT = new HFAEntry( hHFA->papoBand[nBand-1]->psInfo, 
                             "Descriptor_Table", "Edsc_Table",
                             hHFA->papoBand[nBand-1]->poNode );
   
  
    int nRowCount = poRAT->GetRowCount();

    poDT->SetIntField( "numrows", nRowCount );
    /* Check if binning is set on this RAT */    
    double dfBinSize, dfRow0Min;
    if(poRAT->GetLinearBinning( &dfRow0Min, &dfBinSize)) 
    {
        /* then it should have an Edsc_BinFunction */
        HFAEntry *poBinFunction = poDT->GetNamedChild( "#Bin_Function#" );
        if( poBinFunction == NULL || !EQUAL(poBinFunction->GetType(),"Edsc_BinFunction") )
            poBinFunction = new HFAEntry( hHFA->papoBand[nBand-1]->psInfo,
                                          "#Bin_Function#", "Edsc_BinFunction",
                                          poDT );
       
        poBinFunction->SetStringField("binFunction", "direct");
        poBinFunction->SetDoubleField("minLimit",dfRow0Min);
        poBinFunction->SetDoubleField("maxLimit",(nRowCount -1)*dfBinSize+dfRow0Min);
        poBinFunction->SetIntField("numBins",nRowCount);
    }
 
/* -------------------------------------------------------------------- */
/*      Loop through each column in the RAT                             */
/* -------------------------------------------------------------------- */
    for(int col = 0; col < poRAT->GetColumnCount(); col++)
    {
        const char *pszName = NULL;
 
        if( poRAT->GetUsageOfCol(col) == GFU_Red )
        {
            pszName = "Red";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_Green )
        {
            pszName = "Green";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_Blue )
        {
            pszName = "Blue";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_Alpha )
        {
            pszName = "Opacity";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_PixelCount )
        {
            pszName = "Histogram";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_Name )
        {
            pszName = "Class_Names";
        }
        else
        {
            pszName = poRAT->GetNameOfCol(col);
        }
            
/* -------------------------------------------------------------------- */
/*      Check to see if a column with pszName exists and create if      */
/*      if necessary.                                                   */
/* -------------------------------------------------------------------- */

        HFAEntry        *poColumn;
        poColumn = poDT->GetNamedChild(pszName);
	    
        if(poColumn == NULL || !EQUAL(poColumn->GetType(),"Edsc_Column"))
	    poColumn = new HFAEntry( hHFA->papoBand[nBand-1]->psInfo,
                                     pszName, "Edsc_Column",
                                     poDT );


        poColumn->SetIntField( "numRows", nRowCount );
        // color cols which are integer in GDAL are written as floats in HFA
        int bIsColorCol = FALSE;
        if( ( poRAT->GetUsageOfCol(col) == GFU_Red ) || ( poRAT->GetUsageOfCol(col) == GFU_Green ) ||
            ( poRAT->GetUsageOfCol(col) == GFU_Blue) || ( poRAT->GetUsageOfCol(col) == GFU_Alpha ) )
        {
            bIsColorCol = TRUE;
        }
   
        if( ( poRAT->GetTypeOfCol(col) == GFT_Real ) || bIsColorCol )
        {
            int nOffset = HFAAllocateSpace( hHFA->papoBand[nBand-1]->psInfo,
                                            nRowCount * sizeof(double) );
            poColumn->SetIntField( "columnDataPtr", nOffset );
            poColumn->SetStringField( "dataType", "real" );
            
            double *padfColData = (double*)CPLCalloc( nRowCount, sizeof(double) );
            for( int i = 0; i < nRowCount; i++)
            {
                if( bIsColorCol )
                    // stored 0..1
                    padfColData[i] = poRAT->GetValueAsInt(i,col) / 255.0;
                else
                    padfColData[i] = poRAT->GetValueAsDouble(i,col);
            }
#ifdef CPL_MSB
            GDALSwapWords( padfColData, 8, nRowCount, 8 );
#endif
            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            VSIFWriteL( padfColData, nRowCount, sizeof(double), hHFA->fp );
            CPLFree( padfColData );
        }
        else if( poRAT->GetTypeOfCol(col) == GFT_String )
        {
            unsigned int nMaxNumChars = 0, nNumChars;
            /* find the length of the longest string */
            for( int i = 0; i < nRowCount; i++)
            {
                /* Include terminating byte */
                nNumChars = strlen(poRAT->GetValueAsString(i,col)) + 1;
                if(nMaxNumChars < nNumChars)
                {
                    nMaxNumChars = nNumChars;
                }
            }
       
            int nOffset = HFAAllocateSpace( hHFA->papoBand[nBand-1]->psInfo,
                                            (nRowCount+1) * nMaxNumChars );
            poColumn->SetIntField( "columnDataPtr", nOffset );
            poColumn->SetStringField( "dataType", "string" );
            poColumn->SetIntField( "maxNumChars", nMaxNumChars );
       
            char *pachColData = (char*)CPLCalloc(nRowCount+1,nMaxNumChars);
            for( int i = 0; i < nRowCount; i++)
            {
                strcpy(&pachColData[nMaxNumChars*i],poRAT->GetValueAsString(i,col));
            }
            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            VSIFWriteL( pachColData, nRowCount, nMaxNumChars, hHFA->fp );
            CPLFree( pachColData );
        }
        else if (poRAT->GetTypeOfCol(col) == GFT_Integer)
        {
            int nOffset = HFAAllocateSpace( hHFA->papoBand[nBand-1]->psInfo,
                                            nRowCount * sizeof(GInt32) );
            poColumn->SetIntField( "columnDataPtr", nOffset );
            poColumn->SetStringField( "dataType", "integer" );
            
            GInt32 *panColData = (GInt32*)CPLCalloc(nRowCount, sizeof(GInt32));
            for( int i = 0; i < nRowCount; i++)
            {
                panColData[i] = poRAT->GetValueAsInt(i,col);
            }
#ifdef CPL_MSB
            GDALSwapWords( panColData, 4, nRowCount, 4 );
#endif
            VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
            VSIFWriteL( panColData, nRowCount, sizeof(GInt32), hHFA->fp );
            CPLFree( panColData );
        }
        else
        {
            /* can't deal with any of the others yet */
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Writing this data type in a column is not supported for this Raster Attribute Table.");
        }
    }
   
    return CE_None;  
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
    int                 bPEStringStored = FALSE;

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
        if( sDatum.datumname == NULL )
            sDatum.datumname = (char*) "";

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
        if( nGCS == 4283 )
            sDatum.datumname = (char*) "GDA94";
            
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
        bPEStringStored = WritePeStringIfNeeded(&oSRS, hHFA);

        sPro.proSpheroid.sphereName = (char *)
            poGeogSRS->GetAttrValue( "GEOGCS|DATUM|SPHEROID" );
        sPro.proSpheroid.a = poGeogSRS->GetSemiMajor();
        sPro.proSpheroid.b = poGeogSRS->GetSemiMinor();
        sPro.proSpheroid.radius = sPro.proSpheroid.a;

        double a2 = sPro.proSpheroid.a*sPro.proSpheroid.a;
        double b2 = sPro.proSpheroid.b*sPro.proSpheroid.b;

        sPro.proSpheroid.eSquared = (a2-b2)/a2;
    }

    if( sDatum.datumname == NULL )
        sDatum.datumname = (char*) "";
    if( sPro.proSpheroid.sphereName == NULL )
        sPro.proSpheroid.sphereName = (char*) "";

/* -------------------------------------------------------------------- */
/*      Recognise various projections.                                  */
/* -------------------------------------------------------------------- */
    const char * pszProjName = NULL;

    if( bHaveSRS )
        pszProjName = oSRS.GetAttrValue( "PROJCS|PROJECTION" );

    if( bForceToPEString && !bPEStringStored )
    {
        char *pszPEString = NULL;
        oSRS.morphToESRI();
        oSRS.exportToWkt( &pszPEString );
        // need to transform this into ESRI format.
        HFASetPEString( hHFA, pszPEString );
        CPLFree( pszPEString );

        bPEStringStored = TRUE;
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
    else if( EQUAL(pszProjName,SRS_PT_MERCATOR_1SP) 
             && oSRS.GetProjParm(SRS_PP_SCALE_FACTOR) == 1.0 )
    {
        sPro.proNumber = EPRJ_MERCATOR;
        sPro.proName = (char*) "Mercator";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_MERCATOR_1SP) )
    {
        sPro.proNumber = EPRJ_MERCATOR_VARIANT_A;
        sPro.proName = (char*) "Mercator (Variant A)";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR);
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,SRS_PT_KROVAK) )
    {
        sPro.proNumber = EPRJ_KROVAK;
        sPro.proName = (char*) "Krovak";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR);
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH)*D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[9] = oSRS.GetProjParm(SRS_PP_PSEUDO_STD_PARALLEL_1);

        // XY plane rotation
        sPro.proParams[8] = 0.0;
        // X scale
        sPro.proParams[10] = 1.0;
        // Y scale
        sPro.proParams[11] = 1.0;
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
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
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
    else if( EQUAL(pszProjName,SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER) )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER;
        sPro.proName = (char*) "Hotine Oblique Mercator Azimuth Center";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH)*D2R;
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
    else if( EQUAL(pszProjName,SRS_PT_TWO_POINT_EQUIDISTANT) )
    {
        sPro.proNumber = EPRJ_TWO_POINT_EQUIDISTANT;
        sPro.proName = (char*) "Two_Point_Equidistant";
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0)*D2R;
        sPro.proParams[9] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0)*D2R;
        sPro.proParams[10] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 60.0)*D2R;
        sPro.proParams[11] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_2, 60.0)*D2R;
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
    else if( EQUAL(pszProjName,"Behrmann") )
    {
        sPro.proNumber = EPRJ_BEHRMANN;
        sPro.proName = (char*) "Behrmann";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName,"Equidistant_Cylindrical") )
    {
        sPro.proNumber = EPRJ_EQUIDISTANT_CYLINDRICAL;
        sPro.proName = (char*) "Equidistant_Cylindrical";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_KROVAK) )
    {
        sPro.proNumber = EPRJ_KROVAK;
        sPro.proName = (char*) "Krovak";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH)*D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] = oSRS.GetProjParm("XY_Plane_Rotation", 0.0)*D2R;
        sPro.proParams[9] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[10] = oSRS.GetProjParm("X_Scale", 1.0);
        sPro.proParams[11] = oSRS.GetProjParm("Y_Scale", 1.0);
    }
    else if( EQUAL(pszProjName, "Double_Stereographic") )
    {
        sPro.proNumber = EPRJ_DOUBLE_STEREOGRAPHIC;
        sPro.proName = (char*) "Double_Stereographic";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Aitoff") )
    {
        sPro.proNumber = EPRJ_AITOFF;
        sPro.proName = (char*) "Aitoff";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Craster_Parabolic") )
    {
        sPro.proNumber = EPRJ_CRASTER_PARABOLIC;
        sPro.proName = (char*) "Craster_Parabolic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        sPro.proNumber = EPRJ_CYLINDRICAL_EQUAL_AREA;
        sPro.proName = (char*) "Cylindrical_Equal_Area";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Flat_Polar_Quartic") )
    {
        sPro.proNumber = EPRJ_FLAT_POLAR_QUARTIC;
        sPro.proName = (char*) "Flat_Polar_Quartic";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Times") )
    {
        sPro.proNumber = EPRJ_TIMES;
        sPro.proName = (char*) "Times";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Winkel_Tripel") )
    {
        sPro.proNumber = EPRJ_WINKEL_TRIPEL;
        sPro.proName = (char*) "Winkel_Tripel";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1)*D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Hammer_Aitoff") )
    {
        sPro.proNumber = EPRJ_HAMMER_AITOFF;
        sPro.proName = (char*) "Hammer_Aitoff";
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Vertical_Near_Side_Perspective") )
    {
        sPro.proNumber = EPRJ_VERTICAL_NEAR_SIDE_PERSPECTIVE;
        sPro.proName = (char*) "Vertical_Near_Side_Perspective";
        sPro.proParams[2] = oSRS.GetProjParm("Height");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER, 75.0)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER, 40.0)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Hotine_Oblique_Mercator_Two_Point_Center") )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_CENTER;
        sPro.proName = (char*) "Hotine_Oblique_Mercator_Two_Point_Center";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER, 40.0)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0)*D2R;
        sPro.proParams[9] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0)*D2R;
        sPro.proParams[10] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 60.0)*D2R;
        sPro.proParams[11] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_2, 60.0)*D2R;
    }
    else if( EQUAL(pszProjName, SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN;
        sPro.proName = (char*) "Hotine_Oblique_Mercator_Two_Point_Natural_Origin";
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER, 40.0)*D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0)*D2R;
        sPro.proParams[9] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0)*D2R;
        sPro.proParams[10] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 60.0)*D2R;
        sPro.proParams[11] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_2, 60.0)*D2R;
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
        if(!bPEStringStored)
        {
            char *pszPEString = NULL;
            oSRS.morphToESRI();
            oSRS.exportToWkt( &pszPEString );
            // need to transform this into ESRI format.
            HFASetPEString( hHFA, pszPEString );
            CPLFree( pszPEString );
            bPEStringStored = TRUE;
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

        if( !bPEStringStored )
            HFASetPEString( hHFA, "" );
    }
    else if( !bPEStringStored )
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
int WritePeStringIfNeeded(OGRSpatialReference* poSRS, HFAHandle hHFA)
{
  OGRBoolean ret = FALSE;
  if(!poSRS || !hHFA)
    return ret;

  const char *pszGEOGCS = poSRS->GetAttrValue( "GEOGCS" );
  const char *pszDatum = poSRS->GetAttrValue( "DATUM" );
  int gcsNameOffset = 0;
  int datumNameOffset = 0;
  if( pszGEOGCS == NULL )
      pszGEOGCS = "";
  if( pszDatum == NULL )
      pszDatum = "";
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

        // The PCS name from the above function may be different with the input name. 
        // If there is a PCS name in psMapInfo that is different with 
        // the one in psPro, just use it as the PCS name. This case happens 
        // if the dataset's SR was written by the new GDAL. 
        if( psMapInfo && strlen(psMapInfo->proName) > 0 
            && strlen(psPro->proName) > 0 
            && !EQUAL(psMapInfo->proName, psPro->proName) )
            oSRS.SetProjCS( psMapInfo->proName );
        break;

      case EPRJ_STATE_PLANE:
      {
          char *pszUnitsName = NULL;
          double dfLinearUnits = oSRS.GetLinearUnits( &pszUnitsName );
          
          pszUnitsName = CPLStrdup( pszUnitsName );

          /* Historically, hfa used esri state plane zone code. Try esri pe string first. */ 
          int zoneCode = ESRIToUSGSZone(psPro->proZone);
          char nad[32];
          strcpy(nad, "HARN");
          if(psDatum)
              strcpy(nad, psDatum->datumname);
          char units[32];
          strcpy(units, "meters");
          if(psMapInfo)
              strcpy(units, psMapInfo->units);
          else if(pszUnitsName && strlen(pszUnitsName) > 0)
              strcpy(units, pszUnitsName);
          int proNu = 0;
          if(psPro)
              proNu = psPro->proNumber;
          if(oSRS.ImportFromESRIStatePlaneWKT(zoneCode, nad, units, proNu) == OGRERR_NONE)
          {
              CPLFree( pszUnitsName );
              oSRS.morphFromESRI();
              oSRS.AutoIdentifyEPSG();
              oSRS.Fixup();
              if( oSRS.exportToWkt( &pszNewProj ) == OGRERR_NONE )
                  return pszNewProj;
              else
                  return NULL;
          }

          /* Set state plane zone.  Set NAD83/27 on basis of spheroid */
          oSRS.SetStatePlane( ESRIToUSGSZone(psPro->proZone), 
                              fabs(psPro->proSpheroid.a - 6378137.0)< 1.0,
                              pszUnitsName, dfLinearUnits );

          CPLFree( pszUnitsName );

          // Same as the UTM, The following is needed.
          if( psMapInfo && strlen(psMapInfo->proName) > 0 
              && strlen(psPro->proName) > 0 
              && !EQUAL(psMapInfo->proName, psPro->proName) )
              oSRS.SetProjCS( psMapInfo->proName );
      }
      break;

      case EPRJ_ALBERS_CONIC_EQUAL_AREA:
        oSRS.SetACEA( psPro->proParams[2]*R2D, psPro->proParams[3]*R2D,
                      psPro->proParams[5]*R2D, psPro->proParams[4]*R2D,
                      psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_LAMBERT_CONFORMAL_CONIC:
        // check the possible Wisconsin first
        if(psDatum && psMapInfo && EQUAL(psDatum->datumname, "HARN"))
        {
            if(oSRS.ImportFromESRIWisconsinWKT("Lambert_Conformal_Conic", psPro->proParams[4]*R2D, psPro->proParams[5]*R2D, psMapInfo->units) == OGRERR_NONE)
            {
                oSRS.morphFromESRI();
                oSRS.AutoIdentifyEPSG();
                oSRS.Fixup();
                if( oSRS.exportToWkt( &pszNewProj ) == OGRERR_NONE )
                    return pszNewProj;
            }
        }
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
      case EPRJ_GAUSS_KRUGER:
        // check the possible Wisconsin first
        if(psDatum && psMapInfo && EQUAL(psDatum->datumname, "HARN"))
        {
            if(oSRS.ImportFromESRIWisconsinWKT("Transverse_Mercator", psPro->proParams[4]*R2D, psPro->proParams[5]*R2D, psMapInfo->units) == OGRERR_NONE)
            {
                oSRS.morphFromESRI();
                oSRS.AutoIdentifyEPSG();
                oSRS.Fixup();
                if( oSRS.exportToWkt( &pszNewProj ) == OGRERR_NONE )
                    return pszNewProj;
            }
        }
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

      case EPRJ_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER:
        if( psPro->proParams[12] > 0.0 )
            oSRS.SetHOMAC( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D,
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

      case EPRJ_TWO_POINT_EQUIDISTANT:
        oSRS.SetTPED( psPro->proParams[9] * R2D,
                      psPro->proParams[8] * R2D,
                      psPro->proParams[11] * R2D,
                      psPro->proParams[10] * R2D,
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
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_QUARTIC_AUTHALIC:
      {
          oSRS.SetProjection( "Quartic_Authalic" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
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
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
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
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_BEHRMANN:
      {
          oSRS.SetProjection( "Behrmann" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_KROVAK:
        oSRS.SetKrovak( psPro->proParams[4]*R2D, psPro->proParams[5]*R2D,
                        psPro->proParams[3]*R2D, psPro->proParams[9]*R2D,
                        psPro->proParams[2],
                        psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_DOUBLE_STEREOGRAPHIC:
      {
          oSRS.SetProjection( "Double_Stereographic" );
          oSRS.SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 
                           psPro->proParams[5] * R2D );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_SCALE_FACTOR, psPro->proParams[2] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_AITOFF:
      {
          oSRS.SetProjection( "Aitoff" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_CRASTER_PARABOLIC:
      {
          oSRS.SetProjection( "Craster_Parabolic" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_CYLINDRICAL_EQUAL_AREA:
          oSRS.SetCEA(psPro->proParams[2] * R2D, psPro->proParams[4] * R2D, 
                      psPro->proParams[6], psPro->proParams[7]);
      break;

      case EPRJ_FLAT_POLAR_QUARTIC:
      {
          oSRS.SetProjection( "Flat_Polar_Quartic" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_TIMES:
      {
          oSRS.SetProjection( "Times" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_WINKEL_TRIPEL:
      {
          oSRS.SetProjection( "Winkel_Tripel" );
          oSRS.SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1,
                           psPro->proParams[2] * R2D );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_HAMMER_AITOFF:
      {
          oSRS.SetProjection( "Hammer_Aitoff" );
          oSRS.SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_VERTICAL_NEAR_SIDE_PERSPECTIVE:
      {
          oSRS.SetProjection( "Vertical_Near_Side_Perspective" );
          oSRS.SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER,
                           psPro->proParams[5] * R2D );
          oSRS.SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER,
                           psPro->proParams[4] * R2D );
          oSRS.SetNormProjParm( "height",
                           psPro->proParams[2] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_CENTER:
      {
          oSRS.SetProjection( "Hotine_Oblique_Mercator_Twp_Point_Center" );
          oSRS.SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER,
                           psPro->proParams[5] * R2D );
          oSRS.SetNormProjParm( SRS_PP_LATITUDE_OF_1ST_POINT,
                           psPro->proParams[9] * R2D );
          oSRS.SetNormProjParm( SRS_PP_LONGITUDE_OF_1ST_POINT,
                           psPro->proParams[8] * R2D );
          oSRS.SetNormProjParm( SRS_PP_LATITUDE_OF_2ND_POINT,
                           psPro->proParams[11] * R2D );
          oSRS.SetNormProjParm( SRS_PP_LONGITUDE_OF_2ND_POINT,
                           psPro->proParams[10] * R2D );
          oSRS.SetNormProjParm( SRS_PP_SCALE_FACTOR,
                           psPro->proParams[2] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, psPro->proParams[6] );
          oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, psPro->proParams[7] );
      }
      break;

      case EPRJ_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN:
        oSRS.SetHOM2PNO( psPro->proParams[5] * R2D, 
                         psPro->proParams[8] * R2D,
                         psPro->proParams[9] * R2D,
                         psPro->proParams[10] * R2D,
                         psPro->proParams[11] * R2D,
                         psPro->proParams[2],
                         psPro->proParams[6], psPro->proParams[7] );
      break;

      case EPRJ_LAMBERT_CONFORMAL_CONIC_1SP:
        oSRS.SetLCC1SP( psPro->proParams[3]*R2D, psPro->proParams[2]*R2D,
                        psPro->proParams[4],
                        psPro->proParams[5], psPro->proParams[6] );
        break;

      case EPRJ_MERCATOR_VARIANT_A:
        oSRS.SetMercator( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D,
                          psPro->proParams[2],
                          psPro->proParams[6], psPro->proParams[7] );
        break;

      case EPRJ_PSEUDO_MERCATOR: // likely this is google mercator?
        oSRS.SetMercator( psPro->proParams[5]*R2D, psPro->proParams[4]*R2D,
                          1.0,
                          psPro->proParams[6], psPro->proParams[7] );
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
        && strlen(pszPE_COORDSYS) > 0 
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
    
    CPLFree( pszPE_COORDSYS );

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
    {
        for( i = 0; i < nListBands; i++ )
        {
            if (HFAGetOverviewCount(hHFA, panBandList[i]) > 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "Cannot add external overviews when there are already internal overviews");
                return CE_Failure;
            }
        }

        return GDALDataset::IBuildOverviews( pszResampling, 
                                             nOverviews, panOverviewList, 
                                             nListBands, panBandList, 
                                             pfnProgress, pProgressData );
    }

    for( i = 0; i < nListBands; i++ )
    {
        CPLErr eErr;
        GDALRasterBand *poBand;

        void* pScaledProgressData = GDALCreateScaledProgress(
                i * 1.0 / nListBands, (i + 1) * 1.0 / nListBands,
                pfnProgress, pProgressData);

        poBand = GetRasterBand( panBandList[i] );
        
        //GetRasterBand can return NULL
        if(poBand == NULL)
        {
            CPLError(CE_Failure, CPLE_ObjectNull,
                        "GetRasterBand failed");        
            return CE_Failure; 
        }
        
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

    if( HFAGetIGEFilename( hHFA ) != NULL )
    {
        papszFileList = CSLAddString( papszFileList, 
                                      HFAGetIGEFilename( hHFA ) );
    }

    // Request an overview to force opening of dependent overview
    // files. 
    if( nBands > 0 
        && GetRasterBand(1)->GetOverviewCount() > 0 )
        GetRasterBand(1)->GetOverview(0);

    if( hHFA->psDependent != NULL )
    {
        HFAInfo_t *psDep = hHFA->psDependent;

        papszFileList = 
            CSLAddString( papszFileList, 
                          CPLFormFilename( psDep->pszPath, 
                                           psDep->pszFilename, NULL ));
        
        if( HFAGetIGEFilename( psDep ) != NULL )
            papszFileList = CSLAddString( papszFileList, 
                                          HFAGetIGEFilename( psDep ) );
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
/*                               Rename()                               */
/*                                                                      */
/*      Custom Rename() implementation that knows how to update         */
/*      filename references in .img and .aux files.                     */
/************************************************************************/

CPLErr HFADataset::Rename( const char *pszNewName, const char *pszOldName )

{
/* -------------------------------------------------------------------- */
/*      Rename all the files at the filesystem level.                   */
/* -------------------------------------------------------------------- */
    GDALDriver *poDriver = (GDALDriver*) GDALGetDriverByName( "HFA" );

    CPLErr eErr = poDriver->DefaultRename( pszNewName, pszOldName );
    
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Now try to go into the .img file and update RRDNames[]          */
/*      lists.                                                          */
/* -------------------------------------------------------------------- */
    CPLString osOldBasename, osNewBasename;

    osOldBasename = CPLGetBasename( pszOldName );
    osNewBasename = CPLGetBasename( pszNewName );

    if( osOldBasename != osNewBasename )
    {
        HFAHandle hHFA = HFAOpen( pszNewName, "r+" );

        if( hHFA != NULL )
        {
            eErr = HFARenameReferences( hHFA, osNewBasename, osOldBasename );

            HFAGetOverviewCount( hHFA, 1 );

            if( hHFA->psDependent != NULL )
                HFARenameReferences( hHFA->psDependent, 
                                     osNewBasename, osOldBasename );

            HFAClose( hHFA );
        }
    }

    return eErr;
}

/************************************************************************/
/*                             CopyFiles()                              */
/*                                                                      */
/*      Custom CopyFiles() implementation that knows how to update      */
/*      filename references in .img and .aux files.                     */
/************************************************************************/

CPLErr HFADataset::CopyFiles( const char *pszNewName, const char *pszOldName )

{
/* -------------------------------------------------------------------- */
/*      Rename all the files at the filesystem level.                   */
/* -------------------------------------------------------------------- */
    GDALDriver *poDriver = (GDALDriver*) GDALGetDriverByName( "HFA" );

    CPLErr eErr = poDriver->DefaultCopyFiles( pszNewName, pszOldName );
    
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Now try to go into the .img file and update RRDNames[]          */
/*      lists.                                                          */
/* -------------------------------------------------------------------- */
    CPLString osOldBasename, osNewBasename;

    osOldBasename = CPLGetBasename( pszOldName );
    osNewBasename = CPLGetBasename( pszNewName );

    if( osOldBasename != osNewBasename )
    {
        HFAHandle hHFA = HFAOpen( pszNewName, "r+" );

        if( hHFA != NULL )
        {
            eErr = HFARenameReferences( hHFA, osNewBasename, osOldBasename );

            HFAGetOverviewCount( hHFA, 1 );

            if( hHFA->psDependent != NULL )
                HFARenameReferences( hHFA->psDependent, 
                                     osNewBasename, osOldBasename );

            HFAClose( hHFA );
        }
    }

    return eErr;
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
        {
            delete poDS;
            return NULL;
        }
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

            CPLFree(panHistogram);

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
        poDriver->pfnRename = HFADataset::Rename;
        poDriver->pfnCopyFiles = HFADataset::CopyFiles;
        

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

