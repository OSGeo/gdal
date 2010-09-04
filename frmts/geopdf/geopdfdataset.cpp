/******************************************************************************
 * $Id$
 *
 * Project:  GeoPDF driver
 * Purpose:  GDALDataset driver for GeoPDF dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

/* poppler includes */
#include <Object.h>

#define private public /* Ugly! Page::pageObj is private but we need it... */
#include <Page.h>
#undef private

#include <Dict.h>
#include <Catalog.h>
#include <PDFDoc.h>
#include <splash/SplashBitmap.h>
#include <splash/Splash.h>
#include <SplashOutputDev.h>
#include <GlobalParams.h>

/* g++ -fPIC -g -Wall frmts/geopdf/geopdfdataset.cpp -shared -o gdal_GeoPDF.so -Iport -Igcore -Iogr -L. -lgdal -lpoppler -I/usr/include/poppler */

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_GeoPDF(void);
CPL_C_END

class ObjectAutoFree : public Object
{
public:
    ObjectAutoFree() {}
    ~ObjectAutoFree() { free(); }
};

/************************************************************************/
/* ==================================================================== */
/*                              GeoPDFDataset                              */
/* ==================================================================== */
/************************************************************************/

class GeoPDFRasterBand;

class GeoPDFDataset : public GDALPamDataset
{
    friend class GeoPDFRasterBand;
    char        *pszWKT;
    double       dfDPI;
    double       adfCTM[6];
    double       adfGeoTransform[6];
    PDFDoc*      poDoc;

    double       dfMaxArea;
    int          ParseLGIDictObject(Object& oLGIDict);
    int          ParseLGIDictDict(Dict* poLGIDict);
    int          ParseProjDict(Dict* poProjDict);
    int          ParseVP(Object& oVP);

    int          bTried;
    GByte       *pabyData;

  public:
                 GeoPDFDataset();
    virtual     ~GeoPDFDataset();

    virtual const char* GetProjectionRef();
    virtual CPLErr GetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GeoPDFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GeoPDFRasterBand : public GDALPamRasterBand
{
    friend class GeoPDFDataset;

  public:

                GeoPDFRasterBand( GeoPDFDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           GeoPDFRasterBand()                            */
/************************************************************************/

GeoPDFRasterBand::GeoPDFRasterBand( GeoPDFDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GeoPDFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    GeoPDFDataset *poGDS = (GeoPDFDataset *) poDS;

    if (poGDS->bTried == FALSE)
    {
        poGDS->bTried = TRUE;
        poGDS->pabyData = (GByte*)VSIMalloc3(3, nRasterXSize, nRasterYSize);
        if (poGDS->pabyData == NULL)
            return CE_Failure;

        /* poppler global variable */
        if (globalParams == NULL)
            globalParams = new GlobalParams();

        SplashColor sColor;
        sColor[0] = 255;
        sColor[1] = 255;
        sColor[2] = 255;
        SplashOutputDev *poSplashOut;
        poSplashOut = new SplashOutputDev(splashModeRGB8, 4, gFalse, sColor);
        poSplashOut->startDoc(poGDS->poDoc->getXRef());
        Page* poPage = poGDS->poDoc->getCatalog()->getPage(1);
        double dfDPI = poGDS->dfDPI;
        poGDS->poDoc->displayPageSlice(poSplashOut,
                                       1,
                                       dfDPI, dfDPI,
                                       0,
                                       TRUE, gFalse, gFalse,
                                       0, 0,
                                       poPage->getMediaWidth() * dfDPI / 72,
                                       poPage->getMediaHeight() * dfDPI / 72);
        SplashBitmap* poBitmap = poSplashOut->getBitmap();
        if (poBitmap->getWidth() != nRasterXSize || poBitmap->getHeight() != nRasterYSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bitmap decoded size (%dx%d) doesn't match raster size (%dx%d)" ,
                     poBitmap->getWidth(), poBitmap->getHeight(),
                     nRasterXSize, nRasterYSize);
            VSIFree(poGDS->pabyData);
            poGDS->pabyData = NULL;
            delete poSplashOut;
            return CE_Failure;
        }

        int nRowSize = poBitmap->getRowSize();
        GByte* pabyBitmap = poBitmap->getDataPtr();
        int i, j;
        GByte* pabyDataR = poGDS->pabyData;
        GByte* pabyDataG = poGDS->pabyData + nRasterXSize * nRasterYSize;
        GByte* pabyDataB = poGDS->pabyData + 2 * nRasterXSize * nRasterYSize;
        for(j=0;j<nRasterYSize;j++)
        {
            for(i=0;i<nRasterXSize;i++)
            {
                pabyDataR[j * nRasterXSize + i] = pabyBitmap[j * nRowSize + i * 3];
                pabyDataG[j * nRasterXSize + i] = pabyBitmap[j * nRowSize + i * 3 + 1];
                pabyDataB[j * nRasterXSize + i] = pabyBitmap[j * nRowSize + i * 3 + 2];
            }
        }
        delete poSplashOut;
    }
    if (poGDS->pabyData == NULL)
        return CE_Failure;

    memcpy(pImage,
           poGDS->pabyData + (nBand - 1) * nRasterXSize * nRasterYSize + nBlockYOff * nRasterXSize,
           nRasterXSize);

    return CE_None;
}

/************************************************************************/
/*                            ~GeoPDFDataset()                            */
/************************************************************************/

GeoPDFDataset::GeoPDFDataset()
{
    poDoc = NULL;
    pszWKT = NULL;
    dfMaxArea = 0;
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = -1;
    bTried = FALSE;
    pabyData = NULL;
}

/************************************************************************/
/*                            ~GeoPDFDataset()                            */
/************************************************************************/

GeoPDFDataset::~GeoPDFDataset()
{
    CPLFree(pszWKT);
    CPLFree(pabyData);
    delete poDoc;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int GeoPDFDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if (poOpenInfo->nHeaderBytes < 128)
        return FALSE;

    return strncmp((const char*)poOpenInfo->pabyHeader, "%PDF", 4) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GeoPDFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo))
        return NULL;

    if (poOpenInfo->fp == NULL)
        return NULL;

    /* TODO: use alternate constructor to support virtual I/O */
    PDFDoc* poDoc = new PDFDoc(new GooString(poOpenInfo->pszFilename));
    if ( !poDoc->isOk() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
        delete poDoc;
        return NULL;
    }

    if ( poDoc->getNumPages() != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Currently, we only support 1-page PDF");
        delete poDoc;
        return NULL;
    }

    Catalog* poCatalog = poDoc->getCatalog();
    if ( poCatalog == NULL || !poCatalog->isOk() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid catalog");
        delete poDoc;
        return NULL;
    }

    Page* poPage = poCatalog->getPage(1);
    if ( poPage == NULL || !poPage->isOk() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid page");
        delete poDoc;
        return NULL;
    }

    /* Here's the dirty part: this is a private member */
    /* so we had to #define private public to get it ! */
    Object& oPageObj = poPage->pageObj;
    if ( !oPageObj.isDict() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : !oPageObj.isDict()");
        delete poDoc;
        return NULL;
    }

    Dict* poPageDict = oPageObj.getDict();
    if ( poPageDict == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : poPageDict == NULL");
        delete poDoc;
        return NULL;
    }

    GeoPDFDataset* poDS = new GeoPDFDataset();
    poDS->poDoc = poDoc;
    poDS->dfDPI = atof(CPLGetConfigOption("GDAL_GEOPDF_DPI", "72"));
    if (poDS->dfDPI < 1 || poDS->dfDPI > 7200)
        poDS->dfDPI = 72;

    PDFRectangle* psMediaBox = poPage->getMediaBox();
    double dfX1 = psMediaBox->x1;
    double dfY1 = psMediaBox->y1;
    double dfX2 = psMediaBox->x2;
    double dfY2 = psMediaBox->y2;

    poDS->nRasterXSize = (dfX2 - dfX1) / 72 * poDS->dfDPI;
    poDS->nRasterYSize = (dfY2 - dfY1) / 72 * poDS->dfDPI;

    ObjectAutoFree oLGIDict;
    ObjectAutoFree oVP;
    if ( poPageDict->lookup((char*)"LGIDict",&oLGIDict) != NULL && !oLGIDict.isNull())
    {
        /* Cf 08-139r2_GeoPDF_Encoding_Best_Practice_Version_2.2.pdf */
        CPLDebug("GeoPDF", "TerraGo/OGC GeoPDF style GeoPDF detected");
        if (poDS->ParseLGIDictObject(oLGIDict))
        {
            double dfPixelPerPt = poDS->nRasterXSize / (dfX2 - dfX1);
            poDS->adfGeoTransform[0] = poDS->adfCTM[4] + poDS->adfCTM[0] * dfX1 + poDS->adfCTM[2] * dfY2;
            poDS->adfGeoTransform[1] = poDS->adfCTM[0] / dfPixelPerPt;
            poDS->adfGeoTransform[2] = poDS->adfCTM[1] / dfPixelPerPt;
            poDS->adfGeoTransform[3] = poDS->adfCTM[5] + poDS->adfCTM[1] * dfX1 + poDS->adfCTM[3] * dfY2;
            poDS->adfGeoTransform[4] = - poDS->adfCTM[2] / dfPixelPerPt;
            poDS->adfGeoTransform[5] = - poDS->adfCTM[3] / dfPixelPerPt;
        }
    }
    else if ( poPageDict->lookup((char*)"VP",&oVP) != NULL && !oVP.isNull())
    {
        /* Cf adobe_supplement_iso32000.pdf */
        CPLDebug("GeoPDF", "Adobe ISO3200 style GeoPDF detected");
        poDS->ParseVP(oVP);
    }
    else
    {
        /* Not a GeoPDF doc */
    }

    int iBand;
    for(iBand = 1; iBand <= 3; iBand ++)
        poDS->SetBand(iBand, new GeoPDFRasterBand(poDS, iBand));

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
    return( poDS );
}

/************************************************************************/
/*                       ParseLGIDictObject()                           */
/************************************************************************/

int GeoPDFDataset::ParseLGIDictObject(Object& oLGIDict)
{
    int i;
    int bOK = FALSE;
    if (oLGIDict.isArray())
    {
        int nArrayLength = oLGIDict.arrayGetLength();
        for (i=0; i<nArrayLength; i++)
        {
            ObjectAutoFree oArrayElt;
            if ( oLGIDict.arrayGet(i,&oArrayElt) == NULL ||
                 !oArrayElt.isDict() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "LGIDict[%d] is not a dictionary", i);
                return FALSE;
            }
            bOK |= ParseLGIDictDict(oArrayElt.getDict());
        }
    }
    else if (oLGIDict.isDict())
    {
        bOK = ParseLGIDictDict(oLGIDict.getDict());
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "LGIDict is of type %s", oLGIDict.getTypeName());
    }

    if (!bOK)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                            GetValue()                                */
/************************************************************************/

static double GetValue(Object& o, int nIndice = -1)
{
    if (o.isArray() && nIndice >= 0)
    {
        ObjectAutoFree oElt;
        if (o.arrayGet(nIndice, &oElt) == NULL)
            return 0;
        return GetValue(oElt);
    }
    else if (o.isInt())
        return o.getInt();
    else if (o.isReal())
        return o.getReal();
    else if (o.isString())
    {
        const char* pszStr = o.getString()->getCString();
        int nLen = strlen(pszStr);
        /* cf Military_Installations_2008.pdf that has values like "96 0 0.0W" */
        char chLast = pszStr[nLen-1];
        if (chLast == 'W' || chLast == 'E' || chLast == 'N' || chLast == 'S')
        {
            double dfDeg = atof(pszStr);
            double dfMin = 0, dfSec = 0;
            const char* pszNext = strchr(pszStr, ' ');
            if (pszNext)
                pszNext ++;
            if (pszNext)
                dfMin = atof(pszNext);
            if (pszNext)
                pszNext = strchr(pszNext, ' ');
            if (pszNext)
                pszNext ++;
            if (pszNext)
                dfSec = atof(pszNext);
            double dfVal = dfDeg + dfMin / 60 + dfSec / 3600;
            if (chLast == 'W' || chLast == 'S')
                return -dfVal;
            else
                return dfVal;
        }
        return atof(pszStr);
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Unexpected type : %s",
                 o.getTypeName());
        return 0;
    }
}

/************************************************************************/
/*                            GetValue()                                */
/************************************************************************/

static double GetValue(Dict* poDict, const char* pszName)
{
    ObjectAutoFree o;
    if (poDict->lookup((char*)pszName, &o) != NULL)
        return GetValue(o);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Cannot find parameter %s", pszName);
    return 0;
}

/************************************************************************/
/*                       ParseLGIDictDict()                             */
/************************************************************************/

int GeoPDFDataset::ParseLGIDictDict(Dict* poLGIDict)
{
    int i;

    if (poLGIDict == NULL)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Extract Type attribute                                          */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oType;
    if (poLGIDict->lookup((char*)"Type",&oType) == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Type of LGIDict object");
        return FALSE;
    }

    if ( !oType.isName() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid type for Type of LGIDict object");
        return FALSE;
    }

    if ( strcmp(oType.getName(), "LGIDict") != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for Type of LGIDict object : %s",
                 oType.getName());
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Version attribute                                       */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oVersion;
    if (poLGIDict->lookup((char*)"Version",&oVersion) == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Version of LGIDict object");
        return FALSE;
    }

    if (oVersion.isString())
    {
        /* OGC GeoPDF is 2.1 */
        CPLDebug("GeoPDF", "LGIDict Version : %s",
                 oVersion.getString()->getCString());
    }
    else if (oVersion.isInt())
    {
        /* Old TerraGo is 2 */
        CPLDebug("GeoPDF", "LGIDict Version : %d",
                 oVersion.getInt());
    }

/* -------------------------------------------------------------------- */
/*      Extract Neatline attribute                                      */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oNeatline;
    if (poLGIDict->lookup((char*)"Neatline", &oNeatline) != NULL &&
        oNeatline.isArray() )
    {
        int nLength = oNeatline.arrayGetLength();
        if ( (nLength % 2) != 0 || nLength < 4 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid length for Neatline");
            return FALSE;
        }

        double dfMinX = 0, dfMinY = 0, dfMaxX = 0, dfMaxY = 0;
        for(i=0;i<nLength;i+=2)
        {
            double dfX = GetValue(oNeatline, i);
            double dfY = GetValue(oNeatline, i + 1);
            if (i == 0 || dfX < dfMinX) dfMinX = dfX;
            if (i == 0 || dfY < dfMinY) dfMinY = dfY;
            if (i == 0 || dfX > dfMaxX) dfMaxX = dfX;
            if (i == 0 || dfY > dfMaxY) dfMaxY = dfY;
        }
        double dfArea = (dfMaxX - dfMinX) * (dfMaxY - dfMinY);
        if (dfArea < dfMaxArea)
        {
            CPLDebug("GeoPDF", "Not the larger neatline. Skipping it");
            return TRUE;
        }
        CPLDebug("GeoPDF", "This is a the larger neatline for now");
        dfMaxArea = dfArea;
    }

/* -------------------------------------------------------------------- */
/*      Extract CTM attribute                                           */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oCTM;
    int bHasCTM = FALSE;
    if (poLGIDict->lookup((char*)"CTM", &oCTM) != NULL &&
        oCTM.isArray())
    {
        int nLength = oCTM.arrayGetLength();
        if ( nLength != 6 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid length for CTM");
            return FALSE;
        }

        bHasCTM = TRUE;
        for(i=0;i<nLength;i++)
        {
            adfCTM[i] = GetValue(oCTM, i);
            CPLDebug("GeoPDF", "CTM[%d] = %f", i, adfCTM[i]);
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract Registration attribute                                  */
/* -------------------------------------------------------------------- */
    if (!bHasCTM)
    {
        ObjectAutoFree oRegistration;
        if (poLGIDict->lookup((char*)"Registration", &oRegistration) != NULL)
        {
            /* TODO */
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Registration unhandled for now");
            return FALSE;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Neither CTM nor Registration found");
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract Projection attribute                                    */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oProjection;
    if (poLGIDict->lookup((char*)"Projection", &oProjection) == NULL ||
        !oProjection.isDict())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not find Projection");
        return FALSE;
    }

    return ParseProjDict(oProjection.getDict());
}

/************************************************************************/
/*                         ParseProjDict()                               */
/************************************************************************/

int GeoPDFDataset::ParseProjDict(Dict* poProjDict)
{
    if (poProjDict == NULL)
        return FALSE;
    OGRSpatialReference oSRS;

/* -------------------------------------------------------------------- */
/*      Extract Type attribute                                          */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oType;
    if (poProjDict->lookup((char*)"Type",&oType) == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Type of Projection object");
        return FALSE;
    }

    if ( !oType.isName() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid type for Type of Projection object");
        return FALSE;
    }

    if ( strcmp(oType.getName(), "Projection") != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for Type of Projection object : %s",
                 oType.getName());
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Datum attribute                                         */
/* -------------------------------------------------------------------- */
    int bIsWGS84 = FALSE;
    int bIsNAD83 = FALSE;

    ObjectAutoFree oDatum;
    if (poProjDict->lookup((char*)"Datum",&oDatum) != NULL)
    {
        if (oDatum.isString())
        {
            char* pszDatum = oDatum.getString()->getCString();
            if (EQUAL(pszDatum, "WE") || EQUAL(pszDatum, "WGE"))
            {
                bIsWGS84 = TRUE;
                oSRS.SetWellKnownGeogCS("WGS84");
            }
            else if (EQUAL(pszDatum, "NAR"))
            {
                bIsNAD83 = TRUE;
                oSRS.SetWellKnownGeogCS("NAD83");
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Unhandled (yet) value for Datum : %s",
                        pszDatum);
            }
        }
        else if (oDatum.isDict())
        {
            /* TODO */
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract Hemisphere attribute                                    */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oHemisphere;
    CPLString osHemisphere;
    if (poProjDict->lookup((char*)"Hemisphere",&oHemisphere) != NULL &&
        oHemisphere.isString())
    {
        osHemisphere = oHemisphere.getString()->getCString();
    }

/* -------------------------------------------------------------------- */
/*      Extract ProjectionType attribute                                */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oProjectionType;
    if (poProjDict->lookup((char*)"ProjectionType",&oProjectionType) == NULL ||
        !oProjectionType.isString())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find ProjectionType of Projection object");
        return FALSE;
    }
    CPLString osProjectionType(oProjectionType.getString()->getCString());
    CPLDebug("GeoPDF", "Projection.ProjectionType = %s", osProjectionType.c_str());

    if (EQUAL(osProjectionType, "TC"))
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfScale = GetValue(poProjDict, "ScaleFactor");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        if (dfCenterLat == 0.0 && dfScale == 0.9996 && dfFalseEasting == 500000 &&
            (dfFalseNorthing == 0.0 || dfFalseNorthing == 10000000.0))
        {
            int nZone = (int) floor( (dfCenterLong + 180.0) / 6.0 ) + 1;
            int bNorth = dfFalseNorthing == 0;
            if (bIsWGS84)
                oSRS.importFromEPSG( ((bNorth) ? 32600 : 32700) + nZone );
            else if (bIsNAD83 && bNorth)
                oSRS.importFromEPSG( 26900 + nZone );
            else
                oSRS.SetUTM( nZone, bNorth );
        }
        else
        {
            oSRS.SetTM( dfCenterLat, dfCenterLong,
                        dfScale,
                        dfFalseEasting, dfFalseNorthing );
        }
    }
    else if (EQUAL(osProjectionType, "LE"))
    {
        double dfStdP1 = GetValue(poProjDict, "StandardParallelOne");
        double dfStdP2 = GetValue(poProjDict, "StandardParallelTwo");
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetLCC( dfStdP1, dfStdP2,
                     dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }
    else if (EQUAL(osProjectionType, "PH"))
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetPolyconic( dfCenterLat, dfCenterLong,
                           dfFalseEasting, dfFalseNorthing );
    }
    else if (EQUAL(osProjectionType, "UT"))
    {
        int nZone = (int)GetValue(poProjDict, "Zone");
        int bNorth = EQUAL(osHemisphere, "N");
        if (bIsWGS84)
            oSRS.importFromEPSG( ((bNorth) ? 32600 : 32700) + nZone );
        else
            oSRS.SetUTM( nZone, bNorth );
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled (yet) value for ProjectionType : %s",
                 osProjectionType.c_str());
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Units attribute                                         */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oUnits;
    CPLString osUnits;
    if (poProjDict->lookup((char*)"Units",&oUnits) != NULL &&
        oUnits.isString())
    {
        osUnits = oUnits.getString()->getCString();
        CPLDebug("GeoPDF", "Projection.Units = %s", osUnits.c_str());

        if (EQUAL(osUnits, "FT"))
            oSRS.SetLinearUnits( "Foot", 0.3048 );
    }

/* -------------------------------------------------------------------- */
/*      Export SpatialRef                                               */
/* -------------------------------------------------------------------- */
    CPLFree(pszWKT);
    pszWKT = NULL;
    if (oSRS.exportToWkt(&pszWKT) != OGRERR_NONE)
    {
        CPLFree(pszWKT);
        pszWKT = NULL;
    }

    return TRUE;
}

/************************************************************************/
/*                              ParseVP()                               */
/************************************************************************/

int GeoPDFDataset::ParseVP(Object& oVP)
{
    int i;

    if (!oVP.isArray())
        return FALSE;

    int nLength = oVP.arrayGetLength();
    CPLDebug("GeoPDF", "VP length = %d", nLength);
    if (nLength < 1)
        return FALSE;

    ObjectAutoFree oVPElt;
    if ( !oVP.arrayGet(0,&oVPElt) || !oVPElt.isDict() )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Measure attribute                                       */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oMeasure;
    if( !oVPElt.dictLookup((char*)"Measure",&oMeasure) ||
        !oMeasure.isDict() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Measure object");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Subtype attribute                                       */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oSubtype;
    if( !oMeasure.dictLookup((char*)"Subtype",&oSubtype) ||
        !oSubtype.isName() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Subtype object");
        return FALSE;
    }

    CPLDebug("GeoPDF", "Subtype = %s", oSubtype.getName());

/* -------------------------------------------------------------------- */
/*      Extract Bounds attribute                                       */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oBounds;
    if( !oMeasure.dictLookup((char*)"Bounds",&oBounds) ||
        !oBounds.isArray() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Bounds object");
        return FALSE;
    }

    int nBoundsLength = oBounds.arrayGetLength();
    if (nBoundsLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for Bounds object");
        return FALSE;
    }

    double adfBounds[8];
    for(i=0;i<8;i++)
    {
        adfBounds[i] = GetValue(oBounds, i);
        CPLDebug("GeoPDF", "Bounds[%d] = %f", i, adfBounds[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract GPTS attribute                                          */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oGPTS;
    if( !oMeasure.dictLookup((char*)"GPTS",&oGPTS) ||
        !oGPTS.isArray() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GPTS object");
        return FALSE;
    }

    int nGPTSLength = oGPTS.arrayGetLength();
    if (nGPTSLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for GPTS object");
        return FALSE;
    }

    double adfGPTS[8];
    for(i=0;i<8;i++)
    {
        adfGPTS[i] = GetValue(oGPTS, i);
        CPLDebug("GeoPDF", "GPTS[%d] = %f", i, adfGPTS[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract LPTS attribute                                          */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oLPTS;
    if( !oMeasure.dictLookup((char*)"LPTS",&oLPTS) ||
        !oLPTS.isArray() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find LPTS object");
        return FALSE;
    }

    int nLPTSLength = oLPTS.arrayGetLength();
    if (nLPTSLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for LPTS object");
        return FALSE;
    }

    double adfLPTS[8];
    for(i=0;i<8;i++)
    {
        adfLPTS[i] = GetValue(oLPTS, i);
        CPLDebug("GeoPDF", "LPTS[%d] = %f", i, adfLPTS[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract GCS attribute                                           */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oGCS;
    if( !oMeasure.dictLookup((char*)"GCS",&oGCS) ||
        !oGCS.isDict() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS object");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract GCS.Type attribute                                      */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oGCSType;
    if( !oGCS.dictLookup((char*)"Type",&oGCSType) ||
        !oGCSType.isName() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS.Type object");
        return FALSE;
    }

    CPLDebug("GeoPDF", "GCS.Type = %s", oGCSType.getName());

/* -------------------------------------------------------------------- */
/*      Extract GCS.WKT attribute                                      */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oGCSWKT;
    if( !oGCS.dictLookup((char*)"WKT",&oGCSWKT) ||
        !oGCSWKT.isString() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS.WKT object");
        return FALSE;
    }

    CPLDebug("GeoPDF", "GCS.WKT = %s", oGCSWKT.getString()->getCString());


/* -------------------------------------------------------------------- */
/*      Extract PointData attribute                                     */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oPointData;
    if( oVPElt.dictLookup((char*)"PtData",&oPointData) &&
        oPointData.isDict() )
    {
        CPLDebug("GeoPDF", "Found PointData");
    }

    /* TODO */

    return FALSE;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* GeoPDFDataset::GetProjectionRef()
{
    if (pszWKT)
        return pszWKT;
    return "";
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GeoPDFDataset::GetGeoTransform( double * padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, 6 * sizeof(double));

    return( CE_None );
}

/************************************************************************/
/*                         GDALRegister_GeoPDF()                           */
/************************************************************************/

void GDALRegister_GeoPDF()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "GeoPDF" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GeoPDF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "GeoPDF" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_geopdf.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "pdf" );

        //poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = GeoPDFDataset::Open;
        poDriver->pfnIdentify = GeoPDFDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

