/******************************************************************************
 * Purpose:  ASRP/USRP Reader
 * Author:   Frank Warmerdam (warmerdam@pobox.com)
 *
 * Derived from ADRG driver by Even Rouault, even.rouault at spatialys.com.
 *
 ******************************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_pam.h"
#include "gdal_frmts.h"
#include "iso8211.h"
#include "ogr_spatialref.h"

#include <cstdlib>
#include <algorithm>

// Uncomment to recognize also .gen files in addition to .img files
// #define OPEN_GEN

CPL_CVSID("$Id$")

class SRPDataset final: public GDALPamDataset
{
    friend class SRPRasterBand;

    static CPLString ResetTo01( const char* str );

    VSILFILE*        fdIMG;
    int*         TILEINDEX;
    int          offsetInIMG;
    CPLString    osProduct;
    CPLString    osSRS;
    CPLString    osGENFileName;
    CPLString    osQALFileName;
    CPLString    osIMGFileName;
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

    char**       papszSubDatasets;

    GDALColorTable oCT;

    static char** GetGENListFromTHF(const char* pszFileName);
    static char** GetIMGListFromGEN(const char* pszFileName, int* pnRecordIndex = nullptr);
    static SRPDataset* OpenDataset(const char* pszGENFileName, const char* pszIMGFileName, DDFRecord* record = nullptr);
    static DDFRecord*  FindRecordInGENForIMG(DDFModule& module,
        const char* pszGENFileName, const char* pszIMGFileName);

  public:
    SRPDataset();
    ~SRPDataset() override;

    const char *_GetProjectionRef(void) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr GetGeoTransform( double * padfGeoTransform ) override;

    char **GetMetadata( const char * pszDomain = "" ) override;

    char **GetFileList() override;

    bool GetFromRecord( const char* pszFileName, DDFRecord *record );
    void AddSubDataset( const char* pszGENFileName,
                        const char* pszIMGFileName );
    void AddMetadatafromFromTHF(const char* pszFileName);

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            SRPRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class SRPRasterBand final: public GDALPamRasterBand
{
    friend class SRPDataset;

  public:
    SRPRasterBand( SRPDataset *, int );

    CPLErr IReadBlock( int, int, void * ) override;

    double GetNoDataValue( int *pbSuccess = nullptr ) override;

    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
};

/************************************************************************/
/*                           SRPRasterBand()                            */
/************************************************************************/

SRPRasterBand::SRPRasterBand( SRPDataset *poDSIn, int nBandIn )

{
    poDS = poDSIn;
    nBand = nBandIn;

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
    SRPDataset* l_poDS = (SRPDataset*)this->poDS;

    if( l_poDS->oCT.GetColorEntryCount() > 0 )
        return GCI_PaletteIndex;
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *SRPRasterBand::GetColorTable()

{
    SRPDataset* l_poDS = (SRPDataset*)this->poDS;

    if( l_poDS->oCT.GetColorEntryCount() > 0 )
        return &(l_poDS->oCT);
    else
        return nullptr;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SRPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    SRPDataset* l_poDS = (SRPDataset*)this->poDS;
    vsi_l_offset offset;
    int nBlock = nBlockYOff * l_poDS->NFC + nBlockXOff;
    if (nBlockXOff >= l_poDS->NFC || nBlockYOff >= l_poDS->NFL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nBlockXOff=%d, NFC=%d, nBlockYOff=%d, NFL=%d",
                 nBlockXOff, l_poDS->NFC, nBlockYOff, l_poDS->NFL);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Is this a null block?                                           */
/* -------------------------------------------------------------------- */
    if (l_poDS->TILEINDEX && l_poDS->TILEINDEX[nBlock] <= 0)
    {
        memset(pImage, 0, 128 * 128);
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Compute the offset to the block.                                */
/* -------------------------------------------------------------------- */
    if (l_poDS->TILEINDEX)
    {
        if( l_poDS->PCB == 0 ) // uncompressed
            offset = l_poDS->offsetInIMG + static_cast<vsi_l_offset>(l_poDS->TILEINDEX[nBlock] - 1) * 128 * 128;
        else // compressed
            offset = l_poDS->offsetInIMG +  static_cast<vsi_l_offset>(l_poDS->TILEINDEX[nBlock] - 1);
    }
    else
        offset = l_poDS->offsetInIMG + static_cast<vsi_l_offset>(nBlock) * 128 * 128;

/* -------------------------------------------------------------------- */
/*      Seek to target location.                                        */
/* -------------------------------------------------------------------- */
    if (VSIFSeekL(l_poDS->fdIMG, offset, SEEK_SET) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot seek to offset " CPL_FRMT_GUIB, offset);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      For uncompressed case we read the 128x128 and return with no    */
/*      further processing.                                             */
/* -------------------------------------------------------------------- */
    if( l_poDS->PCB == 0 )
    {
        if( VSIFReadL(pImage, 1, 128 * 128, l_poDS->fdIMG) != 128*128 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Cannot read data at offset " CPL_FRMT_GUIB, offset);
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      If this is compressed data, we read a goodly chunk of data      */
/*      and then decode it.                                             */
/* -------------------------------------------------------------------- */
    else
    {
        const int nBufSize = 128*128*2;
        GByte *pabyCData = (GByte *) CPLCalloc(nBufSize,1);

        const int nBytesRead =
            static_cast<int>(VSIFReadL(pabyCData, 1, nBufSize, l_poDS->fdIMG));
        if( nBytesRead == 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Cannot read data at offset " CPL_FRMT_GUIB, offset);
            CPLFree(pabyCData);
            return CE_Failure;
        }

        CPLAssert( l_poDS->PVB == 8 );
        CPLAssert( l_poDS->PCB == 4 || l_poDS->PCB == 8 );

        bool bHalfByteUsed = false;
        for( int iSrc = 0, iPixel = 0; iPixel < 128 * 128; )
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

            if( l_poDS->PCB == 8 )
            {
                nCount = pabyCData[iSrc++];
                nValue = pabyCData[iSrc++];
            }
            else if( l_poDS->PCB == 4 )
            {
                if( (iPixel % 128) == 0 && bHalfByteUsed )
                {
                    iSrc++;
                    bHalfByteUsed = false;
                    continue;
                }

                if( bHalfByteUsed )
                {
                    nCount = pabyCData[iSrc++] & 0xf;
                    nValue = pabyCData[iSrc++];
                    bHalfByteUsed = false;
                }
                else
                {
                    nCount = pabyCData[iSrc] >> 4;
                    nValue = ((pabyCData[iSrc] & 0xf) << 4)
                        + (pabyCData[iSrc+1] >> 4);
                    bHalfByteUsed = true;
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

SRPDataset::SRPDataset() :
    fdIMG(nullptr),
    TILEINDEX(nullptr),
    offsetInIMG(0),
    NFC(0),
    NFL(0),
    ZNA(0),
    LSO(0.0),
    PSO(0.0),
    LOD(0.0),
    LAD(0.0),
    ARV(0),
    BRV(0),
    PCB(0),
    PVB(0),
    papszSubDatasets(nullptr)
{}

/************************************************************************/
/*                          ~SRPDataset()                              */
/************************************************************************/

SRPDataset::~SRPDataset()
{
    CSLDestroy(papszSubDatasets);

    if( fdIMG )
    {
        VSIFCloseL(fdIMG);
    }

    if( TILEINDEX )
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
/*                        GetProjectionRef()                            */
/************************************************************************/

const char* SRPDataset::_GetProjectionRef()
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
        if( ARV == 0 )
            return CE_Failure;
        if( ZNA == 9)
        {
            // North Polar Case
            padfGeoTransform[0] = 111319.4907933 * (90.0 - PSO/3600.0) * sin(LSO * M_PI / 648000.0);
            padfGeoTransform[1] = 40075016.68558 / ARV;
            padfGeoTransform[2] = 0.0;
            padfGeoTransform[3] = -111319.4907933 * (90.0 - PSO/3600.0) * cos(LSO * M_PI / 648000.0);
            padfGeoTransform[4] = 0.0;
            padfGeoTransform[5] = -40075016.68558 / ARV;
        }
        else if (ZNA == 18)
        {
            // South Polar Case
            padfGeoTransform[0] = 111319.4907933 * (90.0 + PSO/3600.0) * sin(LSO * M_PI / 648000.0);
            padfGeoTransform[1] = 40075016.68558 / ARV;
            padfGeoTransform[2] = 0.0;
            padfGeoTransform[3] = 111319.4907933 * (90.0 + PSO/3600.0) * cos(LSO * M_PI / 648000.0);
            padfGeoTransform[4] = 0.0;
            padfGeoTransform[5] = -40075016.68558 / ARV;
        }
        else
        {
            if( BRV == 0 )
                return CE_Failure;
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
/*                           GetFromRecord()                            */
/************************************************************************/

bool SRPDataset::GetFromRecord( const char* pszFileName, DDFRecord * record )
{
    int bSuccess;

/* -------------------------------------------------------------------- */
/*      Read a variety of header fields of interest from the .GEN       */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    const int nSTR = record->GetIntSubfield( "GEN", 0, "STR", 0, &bSuccess );
    if( !bSuccess || nSTR != 4 )
    {
        CPLDebug( "SRP", "Failed to extract STR, or not 4." );
        return false;
    }

    const int SCA = record->GetIntSubfield( "GEN", 0, "SCA", 0, &bSuccess );
    CPLDebug("SRP", "SCA=%d", SCA);

    ZNA = record->GetIntSubfield( "GEN", 0, "ZNA", 0, &bSuccess );
    CPLDebug("SRP", "ZNA=%d", ZNA);

    const double PSP = record->GetFloatSubfield( "GEN", 0, "PSP", 0, &bSuccess );
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

    const auto knIntMax = std::numeric_limits<int>::max();
    if( NFL <= 0 || NFC <= 0 ||
        NFL > knIntMax / 128 ||
        NFC > knIntMax / 128 ||
        NFL > knIntMax / NFC )
    {
        CPLError( CE_Failure, CPLE_AppDefined,"Invalid NFL / NFC values");
        return false;
    }

    const int PNC = record->GetIntSubfield( "SPR", 0, "PNC", 0, &bSuccess );
    CPLDebug("SRP", "PNC=%d", PNC);

    const int PNL = record->GetIntSubfield( "SPR", 0, "PNL", 0, &bSuccess );
    CPLDebug("SRP", "PNL=%d", PNL);

    if( PNL != 128 || PNC != 128 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,"Unsupported PNL or PNC value.");
        return false;
    }

    PCB = record->GetIntSubfield( "SPR", 0, "PCB", 0 );
    PVB = record->GetIntSubfield( "SPR", 0, "PVB", 0 );
    if( (PCB != 8 && PCB != 4 && PCB != 0) || PVB != 8 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCB(%d) or PVB(%d) value unsupported.", PCB, PVB );
        return false;
    }

    const char* pszBAD =
        record->GetStringSubfield( "SPR", 0, "BAD", 0, &bSuccess );
    if( pszBAD == nullptr )
        return false;
    const CPLString osBAD = pszBAD;
    {
        char* c = (char*) strchr(osBAD, ' ');
        if (c)
            *c = 0;
    }
    CPLDebug("SRP", "BAD=%s", osBAD.c_str());

/* -------------------------------------------------------------------- */
/*      Read the tile map if available.                                 */
/* -------------------------------------------------------------------- */
    const char* pszTIF = record->GetStringSubfield( "SPR", 0, "TIF", 0 );
    const bool TIF = pszTIF != nullptr && EQUAL(pszTIF,"Y");
    CPLDebug("SRP", "TIF=%s", TIF ? "true": "false");

    if( TIF )
    {
        DDFField* field = record->FindField( "TIM" );
        if( field == nullptr )
            return false;

        DDFFieldDefn *fieldDefn = field->GetFieldDefn();
        DDFSubfieldDefn *subfieldDefn = fieldDefn->FindSubfieldDefn( "TSI" );
        if( subfieldDefn == nullptr )
            return false;

        const int nIndexValueWidth = subfieldDefn->GetWidth();

        char offset[30] = {0};
        /* Should be strict comparison, but apparently a few datasets */
        /* have GetDataSize() greater than the required minimum (#3862) */
        if (nIndexValueWidth <= 0 ||
            static_cast<size_t>(nIndexValueWidth) >= sizeof(offset) ||
            nIndexValueWidth > (INT_MAX - 1) / (NFL * NFC) ||
            field->GetDataSize() < nIndexValueWidth * NFL * NFC + 1)
        {
            return false;
        }

        try
        {
            TILEINDEX = new int [NFL * NFC];
        }
        catch( const std::exception& )
        {
            return false;
        }
        const char* ptr = field->GetData();
        offset[nIndexValueWidth] = '\0';

        for( int i = 0; i < NFL * NFC; i++ )
        {
            strncpy(offset, ptr, nIndexValueWidth);
            ptr += nIndexValueWidth;
            TILEINDEX[i] = atoi(offset);
            // CPLDebug("SRP", "TSI[%d]=%d", i, TILEINDEX[i]);
        }
    }

/* -------------------------------------------------------------------- */
/*      Open the .IMG file.  Try to recover gracefully if the case      */
/*      of the filename is wrong.                                       */
/* -------------------------------------------------------------------- */
    const CPLString osDirname = CPLGetDirname(pszFileName);
    const CPLString osImgName = CPLFormCIFilename(osDirname, osBAD, nullptr);

    fdIMG = VSIFOpenL(osImgName, "rb");
    if (fdIMG == nullptr)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot find %s", osImgName.c_str() );
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Establish the offset to the first byte of actual image data     */
/*      in the IMG file, skipping the ISO8211 header.                   */
/*                                                                      */
/*      This code is awfully fragile!                                   */
/* -------------------------------------------------------------------- */
    char c;
    if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
    {
        return false;
    }
    while (!VSIFEofL(fdIMG))
    {
        if (c == 30)
        {
            char recordName[3] = {};
            if (VSIFReadL(recordName, 1, 3, fdIMG) != 3)
            {
                return false;
            }
            offsetInIMG += 3;
            if (STARTS_WITH(recordName, "IMG"))
            {
                offsetInIMG += 4;
                if (VSIFSeekL(fdIMG,3,SEEK_CUR) != 0)
                {
                    return false;
                }
                if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
                {
                    return false;
                }
                while( c != 30 )
                {
                    offsetInIMG ++;
                    if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
                    {
                        return false;
                    }
                }
                offsetInIMG ++;
                break;
            }
        }

        offsetInIMG ++;
        if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
        {
            return false;
        }
    }

    if (VSIFEofL(fdIMG))
    {
        return false;
    }

    CPLDebug("SRP", "Img offset data = %d", offsetInIMG);

/* -------------------------------------------------------------------- */
/*      Establish the SRP Dataset.                                     */
/* -------------------------------------------------------------------- */
    nRasterXSize = NFC * 128;
    nRasterYSize = NFL * 128;

    char szValue[32] = {};
    snprintf(szValue, sizeof(szValue), "%d", SCA);
    SetMetadataItem( "SRP_SCA", szValue );

    nBands = 1;
    for( int i = 0; i < nBands; i++ )
        SetBand( i+1, new SRPRasterBand( this, i+1 ) );

/* -------------------------------------------------------------------- */
/*      Try to collect a color map from the .QAL file.                  */
/* -------------------------------------------------------------------- */
    const CPLString osBasename = CPLGetBasename(pszFileName);
    osQALFileName = CPLFormCIFilename(osDirname, osBasename, "QAL");

    DDFModule oQALModule;

    if( oQALModule.Open( osQALFileName, TRUE ) )
    {
        while( (record = oQALModule.ReadRecord()) != nullptr)
        {
            if( record->FindField( "COL" ) != nullptr )
            {
                const int nColorCount =
                    std::min(256, record->FindField("COL")->GetRepeatCount());

                for( int iColor = 0; iColor < nColorCount; iColor++ )
                {
                    const int nCCD =
                        record->GetIntSubfield( "COL", 0, "CCD", iColor,
                                                &bSuccess );
                    if( !bSuccess || nCCD < 0 || nCCD > 255 )
                        break;

                    int nNSR = record->GetIntSubfield("COL", 0, "NSR", iColor);
                    int nNSG = record->GetIntSubfield("COL", 0, "NSG", iColor);
                    int nNSB = record->GetIntSubfield("COL", 0, "NSB", iColor);

                    GDALColorEntry sEntry = {
                        static_cast<short>(nNSR),
                        static_cast<short>(nNSG),
                        static_cast<short>(nNSB),
                        255
                    };

                    oCT.SetColorEntry( nCCD, &sEntry );
                }
            }

            if (record->FindField( "QUV" ) != nullptr )
            {
                // TODO: Translate to English or state why this should not be in
                // English.
                // Date de production du produit : QAL.QUV.DAT1
                // Numero d'edition  du produit : QAL.QUV.EDN

                const int EDN =
                    record->GetIntSubfield( "QUV", 0, "EDN", 0, &bSuccess );
                if (bSuccess)
                {
                    CPLDebug("SRP", "EDN=%d", EDN);
                    snprintf(szValue, sizeof(szValue), "%d", EDN);
                    SetMetadataItem( "SRP_EDN", szValue );
                }

                const char* pszCDV07 =
                    record->GetStringSubfield( "QUV", 0, "CDV07", 0 );
                if (pszCDV07!=nullptr)
                    SetMetadataItem( "SRP_CREATIONDATE", pszCDV07 );
                else
                { /*USRP1.2*/
                    const char* pszDAT =
                        record->GetStringSubfield("QUV", 0, "DAT1", 0);
                    if( pszDAT != nullptr && strlen(pszDAT) >= 12 )
                    {
                        char dat[9];
                        strncpy(dat, pszDAT+4, 8);
                        dat[8]='\0';
                        CPLDebug("SRP", "Record DAT %s",dat);
                        SetMetadataItem( "SRP_CREATIONDATE", dat );
                    }
                }

                const char* pszCDV24 =
                    record->GetStringSubfield( "QUV", 0, "CDV24", 0 );
                if (pszCDV24!=nullptr)
                {
                    SetMetadataItem( "SRP_REVISIONDATE", pszCDV24 );
                }
                else
                { /*USRP1.2*/
                    const char* pszDAT =
                        record->GetStringSubfield("QUV", 0, "DAT2", 0);
                    if( pszDAT != nullptr && strlen(pszDAT) >= 12 )
                    {
                        char dat[9];
                        strncpy(dat,pszDAT+4,8);
                        dat[8]='\0';
                        CPLDebug("SRP", "Record DAT %s",dat);
                        SetMetadataItem( "SRP_REVISIONDATE", dat );
                    }
                }

                const char* pszQSS =
                    record->GetStringSubfield( "QSR", 0, "QSS", 0 );
                if (pszQSS!=nullptr)
                    SetMetadataItem( "SRP_CLASSIFICATION", pszQSS );
            }
        }
    }
    else
    {
        osQALFileName = "";
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unable to find .QAL file, no color table applied." );
    }

/* -------------------------------------------------------------------- */
/*      Derive the coordinate system.                                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(osProduct,"ASRP") )
    {
        osSRS = SRS_WKT_WGS84_LAT_LONG;

        if( ZNA == 9 )
        {
            osSRS =
                "PROJCS[\"ARC_System_Zone_09\",GEOGCS[\"GCS_Sphere\","
                "DATUM[\"D_Sphere\",SPHEROID[\"Sphere\",6378137.0,0.0]],"
                "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],"
                "PROJECTION[\"Azimuthal_Equidistant\"],"
                "PARAMETER[\"latitude_of_center\",90],"
                "PARAMETER[\"longitude_of_center\",0],"
                "PARAMETER[\"false_easting\",0],"
                "PARAMETER[\"false_northing\",0],"
                "UNIT[\"metre\",1]]";
        }

        if (ZNA == 18)
        {
            osSRS =
                "PROJCS[\"ARC_System_Zone_18\",GEOGCS[\"GCS_Sphere\","
                "DATUM[\"D_Sphere\",SPHEROID[\"Sphere\",6378137.0,0.0]],"
                "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],"
                "PROJECTION[\"Azimuthal_Equidistant\"],"
                "PARAMETER[\"latitude_of_center\",-90],"
                "PARAMETER[\"longitude_of_center\",0],"
                "PARAMETER[\"false_easting\",0],"
                "PARAMETER[\"false_northing\",0],"
                "UNIT[\"metre\",1]]";
        }
    }
    else
    {
        OGRSpatialReference oSRS;

        if( std::abs(ZNA) >= 1 && std::abs(ZNA) <= 60 )
        {
            oSRS.SetUTM(std::abs(ZNA), ZNA > 0);
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

        char *pszWKT = nullptr;
        oSRS.exportToWkt( &pszWKT );
        osSRS = pszWKT;
        CPLFree( pszWKT );
    }

    snprintf(szValue, sizeof(szValue), "%d", ZNA);
    SetMetadataItem( "SRP_ZNA", szValue );

    return true;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **SRPDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();
    if (!osGENFileName.empty() && !osIMGFileName.empty())
    {
        CPLString osMainFilename = GetDescription();
        VSIStatBufL  sStat;

        const bool bMainFileReal = VSIStatL( osMainFilename, &sStat ) == 0;
        if (bMainFileReal)
        {
            CPLString osShortMainFilename = CPLGetFilename(osMainFilename);
            CPLString osShortGENFileName = CPLGetFilename(osGENFileName);
            if( !EQUAL(osShortMainFilename.c_str(),
                       osShortGENFileName.c_str()) )
                papszFileList =
                    CSLAddString(papszFileList, osGENFileName.c_str());
        }
        else
        {
            papszFileList = CSLAddString(papszFileList, osGENFileName.c_str());
        }

        papszFileList = CSLAddString(papszFileList, osIMGFileName.c_str());

        if( !osQALFileName.empty() )
            papszFileList = CSLAddString( papszFileList, osQALFileName );
    }
    return papszFileList;
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void SRPDataset::AddSubDataset( const char* pszGENFileName,
                                const char* pszIMGFileName )
{
    const int nCount = CSLCount(papszSubDatasets ) / 2;

    CPLString osSubDatasetName = "SRP:";
    osSubDatasetName += pszGENFileName;
    osSubDatasetName += ",";
    osSubDatasetName += pszIMGFileName;

    char szName[80];
    snprintf(szName, sizeof(szName), "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName, osSubDatasetName);

    snprintf(szName, sizeof(szName), "SUBDATASET_%d_DESC", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName, osSubDatasetName);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **SRPDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != nullptr && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                      FindRecordInGENForIMG()                         */
/************************************************************************/

DDFRecord* SRPDataset::FindRecordInGENForIMG( DDFModule& module,
                                              const char* pszGENFileName,
                                              const char* pszIMGFileName )
{
    /* Finds the GEN file corresponding to the IMG file */
    if (!module.Open(pszGENFileName, TRUE))
        return nullptr;

    CPLString osShortIMGFilename = CPLGetFilename(pszIMGFileName);

    DDFField* field = nullptr;
    DDFFieldDefn *fieldDefn = nullptr;

    // Now finds the record.
    while( true )
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        DDFRecord* record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == nullptr)
            return nullptr;

        if (record->GetFieldCount() >= 5)
        {
            field = record->GetField(0);
            fieldDefn = field->GetFieldDefn();
            if (!(strcmp(fieldDefn->GetName(), "001") == 0 &&
                fieldDefn->GetSubfieldCount() == 2))
            {
                continue;
            }

            const char* RTY = record->GetStringSubfield("001", 0, "RTY", 0);
            if( RTY == nullptr )
                continue;
            /* Ignore overviews */
            if ( strcmp(RTY, "OVV") == 0 )
                continue;

            if ( strcmp(RTY, "GIN") != 0 )
                continue;

            field = record->GetField(3);
            fieldDefn = field->GetFieldDefn();

            if (!(strcmp(fieldDefn->GetName(), "SPR") == 0 &&
                fieldDefn->GetSubfieldCount() == 15))
            {
                continue;
            }

            const char* pszBAD = record->GetStringSubfield("SPR", 0, "BAD", 0);
            if( pszBAD == nullptr || strlen(pszBAD) != 12 )
                continue;
            const CPLString osBAD = pszBAD;
            {
                char* c = (char*) strchr(osBAD.c_str(), ' ');
                if (c)
                    *c = 0;
            }

            if (EQUAL(osShortIMGFilename.c_str(), osBAD.c_str()))
            {
                return record;
            }
        }
    }
}
/************************************************************************/
/*                           OpenDataset()                              */
/************************************************************************/

SRPDataset* SRPDataset::OpenDataset(
    const char* pszGENFileName, const char* pszIMGFileName, DDFRecord* record)
{
    DDFModule module; // Don't move this line as it holds ownership of record.

    if (record == nullptr)
    {
        record = FindRecordInGENForIMG(module, pszGENFileName, pszIMGFileName);
        if (record == nullptr)
            return nullptr;
    }

    DDFField* field = record->GetField(1);
    if( field == nullptr )
        return nullptr;
    DDFFieldDefn *fieldDefn = field->GetFieldDefn();

    if (!(strcmp(fieldDefn->GetName(), "DSI") == 0 &&
        fieldDefn->GetSubfieldCount() == 2))
    {
        return nullptr;
    }

    const char* pszPRT = record->GetStringSubfield("DSI", 0, "PRT", 0);
    if( pszPRT == nullptr)
        return nullptr;

    CPLString osPRT = pszPRT;
    osPRT.resize(4);
    CPLDebug("SRP", "osPRT=%s", osPRT.c_str());
    if( !EQUAL(osPRT,"ASRP") && !EQUAL(osPRT,"USRP") )
        return nullptr;

    const char* pszNAM = record->GetStringSubfield("DSI", 0, "NAM", 0);
    if( pszNAM == nullptr  )
        return nullptr;

    const CPLString osNAM = pszNAM;
    CPLDebug("SRP", "osNAM=%s", osNAM.c_str());
    if ( strlen(pszNAM) != 8 )
    {
        CPLDebug("SRP", "Name Size=%d", (int)strlen(pszNAM) );
    }

    SRPDataset* poDS = new SRPDataset();

    poDS->osProduct = osPRT;
    poDS->osGENFileName = pszGENFileName;
    poDS->osIMGFileName = pszIMGFileName;

    poDS->SetMetadataItem( "SRP_NAM", osNAM );
    poDS->SetMetadataItem( "SRP_PRODUCT", osPRT );

    if (!poDS->GetFromRecord( pszGENFileName, record ) )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                          GetGENListFromTHF()                         */
/************************************************************************/

char** SRPDataset::GetGENListFromTHF(const char* pszFileName)
{
    DDFModule module;
    DDFRecord * record = nullptr;
    DDFField* field = nullptr;
    DDFFieldDefn *fieldDefn = nullptr;
    int nFilenames = 0;

    char** papszFileNames = nullptr;
    if (!module.Open(pszFileName, TRUE))
        return papszFileNames;

    CPLString osDirName(CPLGetDirname(pszFileName));

    while( true )
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == nullptr)
            break;
        if (record->GetFieldCount() > 2)
        {
            field = record->GetField(0);
            fieldDefn = field->GetFieldDefn();
            if (!(strcmp(fieldDefn->GetName(), "001") == 0 &&
                fieldDefn->GetSubfieldCount() == 2))
            {
                continue;
            }

            const char* RTY = record->GetStringSubfield("001", 0, "RTY", 0);
            if ( RTY == nullptr )
            {
                continue;
            }

            if ( strcmp(RTY, "THF") == 0 )
            {
                field = record->GetField(1);
                fieldDefn = field->GetFieldDefn();
                if (!(strcmp(fieldDefn->GetName(), "VDR") == 0 &&
                    fieldDefn->GetSubfieldCount() == 8))
                {
                    continue;
                }

                int iFDRFieldInstance = 0;
                for( int i = 2; i < record->GetFieldCount() ; i++ )
                {
                    field = record->GetField(i);
                    fieldDefn = field->GetFieldDefn();

                    if (!(strcmp(fieldDefn->GetName(), "FDR") == 0 &&
                        fieldDefn->GetSubfieldCount() == 7))
                    {
                        CPLDebug("SRP", "Record FDR  %d",fieldDefn->GetSubfieldCount());
                        continue;
                    }

                    const char* pszNAM = record->GetStringSubfield("FDR", iFDRFieldInstance++, "NAM", 0);
                    if( pszNAM == nullptr)
                        continue;

                    CPLString osName = CPLString(pszNAM);

                    /* Define a subdirectory from Dataset but with only 6 characters */
                    CPLString osDirDataset = pszNAM;
                    osDirDataset.resize(6);
                    CPLString osDatasetDir = CPLFormFilename(osDirName.c_str(), osDirDataset.c_str(), nullptr);

                    CPLString osGENFileName="";

                    int bFound=0;

                    {
                        char** papszDirContent = VSIReadDir(osDatasetDir.c_str());
                        char** ptrDir = papszDirContent;
                        if (ptrDir)
                        {
                            while(*ptrDir)
                            {
                                if ( EQUAL(CPLGetExtension(*ptrDir), "GEN") )
                                {
                                    bFound = 1;
                                    osGENFileName = CPLFormFilename(osDatasetDir.c_str(), *ptrDir, nullptr);
                                    CPLDebug("SRP", "Building GEN full file name : %s", osGENFileName.c_str());
                                    break;
                                }
                                ptrDir ++;
                            }
                            CSLDestroy(papszDirContent);
                        }
                    }

                    /* If not found in sub directory then search in the same directory of the THF file */
                    if (bFound ==0)
                    {
                        char** papszDirContent = VSIReadDir(osDirName.c_str());
                        char** ptrDir = papszDirContent;
                        if (ptrDir)
                        {
                            while(*ptrDir)
                            {
                                if ( EQUAL(CPLGetExtension(*ptrDir), "GEN") &&  EQUALN(CPLGetBasename(*ptrDir), osName,6))
                                {
                                    bFound = 1;
                                    osGENFileName = CPLFormFilename(osDirName.c_str(), *ptrDir, nullptr);
                                    CPLDebug("SRP", "Building GEN full file name : %s", osGENFileName.c_str());
                                    break;
                                }
                                ptrDir ++;
                            }
                            CSLDestroy(papszDirContent);
                        }
                    }

                    if (bFound ==1)
                    {
                        papszFileNames = (char**)CPLRealloc(papszFileNames, sizeof(char*) * (nFilenames + 2));
                        papszFileNames[nFilenames] = CPLStrdup(osGENFileName.c_str());
                        papszFileNames[nFilenames + 1] = nullptr;
                        nFilenames ++;
                    }
                }
            }
        }
    }
    return papszFileNames;
}

/************************************************************************/
/*                          AddMetadatafromFromTHF()                         */
/************************************************************************/

void SRPDataset::AddMetadatafromFromTHF(const char* pszFileName)
{
    DDFModule module;
    DDFRecord * record = nullptr;
    DDFField* field = nullptr;
    DDFFieldDefn *fieldDefn = nullptr;

    int bSuccess=0;
    if (!module.Open(pszFileName, TRUE))
        return ;

    while( true )
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == nullptr || record->GetFieldCount() <= 2)
            break;

        field = record->GetField(0);
        fieldDefn = field->GetFieldDefn();
        if (!(strcmp(fieldDefn->GetName(), "001") == 0) ||
            fieldDefn->GetSubfieldCount() != 2)
            break;

        const char* RTY = record->GetStringSubfield("001", 0, "RTY", 0);
        if ( RTY != nullptr &&  strcmp(RTY, "THF") == 0 )
        {
            field = record->GetField(1);
            fieldDefn = field->GetFieldDefn();
            if ((strcmp(fieldDefn->GetName(), "VDR") == 0 &&
                fieldDefn->GetSubfieldCount() == 8))
            {

                const char* pszVOO = record->GetStringSubfield("VDR", 0, "VOO", 0);
                if( pszVOO != nullptr )
                {
                    CPLDebug("SRP", "Record VOO %s",pszVOO);
                    SetMetadataItem( "SRP_VOO", pszVOO );
                }

                int EDN = record->GetIntSubfield( "VDR", 0, "EDN", 0, &bSuccess );
                if (bSuccess)
                {
                    CPLDebug("SRP", "Record EDN %d",EDN);
                    char szValue[5];
                    snprintf(szValue, sizeof(szValue), "%d", EDN);
                    SetMetadataItem( "SRP_EDN", szValue );
                }

                const char* pszCDV07 = record->GetStringSubfield("VDR", 0, "CDV07", 0);
                if( pszCDV07 != nullptr )
                {
                    CPLDebug("SRP", "Record pszCDV07 %s",pszCDV07);
                    SetMetadataItem( "SRP_CREATIONDATE", pszCDV07 );
                }
                else
                {  /*USRP1.2*/
                    const char* pszDAT = record->GetStringSubfield("VDR", 0, "DAT", 0);
                    if( pszDAT != nullptr )
                    {
                        char dat[9];
                        strncpy(dat,pszDAT+4,8);
                        dat[8]='\0';
                        CPLDebug("SRP", "Record DAT %s",dat);
                        SetMetadataItem( "SRP_CREATIONDATE", dat );
                    }
                }
            }
        } /* End of THF part */

        if ( RTY != nullptr && strcmp(RTY, "LCF") == 0 )
        {
            field = record->GetField(1);
            fieldDefn = field->GetFieldDefn();
            if ((strcmp(fieldDefn->GetName(), "QSR") == 0 &&
                fieldDefn->GetSubfieldCount() == 4))
            {

                const char* pszQSS = record->GetStringSubfield("QSR", 0, "QSS", 0);
                if( pszQSS != nullptr )
                {
                    CPLDebug("SRP", "Record Classification %s",pszQSS);
                    SetMetadataItem( "SRP_CLASSIFICATION", pszQSS );
                }
            }

            field = record->GetField(2);
            fieldDefn = field->GetFieldDefn();
            if ((strcmp(fieldDefn->GetName(), "QUV") == 0 &&
                fieldDefn->GetSubfieldCount() == 6))
            {
                const char* pszSRC2 = record->GetStringSubfield("QUV", 0, "SRC1", 0);
                if( pszSRC2 != nullptr )
                {
                    SetMetadataItem( "SRP_PRODUCTVERSION", pszSRC2 );
                }
                else
                {
                    const char* pszSRC = record->GetStringSubfield("QUV", 0, "SRC", 0);
                    if( pszSRC != nullptr )
                    {
                        SetMetadataItem( "SRP_PRODUCTVERSION", pszSRC );
                    }
                }
            }
        }  /* End of LCF part */
    }
}

/************************************************************************/
/*                          GetIMGListFromGEN()                         */
/************************************************************************/

char** SRPDataset::GetIMGListFromGEN(const char* pszFileName,
                                    int *pnRecordIndex)
{
    DDFRecord * record = nullptr;
    DDFField* field = nullptr;
    DDFFieldDefn *fieldDefn = nullptr;
    int nFilenames = 0;
    char** papszFileNames = nullptr;
    int nRecordIndex = -1;

    if (pnRecordIndex)
        *pnRecordIndex = -1;

    DDFModule module;
    if (!module.Open(pszFileName, TRUE))
        return nullptr;

    while( true )
    {
        nRecordIndex ++;

        CPLPushErrorHandler( CPLQuietErrorHandler );
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == nullptr)
            break;

        if (record->GetFieldCount() >= 5)
        {
            field = record->GetField(0);
            fieldDefn = field->GetFieldDefn();
            if (!(strcmp(fieldDefn->GetName(), "001") == 0 &&
                fieldDefn->GetSubfieldCount() == 2))
            {
                continue;
            }

            const char* RTY = record->GetStringSubfield("001", 0, "RTY", 0);
            if( RTY == nullptr )
                continue;
            /* Ignore overviews */
            if ( strcmp(RTY, "OVV") == 0 )
                continue;

            if ( strcmp(RTY, "GIN") != 0 )
                continue;

            /* make sure that the GEN file is part of a SRP dataset, not an ADRG dataset, by checking that the GEN field does not contain a NOW subfield */
            const char* NWO = record->GetStringSubfield("GEN", 0, "NWO", 0);
            if( NWO )
            {
                CSLDestroy(papszFileNames);
                return nullptr;
            }

            field = record->GetField(3);
            if( field == nullptr )
                continue;
            fieldDefn = field->GetFieldDefn();

            if (!(strcmp(fieldDefn->GetName(), "SPR") == 0 &&
                fieldDefn->GetSubfieldCount() == 15))
            {
                continue;
            }

            const char* pszBAD = record->GetStringSubfield("SPR", 0, "BAD", 0);
            if( pszBAD == nullptr || strlen(pszBAD) != 12 )
                continue;
            CPLString osBAD = pszBAD;
            {
                char* c = (char*) strchr(osBAD.c_str(), ' ');
                if (c)
                    *c = 0;
            }
            CPLDebug("SRP", "BAD=%s", osBAD.c_str());

            /* Build full IMG file name from BAD value */
            CPLString osGENDir(CPLGetDirname(pszFileName));

            CPLString osFileName = CPLFormFilename(osGENDir.c_str(), osBAD.c_str(), nullptr);
            VSIStatBufL sStatBuf;
            if( VSIStatL( osFileName, &sStatBuf ) == 0 )
            {
                osBAD = osFileName;
                CPLDebug("SRP", "Building IMG full file name : %s", osBAD.c_str());
            }
            else
            {
                char** papszDirContent = nullptr;
                if (strcmp(osGENDir.c_str(), "/vsimem") == 0)
                {
                    CPLString osTmp = osGENDir + "/";
                    papszDirContent = VSIReadDir(osTmp);
                }
                else
                    papszDirContent = VSIReadDir(osGENDir);
                char** ptrDir = papszDirContent;
                while(ptrDir && *ptrDir)
                {
                    if (EQUAL(*ptrDir, osBAD.c_str()))
                    {
                        osBAD = CPLFormFilename(osGENDir.c_str(), *ptrDir, nullptr);
                        CPLDebug("SRP", "Building IMG full file name : %s", osBAD.c_str());
                        break;
                    }
                    ptrDir ++;
                }
                CSLDestroy(papszDirContent);
            }

            if (nFilenames == 0 && pnRecordIndex)
                *pnRecordIndex = nRecordIndex;

            papszFileNames = (char**)CPLRealloc(papszFileNames, sizeof(char*) * (nFilenames + 2));
            papszFileNames[nFilenames] = CPLStrdup(osBAD.c_str());
            papszFileNames[nFilenames + 1] = nullptr;
            nFilenames ++;
        }
    }

    return papszFileNames;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SRPDataset::Open( GDALOpenInfo * poOpenInfo )
{
    int nRecordIndex = -1;
    CPLString osGENFileName;
    CPLString osIMGFileName;
    int bFromSubdataset = FALSE;
    int bTHFWithSingleGEN = FALSE;

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SRP:") )
    {
        char** papszTokens = CSLTokenizeString2(poOpenInfo->pszFilename + 4, ",", 0);
        if (CSLCount(papszTokens) == 2)
        {
            osGENFileName = papszTokens[0];
            osIMGFileName = papszTokens[1];
            bFromSubdataset = TRUE;
        }
        CSLDestroy(papszTokens);
    }
    else
    {
        if( poOpenInfo->nHeaderBytes < 500 )
            return nullptr;
        CPLString osFileName(poOpenInfo->pszFilename);

        if (EQUAL(CPLGetExtension(osFileName.c_str()), "THF"))
        {

            CPLDebug("SRP", "Read THF");

            char** papszFileNames = GetGENListFromTHF(osFileName.c_str());
            if (papszFileNames == nullptr)
                return nullptr;
            if (papszFileNames[1] == nullptr &&
                CPLTestBool(CPLGetConfigOption("SRP_SINGLE_GEN_IN_THF_AS_DATASET", "TRUE")))
            {
                osFileName = papszFileNames[0];
                CSLDestroy(papszFileNames);
                bTHFWithSingleGEN = TRUE;
            }
            else
            {
                char** ptr = papszFileNames;
                SRPDataset* poDS = new SRPDataset();
                poDS->AddMetadatafromFromTHF(osFileName.c_str());
                while(*ptr)
                {
                    char** papszIMGFileNames = GetIMGListFromGEN(*ptr);
                    char** papszIMGIter = papszIMGFileNames;
                    while(papszIMGIter && *papszIMGIter)
                    {
                        poDS->AddSubDataset(*ptr, *papszIMGIter);
                        papszIMGIter ++;
                    }
                    CSLDestroy(papszIMGFileNames);

                    ptr ++;
                }
                CSLDestroy(papszFileNames);
                return poDS;
            }
        }

        if ( bTHFWithSingleGEN
#ifdef OPEN_GEN
                || EQUAL(CPLGetExtension(osFileName.c_str()), "GEN")
#endif
            )
        {
            osGENFileName = osFileName;

            char** papszFileNames = GetIMGListFromGEN(osFileName.c_str(), &nRecordIndex);
            if (papszFileNames == nullptr)
                return nullptr;
            if (papszFileNames[1] == nullptr)
            {
                osIMGFileName = papszFileNames[0];
                CSLDestroy(papszFileNames);
            }
            else
            {
                char** ptr = papszFileNames;
                SRPDataset* poDS = new SRPDataset();
                while(*ptr)
                {
                    poDS->AddSubDataset(osFileName.c_str(), *ptr);
                    ptr ++;
                }
                CSLDestroy(papszFileNames);
                return poDS;
            }
        }

        if (EQUAL(CPLGetExtension(osFileName.c_str()), "IMG"))
        {

            osIMGFileName = osFileName;

            constexpr int nLeaderSize = 24;

            int i = 0;  // Used after for.
            for( ; i < nLeaderSize; i++ )
            {
                if( poOpenInfo->pabyHeader[i] < 32
                    || poOpenInfo->pabyHeader[i] > 126 )
                    return nullptr;
            }

            if( poOpenInfo->pabyHeader[5] != '1'
                && poOpenInfo->pabyHeader[5] != '2'
                && poOpenInfo->pabyHeader[5] != '3' )
                return nullptr;

            if( poOpenInfo->pabyHeader[6] != 'L' )
                return nullptr;
            if( poOpenInfo->pabyHeader[8] != '1' && poOpenInfo->pabyHeader[8] != ' ' )
                return nullptr;

            // --------------------------------------------------------------------
            //      Find and open the .GEN file.
            // --------------------------------------------------------------------
            VSIStatBufL sStatBuf;

            CPLString basename = CPLGetBasename( osFileName );
            if( basename.size() != 8 )
            {
                CPLDebug("SRP", "Invalid basename file");
                return nullptr;
            }

            nRecordIndex = static_cast<int>(CPLScanLong( basename + 6, 2 ));

            CPLString path = CPLGetDirname( osFileName );
            CPLString basename01 = ResetTo01( basename );
            osFileName = CPLFormFilename( path, basename01, ".IMG" );

            osFileName = CPLResetExtension( osFileName, "GEN" );
            if( VSIStatL( osFileName, &sStatBuf ) != 0 )
            {
                osFileName = CPLResetExtension( osFileName, "gen" );
                if( VSIStatL( osFileName, &sStatBuf ) != 0 )
                    return nullptr;
            }

            osGENFileName = osFileName;
        }
    }

    if (!osGENFileName.empty() &&
        !osIMGFileName.empty())
    {

        if( poOpenInfo->eAccess == GA_Update )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                "The SRP driver does not support update access to existing"
                " datasets.\n" );
            return nullptr;
        }

        DDFModule module;
        DDFRecord* record = nullptr;
        if (nRecordIndex >= 0 &&
            module.Open(osGENFileName.c_str(), TRUE))
        {
            for( int i = 0; i < nRecordIndex; i++ )
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                record = module.ReadRecord();
                CPLPopErrorHandler();
                CPLErrorReset();
                if (record == nullptr)
                    break;
            }
        }
        SRPDataset* poDS = OpenDataset(osGENFileName.c_str(), osIMGFileName.c_str(), record);

        if (poDS)
        {
            /* ---------------------------------------------------------- */
            /*      Initialize any PAM information.                       */
            /* ---------------------------------------------------------- */
            poDS->SetDescription( poOpenInfo->pszFilename );
            poDS->TryLoadXML();

            /* ---------------------------------------------------------- */
            /*      Check for external overviews.                         */
            /* ---------------------------------------------------------- */
            if( bFromSubdataset )
                poDS->oOvManager.Initialize( poDS, osIMGFileName.c_str() );
            else
                poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

            return poDS;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                         GDALRegister_SRP()                          */
/************************************************************************/

void GDALRegister_SRP()

{
    if( GDALGetDriverByName( "SRP" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SRP" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Standard Raster Product (ASRP/USRP)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/srp.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "img" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = SRPDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
