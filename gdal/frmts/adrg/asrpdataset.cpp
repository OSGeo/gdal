/******************************************************************************
 *
 * Purpose:  ASRP Reader
 * Author:   Frank Warmerdam (warmerdam@pobox.com)
 *
 * Derived from ADRG driver by Even Rouault, even.rouault at mines-paris.org.
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
 * Copyright (c) 2009, Frank Warmerdam
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
#include "ogr_spatialref.h"
#include "cpl_string.h"
#include "iso8211.h"

CPL_CVSID("$Id: geotiff.cpp 16147 2009-01-20 21:18:58Z warmerdam $");

#define N_ELEMENTS(x)  (sizeof(x)/sizeof(x[0]))

class ASRPDataset : public GDALPamDataset
{
    friend class ASRPRasterBand;

    FILE*        fdIMG;
    int*         TILEINDEX;
    int          offsetInIMG;
    int          NFC;
    int          NFL;
    double       LSO;
    double       PSO;
    int          ARV;
    int          BRV;
    int          PCB;
    int          PVB;

    char**       papszSubDatasets;
    
    ASRPDataset* poOverviewDS;
    
    /* For creation */
    int          bCreation;
    FILE*        fdGEN;
    FILE*        fdTHF;
    int          bGeoTransformValid;
    double       adfGeoTransform[6];
    int          nNextAvailableBlock;
    CPLString    osBaseFileName;

    GDALColorTable oCT;

  public:
                 ASRPDataset();
    virtual     ~ASRPDataset();
    
    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * padfGeoTransform );
    virtual CPLErr SetGeoTransform( double * padfGeoTransform );

    virtual char      **GetMetadata( const char * pszDomain = "" );

    void                AddSubDataset(const char* pszFilename);

    static char** GetGENListFromTHF(const char* pszFileName);
    static ASRPDataset* GetFromRecord(const char* pszFileName, DDFRecord * record);
    static GDALDataset *Open( GDALOpenInfo * );
    
    static double GetLongitudeFromString(const char* str);
    static double GetLatitudeFromString(const char* str);
};

/************************************************************************/
/* ==================================================================== */
/*                            ASRPRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ASRPRasterBand : public GDALPamRasterBand
{
    friend class ASRPDataset;

  public:
                            ASRPRasterBand( ASRPDataset *, int );

    virtual CPLErr          IReadBlock( int, int, void * );

    virtual double          GetNoDataValue( int *pbSuccess = NULL );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};


/************************************************************************/
/*                           ASRPRasterBand()                            */
/************************************************************************/

ASRPRasterBand::ASRPRasterBand( ASRPDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_Byte;

    nBlockXSize = 128;
    nBlockYSize = 128;
}

/************************************************************************/
/*                            GetNoDataValue()                          */
/************************************************************************/

double  ASRPRasterBand::GetNoDataValue( int *pbSuccess )
{
    if (pbSuccess)
        *pbSuccess = TRUE;

    return 0;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp ASRPRasterBand::GetColorInterpretation()

{
    ASRPDataset* poDS = (ASRPDataset*)this->poDS;

    if( poDS->oCT.GetColorEntryCount() > 0 )
        return GCI_PaletteIndex;
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *ASRPRasterBand::GetColorTable()

{
    ASRPDataset* poDS = (ASRPDataset*)this->poDS;

    if( poDS->oCT.GetColorEntryCount() > 0 )
        return &(poDS->oCT);
    else
        return NULL;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ASRPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    ASRPDataset* poDS = (ASRPDataset*)this->poDS;
    int offset;
    int nBlock = nBlockYOff * poDS->NFC + nBlockXOff;
    if (nBlockXOff >= poDS->NFC || nBlockYOff >= poDS->NFL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nBlockXOff=%d, NFC=%d, nBlockYOff=%d, NFL=%d",
                 nBlockXOff, poDS->NFC, nBlockYOff, poDS->NFL);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Is this a null block?                                           */
/* -------------------------------------------------------------------- */
    if (poDS->TILEINDEX && poDS->TILEINDEX[nBlock] == 0)
    {
        memset(pImage, 0, 128 * 128);
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Compute the offset to the block.                                */
/* -------------------------------------------------------------------- */
    if (poDS->TILEINDEX)
    {
        if( poDS->PCB == 0 ) // uncompressed 
            offset = poDS->offsetInIMG + (poDS->TILEINDEX[nBlock] - 1) * 128 * 128 * 3 + (nBand - 1) * 128 * 128;
        else // compressed 
            offset = poDS->offsetInIMG + (poDS->TILEINDEX[nBlock] - 1);
    }
    else
        offset = poDS->offsetInIMG + nBlock * 128 * 128 * 3 + (nBand - 1) * 128 * 128;
    
/* -------------------------------------------------------------------- */
/*      Seek to target location.                                        */
/* -------------------------------------------------------------------- */
    if (VSIFSeekL(poDS->fdIMG, offset, SEEK_SET) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot seek to offset %d", offset);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      For uncompressed case we read the 128x128 and return with no    */
/*      further processing.                                             */
/* -------------------------------------------------------------------- */
    if( poDS->PCB == 0 )
    {
        if( VSIFReadL(pImage, 1, 128 * 128, poDS->fdIMG) != 128*128 )
        {
            CPLError(CE_Failure, CPLE_FileIO, 
                     "Cannot read data at offset %d", offset);
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      If this is compressed data, we read a goodly chunk of data      */
/*      and then decode it.                                             */
/* -------------------------------------------------------------------- */
    else if( poDS->PCB != 0 )
    {
        int    nBufSize = 128*128 + 500;
        int    nBytesRead, iPixel, iSrc;
        GByte *pabyCData = (GByte *) CPLCalloc(nBufSize,1);

        nBytesRead = VSIFReadL(pabyCData, 1, nBufSize, poDS->fdIMG);
        if( nBytesRead == 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, 
                     "Cannot read data at offset %d", offset);
            return CE_Failure;
        }
        
        CPLAssert( poDS->PCB == 8 && poDS->PVB == 8 );

        for( iSrc = 0, iPixel = 0; iPixel < 128 * 128; )
        {
            if( iSrc + 2 > nBytesRead )
            {
                CPLFree( pabyCData );
                CPLError(CE_Failure, CPLE_AppDefined, 
                         "Out of data decoding image block, only %d available.",
                         iSrc );
                return CE_Failure;
            }

            int nCount = pabyCData[iSrc++];
            int nValue = pabyCData[iSrc++];

            if( iPixel + nCount > 128 * 128 )
            {
                CPLFree( pabyCData );
                CPLError(CE_Failure, CPLE_AppDefined, 
                      "Too much data decoding image block, likely corrupt." );
                return CE_Failure;
            }

            while( nCount > 0 )
            {
                ((GByte *) pImage)[iPixel++] = nValue;
                nCount--;
            }
        }

        CPLFree( pabyCData );
    }
    
    return CE_None;
}

/************************************************************************/
/*                          ASRPDataset()                               */
/************************************************************************/

ASRPDataset::ASRPDataset()
{
    bCreation = FALSE;
    poOverviewDS = NULL;
    fdIMG = NULL;
    fdGEN = NULL;
    fdTHF = NULL;
    TILEINDEX = NULL;
    papszSubDatasets = NULL;
}

/************************************************************************/
/*                          ~ASRPDataset()                              */
/************************************************************************/

ASRPDataset::~ASRPDataset()
{
    if (poOverviewDS)
    {
        delete poOverviewDS;
    }
    
    CSLDestroy(papszSubDatasets);

    if (fdIMG)
    {
        VSIFCloseL(fdIMG);
    }
    
    if (fdGEN)
    {
        VSIFCloseL(fdGEN);
    }
    if (fdTHF)
    {
        VSIFCloseL(fdTHF);
    }

    if (TILEINDEX)
    {
        delete [] TILEINDEX;
    }
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void ASRPDataset::AddSubDataset( const char* pszFilename)
{
    char	szName[80];
    int		nCount = CSLCount(papszSubDatasets ) / 2;

    sprintf( szName, "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName, pszFilename);

    sprintf( szName, "SUBDATASET_%d_DESC", nCount+1 );
    papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName, pszFilename);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **ASRPDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                        GetProjectionRef()                            */
/************************************************************************/

const char* ASRPDataset::GetProjectionRef()
{
    return( SRS_WKT_WGS84 );
}

/************************************************************************/
/*                        GetGeoTransform()                             */
/************************************************************************/

CPLErr ASRPDataset::GetGeoTransform( double * padfGeoTransform)
{
    if (papszSubDatasets != NULL)
        return CE_Failure;

    padfGeoTransform[0] = LSO/3600.0;
    padfGeoTransform[1] = 360. / ARV;
    padfGeoTransform[2] = 0.0;
    padfGeoTransform[3] = PSO/3600.0;
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[5] = - 360. / BRV;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ASRPDataset::SetGeoTransform( double * padfGeoTransform )

{
    memcpy( adfGeoTransform, padfGeoTransform, sizeof(double)*6 );
    bGeoTransformValid = TRUE;
    return CE_None;
}

/************************************************************************/
/*                     GetLongitudeFromString()                         */
/************************************************************************/

double ASRPDataset::GetLongitudeFromString(const char* str)
{
    char ddd[3+1] = { 0 };
    char mm[2+1] = { 0 };
    char ssdotss[5+1] = { 0 };
    int sign = (str[0] == '+') ? 1 : - 1;
    str++;
    strncpy(ddd, str, 3);
    str+=3;
    strncpy(mm, str, 2);
    str+=2;
    strncpy(ssdotss, str, 5);
    return sign * (atof(ddd) + atof(mm) / 60 + atof(ssdotss) / 3600);
}

/************************************************************************/
/*                      GetLatitudeFromString()                         */
/************************************************************************/

double ASRPDataset::GetLatitudeFromString(const char* str)
{
    char ddd[2+1] = { 0 };
    char mm[2+1] = { 0 };
    char ssdotss[5+1] = { 0 };
    int sign = (str[0] == '+') ? 1 : - 1;
    str++;
    strncpy(ddd, str, 2);
    str+=2;
    strncpy(mm, str, 2);
    str+=2;
    strncpy(ssdotss, str, 5);
    return sign * (atof(ddd) + atof(mm) / 60 + atof(ssdotss) / 3600);
}


/************************************************************************/
/*                           GetFromRecord()                            */
/************************************************************************/

ASRPDataset* ASRPDataset::GetFromRecord(const char* pszFileName, DDFRecord * record)
{
    int SCA = 0;
    int ZNA = 0;
    double PSP;
    int ARV;
    int BRV;
    double LSO;
    double PSO;
    int NFL;
    int NFC;
    CPLString osBAD;
    int TIF;
    int* TILEINDEX = NULL;
    int i;

    DDFField* field;
    DDFFieldDefn *fieldDefn;
    int bSuccess;
    int nSTR;

/* -------------------------------------------------------------------- */
/*      Read a variety of header fields of interest from the .GEN       */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    nSTR = record->GetIntSubfield( "GEN", 0, "STR", 0, &bSuccess );
    if( !bSuccess || nSTR != 4 )
    {
        CPLDebug( "ASRP", "Failed to extract STR, or not 4." );
        return NULL;
    }
        
    SCA = record->GetIntSubfield( "GEN", 0, "SCA", 0, &bSuccess );
    CPLDebug("ASRP", "SCA=%d", SCA);
        
    ZNA = record->GetIntSubfield( "GEN", 0, "ZNA", 0, &bSuccess );
    CPLDebug("ASRP", "ZNA=%d", ZNA);
        
    if (ZNA == 9 || ZNA == 18)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Polar cases are not handled by ASRP driver");
        return NULL;
    }
    
    PSP = record->GetFloatSubfield( "GEN", 0, "PSP", 0, &bSuccess );
    CPLDebug("ASRP", "PSP=%f", PSP);

    if (PSP != 100)
        return NULL;
        
    ARV = record->GetIntSubfield( "GEN", 0, "ARV", 0, &bSuccess );
    CPLDebug("ASRP", "ARV=%d", ARV);

    BRV = record->GetIntSubfield( "GEN", 0, "BRV", 0, &bSuccess );
    CPLDebug("ASRP", "BRV=%d", BRV);

    LSO = record->GetFloatSubfield( "GEN", 0, "LSO", 0, &bSuccess );
    CPLDebug("ASRP", "LSO=%f", LSO);

    PSO = record->GetFloatSubfield( "GEN", 0, "PSO", 0, &bSuccess );
    CPLDebug("ASRP", "PSO=%f", PSO);

    NFL = record->GetIntSubfield( "SPR", 0, "NFL", 0, &bSuccess );
    CPLDebug("ASRP", "NFL=%d", NFL);

    NFC = record->GetIntSubfield( "SPR", 0, "NFC", 0, &bSuccess );
    CPLDebug("ASRP", "NFC=%d", NFC);

    int PNC = record->GetIntSubfield( "SPR", 0, "PNC", 0, &bSuccess );
    CPLDebug("ASRP", "PNC=%d", PNC);

    int PNL = record->GetIntSubfield( "SPR", 0, "PNL", 0, &bSuccess );
    CPLDebug("ASRP", "PNL=%d", PNL);

    if( PNL != 128 || PNC != 128 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,"Unsupported PNL or PNC value.");
        return NULL;
    }

    int PCB = record->GetIntSubfield( "SPR", 0, "PCB", 0 );
    int PVB = record->GetIntSubfield( "SPR", 0, "PVB", 0 );
    if( (PCB != 8 && PCB != 0) || PVB != 8 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCB(%d) or PVB(%d) value unsupported.", PCB, PVB );
        return NULL;
    }

    osBAD = record->GetStringSubfield( "SPR", 0, "BAD", 0, &bSuccess );
    {
        char* c = (char*) strchr(osBAD, ' ');
        if (c)
            *c = 0;
    }
    CPLDebug("ASRP", "BAD=%s", osBAD.c_str());
    
/* -------------------------------------------------------------------- */
/*      Read the tile map if available.                                 */
/* -------------------------------------------------------------------- */
    TIF = EQUAL(record->GetStringSubfield( "SPR", 0, "TIF", 0 ),"Y");
    CPLDebug("ASRP", "TIF=%d", TIF);
    
    if (TIF)
    {
        field = record->FindField( "TIM" );
        if( field == NULL )
            return NULL;

        fieldDefn = field->GetFieldDefn();
        DDFSubfieldDefn *subfieldDefn = fieldDefn->FindSubfieldDefn( "TSI" );
        if( subfieldDefn == NULL )
            return NULL;

        int nIndexValueWidth = subfieldDefn->GetWidth();

        if (field->GetDataSize() != nIndexValueWidth * NFL * NFC + 1)
        {
            return NULL;
        }
    
        TILEINDEX = new int [NFL * NFC];
        const char* ptr = field->GetData();
        char offset[30]={0};
        offset[nIndexValueWidth] = '\0';

        for(i=0;i<NFL*NFC;i++)
        {
            strncpy(offset, ptr, nIndexValueWidth);
            ptr += nIndexValueWidth;
            TILEINDEX[i] = atoi(offset);
            //CPLDebug("ASRP", "TSI[%d]=%d", i, TILEINDEX[i]);
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Open the .IMG file.  Try to recover gracefully if the case      */
/*      of the filename is wrong.                                       */
/* -------------------------------------------------------------------- */
    CPLString osDirname = CPLGetDirname(pszFileName);
    CPLString osImgName = CPLFormCIFilename(osDirname, osBAD, NULL);

    FILE* fdIMG = VSIFOpenL(osImgName, "rb");
    
/* -------------------------------------------------------------------- */
/*      Establish the offset to the first byte of actual image data     */
/*      in the IMG file, skipping the ISO8211 header.                   */
/*                                                                      */
/*      This code is awfully fragile!                                   */
/* -------------------------------------------------------------------- */
    int offsetInIMG = 0;
    char c;
    char recordName[3];
    if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
    {
        return NULL;
    }
    while (!VSIFEofL(fdIMG))
    {
        if (c == 30)
        {
            if (VSIFReadL(recordName, 1, 3, fdIMG) != 3)
            {
                return NULL;
            }
            offsetInIMG += 3;
            if (strncmp(recordName,"IMG",3) == 0)
            {
                offsetInIMG += 4;
                if (VSIFSeekL(fdIMG,3,SEEK_CUR) != 0)
                {
                    return NULL;
                }
                if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
                {
                    return NULL;
                }
                while(c ==' ' || c== '^')
                {
                    offsetInIMG ++;
                    if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
                    {
                        return NULL;
                    }
                }
                offsetInIMG ++;
                break;
            }
        }

        offsetInIMG ++;
        if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
        {
            return NULL;
        }
    }
    
    if (VSIFEofL(fdIMG))
    {
        return NULL;
    }
    
    CPLDebug("ASRP", "Img offset data = %d", offsetInIMG);
    
/* -------------------------------------------------------------------- */
/*      Establish the ASRP Dataset.                                     */
/* -------------------------------------------------------------------- */
    ASRPDataset* poDS = new ASRPDataset();
    
    poDS->NFC = NFC;
    poDS->NFL = NFL;
    poDS->nRasterXSize = NFC * 128;
    poDS->nRasterYSize = NFL * 128;
    poDS->LSO = LSO;
    poDS->PSO = PSO;
    poDS->ARV = ARV;
    poDS->BRV = BRV;
    poDS->PCB = PCB;
    poDS->PVB = PVB;
    poDS->TILEINDEX = TILEINDEX;
    poDS->fdIMG = fdIMG;
    poDS->offsetInIMG = offsetInIMG;
    poDS->poOverviewDS = NULL;
    
    char pszValue[32];
    sprintf(pszValue, "%d", SCA);
    poDS->SetMetadataItem( "ASRP_SCA", pszValue );
    
    poDS->nBands = 1;
    for( i = 0; i < poDS->nBands; i++ )
        poDS->SetBand( i+1, new ASRPRasterBand( poDS, i+1 ) );

/* -------------------------------------------------------------------- */
/*      Try to collect a color map from the .QAL file.                  */
/* -------------------------------------------------------------------- */
    CPLString osBasename = CPLGetBasename(pszFileName);
    CPLString osQALFilename = CPLFormCIFilename(osDirname, osBasename, "QAL");

    DDFModule oQALModule;

    if( oQALModule.Open( osQALFilename, TRUE ) )
    {
        while( (record = oQALModule.ReadRecord()) != NULL
               && record->FindField( "COL" ) == NULL ) {}

        if( record != NULL )
        {
            int            iColor;
            int            nColorCount = 
                record->FindField("COL")->GetRepeatCount();

            for( iColor = 0; iColor < nColorCount; iColor++ )
            {
                int bSuccess;
                int nCCD, nNSR, nNSG, nNSB;
                GDALColorEntry sEntry;

                nCCD = record->GetIntSubfield( "COL", 0, "CCD", iColor,
                                               &bSuccess );
                if( !bSuccess )
                    break;

                nNSR = record->GetIntSubfield( "COL", 0, "NSR", iColor );
                nNSG = record->GetIntSubfield( "COL", 0, "NSG", iColor );
                nNSB = record->GetIntSubfield( "COL", 0, "NSB", iColor );

                sEntry.c1 = nNSR;
                sEntry.c2 = nNSG;
                sEntry.c3 = nNSB;
                sEntry.c4 = 255;

                poDS->oCT.SetColorEntry( nCCD, &sEntry );
            }
        }
    }
    else
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unable to find .QAL file, no color table applied." );

    return poDS;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ASRPDataset::Open( GDALOpenInfo * poOpenInfo )
{
    DDFModule module;
    DDFRecord * record;
    ASRPDataset* overviewDS = NULL;
    CPLString osFileName(poOpenInfo->pszFilename);
    CPLString osNAM;

    if( poOpenInfo->nHeaderBytes < 500 )
        return NULL;

    if (!EQUAL(CPLGetExtension(osFileName), "gen"))
        return NULL;

    if (!module.Open(osFileName, TRUE))
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The ASRP driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

    while (TRUE)
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == NULL)
          break;

        const char* RTY = record->GetStringSubfield( "001", 0, "RTY", 0 );
        if( RTY == NULL || !EQUAL(RTY,"GIN") )
            continue;

        const char *PRT = record->GetStringSubfield( "DSI", 0, "PRT", 0 );
        if( PRT == NULL || !EQUALN(PRT,"ASRP",4) )
            continue;
        
        osNAM = record->GetStringSubfield( "DSI", 0, "NAM", 0 );
        CPLDebug("ASRP", "NAM=%s", osNAM.c_str());
        
        ASRPDataset* poDS = GetFromRecord(osFileName, record);
        if (poDS)
        {
            poDS->SetMetadataItem( "ASRP_NAM", osNAM );
            
            poDS->poOverviewDS = overviewDS;
                
            /* ---------------------------------------------------------- */
            /*      Check for external overviews.                         */
            /* ---------------------------------------------------------- */
            poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
                
            /* ---------------------------------------------------------- */
            /*      Initialize any PAM information.                       */
            /* ---------------------------------------------------------- */
            poDS->SetDescription( poOpenInfo->pszFilename );
            poDS->TryLoadXML();
        }
        else if (overviewDS)
        {
            delete overviewDS;
        }

        return poDS;
    }
    
    if (overviewDS)
        delete overviewDS;
    
    return NULL;
}

/************************************************************************/
/*                         GDALRegister_ASRP()                          */
/************************************************************************/

void GDALRegister_ASRP()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "ASRP" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ASRP" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ARC Standard Raster Product" );
//        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
//                                   "frmt_various.html#ASRP" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gen" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = ASRPDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

