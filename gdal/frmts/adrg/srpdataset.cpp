/******************************************************************************
 * $Id$
 * Purpose:  ASRP/USRP Reader
 * Author:   Frank Warmerdam (warmerdam@pobox.com)
 *
 * Derived from ADRG driver by Even Rouault, even.rouault at mines-paris.org.
 *
 ******************************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

// Uncomment to recognize also .gen files in addition to .img files
// #define OPEN_GEN

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
    static char** GetIMGListFromGEN(const char* pszFileName, int* pnRecordIndex = NULL);
    static SRPDataset* OpenDataset(const char* pszGENFileName, const char* pszIMGFileName, DDFRecord* record = NULL);
    static DDFRecord*  FindRecordInGENForIMG(DDFModule& module,
        const char* pszGENFileName, const char* pszIMGFileName);

public:
    SRPDataset();
    virtual     ~SRPDataset();
    
    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * padfGeoTransform );

    virtual char      **GetMetadata( const char * pszDomain = "" );

    virtual char **GetFileList();

    int                 GetFromRecord( const char* pszFileName, 
                                       DDFRecord * record);
    void                AddSubDataset( const char* pszGENFileName, const char* pszIMGFileName );
    void  AddMetadatafromFromTHF(const char* pszFileName);

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
    papszSubDatasets = NULL;
}

/************************************************************************/
/*                          ~SRPDataset()                              */
/************************************************************************/

SRPDataset::~SRPDataset()
{

    CSLDestroy(papszSubDatasets);

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

    const char* pszBAD = record->GetStringSubfield( "SPR", 0, "BAD", 0, &bSuccess );
    if( pszBAD == NULL )
        return FALSE;
    osBAD = pszBAD;
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
    int TIF = pszTIF != NULL && EQUAL(pszTIF,"Y");
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
    osQALFileName = CPLFormCIFilename(osDirname, osBasename, "QAL");

    DDFModule oQALModule;

    if( oQALModule.Open( osQALFileName, TRUE ) )
    {
        while( (record = oQALModule.ReadRecord()) != NULL)
        {
            if( record->FindField( "COL" ) != NULL ) 
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

            if (record->FindField( "QUV" ) != NULL )
            {
                //Date de production du produit : QAL.QUV.DAT1
                //Num�ro d'�dition  du produit : QAL.QUV.EDN

                int EDN = record->GetIntSubfield( "QUV", 0, "EDN", 0, &bSuccess );
                if (bSuccess)
                {
                    CPLDebug("SRP", "EDN=%d", EDN);
                    char pszValue[5];
                    sprintf(pszValue, "%d", EDN);
                    SetMetadataItem( "SRP_EDN", pszValue );
                }


                const char* pszCDV07 = record->GetStringSubfield( "QUV", 0, "CDV07", 0 );
                if (pszCDV07!=NULL)
                    SetMetadataItem( "SRP_CREATIONDATE", pszCDV07 );
                else
                { /*USRP1.2*/
                    const char* pszDAT = record->GetStringSubfield("QUV", 0, "DAT1", 0);
                    if( pszDAT != NULL )
                    {
                        char dat[9];
                        strncpy(dat,pszDAT+4,8);
                        dat[8]='\0';
                        CPLDebug("SRP", "Record DAT %s",dat);
                        SetMetadataItem( "SRP_CREATIONDATE", dat );
                    }
                }

                const char* pszCDV24 = record->GetStringSubfield( "QUV", 0, "CDV24", 0 );
                if (pszCDV24!=NULL)
                    SetMetadataItem( "SRP_REVISIONDATE", pszCDV24 );
                else
                { /*USRP1.2*/
                    const char* pszDAT = record->GetStringSubfield("QUV", 0, "DAT2", 0);
                    if( pszDAT != NULL )
                    {
                        char dat[9];
                        strncpy(dat,pszDAT+4,8);
                        dat[8]='\0';
                        CPLDebug("SRP", "Record DAT %s",dat);
                        SetMetadataItem( "SRP_REVISIONDATE", dat );
                    }
                }

                const char* pszQSS = record->GetStringSubfield( "QSR", 0, "QSS", 0 );
                if (pszQSS!=NULL)
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

    sprintf(pszValue, "%d", ZNA);
    SetMetadataItem( "SRP_ZNA", pszValue );

    return TRUE;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **SRPDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();
    if (osGENFileName.size() > 0 && osIMGFileName.size() > 0)
    {
        CPLString osMainFilename = GetDescription();
        int bMainFileReal;
        VSIStatBufL  sStat;

        bMainFileReal = VSIStatL( osMainFilename, &sStat ) == 0;
        if (bMainFileReal)
        {
            CPLString osShortMainFilename = CPLGetFilename(osMainFilename);
            CPLString osShortGENFileName = CPLGetFilename(osGENFileName);
            if ( !EQUAL(osShortMainFilename.c_str(), osShortGENFileName.c_str()) )
                papszFileList = CSLAddString(papszFileList, osGENFileName.c_str());
        }
        else
            papszFileList = CSLAddString(papszFileList, osGENFileName.c_str());

        papszFileList = CSLAddString(papszFileList, osIMGFileName.c_str());


        if( strlen(osQALFileName) > 0 )
            papszFileList = CSLAddString( papszFileList, osQALFileName );
    }
    return papszFileList;
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void SRPDataset::AddSubDataset( const char* pszGENFileName, const char* pszIMGFileName )
{
    char	szName[80];
    int		nCount = CSLCount(papszSubDatasets ) / 2;

    CPLString osSubDatasetName;
    osSubDatasetName = "SRP:";
    osSubDatasetName += pszGENFileName;
    osSubDatasetName += ",";
    osSubDatasetName += pszIMGFileName;

    sprintf( szName, "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName, osSubDatasetName);

    sprintf( szName, "SUBDATASET_%d_DESC", nCount+1 );
    papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName, osSubDatasetName);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **SRPDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                      FindRecordInGENForIMG()                         */
/************************************************************************/

DDFRecord* SRPDataset::FindRecordInGENForIMG(DDFModule& module,
                                            const char* pszGENFileName,
                                            const char* pszIMGFileName)
{
    /* Finds the GEN file corresponding to the IMG file */
    if (!module.Open(pszGENFileName, TRUE))
        return NULL;


    CPLString osShortIMGFilename = CPLGetFilename(pszIMGFileName);

    DDFField* field;
    DDFFieldDefn *fieldDefn;

    /* Now finds the record */
    while (TRUE)
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        DDFRecord* record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == NULL)
            return NULL;

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
            if( RTY == NULL )
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
            if( pszBAD == NULL || strlen(pszBAD) != 12 )
                continue;
            CPLString osBAD = pszBAD;
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
    DDFModule module;    
    DDFField* field;
    DDFFieldDefn *fieldDefn;

    if (record == NULL)
    {
        record = FindRecordInGENForIMG(module, pszGENFileName, pszIMGFileName);
        if (record == NULL)
            return NULL;
    }

    field = record->GetField(1);
    if( field == NULL )
        return NULL;
    fieldDefn = field->GetFieldDefn();

    if (!(strcmp(fieldDefn->GetName(), "DSI") == 0 &&
        fieldDefn->GetSubfieldCount() == 2))
    {
        return NULL;
    }

    const char* pszPRT = record->GetStringSubfield("DSI", 0, "PRT", 0);
    if( pszPRT == NULL) 
        return NULL;

    CPLString osPRT = pszPRT;
    osPRT.resize(4);
    CPLDebug("SRP", "osPRT=%s", osPRT.c_str());
    if( !EQUAL(osPRT,"ASRP") && !EQUAL(osPRT,"USRP") )
        return NULL;

    const char* pszNAM = record->GetStringSubfield("DSI", 0, "NAM", 0);
    if( pszNAM == NULL  )
        return NULL;


    CPLString osNAM = pszNAM;
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
        return NULL;
    }

    return poDS;

}



/************************************************************************/
/*                          GetGENListFromTHF()                         */
/************************************************************************/

char** SRPDataset::GetGENListFromTHF(const char* pszFileName)
{
    DDFModule module;
    DDFRecord * record;
    DDFField* field;
    DDFFieldDefn *fieldDefn;
    int i;
    int nFilenames = 0;

    char** papszFileNames = NULL;
    if (!module.Open(pszFileName, TRUE))
        return papszFileNames;

    CPLString osDirName(CPLGetDirname(pszFileName));

    while (TRUE)
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == NULL)
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
            if ( RTY == NULL )
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
                for (i = 2; i < record->GetFieldCount() ; i++)
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
                    if( pszNAM == NULL)
                        continue;


                    CPLString osName = CPLString(pszNAM);

                    /* Define a subdirectory from Dataset but with only 6 caracatere */
                    CPLString osDirDataset = pszNAM;
                    osDirDataset.resize(6); 
                    CPLString osDatasetDir = CPLFormFilename(osDirName.c_str(), osDirDataset.c_str(), NULL);

                    CPLString osGENFileName="";


                    int bFound=0;
                    if (bFound ==0)
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
                                    osGENFileName = CPLFormFilename(osDatasetDir.c_str(), *ptrDir, NULL);
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
                                    osGENFileName = CPLFormFilename(osDirName.c_str(), *ptrDir, NULL);
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
                        papszFileNames[nFilenames + 1] = NULL;
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
    DDFRecord * record;
    DDFField* field;
    DDFFieldDefn *fieldDefn;

    int bSuccess=0;
    if (!module.Open(pszFileName, TRUE))
        return ;

    CPLString osDirName(CPLGetDirname(pszFileName));

    while (TRUE)
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == NULL || record->GetFieldCount() <= 2)
            break;

        field = record->GetField(0);
        fieldDefn = field->GetFieldDefn();
        if (!(strcmp(fieldDefn->GetName(), "001") == 0) ||
            fieldDefn->GetSubfieldCount() != 2)
            break;

        const char* RTY = record->GetStringSubfield("001", 0, "RTY", 0);
        if ( RTY != NULL &&  strcmp(RTY, "THF") == 0 )
        {
            field = record->GetField(1);
            fieldDefn = field->GetFieldDefn();
            if ((strcmp(fieldDefn->GetName(), "VDR") == 0 &&
                fieldDefn->GetSubfieldCount() == 8))
            {

                const char* pszVOO = record->GetStringSubfield("VDR", 0, "VOO", 0);
                if( pszVOO != NULL )
                {
                    CPLDebug("SRP", "Record VOO %s",pszVOO);
                    SetMetadataItem( "SRP_VOO", pszVOO );
                }


                int EDN = record->GetIntSubfield( "VDR", 0, "EDN", 0, &bSuccess );
                if (bSuccess)
                {
                    CPLDebug("SRP", "Record EDN %d",EDN);
                    char pszValue[5];
                    sprintf(pszValue, "%d", EDN);
                    SetMetadataItem( "SRP_EDN", pszValue );
                }


                const char* pszCDV07 = record->GetStringSubfield("VDR", 0, "CDV07", 0);
                if( pszCDV07 != NULL )
                {
                    CPLDebug("SRP", "Record pszCDV07 %s",pszCDV07);
                    SetMetadataItem( "SRP_CREATIONDATE", pszCDV07 );
                }
                else
                {  /*USRP1.2*/
                    const char* pszDAT = record->GetStringSubfield("VDR", 0, "DAT", 0);
                    if( pszDAT != NULL )
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

        if ( RTY != NULL && strcmp(RTY, "LCF") == 0 )
        {
            field = record->GetField(1);
            fieldDefn = field->GetFieldDefn();
            if ((strcmp(fieldDefn->GetName(), "QSR") == 0 &&
                fieldDefn->GetSubfieldCount() == 4))
            {

                const char* pszQSS = record->GetStringSubfield("QSR", 0, "QSS", 0);
                if( pszQSS != NULL )
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
                if( pszSRC2 != NULL )
                {
                    SetMetadataItem( "SRP_PRODUCTVERSION", pszSRC2 );
                }
                else
                {
                    const char* pszSRC = record->GetStringSubfield("QUV", 0, "SRC", 0);
                    if( pszSRC != NULL )
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
    DDFRecord * record;
    DDFField* field;
    DDFFieldDefn *fieldDefn;
    int nFilenames = 0;
    char** papszFileNames = NULL;
    int nRecordIndex = -1;

    if (pnRecordIndex)
        *pnRecordIndex = -1;

    DDFModule module;
    if (!module.Open(pszFileName, TRUE))
        return NULL;    

    while (TRUE)
    {
        nRecordIndex ++;

        CPLPushErrorHandler( CPLQuietErrorHandler );
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == NULL)
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
            if( RTY == NULL )
                continue;
            /* Ignore overviews */
            if ( strcmp(RTY, "OVV") == 0 )
                continue;

            if ( strcmp(RTY, "GIN") != 0 )
                continue;

            field = record->GetField(3);
            if( field == NULL )
                continue;
            fieldDefn = field->GetFieldDefn();

            if (!(strcmp(fieldDefn->GetName(), "SPR") == 0 &&
                fieldDefn->GetSubfieldCount() == 15))
            {
                continue;
            }

            const char* pszBAD = record->GetStringSubfield("SPR", 0, "BAD", 0);
            if( pszBAD == NULL || strlen(pszBAD) != 12 )
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

            CPLString osFileName = CPLFormFilename(osGENDir.c_str(), osBAD.c_str(), NULL);
            VSIStatBufL sStatBuf;
            if( VSIStatL( osFileName, &sStatBuf ) == 0 )
            {
                osBAD = osFileName;
                CPLDebug("SRP", "Building IMG full file name : %s", osBAD.c_str());
            }
            else
            {
                char** papszDirContent;
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
                        osBAD = CPLFormFilename(osGENDir.c_str(), *ptrDir, NULL);
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
            papszFileNames[nFilenames + 1] = NULL;
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

    if( EQUALN(poOpenInfo->pszFilename, "SRP:", 4) )
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
            return NULL;
        CPLString osFileName(poOpenInfo->pszFilename);

        if (EQUAL(CPLGetExtension(osFileName.c_str()), "THF"))
        {

            CPLDebug("SRP", "Read THF");

            char** papszFileNames = GetGENListFromTHF(osFileName.c_str());
            if (papszFileNames == NULL)
                return NULL;
            if (papszFileNames[1] == NULL &&
                CSLTestBoolean(CPLGetConfigOption("SRP_SINGLE_GEN_IN_THF_AS_DATASET", "TRUE")))
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
            if (papszFileNames == NULL)
                return NULL;
            if (papszFileNames[1] == NULL)
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

            // --------------------------------------------------------------------
            //      Find and open the .GEN file.                                   
            // -------------------------------------------------------------------- 
            VSIStatBufL sStatBuf;

            CPLString basename = CPLGetBasename( osFileName );
            if( basename.size() != 8 )
            {
                CPLDebug("SRP", "Invalid basename file");
                return NULL;
            }

            nRecordIndex = CPLScanLong( basename + 6, 2 );

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

            osGENFileName = osFileName;
        }
    }

    if (osGENFileName.size() > 0 &&
        osIMGFileName.size() > 0)
    {


        if( poOpenInfo->eAccess == GA_Update )
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                "The SRP driver does not support update access to existing"
                " datasets.\n" );
            return NULL;
        }

        DDFModule module;
        DDFRecord* record = NULL;
        if (nRecordIndex >= 0 &&
            module.Open(osGENFileName.c_str(), TRUE))
        {
            int i;
            for(i=0;i<nRecordIndex;i++)
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                record = module.ReadRecord();
                CPLPopErrorHandler();
                CPLErrorReset();
                if (record == NULL)
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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Standard Raster Product (ASRP/USRP)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#SRP" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "img" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = SRPDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

