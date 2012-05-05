/******************************************************************************
 * $Id$
 * Purpose:  ASRP/USRP Reader
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

CPL_CVSID("$Id$");

class SRPDataset : public GDALPamDataset
{
    friend class SRPRasterBand;

    static CPLString ResetTo01( const char* str );

    VSILFILE*        fdIMG;
    int*         TILEINDEX;
    int          offsetInIMG;
    CPLString    osProduct;
    CPLString    osSRS;
    CPLString    osGENFilename;
    CPLString    osQALFilename;
    int          NFC;
    int          NFL;
    int          ZNA;
    double       LSO;
    double       PSO;
    double       LOD;
    double       LAD;
    int          ARV;
    int          BRV;
    int          PCB;
    int          PVB;


    int          bGeoTransformValid;
    double       adfGeoTransform[6];

    GDALColorTable oCT;

  public:
                 SRPDataset();
    virtual     ~SRPDataset();
    
    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * padfGeoTransform );
    virtual CPLErr SetGeoTransform( double * padfGeoTransform );

    virtual char **GetFileList();

    virtual char      **GetMetadata( const char * pszDomain = "" );

    void                AddSubDataset(const char* pszFilename);

    int                 GetFromRecord( const char* pszFileName, 
                                       DDFRecord * record);

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            SRPRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class SRPRasterBand : public GDALPamRasterBand
{
    friend class SRPDataset;

  public:
                            SRPRasterBand( SRPDataset *, int );

    virtual CPLErr          IReadBlock( int, int, void * );

    virtual double          GetNoDataValue( int *pbSuccess = NULL );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};


/************************************************************************/
/*                           SRPRasterBand()                            */
/************************************************************************/

SRPRasterBand::SRPRasterBand( SRPDataset *poDS, int nBand )

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

double  SRPRasterBand::GetNoDataValue( int *pbSuccess )
{
    if (pbSuccess)
        *pbSuccess = TRUE;

    return 0;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp SRPRasterBand::GetColorInterpretation()

{
    SRPDataset* poDS = (SRPDataset*)this->poDS;

    if( poDS->oCT.GetColorEntryCount() > 0 )
        return GCI_PaletteIndex;
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *SRPRasterBand::GetColorTable()

{
    SRPDataset* poDS = (SRPDataset*)this->poDS;

    if( poDS->oCT.GetColorEntryCount() > 0 )
        return &(poDS->oCT);
    else
        return NULL;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SRPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    SRPDataset* poDS = (SRPDataset*)this->poDS;
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
            offset = poDS->offsetInIMG + (poDS->TILEINDEX[nBlock] - 1) * 128 * 128;
        else // compressed 
            offset = poDS->offsetInIMG + (poDS->TILEINDEX[nBlock] - 1);
    }
    else
        offset = poDS->offsetInIMG + nBlock * 128 * 128;
    
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
        int    nBufSize = 128*128*2;
        int    nBytesRead, iPixel, iSrc, bHalfByteUsed = FALSE;
        GByte *pabyCData = (GByte *) CPLCalloc(nBufSize,1);

        nBytesRead = VSIFReadL(pabyCData, 1, nBufSize, poDS->fdIMG);
        if( nBytesRead == 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, 
                     "Cannot read data at offset %d", offset);
            return CE_Failure;
        }
        
        CPLAssert( poDS->PVB == 8 );
        CPLAssert( poDS->PCB == 4 || poDS->PCB == 8 );

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

            int nCount = 0;
            int nValue = 0;

            if( poDS->PCB == 8 )
            {
                nCount = pabyCData[iSrc++];
                nValue = pabyCData[iSrc++];
            }
            else if( poDS->PCB == 4 )
            {
                if( (iPixel % 128) == 0 && bHalfByteUsed )
                {
                    iSrc++;
                    bHalfByteUsed = FALSE;
                }

                if( bHalfByteUsed )
                {
                    nCount = pabyCData[iSrc++] & 0xf;
                    nValue = pabyCData[iSrc++];
                    bHalfByteUsed = FALSE;
                }
                else
                {
                    nCount = pabyCData[iSrc] >> 4;
                    nValue = ((pabyCData[iSrc] & 0xf) << 4)
                        + (pabyCData[iSrc+1] >> 4);
                    bHalfByteUsed = TRUE;
                    iSrc++;
                }
            }

            if( iPixel + nCount > 128 * 128 )
            {
                CPLFree( pabyCData );
                CPLError(CE_Failure, CPLE_AppDefined, 
                      "Too much data decoding image block, likely corrupt." );
                return CE_Failure;
            }

            while( nCount > 0 )
            {
                ((GByte *) pImage)[iPixel++] = (GByte) nValue;
                nCount--;
            }
        }

        CPLFree( pabyCData );
    }
    
    return CE_None;
}

/************************************************************************/
/*                          SRPDataset()                               */
/************************************************************************/

SRPDataset::SRPDataset()
{
    fdIMG = NULL;
    TILEINDEX = NULL;
    offsetInIMG = 0;
}

/************************************************************************/
/*                          ~SRPDataset()                              */
/************************************************************************/

SRPDataset::~SRPDataset()
{
    if (fdIMG)
    {
        VSIFCloseL(fdIMG);
    }
    
    if (TILEINDEX)
    {
        delete [] TILEINDEX;
    }
}

/************************************************************************/
/*                          ResetTo01()                                 */
/* Replace the DD in ZZZZZZDD.XXX with 01.                              */
/************************************************************************/

CPLString SRPDataset::ResetTo01( const char* str )
{
    CPLString osResult = str;

    osResult[6] = '0';
    osResult[7] = '1';

    return osResult;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **SRPDataset::GetMetadata( const char *pszDomain )

{
    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                        GetProjectionRef()                            */
/************************************************************************/

const char* SRPDataset::GetProjectionRef()
{
    return osSRS;
}

/************************************************************************/
/*                        GetGeoTransform()                             */
/************************************************************************/

CPLErr SRPDataset::GetGeoTransform( double * padfGeoTransform)
{
    if( EQUAL(osProduct,"ASRP") )
    {
        if( ZNA == 9 || ZNA == 18 )
        {
            padfGeoTransform[0] = -1152000.0;
            padfGeoTransform[1] = 500.0;
            padfGeoTransform[2] = 0.0;
            padfGeoTransform[3] = 1152000.0;
            padfGeoTransform[4] = 0.0;
            padfGeoTransform[5] = -500.0;

        }
        else
        {
            padfGeoTransform[0] = LSO/3600.0;
            padfGeoTransform[1] = 360. / ARV;
            padfGeoTransform[2] = 0.0;
            padfGeoTransform[3] = PSO/3600.0;
            padfGeoTransform[4] = 0.0;
            padfGeoTransform[5] = - 360. / BRV;
        }

        return CE_None;
    }
    else if( EQUAL(osProduct,"USRP") )
    {
        padfGeoTransform[0] = LSO;
        padfGeoTransform[1] = LOD;
        padfGeoTransform[2] = 0.0;
        padfGeoTransform[3] = PSO;
        padfGeoTransform[4] = 0.0;
        padfGeoTransform[5] = -LAD;
        return CE_None;
    }

    return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr SRPDataset::SetGeoTransform( double * padfGeoTransform )

{
    memcpy( adfGeoTransform, padfGeoTransform, sizeof(double)*6 );
    bGeoTransformValid = TRUE;
    return CE_None;
}

/************************************************************************/
/*                           GetFromRecord()                            */
/************************************************************************/

int SRPDataset::GetFromRecord(const char* pszFileName, DDFRecord * record)
{
    CPLString osBAD;
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
        CPLDebug( "SRP", "Failed to extract STR, or not 4." );
        return FALSE;
    }
        
    int SCA = record->GetIntSubfield( "GEN", 0, "SCA", 0, &bSuccess );
    CPLDebug("SRP", "SCA=%d", SCA);
        
    ZNA = record->GetIntSubfield( "GEN", 0, "ZNA", 0, &bSuccess );
    CPLDebug("SRP", "ZNA=%d", ZNA);
        
    double PSP = record->GetFloatSubfield( "GEN", 0, "PSP", 0, &bSuccess );
    CPLDebug("SRP", "PSP=%f", PSP);
        
    ARV = record->GetIntSubfield( "GEN", 0, "ARV", 0, &bSuccess );
    CPLDebug("SRP", "ARV=%d", ARV);

    BRV = record->GetIntSubfield( "GEN", 0, "BRV", 0, &bSuccess );
    CPLDebug("SRP", "BRV=%d", BRV);

    LSO = record->GetFloatSubfield( "GEN", 0, "LSO", 0, &bSuccess );
    CPLDebug("SRP", "LSO=%f", LSO);

    PSO = record->GetFloatSubfield( "GEN", 0, "PSO", 0, &bSuccess );
    CPLDebug("SRP", "PSO=%f", PSO);

    LAD = record->GetFloatSubfield( "GEN", 0, "LAD", 0 );
    LOD = record->GetFloatSubfield( "GEN", 0, "LOD", 0 );

    NFL = record->GetIntSubfield( "SPR", 0, "NFL", 0, &bSuccess );
    CPLDebug("SRP", "NFL=%d", NFL);

    NFC = record->GetIntSubfield( "SPR", 0, "NFC", 0, &bSuccess );
    CPLDebug("SRP", "NFC=%d", NFC);

    int PNC = record->GetIntSubfield( "SPR", 0, "PNC", 0, &bSuccess );
    CPLDebug("SRP", "PNC=%d", PNC);

    int PNL = record->GetIntSubfield( "SPR", 0, "PNL", 0, &bSuccess );
    CPLDebug("SRP", "PNL=%d", PNL);

    if( PNL != 128 || PNC != 128 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,"Unsupported PNL or PNC value.");
        return FALSE;
    }

    PCB = record->GetIntSubfield( "SPR", 0, "PCB", 0 );
    PVB = record->GetIntSubfield( "SPR", 0, "PVB", 0 );
    if( (PCB != 8 && PCB != 4 && PCB != 0) || PVB != 8 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCB(%d) or PVB(%d) value unsupported.", PCB, PVB );
        return FALSE;
    }

    osBAD = record->GetStringSubfield( "SPR", 0, "BAD", 0, &bSuccess );
    {
        char* c = (char*) strchr(osBAD, ' ');
        if (c)
            *c = 0;
    }
    CPLDebug("SRP", "BAD=%s", osBAD.c_str());
    
/* -------------------------------------------------------------------- */
/*      Read the tile map if available.                                 */
/* -------------------------------------------------------------------- */
    int TIF = EQUAL(record->GetStringSubfield( "SPR", 0, "TIF", 0 ),"Y");
    CPLDebug("SRP", "TIF=%d", TIF);
    
    if (TIF)
    {
        field = record->FindField( "TIM" );
        if( field == NULL )
            return FALSE;

        fieldDefn = field->GetFieldDefn();
        DDFSubfieldDefn *subfieldDefn = fieldDefn->FindSubfieldDefn( "TSI" );
        if( subfieldDefn == NULL )
            return FALSE;

        int nIndexValueWidth = subfieldDefn->GetWidth();

        /* Should be strict comparison, but apparently a few datasets */
        /* have GetDataSize() greater than the required minimum (#3862) */
        if (field->GetDataSize() < nIndexValueWidth * NFL * NFC + 1)
        {
            return FALSE;
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
            //CPLDebug("SRP", "TSI[%d]=%d", i, TILEINDEX[i]);
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Open the .IMG file.  Try to recover gracefully if the case      */
/*      of the filename is wrong.                                       */
/* -------------------------------------------------------------------- */
    CPLString osDirname = CPLGetDirname(pszFileName);
    CPLString osImgName = CPLFormCIFilename(osDirname, osBAD, NULL);

    fdIMG = VSIFOpenL(osImgName, "rb");
    if (fdIMG == NULL)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Cannot find %s", osImgName.c_str());
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Establish the offset to the first byte of actual image data     */
/*      in the IMG file, skipping the ISO8211 header.                   */
/*                                                                      */
/*      This code is awfully fragile!                                   */
/* -------------------------------------------------------------------- */
    char c;
    char recordName[3];
    if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
    {
        return FALSE;
    }
    while (!VSIFEofL(fdIMG))
    {
        if (c == 30)
        {
            if (VSIFReadL(recordName, 1, 3, fdIMG) != 3)
            {
                return FALSE;
            }
            offsetInIMG += 3;
            if (strncmp(recordName,"IMG",3) == 0)
            {
                offsetInIMG += 4;
                if (VSIFSeekL(fdIMG,3,SEEK_CUR) != 0)
                {
                    return FALSE;
                }
                if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
                {
                    return FALSE;
                }
                while( c != 30 )
                {
                    offsetInIMG ++;
                    if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
                    {
                        return FALSE;
                    }
                }
                offsetInIMG ++;
                break;
            }
        }

        offsetInIMG ++;
        if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
        {
            return FALSE;
        }
    }
    
    if (VSIFEofL(fdIMG))
    {
        return FALSE;
    }
    
    CPLDebug("SRP", "Img offset data = %d", offsetInIMG);
    
/* -------------------------------------------------------------------- */
/*      Establish the SRP Dataset.                                     */
/* -------------------------------------------------------------------- */
    nRasterXSize = NFC * 128;
    nRasterYSize = NFL * 128;
    
    char pszValue[32];
    sprintf(pszValue, "%d", SCA);
    SetMetadataItem( "SRP_SCA", pszValue );
    
    nBands = 1;
    for( i = 0; i < nBands; i++ )
        SetBand( i+1, new SRPRasterBand( this, i+1 ) );

/* -------------------------------------------------------------------- */
/*      Try to collect a color map from the .QAL file.                  */
/* -------------------------------------------------------------------- */
    CPLString osBasename = CPLGetBasename(pszFileName);
    osQALFilename = CPLFormCIFilename(osDirname, osBasename, "QAL");

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

                sEntry.c1 = (short) nNSR;
                sEntry.c2 = (short) nNSG;
                sEntry.c3 = (short) nNSB;
                sEntry.c4 = 255;

                oCT.SetColorEntry( nCCD, &sEntry );
            }
        }
    }
    else
    {
        osQALFilename = "";
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unable to find .QAL file, no color table applied." );
    }

/* -------------------------------------------------------------------- */
/*      Derive the coordinate system.                                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(osProduct,"ASRP") )
    {
        osSRS = SRS_WKT_WGS84;

        if( ZNA == 9 )
        {
            osSRS = "PROJCS[\"unnamed\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Azimuthal_Equidistant\"],PARAMETER[\"latitude_of_center\",90],PARAMETER[\"longitude_of_center\",0],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0]]";
        }

        if (ZNA == 18)
        {
            osSRS = "PROJCS[\"unnamed\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Azimuthal_Equidistant\"],PARAMETER[\"latitude_of_center\",-90],PARAMETER[\"longitude_of_center\",0],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0]]";
        }
    }
    else
    {
        OGRSpatialReference oSRS;

        if( ABS(ZNA) >= 1 && ABS(ZNA) <= 60 )
        {
            oSRS.SetUTM( ABS(ZNA), ZNA > 0 );
            oSRS.SetWellKnownGeogCS( "WGS84" );
        }
        else if( ZNA == 61 )
        {
            oSRS.importFromEPSG( 32661 ); // WGS84 UPS North 
        }
        else if( ZNA == -61 )
        {
            oSRS.importFromEPSG( 32761 ); // WGS84 UPS South 
        }

        char *pszWKT = NULL;
        oSRS.exportToWkt( &pszWKT );
        osSRS = pszWKT;
        CPLFree( pszWKT );
    }

    return TRUE;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **SRPDataset::GetFileList()

{
     char **papszFileList = GDALPamDataset::GetFileList();

     papszFileList = CSLAddString( papszFileList, osGENFilename );

     if( strlen(osQALFilename) > 0 )
         papszFileList = CSLAddString( papszFileList, osQALFilename );

     return papszFileList;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SRPDataset::Open( GDALOpenInfo * poOpenInfo )
{
    DDFModule module;
    DDFRecord * record;
    CPLString osFileName(poOpenInfo->pszFilename);
    CPLString osNAM;

/* -------------------------------------------------------------------- */
/*      Verify that this appears to be a valid ISO8211 .IMG file.       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 500 )
        return NULL;

    if (!EQUAL(CPLGetExtension(osFileName), "img"))
        return NULL;

    static const size_t nLeaderSize = 24;
    int         i;

    for( i = 0; i < (int)nLeaderSize; i++ )
    {
        if( poOpenInfo->pabyHeader[i] < 32 
            || poOpenInfo->pabyHeader[i] > 126 )
            return NULL;
    }

    if( poOpenInfo->pabyHeader[5] != '1' 
        && poOpenInfo->pabyHeader[5] != '2' 
        && poOpenInfo->pabyHeader[5] != '3' )
        return NULL;

    if( poOpenInfo->pabyHeader[6] != 'L' )
        return NULL;
    if( poOpenInfo->pabyHeader[8] != '1' && poOpenInfo->pabyHeader[8] != ' ' )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Find and open the .GEN file.                                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    CPLString basename = CPLGetBasename( osFileName );
    if( basename.size() != 8 )
    {
        CPLDebug("SRP", "Invalid basename file");
        return NULL;
    }

    int zoneNumber = CPLScanLong( basename + 6, 2 );

    CPLString path = CPLGetDirname( osFileName );
    CPLString basename01 = ResetTo01( basename );
    osFileName = CPLFormFilename( path, basename01, ".IMG" );

    osFileName = CPLResetExtension( osFileName, "GEN" );
    if( VSIStatL( osFileName, &sStatBuf ) != 0 )
    {
        osFileName = CPLResetExtension( osFileName, "gen" );
        if( VSIStatL( osFileName, &sStatBuf ) != 0 )
            return NULL;
    }

    if (!module.Open(osFileName, TRUE))
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The SRP driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop processing records - we are basically looking for the      */
/*      GIN record which is normally first in the .GEN file.            */
/* -------------------------------------------------------------------- */
    int recordIndex = 0;
    while (TRUE)
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == NULL)
          break;
        if ( ++recordIndex < zoneNumber )
          continue;

        const char* RTY = record->GetStringSubfield( "001", 0, "RTY", 0 );
        if( RTY == NULL || !EQUAL(RTY,"GIN") )
            continue;

        const char *PRT = record->GetStringSubfield( "DSI", 0, "PRT", 0 );
        if( PRT == NULL )
            continue;

        CPLString osPRT = PRT;
        osPRT.resize(4);
        if( !EQUAL(osPRT,"ASRP") && !EQUAL(osPRT,"USRP") )
            continue;

        osNAM = record->GetStringSubfield( "DSI", 0, "NAM", 0 );
        CPLDebug("SRP", "NAM=%s", osNAM.c_str());

        SRPDataset *poDS = new SRPDataset();
        
        poDS->osProduct = osPRT;
        poDS->osGENFilename = osFileName;
        poDS->SetMetadataItem( "SRP_NAM", osNAM );
        poDS->SetMetadataItem( "SRP_PRODUCT", osPRT );

        if (!poDS->GetFromRecord( osFileName, record ) )
        {
            delete poDS;
            continue;
        }
        
        /* ---------------------------------------------------------- */
        /*      Initialize any PAM information.                       */
        /* ---------------------------------------------------------- */
        poDS->SetDescription( poOpenInfo->pszFilename );
        poDS->TryLoadXML();

        /* ---------------------------------------------------------- */
        /*      Check for external overviews.                         */
        /* ---------------------------------------------------------- */
        poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

        return poDS;
    }
    
    return NULL;
}

/************************************************************************/
/*                         GDALRegister_SRP()                          */
/************************************************************************/

void GDALRegister_SRP()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "SRP" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "SRP" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Standard Raster Product (ASRP/USRP)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#SRP" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "img" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = SRPDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

