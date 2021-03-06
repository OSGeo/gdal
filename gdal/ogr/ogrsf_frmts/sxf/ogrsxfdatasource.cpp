/******************************************************************************
 *
 * Project:  SXF Translator
 * Purpose:  Definition of classes for OGR SXF Datasource.
 * Author:   Ben Ahmed Daho Ali, bidandou(at)yahoo(dot)fr
 *           Dmitry Baryshnikov, polimax@mail.ru
 *           Alexandr Lisovenko, alexander.lisovenko@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Ben Ahmed Daho Ali
 * Copyright (c) 2013, NextGIS
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2019, NextGIS, <info@nextgis.com>
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

#include "cpl_conv.h"
#include "ogr_sxf.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"

#include <math.h>
#include <map>
#include <string>

CPL_CVSID("$Id$")

// EPSG code range http://gis.stackexchange.com/a/18676/9904
constexpr int MIN_EPSG = 1000;
constexpr int MAX_EPSG = 32768;

/************************************************************************/
/*                         OGRSXFDataSource()                           */
/************************************************************************/

OGRSXFDataSource::OGRSXFDataSource() :
    papoLayers(nullptr),
    nLayers(0),
    fpSXF(nullptr),
    hIOMutex(nullptr)
{
    oSXFPassport.stMapDescription.pSpatRef = nullptr;
}

/************************************************************************/
/*                          ~OGRSXFDataSource()                         */
/************************************************************************/

OGRSXFDataSource::~OGRSXFDataSource()

{
    for( size_t i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (nullptr != oSXFPassport.stMapDescription.pSpatRef)
    {
        oSXFPassport.stMapDescription.pSpatRef->Release();
    }

    CloseFile();

    if (hIOMutex != nullptr)
    {
        CPLDestroyMutex(hIOMutex);
        hIOMutex = nullptr;
    }
}

/************************************************************************/
/*                     CloseFile()                                      */
/************************************************************************/
void  OGRSXFDataSource::CloseFile()
{
    if (nullptr != fpSXF)
    {
        VSIFCloseL( fpSXF );
        fpSXF = nullptr;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSXFDataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSXFDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= (int)nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSXFDataSource::Open(const char * pszFilename, bool bUpdateIn,
                           const char* const* papszOpenOpts)
{
    if( bUpdateIn )
    {
        return FALSE;
    }

    pszName = pszFilename;

    fpSXF = VSIFOpenL(pszName, "rb");
    if ( fpSXF == nullptr )
    {
        CPLError(CE_Warning, CPLE_OpenFailed, "SXF open file %s failed", pszFilename);
        return FALSE;
    }

    //read header
    const int nFileHeaderSize = sizeof(SXFHeader);
    SXFHeader stSXFFileHeader;
    const size_t nObjectsRead =
        VSIFReadL(&stSXFFileHeader, nFileHeaderSize, 1, fpSXF);

    if (nObjectsRead != 1)
    {
        CPLError(CE_Failure, CPLE_None, "SXF head read failed");
        CloseFile();
        return FALSE;
    }
    CPL_LSBPTR32(&(stSXFFileHeader.nHeaderLength));
    CPL_LSBPTR32(&(stSXFFileHeader.nCheckSum));

    //check version
    oSXFPassport.version = 0;
    if (stSXFFileHeader.nHeaderLength > 256) //if size == 400 then version >= 4
    {
        oSXFPassport.version = stSXFFileHeader.nFormatVersion[2];
    }
    else
    {
        oSXFPassport.version = stSXFFileHeader.nFormatVersion[1];
    }

    if ( oSXFPassport.version < 3 )
    {
        CPLError(CE_Failure, CPLE_NotSupported , "SXF File version not supported");
        CloseFile();
        return FALSE;
    }

    // read description
    if (ReadSXFDescription(fpSXF, oSXFPassport) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "SXF. Wrong description.");
        CloseFile();
        return FALSE;
    }

    //read flags
    if (ReadSXFInformationFlags(fpSXF, oSXFPassport) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "SXF. Wrong state of the data.");
        CloseFile();
        return FALSE;
    }

    if(oSXFPassport.version == 3 &&
               oSXFPassport.informationFlags.bProjectionDataCompliance == false)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SXF. Data does not correspond to the projection." );
        CloseFile();
        return FALSE;
    }

    //read spatial data
    if (ReadSXFMapDescription(fpSXF, oSXFPassport, papszOpenOpts) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "SXF. Wrong state of the data.");
        CloseFile();
        return FALSE;
    }

    if(oSXFPassport.informationFlags.bRealCoordinatesCompliance == false )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "SXF. Given material may be rotated in the conditional system of coordinates" );
    }

/*---------------- TRY READ THE RSC FILE HEADER  -----------------------*/

    CPLString soRSCRileName;
    const char* pszRSCRileName =
        CSLFetchNameValueDef(papszOpenOpts, "SXF_RSC_FILENAME",
                             CPLGetConfigOption("SXF_RSC_FILENAME", ""));
    if (pszRSCRileName != nullptr && CPLCheckForFile((char *)pszRSCRileName, nullptr) == TRUE)
    {
        soRSCRileName = pszRSCRileName;
    }

    if(soRSCRileName.empty())
    {
        pszRSCRileName = CPLResetExtension(pszFilename, "rsc");
        if (CPLCheckForFile((char *)pszRSCRileName, nullptr) == TRUE)
        {
            soRSCRileName = pszRSCRileName;
        }
    }

    if(soRSCRileName.empty())
    {
        pszRSCRileName = CPLResetExtension(pszFilename, "RSC");
        if (CPLCheckForFile((char *)pszRSCRileName, nullptr) == TRUE)
        {
            soRSCRileName = pszRSCRileName;
        }
    }

    // 1. Create layers from RSC file or create default set of layers from
    // gdal_data/default.rsc.

    if(soRSCRileName.empty())
    {
        pszRSCRileName = CPLFindFile( "gdal", "default.rsc" );
        if (nullptr != pszRSCRileName)
        {
            soRSCRileName = pszRSCRileName;
        }
        else
        {
            CPLDebug( "OGRSXFDataSource", "Default RSC file not found" );
        }
    }

    if (soRSCRileName.empty())
    {
        CPLError(CE_Warning, CPLE_None, "RSC file for %s not exist", pszFilename);
    }
    else
    {
        VSILFILE* fpRSC = VSIFOpenL(soRSCRileName, "rb");
        if (fpRSC == nullptr)
        {
            CPLError(CE_Warning, CPLE_OpenFailed, "RSC file %s open failed",
                     soRSCRileName.c_str());
        }
        else
        {
            CPLDebug( "OGRSXFDataSource", "RSC Filename: %s",
                      soRSCRileName.c_str() );
            CreateLayers(fpRSC, papszOpenOpts);
            VSIFCloseL(fpRSC);
        }
    }

    if (nLayers == 0)//create default set of layers
    {
        CreateLayers();
    }

    FillLayers();

    return TRUE;
}

OGRErr OGRSXFDataSource::ReadSXFDescription(VSILFILE* fpSXFIn, SXFPassport& passport)
{
    // int nObjectsRead = 0;

    if (passport.version == 3)
    {
        //78
        GByte buff[62];
        /* nObjectsRead = */ VSIFReadL(&buff, 62, 1, fpSXFIn);
        char date[3] = { 0 };

        //read year
        memcpy(date, buff, 2);
        passport.dtCrateDate.nYear = static_cast<GUInt16>(atoi(date));
        if (passport.dtCrateDate.nYear < 50)
            passport.dtCrateDate.nYear += 2000;
        else
            passport.dtCrateDate.nYear += 1900;

        memcpy(date, buff + 2, 2);

        passport.dtCrateDate.nMonth = static_cast<GUInt16>(atoi(date));

        memcpy(date, buff + 4, 2);

        passport.dtCrateDate.nDay = static_cast<GUInt16>(atoi(date));

        char szName[26] = { 0 };
        memcpy(szName, buff + 8, 24);
        szName[ sizeof(szName) - 1 ] = '\0';
        char* pszRecoded = CPLRecode(szName, "CP1251", CPL_ENC_UTF8);// szName + 2
        passport.sMapSheet = pszRecoded; //TODO: check the encoding in SXF created in Linux
        CPLFree(pszRecoded);

        memcpy(&passport.nScale, buff + 32, 4);
        CPL_LSBPTR32(&passport.nScale);

        memcpy(szName, buff + 36, 26);
        szName[ sizeof(szName) - 1 ] = '\0';
        pszRecoded = CPLRecode(szName, "CP866", CPL_ENC_UTF8);
        passport.sMapSheetName = pszRecoded; //TODO: check the encoding in SXF created in Linux
        CPLFree(pszRecoded);
    }
    else if (passport.version == 4)
    {
        //96
        GByte buff[80];
        /* nObjectsRead = */ VSIFReadL(&buff, 80, 1, fpSXFIn);
        char date[5] = { 0 };

        //read year
        memcpy(date, buff, 4);
        passport.dtCrateDate.nYear = static_cast<GUInt16>(atoi(date));

        memcpy(date, buff + 4, 2);
        memset(date+2, 0, 3);

        passport.dtCrateDate.nMonth = static_cast<GUInt16>(atoi(date));

        memcpy(date, buff + 6, 2);

        passport.dtCrateDate.nDay = static_cast<GUInt16>(atoi(date));

        char szName[32] = { 0 };
        memcpy(szName, buff + 12, 32);
        szName[ sizeof(szName) - 1 ] = '\0';
        char* pszRecoded = CPLRecode(szName, "CP1251", CPL_ENC_UTF8); //szName + 2
        passport.sMapSheet = pszRecoded; //TODO: check the encoding in SXF created in Linux
        CPLFree(pszRecoded);

        memcpy(&passport.nScale, buff + 44, 4);
        CPL_LSBPTR32(&passport.nScale);

        memcpy(szName, buff + 48, 32);
        szName[ sizeof(szName) - 1 ] = '\0';
        pszRecoded = CPLRecode(szName, "CP1251", CPL_ENC_UTF8);
        passport.sMapSheetName = pszRecoded; //TODO: check the encoding in SXF created in Linux
        CPLFree(pszRecoded);
    }

    SetMetadataItem("SHEET", passport.sMapSheet);
    SetMetadataItem("SHEET_NAME", passport.sMapSheetName);
    SetMetadataItem("SHEET_CREATE_DATE", CPLSPrintf( "%.2u-%.2u-%.4u",
                    passport.dtCrateDate.nDay,
                    passport.dtCrateDate.nMonth,
                    passport.dtCrateDate.nYear ));
    SetMetadataItem("SXF_VERSION", CPLSPrintf("%u", passport.version));
    SetMetadataItem("SCALE", CPLSPrintf("1 : %u", passport.nScale));

    return OGRERR_NONE;
}

OGRErr OGRSXFDataSource::ReadSXFInformationFlags(VSILFILE* fpSXFIn, SXFPassport& passport)
{
    // int nObjectsRead = 0;
    GByte val[4];
    /* nObjectsRead = */ VSIFReadL(&val, 4, 1, fpSXFIn);

    if (!(CHECK_BIT(val[0], 0) && CHECK_BIT(val[0], 1))) // xxxxxx11
    {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    if (CHECK_BIT(val[0], 2)) // xxxxx0xx or xxxxx1xx
    {
        passport.informationFlags.bProjectionDataCompliance = true;
    }
    else
    {
        passport.informationFlags.bProjectionDataCompliance = false;
    }

    if (CHECK_BIT(val[0], 4))
    {
        passport.informationFlags.bRealCoordinatesCompliance = true;
    }
    else
    {
        passport.informationFlags.bRealCoordinatesCompliance = false;
    }

    if (CHECK_BIT(val[0], 6))
    {
        passport.informationFlags.stCodingType = SXF_SEM_TXT;
    }
    else
    {
        if (CHECK_BIT(val[0], 5))
        {
            passport.informationFlags.stCodingType = SXF_SEM_HEX;
        }
        else
        {
            passport.informationFlags.stCodingType = SXF_SEM_DEC;
        }
    }

    if (CHECK_BIT(val[0], 7))
    {
        passport.informationFlags.stGenType = SXF_GT_LARGE_SCALE;
    }
    else
    {
        passport.informationFlags.stGenType = SXF_GT_SMALL_SCALE;
    }

    //version specific

    if (passport.version == 3)
    {
        //degrees are ints * 100 000 000
        //meters are ints / 10
        passport.informationFlags.stEnc = SXF_ENC_DOS;
        passport.informationFlags.stCoordAcc = SXF_COORD_ACC_DM;
        passport.informationFlags.bSort = false;
    }
    else if (passport.version == 4)
    {
        passport.informationFlags.stEnc = (SXFTextEncoding)val[1];
        passport.informationFlags.stCoordAcc = (SXFCoordinatesAccuracy)val[2];
        if (CHECK_BIT(val[3], 0))
        {
            passport.informationFlags.bSort = true;
        }
        else
        {
            passport.informationFlags.bSort = false;
        }
    }

    return OGRERR_NONE;
}

void OGRSXFDataSource::SetVertCS(const long iVCS, SXFPassport& passport,
                                 const char* const* papszOpenOpts)
{
    const char* pszSetVertCS =
        CSLFetchNameValueDef(papszOpenOpts,
                             "SXF_SET_VERTCS",
                              CPLGetConfigOption("SXF_SET_VERTCS", "NO"));
    if (!CPLTestBool(pszSetVertCS))
        return;

    passport.stMapDescription.pSpatRef->importVertCSFromPanorama(static_cast<int>(iVCS));
}

OGRErr OGRSXFDataSource::ReadSXFMapDescription(VSILFILE* fpSXFIn, SXFPassport& passport,
                                               const char* const* papszOpenOpts)
{
    // int nObjectsRead = 0;
    passport.stMapDescription.Env.MaxX = -100000000;
    passport.stMapDescription.Env.MinX = 100000000;
    passport.stMapDescription.Env.MaxY = -100000000;
    passport.stMapDescription.Env.MinY = 100000000;

    bool bIsX = true;// passport.informationFlags.bRealCoordinatesCompliance; //if real coordinates we need to swap x & y

    //version specific
    if (passport.version == 3)
    {
        short nNoObjClass, nNoSemClass;
        /* nObjectsRead = */ VSIFReadL(&nNoObjClass, 2, 1, fpSXFIn);
        /* nObjectsRead = */ VSIFReadL(&nNoSemClass, 2, 1, fpSXFIn);
        GByte byMask[8];
        /* nObjectsRead = */ VSIFReadL(&byMask, 8, 1, fpSXFIn);

        int nCorners[8];

        //get projected corner coords
        /* nObjectsRead = */ VSIFReadL(&nCorners, 32, 1, fpSXFIn);

        for( int i = 0; i < 8; i++ )
        {
            CPL_LSBPTR32(&nCorners[i]);
            passport.stMapDescription.stProjCoords[i] = double(nCorners[i]) / 10.0;
            if (bIsX) //X
            {
                if (passport.stMapDescription.Env.MaxY < passport.stMapDescription.stProjCoords[i])
                    passport.stMapDescription.Env.MaxY = passport.stMapDescription.stProjCoords[i];
                if (passport.stMapDescription.Env.MinY > passport.stMapDescription.stProjCoords[i])
                    passport.stMapDescription.Env.MinY = passport.stMapDescription.stProjCoords[i];
            }
            else
            {
                if (passport.stMapDescription.Env.MaxX < passport.stMapDescription.stProjCoords[i])
                    passport.stMapDescription.Env.MaxX = passport.stMapDescription.stProjCoords[i];
                if (passport.stMapDescription.Env.MinX > passport.stMapDescription.stProjCoords[i])
                    passport.stMapDescription.Env.MinX = passport.stMapDescription.stProjCoords[i];
            }
            bIsX = !bIsX;
        }
        //get geographic corner coords
        /* nObjectsRead = */ VSIFReadL(&nCorners, 32, 1, fpSXFIn);

        for( int i = 0; i < 8; i++ )
        {
            CPL_LSBPTR32(&nCorners[i]);
            passport.stMapDescription.stGeoCoords[i] = double(nCorners[i]) * 0.00000057295779513082; //from radians to degree * 100 000 000
        }
    }
    else if (passport.version == 4)
    {
        int nEPSG = 0;
        /* nObjectsRead = */ VSIFReadL(&nEPSG, 4, 1, fpSXFIn);
        CPL_LSBPTR32(&nEPSG);

        if (nEPSG >= MIN_EPSG && nEPSG <= MAX_EPSG) //TODO: check epsg valid range
        {
            passport.stMapDescription.pSpatRef = new OGRSpatialReference();
            passport.stMapDescription.pSpatRef->importFromEPSG(nEPSG);
        }

        double dfCorners[8];
        /* nObjectsRead = */ VSIFReadL(&dfCorners, 64, 1, fpSXFIn);

        for( int i = 0; i < 8; i++ )
        {
            CPL_LSBPTR64(&dfCorners[i]);
            passport.stMapDescription.stProjCoords[i] = dfCorners[i];
            if (bIsX) //X
            {
                if (passport.stMapDescription.Env.MaxY < passport.stMapDescription.stProjCoords[i])
                    passport.stMapDescription.Env.MaxY = passport.stMapDescription.stProjCoords[i];
                if (passport.stMapDescription.Env.MinY > passport.stMapDescription.stProjCoords[i])
                    passport.stMapDescription.Env.MinY = passport.stMapDescription.stProjCoords[i];
            }
            else
            {
                if (passport.stMapDescription.Env.MaxX < passport.stMapDescription.stProjCoords[i])
                    passport.stMapDescription.Env.MaxX = passport.stMapDescription.stProjCoords[i];
                if (passport.stMapDescription.Env.MinX > passport.stMapDescription.stProjCoords[i])
                    passport.stMapDescription.Env.MinX = passport.stMapDescription.stProjCoords[i];
            }
            bIsX = !bIsX;
        }
        //get geographic corner coords
        /* nObjectsRead = */ VSIFReadL(&dfCorners, 64, 1, fpSXFIn);

        for( int i = 0; i < 8; i++ )
        {
            CPL_LSBPTR64(&dfCorners[i]);
            passport.stMapDescription.stGeoCoords[i] = dfCorners[i] * TO_DEGREES; // to degree
        }
    }

    if (nullptr != passport.stMapDescription.pSpatRef)
    {
        return OGRERR_NONE;
    }

    GByte anData[8] = { 0 };
    /* nObjectsRead = */ VSIFReadL(&anData, 8, 1, fpSXFIn);
    long iEllips = anData[0];
    long iVCS = anData[1];
    long iProjSys = anData[2];
    /* long iDatum = anData[3]; Unused. */
    double dfProjScale = 1;

    double adfPrjParams[8] = { 0 };

    if (passport.version == 3)
    {
        switch (anData[4])
        {
        case 1:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_DECIMETRE;
            break;
        case 2:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_CENTIMETRE;
            break;
        case 3:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_MILLIMETRE;
            break;
        case 130:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_RADIAN;
            break;
        case 129:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_DEGREE;
            break;
        default:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_METRE;
            break;
        }

        VSIFSeekL(fpSXFIn, 212, SEEK_SET);
        struct _buff{
            GUInt32 nRes;
            GInt16 anFrame[8];
            // cppcheck-suppress unusedStructMember
            GUInt32 nFrameCode;
        } buff;
        /* nObjectsRead = */ VSIFReadL(&buff, 20, 1, fpSXFIn);
        CPL_LSBPTR32(&buff.nRes);
        CPL_LSBPTR32(&buff.nFrameCode);
        passport.stMapDescription.nResolution = buff.nRes; //resolution

        for( int i = 0; i < 8; i++ )
        {
            CPL_LSBPTR16(&(buff.anFrame[i]));
            passport.stMapDescription.stFrameCoords[i] = buff.anFrame[i];
        }

        int anParams[5];
        /* nObjectsRead = */ VSIFReadL(&anParams, 20, 1, fpSXFIn);
        for(int i = 0; i < 5; i++)
        {
            CPL_LSBPTR32(&anParams[i]);
        }

        if (anParams[0] != -1)
            dfProjScale = double(anParams[0]) / 100000000.0;

        if (anParams[2] != -1)
            passport.stMapDescription.dfXOr = double(anParams[2]) / 100000000.0 * TO_DEGREES;
        else
            passport.stMapDescription.dfXOr = 0;

        if (anParams[3] != -1)
            passport.stMapDescription.dfYOr = double(anParams[2]) / 100000000.0 * TO_DEGREES;
        else
            passport.stMapDescription.dfYOr = 0;

        passport.stMapDescription.dfFalseNorthing = 0;
        passport.stMapDescription.dfFalseEasting = 0;

        //adfPrjParams[0] = double(anParams[0]) / 100000000.0; // to radians
        //adfPrjParams[1] = double(anParams[1]) / 100000000.0;
        //adfPrjParams[2] = double(anParams[2]) / 100000000.0;
        //adfPrjParams[3] = double(anParams[3]) / 100000000.0;
        adfPrjParams[4] = dfProjScale;//?
        //adfPrjParams[5] = 0;//?
        //adfPrjParams[6] = 0;//?
        //adfPrjParams[7] = 0;// importFromPanorama calc it by itself
    }
    else if (passport.version == 4)
    {
        switch (anData[4])
        {
        case 64:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_RADIAN;
            break;
        case 65:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_DEGREE;
            break;
        default:
            passport.stMapDescription.eUnitInPlan = SXF_COORD_MU_METRE;
            break;
        }

        VSIFSeekL(fpSXFIn, 312, SEEK_SET);
        GUInt32 buff[10];
        /* nObjectsRead = */ VSIFReadL(&buff, 40, 1, fpSXFIn);
        for(int i = 0; i < 10; i++)
        {
            CPL_LSBPTR32(&buff[i]);
        }

        passport.stMapDescription.nResolution = buff[0]; //resolution
        for( int i = 0; i < 8; i++ )
            passport.stMapDescription.stFrameCoords[i] = buff[1 + i];

        double adfParams[6] = {};
        /* nObjectsRead = */ VSIFReadL(&adfParams, 48, 1, fpSXFIn);
        for(int i = 0; i < 6; i++)
        {
            CPL_LSBPTR64(&adfParams[i]);
        }

        if (adfParams[1] != -1)
            dfProjScale = adfParams[1];
        passport.stMapDescription.dfXOr = adfParams[2] * TO_DEGREES;
        passport.stMapDescription.dfYOr = adfParams[3] * TO_DEGREES;
        passport.stMapDescription.dfFalseNorthing = adfParams[4];
        passport.stMapDescription.dfFalseEasting = adfParams[5];

        //adfPrjParams[0] = adfParams[0]; // to radians
        //adfPrjParams[1] = adfParams[1];
        //adfPrjParams[2] = adfParams[2];
        //adfPrjParams[3] = adfParams[3];
        adfPrjParams[4] = dfProjScale;//?
        //adfPrjParams[5] = adfParams[4];
        //adfPrjParams[6] = adfParams[5];
        //adfPrjParams[7] = 0;// importFromPanorama calc it by itself
    }

    passport.stMapDescription.dfScale = passport.nScale;

    double dfCoeff = double(passport.stMapDescription.dfScale) / passport.stMapDescription.nResolution;
    passport.stMapDescription.bIsRealCoordinates = passport.informationFlags.bRealCoordinatesCompliance;
    passport.stMapDescription.stCoordAcc = passport.informationFlags.stCoordAcc;

    if (!passport.stMapDescription.bIsRealCoordinates)
    {
        if (passport.stMapDescription.stFrameCoords[0] == 0 && passport.stMapDescription.stFrameCoords[1] == 0 && passport.stMapDescription.stFrameCoords[2] == 0 && passport.stMapDescription.stFrameCoords[3] == 0 && passport.stMapDescription.stFrameCoords[4] == 0 && passport.stMapDescription.stFrameCoords[5] == 0 && passport.stMapDescription.stFrameCoords[6] == 0 && passport.stMapDescription.stFrameCoords[7] == 0)
        {
            passport.stMapDescription.bIsRealCoordinates = true;
        }
        else
        {
            //origin
            passport.stMapDescription.dfXOr = passport.stMapDescription.stProjCoords[1] - passport.stMapDescription.stFrameCoords[1] * dfCoeff;
            passport.stMapDescription.dfYOr = passport.stMapDescription.stProjCoords[0] - passport.stMapDescription.stFrameCoords[0] * dfCoeff;
        }
    }

    //normalize some coordintatessystems
    if ((iEllips == 1 || iEllips == 0 ) && iProjSys == 1) // Pulkovo 1942 / Gauss-Kruger
    {
        double dfCenterLongEnv = passport.stMapDescription.stGeoCoords[1] + fabs(passport.stMapDescription.stGeoCoords[5] - passport.stMapDescription.stGeoCoords[1]) / 2;

        int nZoneEnv = (int)( (dfCenterLongEnv + 3.0) / 6.0 + 0.5 );

        if (nZoneEnv > 1 && nZoneEnv < 33)
        {
            int nEPSG = 28400 + nZoneEnv;
            passport.stMapDescription.pSpatRef = new OGRSpatialReference();
            passport.stMapDescription.pSpatRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            OGRErr eErr = passport.stMapDescription.pSpatRef->importFromEPSG(nEPSG);
            SetVertCS(iVCS, passport, papszOpenOpts);
            return eErr;
        }
        else
        {
            adfPrjParams[7] = nZoneEnv;

            if (adfPrjParams[5] == 0)//False Easting
            {
                if (passport.stMapDescription.Env.MaxX < 500000)
                    adfPrjParams[5] = 500000;
                else
                    adfPrjParams[5] = nZoneEnv * 1000000 + 500000;
            }
        }
    }
    else if (iEllips == 9 && iProjSys == 17) // WGS84 / UTM
    {
        double dfCenterLongEnv = passport.stMapDescription.stGeoCoords[1] + fabs(passport.stMapDescription.stGeoCoords[5] - passport.stMapDescription.stGeoCoords[1]) / 2;
        int nZoneEnv = (int)(30 + (dfCenterLongEnv + 3.0) / 6.0 + 0.5);
        bool bNorth = passport.stMapDescription.stGeoCoords[6] + (passport.stMapDescription.stGeoCoords[2] - passport.stMapDescription.stGeoCoords[6]) / 2 < 0;
        int nEPSG = 0;
        if( bNorth )
        {
            nEPSG = 32600 + nZoneEnv;
        }
        else
        {
            nEPSG = 32700 + nZoneEnv;
        }
        passport.stMapDescription.pSpatRef = new OGRSpatialReference();
        passport.stMapDescription.pSpatRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        OGRErr eErr = passport.stMapDescription.pSpatRef->importFromEPSG(nEPSG);
        SetVertCS(iVCS, passport, papszOpenOpts);
        return eErr;
    }
    else if (iEllips == 45 && iProjSys == 35) //Mercator 3857 on sphere wgs84
    {
        passport.stMapDescription.pSpatRef = new OGRSpatialReference("PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"],AUTHORITY[\"EPSG\",\"3857\"]]");
        passport.stMapDescription.pSpatRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        OGRErr eErr = OGRERR_NONE; //passport.stMapDescription.pSpatRef->importFromEPSG(3857);
        SetVertCS(iVCS, passport, papszOpenOpts);
        return eErr;
    }
    else if (iEllips == 9 && iProjSys == 35) //Mercator 3395 on ellips wgs84
    {
        passport.stMapDescription.pSpatRef = new OGRSpatialReference();
        passport.stMapDescription.pSpatRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        OGRErr eErr = passport.stMapDescription.pSpatRef->importFromEPSG(3395);
        SetVertCS(iVCS, passport, papszOpenOpts);
        return eErr;
    }
    else if (iEllips == 9 && iProjSys == 34) //Miller 54003 on sphere wgs84
    {
        passport.stMapDescription.pSpatRef = new OGRSpatialReference("PROJCS[\"World_Miller_Cylindrical\",GEOGCS[\"GCS_GLOBE\", DATUM[\"GLOBE\", SPHEROID[\"GLOBE\", 6367444.6571, 0.0]],PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",0.017453292519943295]],PROJECTION[\"Miller_Cylindrical\"],PARAMETER[\"False_Easting\",0],PARAMETER[\"False_Northing\",0],PARAMETER[\"Central_Meridian\",0],UNIT[\"Meter\",1],AUTHORITY[\"EPSG\",\"54003\"]]");
        passport.stMapDescription.pSpatRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        OGRErr eErr = OGRERR_NONE; //passport.stMapDescription.pSpatRef->importFromEPSG(3395);
        //OGRErr eErr = passport.stMapDescription.pSpatRef->importFromEPSG(54003);

        SetVertCS(iVCS, passport, papszOpenOpts);
        return eErr;
    }
    else if (iEllips == 9 && iProjSys == 33 &&
        passport.stMapDescription.eUnitInPlan == SXF_COORD_MU_DEGREE)
    {
        passport.stMapDescription.pSpatRef = new OGRSpatialReference(SRS_WKT_WGS84_LAT_LONG);
        passport.stMapDescription.pSpatRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        OGRErr eErr = OGRERR_NONE;
        SetVertCS(iVCS, passport, papszOpenOpts);
        return eErr;
    }

    //TODO: Need to normalize more SRS:
    //PAN_PROJ_WAG1
    //PAN_PROJ_MERCAT
    //PAN_PROJ_PS
    //PAN_PROJ_POLYC
    //PAN_PROJ_EC
    //PAN_PROJ_LCC
    //PAN_PROJ_STEREO
    //PAN_PROJ_AE
    //PAN_PROJ_GNOMON
    //PAN_PROJ_MOLL
    //PAN_PROJ_LAEA
    //PAN_PROJ_EQC
    //PAN_PROJ_CEA
    //PAN_PROJ_IMWP
    //

    passport.stMapDescription.pSpatRef = new OGRSpatialReference();
    passport.stMapDescription.pSpatRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    OGRErr eErr = passport.stMapDescription.pSpatRef->importFromPanorama(anData[2], anData[3], anData[0], adfPrjParams);
    SetVertCS(iVCS, passport, papszOpenOpts);
    return eErr;
}

void OGRSXFDataSource::FillLayers()
{
    CPLDebug("SXF","Create layers");

    //2. Read all records (only classify code and offset) and add this to correspondence layer
    int nObjectsRead = 0;
    vsi_l_offset nOffset = 0;

    //get record count
    GUInt32 nRecordCountMax = 0;
    if (oSXFPassport.version == 3)
    {
        VSIFSeekL(fpSXF, 288, SEEK_SET);
        nObjectsRead = static_cast<int>(VSIFReadL(&nRecordCountMax, 4, 1, fpSXF));
        nOffset = 300;
        CPL_LSBPTR32(&nRecordCountMax);
    }
    else if (oSXFPassport.version == 4)
    {
        VSIFSeekL(fpSXF, 440, SEEK_SET);
        nObjectsRead = static_cast<int>(VSIFReadL(&nRecordCountMax, 4, 1, fpSXF));
        nOffset = 452;
        CPL_LSBPTR32(&nRecordCountMax);
    }
    /* else nOffset and nObjectsRead will be 0 */

    if (nObjectsRead != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Get record count failed");
        return;
    }

    VSIFSeekL(fpSXF, nOffset, SEEK_SET);

    for( GUInt32 nFID = 0; nFID < nRecordCountMax; nFID++ )
    {
        GInt32 buff[6];
        nObjectsRead = static_cast<int>(VSIFReadL(&buff, 24, 1, fpSXF));
        for( int i = 0; i < 6; i++ )
        {
            CPL_LSBPTR32(&buff[i]);
        }

        if (nObjectsRead != 1 || buff[0] != IDSXFOBJ)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Read record %d failed", nFID);
            return;
        }

        bool bHasSemantic = CHECK_BIT(buff[5], 9);
        if (bHasSemantic) //check has attributes
        {
            //we have already 24 byte read
            vsi_l_offset nOffsetSemantic = 8 + buff[2];
            VSIFSeekL(fpSXF, nOffsetSemantic, SEEK_CUR);
        }

        int nSemanticSize = buff[1] - 32 - buff[2];
        if( nSemanticSize < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid value");
            break;
        }

        for( size_t i = 0; i < nLayers; i++ )
        {
            OGRSXFLayer* pOGRSXFLayer = (OGRSXFLayer*)papoLayers[i];
            if (pOGRSXFLayer && pOGRSXFLayer->AddRecord(nFID, buff[3], nOffset, bHasSemantic, nSemanticSize) == TRUE)
            {
                break;
            }
        }
        nOffset += buff[1];
        VSIFSeekL(fpSXF, nOffset, SEEK_SET);
    }
    //3. delete empty layers
    for( size_t i = 0; i < nLayers; i++ )
    {
        OGRSXFLayer* pOGRSXFLayer = (OGRSXFLayer*)papoLayers[i];
        if (pOGRSXFLayer && pOGRSXFLayer->GetFeatureCount() == 0)
        {
            delete pOGRSXFLayer;
            size_t nDeletedLayerIndex = i;
            while (nDeletedLayerIndex < nLayers - 1)
            {
                papoLayers[nDeletedLayerIndex] = papoLayers[nDeletedLayerIndex + 1];
                nDeletedLayerIndex++;
            }
            nLayers--;
            i--;
        }
        else if (pOGRSXFLayer)
            pOGRSXFLayer->ResetReading();
    }
}

OGRSXFLayer* OGRSXFDataSource::GetLayerById(GByte nID)
{
    for (size_t i = 0; i < nLayers; i++)
    {
        OGRSXFLayer* pOGRSXFLayer = (OGRSXFLayer*)papoLayers[i];
        if (pOGRSXFLayer && pOGRSXFLayer->GetId() == nID)
        {
            return pOGRSXFLayer;
        }
    }
    return nullptr;
}

void OGRSXFDataSource::CreateLayers()
{
    //default layers set
    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    OGRSXFLayer* pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 0, CPLString("SYSTEM"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    //default codes
    for (unsigned int i = 1000000001; i < 1000000015; i++)
    {
        pLayer->AddClassifyCode(i);
    }
    pLayer->AddClassifyCode(91000000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    papoLayers[nLayers] = new OGRSXFLayer(fpSXF, &hIOMutex, 255, CPLString("Not_Classified"), oSXFPassport.version, oSXFPassport.stMapDescription);
    nLayers++;
}

void OGRSXFDataSource::CreateLayers(VSILFILE* fpRSC, const char* const* papszOpenOpts)
{

    RSCHeader stRSCFileHeader;
    int nObjectsRead = static_cast<int>(VSIFReadL(&stRSCFileHeader, sizeof(stRSCFileHeader), 1, fpRSC));

    if (nObjectsRead != 1)
    {
        CPLError(CE_Warning, CPLE_None, "RSC head read failed");
        return;
    }

    CPL_LSBPTR32(&(stRSCFileHeader.nFileLength));
    CPL_LSBPTR32(&(stRSCFileHeader.nVersion));
    CPL_LSBPTR32(&(stRSCFileHeader.nEncoding));
    CPL_LSBPTR32(&(stRSCFileHeader.nFileState));
    CPL_LSBPTR32(&(stRSCFileHeader.nFileModState));
    CPL_LSBPTR32(&(stRSCFileHeader.nLang));
    CPL_LSBPTR32(&(stRSCFileHeader.nNextID));
    CPL_LSBPTR32(&(stRSCFileHeader.nScale));

#define SWAP_SECTION(x) \
    CPL_LSBPTR32(&(x.nOffset)); \
    CPL_LSBPTR32(&(x.nLength)); \
    CPL_LSBPTR32(&(x.nRecordCount));

    SWAP_SECTION(stRSCFileHeader.Objects);
    SWAP_SECTION(stRSCFileHeader.Semantic);
    SWAP_SECTION(stRSCFileHeader.ClassifySemantic);
    SWAP_SECTION(stRSCFileHeader.Defaults);
    SWAP_SECTION(stRSCFileHeader.Semantics);
    SWAP_SECTION(stRSCFileHeader.Layers);
    SWAP_SECTION(stRSCFileHeader.Limits);
    SWAP_SECTION(stRSCFileHeader.Parameters);
    SWAP_SECTION(stRSCFileHeader.Print);
    SWAP_SECTION(stRSCFileHeader.Palettes);
    SWAP_SECTION(stRSCFileHeader.Fonts);
    SWAP_SECTION(stRSCFileHeader.Libs);
    SWAP_SECTION(stRSCFileHeader.ImageParams);
    SWAP_SECTION(stRSCFileHeader.Tables);
    CPL_LSBPTR32(&(stRSCFileHeader.nFontEnc));
    CPL_LSBPTR32(&(stRSCFileHeader.nColorsInPalette));

    GByte szLayersID[4];
    struct _layer{
        GUInt32 nLength;
        char szName[32];
        char szShortName[16];
        GByte nNo;
        // cppcheck-suppress unusedStructMember
        GByte nPos;
        // cppcheck-suppress unusedStructMember
        GUInt16 nSemanticCount;
    };

    VSIFSeekL(fpRSC, stRSCFileHeader.Layers.nOffset - sizeof(szLayersID), SEEK_SET);
    VSIFReadL(&szLayersID, sizeof(szLayersID), 1, fpRSC);
    vsi_l_offset nOffset = stRSCFileHeader.Layers.nOffset;
    _layer LAYER;

    for( GUInt32 i = 0; i < stRSCFileHeader.Layers.nRecordCount; ++i )
    {
        VSIFReadL(&LAYER, sizeof(LAYER), 1, fpRSC);
        CPL_LSBPTR32(&(LAYER.nLength));
        CPL_LSBPTR16(&(LAYER.nSemanticCount));
        papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
        bool bLayerFullName = CPLTestBool(
                 CSLFetchNameValueDef(papszOpenOpts,
                                      "SXF_LAYER_FULLNAME",
                                       CPLGetConfigOption("SXF_LAYER_FULLNAME", "NO")));
        char* pszRecoded = nullptr;
        if (bLayerFullName)
        {
            if(LAYER.szName[0] == 0)
                pszRecoded = CPLStrdup("Unnamed");
            else if (stRSCFileHeader.nFontEnc == 125)
                pszRecoded = CPLRecode(LAYER.szName, "KOI8-R", CPL_ENC_UTF8);
            else if (stRSCFileHeader.nFontEnc == 126)
                pszRecoded = CPLRecode(LAYER.szName, "CP1251", CPL_ENC_UTF8);
            else
                pszRecoded = CPLStrdup(LAYER.szName);

            papoLayers[nLayers] = new OGRSXFLayer(fpSXF, &hIOMutex, LAYER.nNo, CPLString(pszRecoded), oSXFPassport.version, oSXFPassport.stMapDescription);
        }
        else
        {
            if(LAYER.szShortName[0] == 0)
                pszRecoded = CPLStrdup("Unnamed");
            else if (stRSCFileHeader.nFontEnc == 125)
                pszRecoded = CPLRecode(LAYER.szShortName, "KOI8-R", CPL_ENC_UTF8);
            else if (stRSCFileHeader.nFontEnc == 126)
                pszRecoded = CPLRecode(LAYER.szShortName, "CP1251", CPL_ENC_UTF8);
            else
                pszRecoded = CPLStrdup(LAYER.szShortName);

            papoLayers[nLayers] = new OGRSXFLayer(fpSXF, &hIOMutex, LAYER.nNo, CPLString(pszRecoded), oSXFPassport.version, oSXFPassport.stMapDescription);
        }
        CPLFree(pszRecoded);
        nLayers++;

        nOffset += LAYER.nLength;
        VSIFSeekL(fpRSC, nOffset, SEEK_SET);
    }

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    papoLayers[nLayers] = new OGRSXFLayer(fpSXF, &hIOMutex, 255, CPLString("Not_Classified"), oSXFPassport.version, oSXFPassport.stMapDescription);
    nLayers++;

    char szObjectsID[4];
    struct _object{
        unsigned nLength;
        unsigned nClassifyCode;
        // cppcheck-suppress unusedStructMember
        unsigned nObjectNumber;
        // cppcheck-suppress unusedStructMember
        unsigned nObjectCode;
        char szShortName[32];
        char szName[32];
        // cppcheck-suppress unusedStructMember
        char szGeomType;
        char szLayernNo;
        // cppcheck-suppress unusedStructMember
        char szUnimportantSeg[14];
    };

    VSIFSeekL(fpRSC, stRSCFileHeader.Objects.nOffset - sizeof(szObjectsID), SEEK_SET);
    VSIFReadL(&szObjectsID, sizeof(szObjectsID), 1, fpRSC);
    nOffset = stRSCFileHeader.Objects.nOffset;
    _object OBJECT;

    for( GUInt32 i = 0; i < stRSCFileHeader.Objects.nRecordCount; ++i )
    {
        VSIFReadL(&OBJECT, sizeof(_object), 1, fpRSC);
        CPL_LSBPTR32(&(OBJECT.nLength));
        CPL_LSBPTR32(&(OBJECT.nClassifyCode));
        CPL_LSBPTR32(&(OBJECT.nObjectNumber));
        CPL_LSBPTR32(&(OBJECT.nObjectCode));

        OGRSXFLayer* pLayer = GetLayerById(OBJECT.szLayernNo);
        if (nullptr != pLayer)
        {
            char* pszRecoded = nullptr;
            if(OBJECT.szName[0] == 0)
                pszRecoded = CPLStrdup("Unnamed");
            else if (stRSCFileHeader.nFontEnc == 125)
                pszRecoded = CPLRecode(OBJECT.szName, "KOI8-R", CPL_ENC_UTF8);
            else if (stRSCFileHeader.nFontEnc == 126)
                pszRecoded = CPLRecode(OBJECT.szName, "CP1251", CPL_ENC_UTF8);
            else
                pszRecoded = CPLStrdup(OBJECT.szName); //already in  CPL_ENC_UTF8

            pLayer->AddClassifyCode(OBJECT.nClassifyCode, pszRecoded);
            //printf("%d;%s\n", OBJECT.nClassifyCode, OBJECT.szName);
            CPLFree(pszRecoded);
        }

        nOffset += OBJECT.nLength;
        VSIFSeekL(fpRSC, nOffset, SEEK_SET);
    }
}
