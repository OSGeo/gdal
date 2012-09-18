/******************************************************************************
 * $Id$
 *
 * Project:  PCIDSK Database File
 * Purpose:  Read/write PCIDSK Database File used by the PCI software, using
 *           the external PCIDSK library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "pcidsk.h"
#include "pcidsk_pct.h"
#include "gdal_pam.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

using namespace PCIDSK;

const PCIDSK::PCIDSKInterfaces *PCIDSK2GetInterfaces(void);

/************************************************************************/
/*                              PCIDSK2Dataset                           */
/************************************************************************/

class PCIDSK2Dataset : public GDALPamDataset
{
    friend class PCIDSK2Band;

    CPLString   osSRS;
    CPLString   osLastMDValue;
    char      **papszLastMDListValue;

    PCIDSKFile  *poFile;

    static GDALDataType  PCIDSKTypeToGDAL( eChanType eType );
    void                 ProcessRPC();

  public:
                PCIDSK2Dataset();
                ~PCIDSK2Dataset();

    static int           Identify( GDALOpenInfo * );
    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *LLOpen( const char *pszFilename, PCIDSK::PCIDSKFile *,
                                 GDALAccess eAccess );
    static GDALDataset  *Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char **papszParmList );

    char              **GetFileList(void);
    CPLErr              GetGeoTransform( double * padfTransform );
    CPLErr              SetGeoTransform( double * );
    const char         *GetProjectionRef();
    CPLErr              SetProjection( const char * );

    CPLErr              SetMetadata( char **, const char * );
    char              **GetMetadata( const char* );
    CPLErr              SetMetadataItem(const char*,const char*,const char*);
    const char         *GetMetadataItem( const char*, const char*);

    virtual void FlushCache(void);

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );
};

/************************************************************************/
/*                             PCIDSK2Band                              */
/************************************************************************/

class PCIDSK2Band : public GDALPamRasterBand
{
    friend class PCIDSK2Dataset;

    PCIDSKChannel *poChannel;
    PCIDSKFile    *poFile;

    void        RefreshOverviewList();
    std::vector<PCIDSK2Band*> apoOverviews;
    
    CPLString   osLastMDValue;
    char      **papszLastMDListValue;

    bool        CheckForColorTable();
    GDALColorTable *poColorTable;
    bool        bCheckedForColorTable;
    int         nPCTSegNumber;

    char      **papszCategoryNames;

    void        Initialize();

  public:
                PCIDSK2Band( PCIDSK2Dataset *, PCIDSKFile *, int );
                PCIDSK2Band( PCIDSKChannel * );
                ~PCIDSK2Band();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual int        GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr SetColorTable( GDALColorTable * ); 

    virtual void        SetDescription( const char * );

    CPLErr              SetMetadata( char **, const char * );
    char              **GetMetadata( const char* );
    CPLErr              SetMetadataItem(const char*,const char*,const char*);
    const char         *GetMetadataItem( const char*, const char*);

    virtual char      **GetCategoryNames();
};

/************************************************************************/
/* ==================================================================== */
/*                            PCIDSK2Band                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            PCIDSK2Band()                             */
/*                                                                      */
/*      This constructor is used for main file channels.                */
/************************************************************************/

PCIDSK2Band::PCIDSK2Band( PCIDSK2Dataset *poDS, 
                          PCIDSKFile *poFile,
                          int nBand )                        

{
    Initialize();

    this->poDS = poDS;
    this->poFile = poFile;
    this->nBand = nBand;

    poChannel = poFile->GetChannel( nBand );

    nBlockXSize = (int) poChannel->GetBlockWidth();
    nBlockYSize = (int) poChannel->GetBlockHeight();
    
    eDataType = PCIDSK2Dataset::PCIDSKTypeToGDAL( poChannel->GetType() );

    if( !EQUALN(poChannel->GetDescription().c_str(),
                "Contents Not Specified",20) )
        GDALMajorObject::SetDescription( poChannel->GetDescription().c_str() );

/* -------------------------------------------------------------------- */
/*      Do we have overviews?                                           */
/* -------------------------------------------------------------------- */
    RefreshOverviewList();
}

/************************************************************************/
/*                            PCIDSK2Band()                             */
/*                                                                      */
/*      This constructor is used for overviews and bitmap segments      */
/*      as bands.                                                       */
/************************************************************************/

PCIDSK2Band::PCIDSK2Band( PCIDSKChannel *poChannel )

{
    Initialize();

    this->poChannel = poChannel;

    nBand = 1;

    nBlockXSize = (int) poChannel->GetBlockWidth();
    nBlockYSize = (int) poChannel->GetBlockHeight();
    
    nRasterXSize = (int) poChannel->GetWidth();
    nRasterYSize = (int) poChannel->GetHeight();

    eDataType = PCIDSK2Dataset::PCIDSKTypeToGDAL( poChannel->GetType() );

    if( poChannel->GetType() == CHN_BIT )
    {
        SetMetadataItem( "NBITS", "1", "IMAGE_STRUCTURE" );

        if( !EQUALN(poChannel->GetDescription().c_str(),
                    "Contents Not Specified",20) )
            GDALMajorObject::SetDescription( poChannel->GetDescription().c_str() );
    }
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void PCIDSK2Band::Initialize()

{
    papszLastMDListValue = NULL;

    poChannel = NULL;
    poFile = NULL;
    poDS = NULL;

    bCheckedForColorTable = false;
    poColorTable = NULL;
    nPCTSegNumber = -1;

    papszCategoryNames = NULL;
}

/************************************************************************/
/*                            ~PCIDSK2Band()                            */
/************************************************************************/

PCIDSK2Band::~PCIDSK2Band()

{
    while( apoOverviews.size() > 0 )
    {
        delete apoOverviews[apoOverviews.size()-1];
        apoOverviews.pop_back();
    }
    CSLDestroy( papszLastMDListValue );
    CSLDestroy( papszCategoryNames );

    delete poColorTable;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void PCIDSK2Band::SetDescription( const char *pszDescription )

{
    try 
    {
        poChannel->SetDescription( pszDescription );

        if( !EQUALN(poChannel->GetDescription().c_str(),
                    "Contents Not Specified",20) )
            GDALMajorObject::SetDescription( poChannel->GetDescription().c_str() );
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/*                                                                      */
/*      Offer category names from Class_*_ metadata.                    */
/************************************************************************/

char **PCIDSK2Band::GetCategoryNames()

{
    // already scanned?
    if( papszCategoryNames != NULL )
        return papszCategoryNames;

    try 
    {
        std::vector<std::string> aosMDKeys = poChannel->GetMetadataKeys();
        size_t i;
        int nClassCount = 0;
        static const int nMaxClasses = 10000;
        papszCategoryNames = (char **) CPLCalloc(nMaxClasses+1, sizeof(char*));
        
        for( i=0; i < aosMDKeys.size(); i++ )
        {
            CPLString osKey = aosMDKeys[i];

            // is this a "Class_n_name" keyword?

            if( !EQUALN(osKey,"Class_",6) )
                continue;

            if( !EQUAL(osKey.c_str() + osKey.size() - 5, "_name") )
                continue;

            // Ignore unreasonable class values.
            int iClass = atoi(osKey.c_str() + 6);

            if( iClass < 0 || iClass > 10000 )
                continue;

            // Fetch the name.
            CPLString osName  = poChannel->GetMetadataValue(osKey);
            
            // do we need to put in place dummy class names for missing values?
            if( iClass >= nClassCount )
            {
                while( iClass >= nClassCount )
                {
                    papszCategoryNames[nClassCount++] = CPLStrdup("");
                    papszCategoryNames[nClassCount] = NULL;
                }
            }

            // Replace target category name.
            CPLFree( papszCategoryNames[iClass] );
            papszCategoryNames[iClass] = NULL;

            papszCategoryNames[iClass] = CPLStrdup(osName);
        }
        
        if( nClassCount == 0 )
            return GDALPamRasterBand::GetCategoryNames();
        else
            return papszCategoryNames;
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return NULL;
    }
}

/************************************************************************/
/*                         CheckForColorTable()                         */
/************************************************************************/

bool PCIDSK2Band::CheckForColorTable()

{
    if( bCheckedForColorTable || poFile == NULL )
        return true;

    bCheckedForColorTable = true;

    try 
    {
/* -------------------------------------------------------------------- */
/*      Try to find an appropriate PCT segment to use.                  */
/* -------------------------------------------------------------------- */
        std::string osDefaultPCT = poChannel->GetMetadataValue("DEFAULT_PCT_REF");
        PCIDSKSegment *poPCTSeg = NULL;

        // If there is no metadata, assume a single PCT in a file with only
        // one raster band must be intended for it.
        if( osDefaultPCT.size() == 0 
            && poDS != NULL 
            && poDS->GetRasterCount() == 1 )
        {
            poPCTSeg = poFile->GetSegment( SEG_PCT, "" );
            if( poPCTSeg != NULL 
                && poFile->GetSegment( SEG_PCT, "", 
                                       poPCTSeg->GetSegmentNumber() ) != NULL )
                poPCTSeg = NULL;
        }
        // Parse default PCT ref assuming an in file reference.
        else if( osDefaultPCT.size() != 0 
                 && strstr(osDefaultPCT.c_str(),"PCT:") != NULL )
        {
            poPCTSeg = poFile->GetSegment( 
                atoi(strstr(osDefaultPCT.c_str(),"PCT:") + 4) );
        }

        if( poPCTSeg != NULL )
        {
            PCIDSK_PCT *poPCT = dynamic_cast<PCIDSK_PCT*>( poPCTSeg );
            poColorTable = new GDALColorTable();
            int i;
            unsigned char abyPCT[768];

            nPCTSegNumber = poPCTSeg->GetSegmentNumber();
            
            poPCT->ReadPCT( abyPCT );
            
            for( i = 0; i < 256; i++ )
            {
                GDALColorEntry sEntry;
                
                sEntry.c1 = abyPCT[256 * 0 + i];
                sEntry.c2 = abyPCT[256 * 1 + i];
                sEntry.c3 = abyPCT[256 * 2 + i];
                sEntry.c4 = 255;
                poColorTable->SetColorEntry( i, &sEntry );
            }
        }

/* -------------------------------------------------------------------- */
/*      If we did not find an appropriate PCT segment, check for        */
/*      Class_n color data from which to construct a color table.       */
/* -------------------------------------------------------------------- */
        std::vector<std::string> aosMDKeys = poChannel->GetMetadataKeys();
        size_t i;
        
        for( i=0; i < aosMDKeys.size(); i++ )
        {
            CPLString osKey = aosMDKeys[i];

            // is this a "Class_n_name" keyword?

            if( !EQUALN(osKey,"Class_",6) )
                continue;

            if( !EQUAL(osKey.c_str() + osKey.size() - 6, "_Color") )
                continue;

            // Ignore unreasonable class values.
            int iClass = atoi(osKey.c_str() + 6);

            if( iClass < 0 || iClass > 10000 )
                continue;

            // Fetch and parse the RGB value "(RGB:red green blue)"
            CPLString osRGB  = poChannel->GetMetadataValue(osKey);
            int nRed, nGreen, nBlue;

            if( !EQUALN(osRGB,"(RGB:",5) )
                continue;

            if( sscanf( osRGB.c_str() + 5, "%d %d %d", 
                        &nRed, &nGreen, &nBlue ) != 3 )
                continue;

            // we have an entry - apply to the color table.
            GDALColorEntry sEntry;

            sEntry.c1 = (short) nRed;
            sEntry.c2 = (short) nGreen;
            sEntry.c3 = (short) nBlue;
            sEntry.c4 = 255;

            if( poColorTable == NULL )
            {
                CPLDebug( "PCIDSK", "Using Class_n_Color metadata for color table." );
                poColorTable = new GDALColorTable();
            }

            poColorTable->SetColorEntry( iClass, &sEntry );
        }
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return false;
    }

    return true;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *PCIDSK2Band::GetColorTable()

{
    CheckForColorTable();

    if( poColorTable )
        return poColorTable;
    else
        return GDALPamRasterBand::GetColorTable();
            
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr PCIDSK2Band::SetColorTable( GDALColorTable *poCT )

{
    if( !CheckForColorTable() )
        return CE_Failure;

    // no color tables on overviews.
    if( poFile == NULL )
        return CE_Failure;

    try 
    {
/* -------------------------------------------------------------------- */
/*      Are we trying to delete the color table?                        */
/* -------------------------------------------------------------------- */
        if( poCT == NULL )
        {
            delete poColorTable;
            poColorTable = NULL;

            if( nPCTSegNumber != -1 )
                poFile->DeleteSegment( nPCTSegNumber );
            poChannel->SetMetadataValue( "DEFAULT_PCT_REF", "" );
            nPCTSegNumber = -1;

            return CE_None;
        }

/* -------------------------------------------------------------------- */
/*      Do we need to create the segment?  If so, also set the          */
/*      default pct metadata.                                           */
/* -------------------------------------------------------------------- */
        if( nPCTSegNumber == -1 )
        {
            nPCTSegNumber = poFile->CreateSegment( "PCTTable", 
                                                   "Default Pseudo-Color Table", 
                                                   SEG_PCT, 0 );
            
            CPLString osRef;
            
            osRef.Printf( "gdb:/{PCT:%d}", nPCTSegNumber );
            poChannel->SetMetadataValue( "DEFAULT_PCT_REF", osRef );
        }

/* -------------------------------------------------------------------- */
/*      Write out the PCT.                                              */
/* -------------------------------------------------------------------- */
        unsigned char abyPCT[768];
        int i, nColorCount = MIN(256,poCT->GetColorEntryCount());

        memset( abyPCT, 0, 768 );

        for( i = 0; i < nColorCount; i++ )
        {
            GDALColorEntry sEntry;

            poCT->GetColorEntryAsRGB( i, &sEntry );
            abyPCT[256 * 0 + i] = (unsigned char) sEntry.c1;
            abyPCT[256 * 1 + i] = (unsigned char) sEntry.c2;
            abyPCT[256 * 2 + i] = (unsigned char) sEntry.c3;
        }

        PCIDSK_PCT *poPCT = dynamic_cast<PCIDSK_PCT*>( 
            poFile->GetSegment( nPCTSegNumber ) );

        poPCT->WritePCT( abyPCT );

        delete poColorTable;
        poColorTable = poCT->Clone();
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }
    
    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp PCIDSK2Band::GetColorInterpretation()

{
    CheckForColorTable();

    if( poColorTable != NULL )
        return GCI_PaletteIndex;
    else
        return GDALPamRasterBand::GetColorInterpretation();
}

/************************************************************************/
/*                        RefreshOverviewList()                         */
/************************************************************************/

void PCIDSK2Band::RefreshOverviewList()

{
/* -------------------------------------------------------------------- */
/*      Clear existing overviews.                                       */
/* -------------------------------------------------------------------- */
    while( apoOverviews.size() > 0 )
    {
        delete apoOverviews[apoOverviews.size()-1];
        apoOverviews.pop_back();
    }

/* -------------------------------------------------------------------- */
/*      Fetch overviews.                                                */
/* -------------------------------------------------------------------- */
    for( int iOver = 0; iOver < poChannel->GetOverviewCount(); iOver++ )
    {								       
        apoOverviews.push_back( 
            new PCIDSK2Band( poChannel->GetOverview(iOver) ) );
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PCIDSK2Band::IReadBlock( int iBlockX, int iBlockY, void *pData )

{
    try 
    {
        poChannel->ReadBlock( iBlockX + iBlockY * nBlocksPerRow,
                              pData );

        // Do we need to upsample 1bit to 8bit?
        if( poChannel->GetType() == CHN_BIT )
        {
            GByte	*pabyData = (GByte *) pData;

            for( int ii = nBlockXSize * nBlockYSize - 1; ii >= 0; ii-- )
            {
                if( (pabyData[ii>>3] & (0x80 >> (ii & 0x7))) )
                    pabyData[ii] = 1;
                else
                    pabyData[ii] = 0;
            }
        }

        return CE_None;
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr PCIDSK2Band::IWriteBlock( int iBlockX, int iBlockY, void *pData )

{
    try 
    {
        poChannel->WriteBlock( iBlockX + iBlockY * nBlocksPerRow,
                               pData );
        return CE_None;
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int PCIDSK2Band::GetOverviewCount()

{
    if( apoOverviews.size() > 0 )
        return (int) apoOverviews.size();
    else
        return GDALPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *PCIDSK2Band::GetOverview(int iOverview)

{
    if( iOverview < 0 || iOverview >= (int) apoOverviews.size() )
        return GDALPamRasterBand::GetOverview( iOverview );
    else
        return apoOverviews[iOverview];
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr PCIDSK2Band::SetMetadata( char **papszMD, 
                                 const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        return GDALPamRasterBand::SetMetadata( papszMD, pszDomain );

/* -------------------------------------------------------------------- */
/*      Set each item individually.                                     */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszLastMDListValue );
    papszLastMDListValue = NULL;

    try
    {
        int iItem;

        for( iItem = 0; papszMD && papszMD[iItem]; iItem++ )
        {
            const char *pszItemValue;
            char *pszItemName = NULL;

            pszItemValue = CPLParseNameValue( papszMD[iItem], &pszItemName);
            if( pszItemName != NULL )
            {
                poChannel->SetMetadataValue( pszItemName, pszItemValue );
                CPLFree( pszItemName );
            }
        }
        return CE_None;
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr PCIDSK2Band::SetMetadataItem( const char *pszName, 
                                     const char *pszValue, 
                                     const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        return GDALPamRasterBand::SetMetadataItem(pszName,pszValue,pszDomain);

/* -------------------------------------------------------------------- */
/*      Set on the file.                                                */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszLastMDListValue );
    papszLastMDListValue = NULL;

    try
    {
        if( !pszValue )
          pszValue = "";
        poChannel->SetMetadataValue( pszName, pszValue );
        return CE_None;
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *PCIDSK2Band::GetMetadataItem( const char *pszName, 
                                          const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        return GDALPamRasterBand::GetMetadataItem( pszName, pszDomain );

/* -------------------------------------------------------------------- */
/*      Try and fetch.                                                  */
/* -------------------------------------------------------------------- */
    try
    {
        osLastMDValue = poChannel->GetMetadataValue( pszName );

        if( osLastMDValue == "" )
            return NULL;
        else
            return osLastMDValue.c_str();
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return NULL;
    }
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **PCIDSK2Band::GetMetadata( const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        return GDALPamRasterBand::GetMetadata( pszDomain );

/* -------------------------------------------------------------------- */
/*      If we have a cached result, just use that.                      */
/* -------------------------------------------------------------------- */
    if( papszLastMDListValue != NULL )
        return papszLastMDListValue;

/* -------------------------------------------------------------------- */
/*      Fetch and build the list.                                       */
/* -------------------------------------------------------------------- */
    try
    {
        std::vector<std::string> aosKeys = poChannel->GetMetadataKeys();
        unsigned int i;
    
        for( i = 0; i < aosKeys.size(); i++ )
        {
            if( aosKeys[i].c_str()[0] == '_' )
                continue;

            papszLastMDListValue =
                CSLSetNameValue( papszLastMDListValue,
                                 aosKeys[i].c_str(), 
                                 poChannel->GetMetadataValue(aosKeys[i]).c_str() );
        }
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return NULL;
    }

    return papszLastMDListValue;
}

/************************************************************************/
/* ==================================================================== */
/*                            PCIDSK2Dataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           PCIDSK2Dataset()                            */
/************************************************************************/

PCIDSK2Dataset::PCIDSK2Dataset()
{
    poFile = NULL;
    papszLastMDListValue = NULL;
}

/************************************************************************/
/*                            ~PCIDSK2Dataset()                          */
/************************************************************************/

PCIDSK2Dataset::~PCIDSK2Dataset()
{
    FlushCache();

    try {
        delete poFile;
        poFile = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "PCIDSK SDK Failure in Close(), unexpected exception." );
    }

    CSLDestroy( papszLastMDListValue );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **PCIDSK2Dataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();
    CPLString osBaseDir = CPLGetPath( GetDescription() );

    try 
    {
        for( int nChan = 1; nChan <= poFile->GetChannels(); nChan++ )
        {
            PCIDSKChannel *poChannel = poFile->GetChannel( nChan );
            CPLString osChanFilename;
            uint64 image_offset, pixel_offset, line_offset;
            bool little_endian;

            poChannel->GetChanInfo( osChanFilename, image_offset, 
                                    pixel_offset, line_offset, little_endian );

            if( osChanFilename != "" )
            {
                papszFileList = 
                    CSLAddString( papszFileList, 
                                  CPLProjectRelativeFilename( osBaseDir, 
                                                              osChanFilename ) );
            }
        }
    
        return papszFileList;
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return papszFileList;
    }
}

/************************************************************************/
/*                             ProcessRPC()                             */
/************************************************************************/

void PCIDSK2Dataset::ProcessRPC()

{
/* -------------------------------------------------------------------- */
/*      Search all BIN segments looking for an RPC segment.             */
/* -------------------------------------------------------------------- */
    PCIDSKSegment *poSeg = poFile->GetSegment( SEG_BIN, "" );
    PCIDSKRPCSegment *poRPCSeg = NULL;

    while( poSeg != NULL 
           && (poRPCSeg = dynamic_cast<PCIDSKRPCSegment*>( poSeg )) == NULL )
			   
    {
        poSeg = poFile->GetSegment( SEG_BIN, "", 
                                    poSeg->GetSegmentNumber() );
    }

    if( poRPCSeg == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Turn RPC segment into GDAL RFC 22 style metadata.               */
/* -------------------------------------------------------------------- */
    try
    {
        CPLString osValue;
        double dfLineOffset, dfLineScale, dfSampOffset, dfSampScale;
        double dfLatOffset, dfLatScale,
            dfLongOffset, dfLongScale,
            dfHeightOffset, dfHeightScale;
		
        poRPCSeg->GetRPCTranslationCoeffs( 
            dfLongOffset, dfLongScale, 
            dfLatOffset, dfLatScale,
            dfHeightOffset, dfHeightScale,
            dfSampOffset, dfSampScale,
            dfLineOffset, dfLineScale );

        osValue.Printf( "%.16g", dfLineOffset );
        GDALPamDataset::SetMetadataItem( "LINE_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLineScale );
        GDALPamDataset::SetMetadataItem( "LINE_SCALE", osValue, "RPC" );

        osValue.Printf( "%.16g", dfSampOffset );
        GDALPamDataset::SetMetadataItem( "SAMP_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfSampScale );
        GDALPamDataset::SetMetadataItem( "SAMP_SCALE", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLongOffset );
        GDALPamDataset::SetMetadataItem( "LONG_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLongScale );
        GDALPamDataset::SetMetadataItem( "LONG_SCALE", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLatOffset );
        GDALPamDataset::SetMetadataItem( "LAT_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLatScale );
        GDALPamDataset::SetMetadataItem( "LAT_SCALE", osValue, "RPC" );

        osValue.Printf( "%.16g", dfHeightOffset );
        GDALPamDataset::SetMetadataItem( "HEIGHT_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfHeightScale );
        GDALPamDataset::SetMetadataItem( "HEIGHT_SCALE", osValue, "RPC" );

        CPLString osCoefList;
        std::vector<double> adfCoef;
        int i;

        if( poRPCSeg->GetXNumerator().size() != 20 
            || poRPCSeg->GetXDenominator().size() != 20 
            || poRPCSeg->GetYNumerator().size() != 20 
            || poRPCSeg->GetYDenominator().size() != 20 )
        {
            GDALPamDataset::SetMetadata( NULL, "RPC" );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Did not get 20 values in the RPC coefficients lists." );
            return;
        }

        adfCoef = poRPCSeg->GetYNumerator();
        osCoefList = "";
        for( i = 0; i < 20; i++ )
        {
            osValue.Printf( "%.16g ", adfCoef[i] );
            osCoefList += osValue;
        }
        GDALPamDataset::SetMetadataItem( "LINE_NUM_COEFF", osCoefList, "RPC" );

        adfCoef = poRPCSeg->GetYDenominator();
        osCoefList = "";
        for( i = 0; i < 20; i++ )
        {
            osValue.Printf( "%.16g ", adfCoef[i] );
            osCoefList += osValue;
        }
        GDALPamDataset::SetMetadataItem( "LINE_DEN_COEFF", osCoefList, "RPC" );

        adfCoef = poRPCSeg->GetXNumerator();
        osCoefList = "";
        for( i = 0; i < 20; i++ )
        {
            osValue.Printf( "%.16g ", adfCoef[i] );
            osCoefList += osValue;
        }
        GDALPamDataset::SetMetadataItem( "SAMP_NUM_COEFF", osCoefList, "RPC" );

        adfCoef = poRPCSeg->GetXDenominator();
        osCoefList = "";
        for( i = 0; i < 20; i++ )
        {
            osValue.Printf( "%.16g ", adfCoef[i] );
            osCoefList += osValue;
        }
        GDALPamDataset::SetMetadataItem( "SAMP_DEN_COEFF", osCoefList, "RPC" );
    }
    catch( PCIDSKException ex )
    {
        GDALPamDataset::SetMetadata( NULL, "RPC" );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void PCIDSK2Dataset::FlushCache()

{
    GDALPamDataset::FlushCache();

    if( poFile )
    {
        try {
            poFile->Synchronize();
        }
        catch( PCIDSKException ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
        }
    }
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr PCIDSK2Dataset::SetMetadata( char **papszMD, 
                                    const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        return GDALPamDataset::SetMetadata( papszMD, pszDomain );

/* -------------------------------------------------------------------- */
/*      Set each item individually.                                     */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszLastMDListValue );
    papszLastMDListValue = NULL;

    try
    {
        int iItem;

        for( iItem = 0; papszMD && papszMD[iItem]; iItem++ )
        {
            const char *pszItemValue;
            char *pszItemName = NULL;

            pszItemValue = CPLParseNameValue( papszMD[iItem], &pszItemName);
            if( pszItemName != NULL )
            {
                poFile->SetMetadataValue( pszItemName, pszItemValue );
                CPLFree( pszItemName );
            }
        }
        return CE_None;
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::SetMetadataItem( const char *pszName, 
                                        const char *pszValue, 
                                        const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        return GDALPamDataset::SetMetadataItem( pszName, pszValue, pszDomain );

/* -------------------------------------------------------------------- */
/*      Set on the file.                                                */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszLastMDListValue );
    papszLastMDListValue = NULL;

    try
    {
        poFile->SetMetadataValue( pszName, pszValue );
        return CE_None;
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *PCIDSK2Dataset::GetMetadataItem( const char *pszName, 
                                             const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        return GDALPamDataset::GetMetadataItem( pszName, pszDomain );

/* -------------------------------------------------------------------- */
/*      Try and fetch.                                                  */
/* -------------------------------------------------------------------- */
    try
    {
        osLastMDValue = poFile->GetMetadataValue( pszName );

        if( osLastMDValue == "" )
            return NULL;
        else
            return osLastMDValue.c_str();
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return NULL;
    }
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **PCIDSK2Dataset::GetMetadata( const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        return GDALPamDataset::GetMetadata( pszDomain );

/* -------------------------------------------------------------------- */
/*      If we have a cached result, just use that.                      */
/* -------------------------------------------------------------------- */
    if( papszLastMDListValue != NULL )
        return papszLastMDListValue;

/* -------------------------------------------------------------------- */
/*      Fetch and build the list.                                       */
/* -------------------------------------------------------------------- */
    try
    {
        std::vector<std::string> aosKeys = poFile->GetMetadataKeys();
        unsigned int i;
    
        for( i = 0; i < aosKeys.size(); i++ )
        {
            if( aosKeys[i].c_str()[0] == '_' )
                continue;

            papszLastMDListValue =
                CSLSetNameValue( papszLastMDListValue,
                                 aosKeys[i].c_str(), 
                                 poFile->GetMetadataValue(aosKeys[i]).c_str() );
        }
    }
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return NULL;
    }

    return papszLastMDListValue;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::SetGeoTransform( double * padfTransform )
{
    PCIDSKGeoref *poGeoref = NULL;
    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( PCIDSKException ex )
    {
        // I should really check whether this is an expected issue.
    }
        
    if( poGeoref == NULL )
        return GDALPamDataset::SetGeoTransform( padfTransform );
    else
    {
        try
        {
            poGeoref->WriteSimple( poGeoref->GetGeosys(), 
                                   padfTransform[0], 
                                   padfTransform[1],
                                   padfTransform[2],
                                   padfTransform[3],
                                   padfTransform[4],
                                   padfTransform[5] );
        }
        catch( PCIDSKException ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
            return CE_Failure;
        }

        return CE_None;
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::GetGeoTransform( double * padfTransform )
{
    PCIDSKGeoref *poGeoref = NULL;
    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( PCIDSKException ex )
    {
        // I should really check whether this is an expected issue.
    }
        
    if( poGeoref != NULL )
    {
        try
        {
            poGeoref->GetTransform( padfTransform[0], 
                                    padfTransform[1],
                                    padfTransform[2],
                                    padfTransform[3],
                                    padfTransform[4],
                                    padfTransform[5] );
        }
        catch( PCIDSKException ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
            return CE_Failure;
        }

        // If we got anything non-default return it.
        if( padfTransform[0] != 0.0
            || padfTransform[1] != 1.0
            || padfTransform[2] != 0.0
            || padfTransform[3] != 0.0
            || padfTransform[4] != 0.0
            || padfTransform[5] != 1.0 )
            return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Check for worldfile if we have no other georeferencing.         */
/* -------------------------------------------------------------------- */
    if( GDALReadWorldFile( GetDescription(), "pxw", 
                           padfTransform ) )
        return CE_None;
    else
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr PCIDSK2Dataset::SetProjection( const char *pszWKT )

{
    osSRS = "";

    PCIDSKGeoref *poGeoref = NULL;

    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( PCIDSKException ex )
    {
        // I should really check whether this is an expected issue.
    }
        
    if( poGeoref == NULL )
    {
        return GDALPamDataset::SetProjection( pszWKT );
    }
    else
    {
        char *pszGeosys = NULL;
        char *pszUnits = NULL;
        double *padfPrjParams = NULL;

        OGRSpatialReference oSRS;
        char *pszWKTWork = (char *) pszWKT;

        if( oSRS.importFromWkt( &pszWKTWork ) == OGRERR_NONE
            && oSRS.exportToPCI( &pszGeosys, &pszUnits, 
                                 &padfPrjParams ) == OGRERR_NONE )
        {
            try
            {
                double adfGT[6];
                std::vector<double> adfPCIParameters;
                unsigned int i;

                poGeoref->GetTransform( adfGT[0], adfGT[1], adfGT[2],
                                        adfGT[3], adfGT[4], adfGT[5] );

                poGeoref->WriteSimple( pszGeosys, 
                                       adfGT[0], adfGT[1], adfGT[2],
                                       adfGT[3], adfGT[4], adfGT[5] );

                for( i = 0; i < 17; i++ )
                    adfPCIParameters.push_back( padfPrjParams[i] );

                if( EQUALN(pszUnits,"FOOT",4) )
                    adfPCIParameters.push_back( 
                        (double)(int) PCIDSK::UNIT_US_FOOT );
                else if( EQUALN(pszUnits,"INTL FOOT",9) )
                    adfPCIParameters.push_back( 
                        (double)(int) PCIDSK::UNIT_INTL_FOOT );
                else if( EQUALN(pszUnits,"DEGREE",6) )
                    adfPCIParameters.push_back( 
                        (double)(int) PCIDSK::UNIT_DEGREE );
                else 
                    adfPCIParameters.push_back( 
                        (double)(int) PCIDSK::UNIT_METER );

                poGeoref->WriteParameters( adfPCIParameters );
            }
            catch( PCIDSKException ex )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "%s", ex.what() );
                return CE_Failure;
            }

            CPLFree( pszGeosys );
            CPLFree( pszUnits );
            CPLFree( padfPrjParams );

            return CE_None;
        }
        else
            return GDALPamDataset::SetProjection( pszWKT );
    }
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PCIDSK2Dataset::GetProjectionRef()
{
    if( osSRS != "" )
        return osSRS.c_str();

    PCIDSKGeoref *poGeoref = NULL;

    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( PCIDSKException ex )
    {
        // I should really check whether this is an expected issue.
    }
        
    if( poGeoref == NULL )
    {
        osSRS = GDALPamDataset::GetProjectionRef();
    }
    else
    {
        CPLString osGeosys;
        const char *pszUnits = NULL;
        OGRSpatialReference oSRS;
        char *pszWKT = NULL;
        std::vector<double> adfParameters;

        adfParameters.resize(18);
        try
        {
            if( poGeoref )
            {
                osGeosys = poGeoref->GetGeosys();
                adfParameters = poGeoref->GetParameters();
                if( ((UnitCode)(int)adfParameters[16]) 
                    == PCIDSK::UNIT_DEGREE )
                    pszUnits = "DEGREE";
                else if( ((UnitCode)(int)adfParameters[16]) 
                         == PCIDSK::UNIT_METER )
                    pszUnits = "METER";
                else if( ((UnitCode)(int)adfParameters[16]) 
                         == PCIDSK::UNIT_US_FOOT )
                    pszUnits = "FOOT";
                else if( ((UnitCode)(int)adfParameters[16]) 
                         == PCIDSK::UNIT_INTL_FOOT )
                    pszUnits = "INTL FOOT";
            }
        }
        catch( PCIDSKException ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
        }
        
        if( oSRS.importFromPCI( osGeosys, pszUnits, 
                                &(adfParameters[0]) ) == OGRERR_NONE )
        {
            oSRS.exportToWkt( &pszWKT );
            osSRS = pszWKT;
            CPLFree( pszWKT );
        }
        else
        {
            osSRS = GDALPamDataset::GetProjectionRef();
        }
    }

    return osSRS.c_str();
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::IBuildOverviews( const char *pszResampling, 
                                        int nOverviews, int *panOverviewList,
                                        int nListBands, int *panBandList, 
                                        GDALProgressFunc pfnProgress, 
                                        void *pProgressData )

{
    if( nListBands == 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Currently no support for clearing overviews.                    */
/* -------------------------------------------------------------------- */
    if( nOverviews == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK2 driver does not currently support clearing existing overviews. " );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    int   i, nNewOverviews, *panNewOverviewList = NULL;
    PCIDSK2Band *poBand = (PCIDSK2Band*) GetRasterBand( panBandList[0] );

    nNewOverviews = 0;
    panNewOverviewList = (int *) CPLCalloc(sizeof(int),nOverviews);
    for( i = 0; i < nOverviews && poBand != NULL; i++ )
    {
        int   j;

        for( j = 0; j < poBand->GetOverviewCount(); j++ )
        {
            int    nOvFactor;
            GDALRasterBand * poOverview = poBand->GetOverview( j );
 
            nOvFactor = (int) 
                (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

            if( nOvFactor == panOverviewList[i] 
                || nOvFactor == GDALOvLevelAdjust( panOverviewList[i], 
                                                   poBand->GetXSize() ) )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
            panNewOverviewList[nNewOverviews++] = panOverviewList[i];
        else
            panOverviewList[i] *= -1;
    }

/* -------------------------------------------------------------------- */
/*      Create the overviews that are missing.                          */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nNewOverviews; i++ )
    {
        try 
        {
            // conveniently our resampling values mostly match PCIDSK.
            poFile->CreateOverviews( nListBands, panBandList, 
                                     panNewOverviewList[i], pszResampling );
        }
        catch( PCIDSKException ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
            CPLFree( panNewOverviewList );
            return CE_Failure;
        }
    }

    CPLFree( panNewOverviewList );
    panNewOverviewList = NULL;

    int iBand;
    for( iBand = 0; iBand < nListBands; iBand++ )
    {
        poBand = (PCIDSK2Band *) GetRasterBand( panBandList[iBand] );
        ((PCIDSK2Band *) poBand)->RefreshOverviewList();
    }

/* -------------------------------------------------------------------- */
/*      Actually generate the overview imagery.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand **papoOverviewBands;
    CPLErr eErr = CE_None;
    std::vector<int> anRegenLevels;

    papoOverviewBands = (GDALRasterBand **) 
        CPLCalloc(sizeof(void*),nOverviews);

    for( iBand = 0; iBand < nListBands && eErr == CE_None; iBand++ )
    {
        nNewOverviews = 0;

        poBand = (PCIDSK2Band*) GetRasterBand( panBandList[iBand] );

        for( i = 0; i < nOverviews && poBand != NULL; i++ )
        {
            int   j;
            
            for( j = 0; j < poBand->GetOverviewCount(); j++ )
            {
                int    nOvFactor;
                GDALRasterBand * poOverview = poBand->GetOverview( j );

                nOvFactor = (int) 
                    (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

                if( nOvFactor == panOverviewList[i] 
                    || nOvFactor == GDALOvLevelAdjust( panOverviewList[i], 
                                                       poBand->GetXSize() ) )
                {
                    papoOverviewBands[nNewOverviews++] = poOverview;
                    anRegenLevels.push_back( j );
                    break;
                }
            }
        }

        if( nNewOverviews > 0 )
        {
            eErr = GDALRegenerateOverviews( (GDALRasterBandH) poBand, 
                                            nNewOverviews, 
                                            (GDALRasterBandH*)papoOverviewBands,
                                            pszResampling, 
                                            pfnProgress, pProgressData );

            // Mark the regenerated overviews as valid.
            for( i = 0; i < (int) anRegenLevels.size(); i++ )
                poBand->poChannel->SetOverviewValidity( anRegenLevels[i], 
                                                        true );
        }
    }

    CPLFree(papoOverviewBands);

    return eErr;
}

/************************************************************************/
/*                         PCIDSKTypeToGDAL()                           */
/************************************************************************/

GDALDataType PCIDSK2Dataset::PCIDSKTypeToGDAL( eChanType eType )
{
    switch( eType )
    {
      case CHN_8U:
        return GDT_Byte;
        
      case CHN_16U:
        return GDT_UInt16;
        
      case CHN_16S:
        return GDT_Int16;
        
      case CHN_32R:
        return GDT_Float32;

      case CHN_BIT:
        return GDT_Byte;
        
      case CHN_C16U:
        return GDT_CInt16;
      
      case CHN_C16S:
        return GDT_CInt16;
      
      case CHN_C32R:
        return GDT_CFloat32;
        
      default:
        return GDT_Unknown;
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int PCIDSK2Dataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->nHeaderBytes < 512 
        || !EQUALN((const char *) poOpenInfo->pabyHeader, "PCIDSK  ", 8) )
        return FALSE;
    else
        return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PCIDSK2Dataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the file.                                           */
/* -------------------------------------------------------------------- */
    try {
        PCIDSKFile *poFile = 
            PCIDSK::Open( poOpenInfo->pszFilename, 
                          poOpenInfo->eAccess == GA_ReadOnly ? "r" : "r+",
                          PCIDSK2GetInterfaces() );
        if( poFile == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to re-open %s within PCIDSK driver.\n",
                      poOpenInfo->pszFilename );
            return NULL;
        }

        /* Check if this is a vector-only PCIDSK file */
        if( poFile->GetChannels() == 0 &&
            poFile->GetSegment( PCIDSK::SEG_VEC, "" ) != NULL )
        {
            delete poFile;
            return NULL;
        }

        return LLOpen( poOpenInfo->pszFilename, poFile, poOpenInfo->eAccess );
    }
/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return NULL;
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK::Create() failed, unexpected exception." );
        return NULL;
    }
}

/************************************************************************/
/*                               LLOpen()                               */
/*                                                                      */
/*      Low level variant of open that takes the preexisting            */
/*      PCIDSKFile.                                                     */
/************************************************************************/

GDALDataset *PCIDSK2Dataset::LLOpen( const char *pszFilename, 
                                     PCIDSK::PCIDSKFile *poFile,
                                     GDALAccess eAccess )

{
    try {
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
        PCIDSK2Dataset   *poDS = NULL;

        poDS = new PCIDSK2Dataset();

        poDS->poFile = poFile;
        poDS->eAccess = eAccess;
        poDS->nRasterXSize = poFile->GetWidth();
        poDS->nRasterYSize = poFile->GetHeight();

/* -------------------------------------------------------------------- */
/*      Are we specifically PIXEL or BAND interleaving?                 */
/*                                                                      */
/*      We don't set anything for FILE since it is harder to know if    */
/*      this is tiled or what the on disk interleaving is.              */
/* -------------------------------------------------------------------- */
        if( EQUAL(poFile->GetInterleaving().c_str(),"PIXEL") )
            poDS->SetMetadataItem( "IMAGE_STRUCTURE", "PIXEL", 
                                   "IMAGE_STRUCTURE" );
        else if( EQUAL(poFile->GetInterleaving().c_str(),"BAND") )
            poDS->SetMetadataItem( "IMAGE_STRUCTURE", "BAND", 
                                   "IMAGE_STRUCTURE" );

/* -------------------------------------------------------------------- */
/*      Create band objects.                                            */
/* -------------------------------------------------------------------- */
        int iBand;

        for( iBand = 0; iBand < poFile->GetChannels(); iBand++ )
        {
            PCIDSKChannel* poChannel = poFile->GetChannel( iBand + 1 );
            if (poChannel->GetBlockWidth() <= 0 ||
                poChannel->GetBlockHeight() <= 0)
            {
                delete poDS;
                return NULL;
            }

            poDS->SetBand( iBand+1, new PCIDSK2Band( poDS, poFile, iBand+1 ));
        }

/* -------------------------------------------------------------------- */
/*      Create band objects for bitmap segments.                        */
/* -------------------------------------------------------------------- */
        int nLastBitmapSegment = 0;
        PCIDSKSegment *poBitSeg;
        
        while( (poBitSeg = poFile->GetSegment( SEG_BIT, "", 
                                               nLastBitmapSegment)) != NULL )
        {
            PCIDSKChannel *poChannel = 
                dynamic_cast<PCIDSKChannel*>( poBitSeg );
            if (poChannel->GetBlockWidth() <= 0 ||
                poChannel->GetBlockHeight() <= 0)
            {
                delete poDS;
                return NULL;
            }

            poDS->SetBand( poDS->GetRasterCount()+1, 
                           new PCIDSK2Band( poChannel ) );

            nLastBitmapSegment = poBitSeg->GetSegmentNumber();
        }

/* -------------------------------------------------------------------- */
/*      Process RPC segment, if there is one.                           */
/* -------------------------------------------------------------------- */
        poDS->ProcessRPC();

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        poDS->SetDescription( pszFilename );
        poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
        poDS->oOvManager.Initialize( poDS, pszFilename );
        
        return( poDS );
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "PCIDSK SDK Failure in Open(), unexpected exception." );
    }

    return NULL;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *PCIDSK2Dataset::Create( const char * pszFilename,
                                     int nXSize, int nYSize, int nBands,
                                     GDALDataType eType,
                                     char **papszParmList )

{
    PCIDSKFile *poFile;

/* -------------------------------------------------------------------- */
/*      Prepare channel type list.                                      */
/* -------------------------------------------------------------------- */
    std::vector<eChanType> aeChanTypes;

    if( eType == GDT_Float32 )
        aeChanTypes.resize( MAX(1,nBands), CHN_32R );
    else if( eType == GDT_Int16 )
        aeChanTypes.resize( MAX(1,nBands), CHN_16S );
    else if( eType == GDT_UInt16 )
        aeChanTypes.resize( MAX(1,nBands), CHN_16U );
    else if( eType == GDT_CInt16 )
        aeChanTypes.resize( MAX(1, nBands), CHN_C16S );
    else if( eType == GDT_CFloat32 )
        aeChanTypes.resize( MAX(1, nBands), CHN_C32R );
    else 
        aeChanTypes.resize( MAX(1,nBands), CHN_8U );

/* -------------------------------------------------------------------- */
/*      Reformat options.  Currently no support for jpeg compression    */
/*      quality.                                                        */
/* -------------------------------------------------------------------- */
    CPLString osOptions;
    const char *pszValue;

    pszValue = CSLFetchNameValue( papszParmList, "INTERLEAVING" );
    if( pszValue == NULL )
        pszValue = "BAND";

    osOptions = pszValue;

    if( osOptions == "TILED" )
    {
        pszValue = CSLFetchNameValue( papszParmList, "TILESIZE" );
        if( pszValue != NULL )
            osOptions += pszValue;

        pszValue = CSLFetchNameValue( papszParmList, "COMPRESSION" );
        if( pszValue != NULL )
        {
            osOptions += " ";
            osOptions += pszValue;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try creation.                                                   */
/* -------------------------------------------------------------------- */
    try {
        poFile = PCIDSK::Create( pszFilename, nXSize, nYSize, nBands, 
                                 &(aeChanTypes[0]), osOptions, 
                                 PCIDSK2GetInterfaces() );

/* -------------------------------------------------------------------- */
/*      Apply band descriptions, if provided as creation options.       */
/* -------------------------------------------------------------------- */
        size_t i;

        for( i = 0; papszParmList != NULL && papszParmList[i] != NULL; i++ )
        {
            if( EQUALN(papszParmList[i],"BANDDESC",8) )
            {
                int nBand = atoi(papszParmList[i] + 8 );
                const char *pszDescription = strstr(papszParmList[i],"=");
                if( pszDescription && nBand > 0 && nBand <= nBands )
                {
                    poFile->GetChannel(nBand)->SetDescription( pszDescription+1 );
                }
            }
        }

        return LLOpen( pszFilename, poFile, GA_Update );
    }
/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return NULL;
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK::Create() failed, unexpected exception." );
        return NULL;
    }
}

/************************************************************************/
/*                        GDALRegister_PCIDSK()                         */
/************************************************************************/

void GDALRegister_PCIDSK()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "PCIDSK" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "PCIDSK" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "PCIDSK Database File" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_pcidsk.html" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "pix" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte UInt16 Int16 Float32 CInt16 CFloat32" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='INTERLEAVING' type='string-select' default='BAND' description='raster data organization'>"
"       <Value>PIXEL</Value>"
"       <Value>BAND</Value>"
"       <Value>FILE</Value>"
"       <Value>TILED</Value>"
"   </Option>"
"   <Option name='COMPRESSION' type='string-select' default='NONE' description='compression - (INTERLEAVING=TILED only)'>"
"       <Value>NONE</Value>"
"       <Value>RLE</Value>"
"       <Value>JPEG</Value>"
"   </Option>"
"   <Option name='TILESIZE' type='int' default='127' description='Tile Size (INTERLEAVING=TILED only)'/>"
"</CreationOptionList>" ); 

        poDriver->pfnIdentify = PCIDSK2Dataset::Identify;
        poDriver->pfnOpen = PCIDSK2Dataset::Open;
        poDriver->pfnCreate = PCIDSK2Dataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}


