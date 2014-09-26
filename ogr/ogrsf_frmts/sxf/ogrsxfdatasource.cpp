/******************************************************************************
 * $Id: ogr_sxfdatasource.cpp  $
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
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id: ogrsxfdatasource.cpp  $");

static void  *hIOMutex = NULL;

static const long aoVCS[] =
{
    0,
    5705,   //1
    5711,   //2
    0,      //3
    5710,   //4
    5710,   //5
    0,      //6
    0,      //7
    0,      //8
    0,      //9
    5716,   //10
    5733,   //11
    0,      //12
    0,      //13
    0,      //14
    0,      //15
    5709,   //16
    5776,   //17
    0,      //18
    0,      //19
    5717,   //20
    5613,   //21
    0,      //22
    5775,   //23
    5702,   //24
    0,      //25
    0,      //26
    5714    //27
};

#define NUMBER_OF_VERTICALCS    (sizeof(aoVCS)/sizeof(aoVCS[0]))

/************************************************************************/
/*                         OGRSXFDataSource()                           */
/************************************************************************/

OGRSXFDataSource::OGRSXFDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    fpSXF = NULL;

    oSXFPassport.stMapDescription.pSpatRef = NULL;
}

/************************************************************************/
/*                          ~OGRSXFDataSource()                         */
/************************************************************************/

OGRSXFDataSource::~OGRSXFDataSource()

{
    for( size_t i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (NULL != oSXFPassport.stMapDescription.pSpatRef)
    {
        oSXFPassport.stMapDescription.pSpatRef->Release();
    }

    CloseFile();

    if (hIOMutex != NULL)
    {
        CPLDestroyMutex(hIOMutex);
        hIOMutex = NULL;
    }
}

/************************************************************************/
/*                     CloseFile()                                      */
/************************************************************************/
void  OGRSXFDataSource::CloseFile()
{ 
    if (NULL != fpSXF)
    {
        VSIFCloseL( fpSXF );
        fpSXF = NULL;
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
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSXFDataSource::Open( const char * pszFilename, int bUpdateIn)
{
    size_t nObjectsRead;
    int nFileHeaderSize;

    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = pszFilename;

    fpSXF = VSIFOpenL(pszName, "rb");
    if ( fpSXF == NULL )
    {
        CPLError(CE_Warning, CPLE_OpenFailed, "SXF open file %s failed", pszFilename);
        return FALSE;
    }
    
    //read header
    nFileHeaderSize = sizeof(SXFHeader);
    SXFHeader stSXFFileHeader;
    nObjectsRead = VSIFReadL(&stSXFFileHeader, nFileHeaderSize, 1, fpSXF);

    if (nObjectsRead != 1)
    {
        CPLError(CE_Failure, CPLE_None, "SXF head read failed");
        CloseFile();
		return FALSE;
    }

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

    if ( oSXFPassport.version == 0 )
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

    if (oSXFPassport.informationFlags.bProjectionDataCompliance == false)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SXF. Data are not corresponde to the projection." );
        CloseFile();
        return FALSE;
    }

    //read spatial data
    if (ReadSXFMapDescription(fpSXF, oSXFPassport) != OGRERR_NONE)
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

    CPLString pszRSCRileName = CPLGetConfigOption("SXF_RSC_FILENAME", "");
    if (CPLCheckForFile((char *)pszRSCRileName.c_str(), NULL) == FALSE)
    {
        pszRSCRileName = CPLResetExtension(pszFilename, "rsc");
        if (CPLCheckForFile((char *)pszRSCRileName.c_str(), NULL) == FALSE)
        {
            CPLError(CE_Warning, CPLE_None, "RSC file %s not exist", pszRSCRileName.c_str());
            pszRSCRileName.Clear();
        }
    }

    //1. Create layers from RSC file or create default set of layers from osm.rsc

    if (!pszRSCRileName.empty())
    {
        VSILFILE* fpRSC;

        fpRSC = VSIFOpenL(pszRSCRileName, "rb");
        if (fpRSC == NULL)
        {
            CPLError(CE_Warning, CPLE_OpenFailed, "RSC open file %s failed", pszFilename);
        }
        else
        {
            CreateLayers(fpRSC);
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

OGRErr OGRSXFDataSource::ReadSXFDescription(VSILFILE* fpSXF, SXFPassport& passport)
{
    /* int nObjectsRead; */

    if (passport.version == 3)
    {
        //78
        GByte buff[62];
        /* nObjectsRead = */ VSIFReadL(&buff, 62, 1, fpSXF);
        char date[3] = { 0 };

        //read year
        memcpy(date, buff, 2);
        passport.dtCrateDate.nYear = atoi(date);
        if (passport.dtCrateDate.nYear < 50)
            passport.dtCrateDate.nYear += 2000;
        else
            passport.dtCrateDate.nYear += 1900;

        memcpy(date, buff + 2, 2);

        passport.dtCrateDate.nMonth = atoi(date);

        memcpy(date, buff + 4, 2);

        passport.dtCrateDate.nDay = atoi(date);

        char szName[26] = { 0 };
        memcpy(szName, buff + 8, 24);
        char* pszRecoded = CPLRecode(szName + 2, "CP1251", CPL_ENC_UTF8);
        passport.sMapSheet = pszRecoded; //TODO: check the encoding in SXF created in Linux
        CPLFree(pszRecoded);

        memcpy(&passport.nScale, buff + 32, 4);
        CPL_LSBPTR32(&passport.nScale);

        memset(szName, 0, 26);
        memcpy(szName, buff + 36, 26);
        pszRecoded = CPLRecode(szName, "CP866", CPL_ENC_UTF8);
        passport.sMapSheetName = pszRecoded; //TODO: check the encoding in SXF created in Linux
        CPLFree(pszRecoded);

    }
    else if (passport.version == 4)
    {
        //96
        GByte buff[80];
        /* nObjectsRead = */ VSIFReadL(&buff, 80, 1, fpSXF);
        char date[5] = { 0 };

        //read year
        memcpy(date, buff, 4);
        passport.dtCrateDate.nYear = atoi(date);

        memset(date, 0, 5);
        memcpy(date, buff + 4, 2);

        passport.dtCrateDate.nMonth = atoi(date);

        memcpy(date, buff + 6, 2);

        passport.dtCrateDate.nDay = atoi(date);

        char szName[32] = { 0 };
        memcpy(szName, buff + 12, 32);
        char* pszRecoded = CPLRecode(szName + 2, "CP1251", CPL_ENC_UTF8);
        passport.sMapSheet = pszRecoded; //TODO: check the encoding in SXF created in Linux
        CPLFree(pszRecoded);
        
        memcpy(&passport.nScale, buff + 44, 4);
        CPL_LSBPTR32(&passport.nScale);

        memset(szName, 0, 32);
        memcpy(szName, buff + 48, 32);
        pszRecoded = CPLRecode(szName, "CP1251", CPL_ENC_UTF8);
        passport.sMapSheetName = pszRecoded; //TODO: check the encoding in SXF created in Linux
        CPLFree(pszRecoded);
    }

    return OGRERR_NONE;
}

OGRErr OGRSXFDataSource::ReadSXFInformationFlags(VSILFILE* fpSXF, SXFPassport& passport)
{
    /* int nObjectsRead; */
    GByte val[4];
    /* nObjectsRead = */ VSIFReadL(&val, 4, 1, fpSXF);

    if (!(CHECK_BIT(val[0], 0) && CHECK_BIT(val[0], 1)))
    {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    if (CHECK_BIT(val[0], 2))
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

void OGRSXFDataSource::SetVertCS(const long iVCS, SXFPassport& passport)
{
    if (!CSLTestBoolean(CPLGetConfigOption("SXF_SET_VERTCS", "NO")))
        return;

    const long nEPSG = aoVCS[iVCS];

    if (nEPSG == 0)
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  CPLString().Printf("SXF. Vertical coordinate system (SXF index %ld) not supported", iVCS) );
        return;
    }

    OGRSpatialReference* sr = new OGRSpatialReference();
    OGRErr eImportFromEPSGErr = sr->importFromEPSG(nEPSG);
    if (eImportFromEPSGErr != OGRERR_NONE)
    {
        CPLError( CE_Warning, CPLE_None,
                  CPLString().Printf("SXF. Vertical coordinate system (SXF index %ld, EPSG %ld) import from EPSG error", iVCS, nEPSG) );
        return;
    }

    if (sr->IsVertical() != 1)
    {
        CPLError( CE_Warning, CPLE_None,
                  CPLString().Printf("SXF. Coordinate system (SXF index %ld, EPSG %ld) is not Vertical", iVCS, nEPSG) );
        return;
    }

    //passport.stMapDescription.pSpatRef->SetVertCS("Baltic", "Baltic Sea");
    OGRErr eSetVertCSErr = passport.stMapDescription.pSpatRef->SetVertCS(sr->GetAttrValue("VERT_CS"), sr->GetAttrValue("VERT_DATUM"));
    if (eSetVertCSErr != OGRERR_NONE)
    {
        CPLError( CE_Warning, CPLE_None,
                  CPLString().Printf("SXF. Vertical coordinate system (SXF index %ld, EPSG %ld) set error", iVCS, nEPSG) );
        return;
    }
}
OGRErr OGRSXFDataSource::ReadSXFMapDescription(VSILFILE* fpSXF, SXFPassport& passport)
{
    /* int nObjectsRead; */
    int i;
    passport.stMapDescription.Env.MaxX = -100000000;
    passport.stMapDescription.Env.MinX = 100000000;
    passport.stMapDescription.Env.MaxY = -100000000;
    passport.stMapDescription.Env.MinY = 100000000;

    bool bIsX = true;// passport.informationFlags.bRealCoordinatesCompliance; //if real coordinates we need to swap x & y

    //version specific
    if (passport.version == 3)
    {
        short nNoObjClass, nNoSemClass;
        /* nObjectsRead = */ VSIFReadL(&nNoObjClass, 2, 1, fpSXF);
        /* nObjectsRead = */ VSIFReadL(&nNoSemClass, 2, 1, fpSXF);
        GByte baMask[8];
        /* nObjectsRead = */ VSIFReadL(&baMask, 8, 1, fpSXF);

        int nCorners[8];

        //get projected corner coords
        /* nObjectsRead = */ VSIFReadL(&nCorners, 32, 1, fpSXF);

        for (i = 0; i < 8; i++)
        {
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
        /* nObjectsRead = */ VSIFReadL(&nCorners, 32, 1, fpSXF);

        for (i = 0; i < 8; i++)
        {
            passport.stMapDescription.stGeoCoords[i] = double(nCorners[i]) * 0.00000057295779513082; //from radians to degree * 100 000 000
        }
    }
    else if (passport.version == 4)
    {
        int nEPSG;
        /* nObjectsRead = */ VSIFReadL(&nEPSG, 4, 1, fpSXF);

        if (nEPSG != 0)
        {
            passport.stMapDescription.pSpatRef = new OGRSpatialReference();
            passport.stMapDescription.pSpatRef->importFromEPSG(nEPSG);
        }

        double dfCorners[8];
        /* nObjectsRead = */ VSIFReadL(&dfCorners, 64, 1, fpSXF);

        for (i = 0; i < 8; i++)
        {
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
        /* nObjectsRead = */ VSIFReadL(&dfCorners, 64, 1, fpSXF);

        for (i = 0; i < 8; i++)
        {
            passport.stMapDescription.stGeoCoords[i] = dfCorners[i] * TO_DEGREES; // to degree 
        }

    }

    if (NULL != passport.stMapDescription.pSpatRef)
    {
        return OGRERR_NONE;
    }

    GByte anData[8] = { 0 };
    /* nObjectsRead = */ VSIFReadL(&anData, 8, 1, fpSXF);
    long iEllips = anData[0];
    long iVCS = anData[1];
    long iProjSys = anData[2];
    /* long iDatum = anData[3]; Unused. */
    double dfProjScale = 1;

    double adfPrjParams[8] = { 0 };

    if (passport.version == 3)
    {
        switch (anData[5])
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


        VSIFSeekL(fpSXF, 212, SEEK_SET);
        struct _buff{
            GUInt32 nRes;
            GInt16 anFrame[8];
            GUInt32 nFrameCode;
        } buff;
        /* nObjectsRead = */ VSIFReadL(&buff, 20, 1, fpSXF);
        passport.stMapDescription.nResolution = buff.nRes; //resolution

        for (i = 0; i < 8; i++)
            passport.stMapDescription.stFrameCoords[i] = buff.anFrame[i];

        int anParams[5];
        /* nObjectsRead = */ VSIFReadL(&anParams, 20, 1, fpSXF);

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
        switch (anData[5])
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

        VSIFSeekL(fpSXF, 312, SEEK_SET);
        GUInt32 buff[10];
        /* nObjectsRead = */ VSIFReadL(&buff, 40, 1, fpSXF);

        passport.stMapDescription.nResolution = buff[0]; //resolution
        for (i = 0; i < 8; i++)
            passport.stMapDescription.stFrameCoords[i] = buff[1 + i];

        double adfParams[6];
        /* nObjectsRead = */ VSIFReadL(&adfParams, 48, 1, fpSXF);

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
    if (iEllips == 1 && iProjSys == 1) // Pulkovo 1942 / Gauss-Kruger
    {
        double dfCenterLongEnv = passport.stMapDescription.stGeoCoords[1] + fabs(passport.stMapDescription.stGeoCoords[5] - passport.stMapDescription.stGeoCoords[1]) / 2;

        int nZoneEnv = (dfCenterLongEnv + 3.0) / 6.0 + 0.5;

        if (nZoneEnv > 1 && nZoneEnv < 33)
        {
            int nEPSG = 28400 + nZoneEnv;
            passport.stMapDescription.pSpatRef = new OGRSpatialReference();
            OGRErr eErr = passport.stMapDescription.pSpatRef->importFromEPSG(nEPSG);
            SetVertCS(iVCS, passport);
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
        int nZoneEnv = 30 + (dfCenterLongEnv + 3.0) / 6.0 + 0.5;
        bool bNorth = passport.stMapDescription.stGeoCoords[6] + (passport.stMapDescription.stGeoCoords[2] - passport.stMapDescription.stGeoCoords[6]) / 2 < 0;
        int nEPSG;
        if (bNorth)
        {
            nEPSG = 32600 + nZoneEnv;
        }
        else
        {
            nEPSG = 32700 + nZoneEnv;
        }
        passport.stMapDescription.pSpatRef = new OGRSpatialReference();
        OGRErr eErr = passport.stMapDescription.pSpatRef->importFromEPSG(nEPSG);
        SetVertCS(iVCS, passport);
        return eErr;
    }
   else if (iEllips == 45 && iProjSys == 35) //Mercator 3395 on sphere wgs84
    {
        passport.stMapDescription.pSpatRef = new OGRSpatialReference("PROJCS[\"WGS_1984_Web_Mercator\",GEOGCS[\"GCS_WGS_1984_Major_Auxiliary_Sphere\",DATUM[\"WGS_1984_Major_Auxiliary_Sphere\",SPHEROID[\"WGS_1984_Major_Auxiliary_Sphere\",6378137.0,0.0]],PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\",0.0174532925199433]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"False_Easting\",0.0],PARAMETER[\"False_Northing\",0.0],PARAMETER[\"Central_Meridian\",0.0],PARAMETER[\"latitude_of_origin\",0.0],UNIT[\"Meter\",1.0]]");
        OGRErr eErr = OGRERR_NONE; //passport.stMapDescription.pSpatRef->importFromEPSG(3395);
        SetVertCS(iVCS, passport);
        return eErr;
    }
    else if (iEllips == 9 && iProjSys == 34) //Miller 54003
    {
        passport.stMapDescription.pSpatRef = new OGRSpatialReference();
        OGRErr eErr = passport.stMapDescription.pSpatRef->importFromEPSG(54003);
        SetVertCS(iVCS, passport);
        return eErr;
    }

    //TODO: Need to normalise more SRS:
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
    OGRErr eErr = passport.stMapDescription.pSpatRef->importFromPanorama(anData[2], anData[3], anData[0], adfPrjParams);
    SetVertCS(iVCS, passport);
    return eErr;
}

void OGRSXFDataSource::FillLayers()
{
    CPLDebug("SXF","Create layers");

    //2. Read all records (only classify code and offset) and add this to correspondence layer
    long nFID;
    int nObjectsRead = 0;
    size_t i;
    vsi_l_offset nOffset = 0, nOffsetSemantic;
    int nDeletedLayerIndex;

    //get record count
    GUInt32 nRecordCountMax = 0;
    if (oSXFPassport.version == 3)
    {
        VSIFSeekL(fpSXF, 288, SEEK_SET);
        nObjectsRead = VSIFReadL(&nRecordCountMax, 4, 1, fpSXF);
        nOffset = 300;
    }
    else if (oSXFPassport.version == 4)
    {
        VSIFSeekL(fpSXF, 440, SEEK_SET);
        nObjectsRead = VSIFReadL(&nRecordCountMax, 4, 1, fpSXF);
        nOffset = 452;
    }
    /* else nOffset and nObjectsRead will be 0 */

    if (nObjectsRead != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Get record count failed");
        return;
    }

    VSIFSeekL(fpSXF, nOffset, SEEK_SET);

    for (nFID = 0; nFID < nRecordCountMax; nFID++)
    {
        GInt32 buff[6];
        nObjectsRead = VSIFReadL(&buff, 24, 1, fpSXF);

        if (nObjectsRead != 1 || buff[0] != IDSXFOBJ)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Read record %ld failed", nFID);
            return;
        }

        bool bHasSemantic = CHECK_BIT(buff[5], 9);
        if (bHasSemantic) //check has attributes
        {
            //we have already 24 byte readed
            nOffsetSemantic = 8 + buff[2];
            VSIFSeekL(fpSXF, nOffsetSemantic, SEEK_CUR);
        }

        int nSemanticSize = buff[1] - 32 - buff[2];
        if( nSemanticSize < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid value");
            break;
        }
        for (i = 0; i < nLayers; i++)
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
    for (i = 0; i < nLayers; i++)
    {
        OGRSXFLayer* pOGRSXFLayer = (OGRSXFLayer*)papoLayers[i];
        if (pOGRSXFLayer && pOGRSXFLayer->GetFeatureCount() == 0)
        {
            delete pOGRSXFLayer;
            nDeletedLayerIndex = i;
            while (nDeletedLayerIndex < (int)(nLayers - 1))
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
    return NULL;
}

void OGRSXFDataSource::CreateLayers()
{
    //codes get from OSM.rsc http://gistoolkit.ru/download/classifiers/osm.zip

    //default layers set
    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    OGRSXFLayer* pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 0, CPLString("SYSTEM"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    //default codes
    for (size_t i = 1000000001; i < 1000000015; i++)
    {
        pLayer->AddClassifyCode(i);
    }
    pLayer->AddClassifyCode(91000000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 1, CPLString("boundary"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(81110000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 2, CPLString("water"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(31410000);
    pLayer->AddClassifyCode(31120000);
    pLayer->AddClassifyCode(31710000);
    pLayer->AddClassifyCode(72310000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 3, CPLString("city"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(41100000);
    pLayer->AddClassifyCode(91100001);
    pLayer->AddClassifyCode(91100002);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 4, CPLString("poi"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(123);
    pLayer->AddClassifyCode(32410000);
    pLayer->AddClassifyCode(44200000);
    pLayer->AddClassifyCode(44200010);
    pLayer->AddClassifyCode(47140000);
    pLayer->AddClassifyCode(51121000);
    pLayer->AddClassifyCode(51130000);
    pLayer->AddClassifyCode(51410000);
    pLayer->AddClassifyCode(51410001);
    pLayer->AddClassifyCode(51420000);
    pLayer->AddClassifyCode(53110000);
    pLayer->AddClassifyCode(53311400);
    pLayer->AddClassifyCode(53421000);
    pLayer->AddClassifyCode(53510000);
    pLayer->AddClassifyCode(53612000);
    pLayer->AddClassifyCode(53612100);
    pLayer->AddClassifyCode(53612101);
    pLayer->AddClassifyCode(53612102);
    pLayer->AddClassifyCode(53612103);
    pLayer->AddClassifyCode(53612104);
    pLayer->AddClassifyCode(53612105);
    pLayer->AddClassifyCode(53612106);
    pLayer->AddClassifyCode(53612107);
    pLayer->AddClassifyCode(53612200);
    pLayer->AddClassifyCode(53612201);
    pLayer->AddClassifyCode(53612202);
    pLayer->AddClassifyCode(53612203);
    pLayer->AddClassifyCode(53612204);
    pLayer->AddClassifyCode(53612205);
    pLayer->AddClassifyCode(53612211);
    pLayer->AddClassifyCode(53612300);
    pLayer->AddClassifyCode(53612301);
    pLayer->AddClassifyCode(53612302);
    pLayer->AddClassifyCode(53612303);
    pLayer->AddClassifyCode(53612304);
    pLayer->AddClassifyCode(53612400);
    pLayer->AddClassifyCode(53612401);
    pLayer->AddClassifyCode(53612402);
    pLayer->AddClassifyCode(53612403);
    pLayer->AddClassifyCode(53612404);
    pLayer->AddClassifyCode(53623000);
    pLayer->AddClassifyCode(53623100);
    pLayer->AddClassifyCode(53623110);
    pLayer->AddClassifyCode(53623200);
    pLayer->AddClassifyCode(53623300);
    pLayer->AddClassifyCode(53623400);
    pLayer->AddClassifyCode(53623500);
    pLayer->AddClassifyCode(53623600);
    pLayer->AddClassifyCode(53624000);
    pLayer->AddClassifyCode(53624001);
    pLayer->AddClassifyCode(53624002);
    pLayer->AddClassifyCode(53624003);
    pLayer->AddClassifyCode(53624004);
    pLayer->AddClassifyCode(53624005);
    pLayer->AddClassifyCode(53630000);
    pLayer->AddClassifyCode(53631000);
    pLayer->AddClassifyCode(53632101);
    pLayer->AddClassifyCode(53632102);
    pLayer->AddClassifyCode(53632103);
    pLayer->AddClassifyCode(53632104);
    pLayer->AddClassifyCode(53632105);
    pLayer->AddClassifyCode(53632106);
    pLayer->AddClassifyCode(53633001);
    pLayer->AddClassifyCode(53633101);
    pLayer->AddClassifyCode(53633102);
    pLayer->AddClassifyCode(53633112);
    pLayer->AddClassifyCode(53633114);
    pLayer->AddClassifyCode(53635000);
    pLayer->AddClassifyCode(53640000);
    pLayer->AddClassifyCode(53641000);
    pLayer->AddClassifyCode(53642000);
    pLayer->AddClassifyCode(53643000);
    pLayer->AddClassifyCode(53644000);
    pLayer->AddClassifyCode(53646011);
    pLayer->AddClassifyCode(53646013);
    pLayer->AddClassifyCode(53646014);
    pLayer->AddClassifyCode(53650000);
    pLayer->AddClassifyCode(53650001);
    pLayer->AddClassifyCode(53650002);
    pLayer->AddClassifyCode(53650003);
    pLayer->AddClassifyCode(53650004);
    pLayer->AddClassifyCode(53650006);
    pLayer->AddClassifyCode(53660000);
    pLayer->AddClassifyCode(53661001);
    pLayer->AddClassifyCode(53661002);
    pLayer->AddClassifyCode(53661003);
    pLayer->AddClassifyCode(53661004);
    pLayer->AddClassifyCode(53661005);
    pLayer->AddClassifyCode(53661006);
    pLayer->AddClassifyCode(53661007);
    pLayer->AddClassifyCode(53661008);
    pLayer->AddClassifyCode(53661009);
    pLayer->AddClassifyCode(53661010);
    pLayer->AddClassifyCode(53661021);
    pLayer->AddClassifyCode(53661100);
    pLayer->AddClassifyCode(53662001);
    pLayer->AddClassifyCode(53662002);
    pLayer->AddClassifyCode(53662003);
    pLayer->AddClassifyCode(53662004);
    pLayer->AddClassifyCode(53672600);
    pLayer->AddClassifyCode(53673300);
    pLayer->AddClassifyCode(53700000);
    pLayer->AddClassifyCode(53710000);
    pLayer->AddClassifyCode(53720100);
    pLayer->AddClassifyCode(53720200);
    pLayer->AddClassifyCode(53720300);
    pLayer->AddClassifyCode(53720301);
    pLayer->AddClassifyCode(53720400);
    pLayer->AddClassifyCode(53720500);
    pLayer->AddClassifyCode(53720510);
    pLayer->AddClassifyCode(53720520);
    pLayer->AddClassifyCode(53720970);
    pLayer->AddClassifyCode(53890000);


    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 5, CPLString("highway"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(10715);
    pLayer->AddClassifyCode(11118);
    pLayer->AddClassifyCode(2000253);
    pLayer->AddClassifyCode(2000727);
    pLayer->AddClassifyCode(51133200);
    pLayer->AddClassifyCode(51220000);
    pLayer->AddClassifyCode(61230000);
    pLayer->AddClassifyCode(61230000);
    pLayer->AddClassifyCode(62132000);
    pLayer->AddClassifyCode(62213100);
    pLayer->AddClassifyCode(62213101);
    pLayer->AddClassifyCode(62213102);
    pLayer->AddClassifyCode(62223000);
    pLayer->AddClassifyCode(62331000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 6, CPLString("railway"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(61111000);
    pLayer->AddClassifyCode(61121100);
    pLayer->AddClassifyCode(61121200);
    pLayer->AddClassifyCode(61122000);
    pLayer->AddClassifyCode(62131000);


    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 7, CPLString("building"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(44100000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 8, CPLString("landuse"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(97);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 9, CPLString("vegetation"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(71111111);
    pLayer->AddClassifyCode(71325000);
    pLayer->AddClassifyCode(53890000);
    pLayer->AddClassifyCode(22700000);
    pLayer->AddClassifyCode(32282000);
    pLayer->AddClassifyCode(71211000);
    pLayer->AddClassifyCode(72120000);
    pLayer->AddClassifyCode(71314000);
    pLayer->AddClassifyCode(71312000);


    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 10, CPLString("fire"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(96);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 11, CPLString("roaddesign"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(60000000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 13, CPLString("RoadStructure"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(62315000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    pLayer = new OGRSXFLayer(fpSXF, &hIOMutex, 14, CPLString("signature"), oSXFPassport.version, oSXFPassport.stMapDescription);
    papoLayers[nLayers] = pLayer;
    nLayers++;

    pLayer->AddClassifyCode(91100000);
    pLayer->AddClassifyCode(91200000);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
    papoLayers[nLayers] = new OGRSXFLayer(fpSXF, &hIOMutex, 255, CPLString("Not_Classified"), oSXFPassport.version, oSXFPassport.stMapDescription);
    nLayers++;

}

void OGRSXFDataSource::CreateLayers(VSILFILE* fpRSC)
{

    RSCHeader stRSCFileHeader;
    int nFileHeaderSize = sizeof(stRSCFileHeader);
    int nObjectsRead = VSIFReadL(&stRSCFileHeader, nFileHeaderSize, 1, fpRSC);

    if (nObjectsRead != 1)
    {
        CPLError(CE_Warning, CPLE_None, "RSC head read failed");
        return;
    }

    GByte szLayersID[4];
    struct _layer{
        GUInt32 nLength;
        char szName[32];
        char szShortName[16];
        GByte nNo;
        GByte nPos;
        GUInt16 nSematicCount;
    };

    int i;
    size_t nLayerStructSize = sizeof(_layer);

    VSIFSeekL(fpRSC, stRSCFileHeader.Layers.nOffset - sizeof(szLayersID), SEEK_SET);
    VSIFReadL(&szLayersID, sizeof(szLayersID), 1, fpRSC);
    vsi_l_offset nOffset = stRSCFileHeader.Layers.nOffset;
    _layer LAYER;

    for (i = 0; (GUInt32)i < stRSCFileHeader.Layers.nRecordCount; ++i)
    {
        VSIFReadL(&LAYER, nLayerStructSize, 1, fpRSC);

        papoLayers = (OGRLayer**)CPLRealloc(papoLayers, sizeof(OGRLayer*)* (nLayers + 1));
        bool bLayerFullName = CSLTestBoolean(CPLGetConfigOption("SXF_LAYER_FULLNAME", "NO"));

        char* pszRecoded;
        if (bLayerFullName)
        {
            if (stRSCFileHeader.nFontEnc == 125)
                pszRecoded = CPLRecode(LAYER.szName, "KOI8-R", CPL_ENC_UTF8);
            else if (stRSCFileHeader.nFontEnc == 126)
                pszRecoded = CPLRecode(LAYER.szName, "CP1251", CPL_ENC_UTF8);
            else
                pszRecoded = CPLStrdup(LAYER.szName);

            papoLayers[nLayers] = new OGRSXFLayer(fpSXF, &hIOMutex, LAYER.nNo, CPLString(pszRecoded), oSXFPassport.version, oSXFPassport.stMapDescription);
        }
        else
        {
            if (stRSCFileHeader.nFontEnc == 125)
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
        unsigned nObjectNumber;
        unsigned nObjectCode;
        char szShortName[32];
        char szName[32];
        char szGeomType;
        char szLayernNo;
        char szUnimportantSeg[14];
    };

    VSIFSeekL(fpRSC, stRSCFileHeader.Objects.nOffset - sizeof(szObjectsID), SEEK_SET);
    VSIFReadL(&szObjectsID, sizeof(szObjectsID), 1, fpRSC);
    nOffset = stRSCFileHeader.Objects.nOffset;
    _object OBJECT;

    for (unsigned i = 0; i < stRSCFileHeader.Objects.nRecordCount; ++i)
    {
        VSIFReadL(&OBJECT, sizeof(_object), 1, fpRSC);

        OGRSXFLayer* pLayer = GetLayerById(OBJECT.szLayernNo);
        if (NULL != pLayer)
        {
            char* pszRecoded;
            if (stRSCFileHeader.nFontEnc == 125)
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
