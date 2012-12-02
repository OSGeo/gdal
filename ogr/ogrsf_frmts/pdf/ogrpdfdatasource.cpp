/******************************************************************************
 * $Id$
 *
 * Project:  PDF Translator
 * Purpose:  Implements OGRPDFDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_pdf.h"
#include "ogr_p.h"
#include "cpl_conv.h"

#include "pdfdataset.h"
#include "pdfcreatecopy.h"

#include "memdataset.h"

#define SQUARE(x) ((x)*(x))
#define EPSILON 1e-5

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRPDFLayer()                             */
/************************************************************************/

OGRPDFLayer::OGRPDFLayer( OGRPDFDataSource* poDS,
                          const char * pszName,
                          OGRSpatialReference *poSRS,
                          OGRwkbGeometryType eGeomType ) :
                                OGRMemLayer(pszName, poSRS, eGeomType )
{
    this->poDS = poDS;
    bGeomTypeSet = FALSE;
    bGeomTypeMixed = FALSE;
}

/************************************************************************/
/*                            CreateFeature()                           */
/************************************************************************/

OGRErr OGRPDFLayer::CreateFeature( OGRFeature *poFeature )
{
    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( !bGeomTypeMixed && poGeom != NULL )
    {
        if (!bGeomTypeSet)
        {
            bGeomTypeSet = TRUE;
            GetLayerDefn()->SetGeomType(poGeom->getGeometryType());
        }
        else if (GetLayerDefn()->GetGeomType() != poGeom->getGeometryType())
        {
            bGeomTypeMixed = TRUE;
            GetLayerDefn()->SetGeomType(wkbUnknown);
        }
    }

    poDS->SetModified();
    return OGRMemLayer::CreateFeature(poFeature);
}

/************************************************************************/
/*                              Fill()                                  */
/************************************************************************/

void OGRPDFLayer::Fill( GDALPDFArray* poArray )
{
    for(int i=0;i<poArray->GetLength();i++)
    {
        GDALPDFObject* poFeatureObj = poArray->Get(i);
        if (poFeatureObj->GetType() != PDFObjectType_Dictionary)
            continue;

        GDALPDFObject* poA = poFeatureObj->GetDictionary()->Get("A");
        if (!(poA != NULL && poA->GetType() == PDFObjectType_Dictionary))
            continue;

        GDALPDFObject* poP = poA->GetDictionary()->Get("P");
        if (!(poP != NULL && poP->GetType() == PDFObjectType_Array))
            continue;

        GDALPDFObject* poK = poFeatureObj->GetDictionary()->Get("K");
        int nK = -1;
        if (poK != NULL && poK->GetType() == PDFObjectType_Int)
            nK = poK->GetInt();

        GDALPDFArray* poPArray = poP->GetArray();
        int j;
        for(j = 0;j<poPArray->GetLength();j++)
        {
            GDALPDFObject* poKV = poPArray->Get(j);
            if (poKV->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFObject* poN = poKV->GetDictionary()->Get("N");
                GDALPDFObject* poV = poKV->GetDictionary()->Get("V");
                if (poN != NULL && poN->GetType() == PDFObjectType_String &&
                    poV != NULL)
                {
                    int nIdx = GetLayerDefn()->GetFieldIndex( poN->GetString().c_str() );
                    OGRFieldType eType = OFTString;
                    if (poV->GetType() == PDFObjectType_Int)
                        eType = OFTInteger;
                    else if (poV->GetType() == PDFObjectType_Real)
                        eType = OFTReal;
                    if (nIdx < 0)
                    {
                        OGRFieldDefn oField(poN->GetString().c_str(), eType);
                        CreateField(&oField);
                    }
                    else if (GetLayerDefn()->GetFieldDefn(nIdx)->GetType() != eType &&
                                GetLayerDefn()->GetFieldDefn(nIdx)->GetType() != OFTString)
                    {
                        OGRFieldDefn oField(poN->GetString().c_str(), OFTString);
                        AlterFieldDefn( nIdx, &oField, ALTER_TYPE_FLAG );
                    }
                }
            }
        }

        OGRFeature* poFeature = new OGRFeature(GetLayerDefn());
        for(j = 0;j<poPArray->GetLength();j++)
        {
            GDALPDFObject* poKV = poPArray->Get(j);
            if (poKV->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFObject* poN = poKV->GetDictionary()->Get("N");
                GDALPDFObject* poV = poKV->GetDictionary()->Get("V");
                if (poN != NULL && poN->GetType() == PDFObjectType_String &&
                    poV != NULL)
                {
                    if (poV->GetType() == PDFObjectType_String)
                        poFeature->SetField(poN->GetString().c_str(), poV->GetString().c_str());
                    else if (poV->GetType() == PDFObjectType_Int)
                        poFeature->SetField(poN->GetString().c_str(), poV->GetInt());
                    else if (poV->GetType() == PDFObjectType_Real)
                        poFeature->SetField(poN->GetString().c_str(), poV->GetReal());
                }
            }
        }

        if (nK >= 0)
        {
            OGRGeometry* poGeom = poDS->GetGeometryFromMCID(nK);
            if (poGeom)
            {
                poGeom->assignSpatialReference(GetSpatialRef());
                poFeature->SetGeometry(poGeom);
            }
        }

        CreateFeature(poFeature);

        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPDFLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    else
        return OGRMemLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                          OGRPDFDataSource()                          */
/************************************************************************/

OGRPDFDataSource::OGRPDFDataSource()

{
    pszName = NULL;
    papszOptions = NULL;

    nLayers = 0;
    papoLayers = NULL;

    bModified = FALSE;
    bWritable = FALSE;

    poGDAL_DS = NULL;
    poPageObj = NULL;
    poCatalogObj = NULL;
    dfPageWidth = dfPageHeight = 0;

    bSetStyle = CSLTestBoolean(CPLGetConfigOption("OGR_PDF_SET_STYLE", "YES"));

    InitMapOperators();
}

/************************************************************************/
/*                         ~OGRPDFDataSource()                          */
/************************************************************************/

OGRPDFDataSource::~OGRPDFDataSource()

{
    SyncToDisk();

    CleanupIntermediateResources();

    CPLFree( pszName );
    CSLDestroy( papszOptions );

    for(int i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                   CleanupIntermediateResources()                     */
/************************************************************************/

void OGRPDFDataSource::CleanupIntermediateResources()
{
    std::map<int,OGRGeometry*>::iterator oMapIter = oMapMCID.begin();
    for( ; oMapIter != oMapMCID.end(); ++oMapIter)
        delete oMapIter->second;
    oMapMCID.erase(oMapMCID.begin(), oMapMCID.end());

    delete poGDAL_DS;
    poGDAL_DS = NULL;

    poPageObj = NULL;
    poCatalogObj = NULL;
}

/************************************************************************/
/*                          InitMapOperators()                          */
/************************************************************************/

typedef struct
{
    char        szOpName[4];
    char        nArgs;
} PDFOperator;

static const PDFOperator asPDFOperators [] =
{
    { "b", 0 },
    { "B", 0 },
    { "b*", 0 },
    { "B*", 0 },
    { "BDC", 2 },
    // BI
    { "BMC", 1 },
    // BT
    { "BX", 0 },
    { "c", 6 },
    { "cm", 6 },
    { "CS", 1 },
    { "cs", 1 },
    { "d", 1 }, /* we have ignored the first arg */
    // d0
    // d1
    { "Do", 1 },
    { "DP", 2 },
    // EI
    { "EMC", 0 },
    // ET
    { "EX", 0 },
    { "f", 0 },
    { "F", 0 },
    { "f*", 0 },
    { "G", 1 },
    { "g", 1 },
    { "gs", 1 },
    { "h", 0 },
    { "i", 1 },
    // ID
    { "j", 1 },
    { "J", 1 },
    { "K", 4 },
    { "k", 4 },
    { "l", 2 },
    { "m", 2 },
    { "M", 1 },
    { "MP", 1 },
    { "n", 0 },
    { "q", 0 },
    { "Q", 0 },
    { "re", 4 },
    { "RG", 3 },
    { "rg", 3 },
    { "ri", 1 },
    { "s", 0 },
    { "S", 0 },
    { "SC", -1 },
    { "sc", -1 },
    { "SCN", -1 },
    { "scn", -1 },
    { "sh", 1 },
    // T*
    // Tc
    // Td
    // TD
    // Tf
    // Tj
    // TJ
    // TL
    // Tm
    // Tr
    // Ts
    // Tw
    // Tz
    { "v", 4 },
    { "w", 1 },
    { "W", 0 },
    { "W*", 0 },
    { "y", 4 },
    // '
    // "
};

void OGRPDFDataSource::InitMapOperators()
{
    for(size_t i=0;i<sizeof(asPDFOperators) / sizeof(asPDFOperators[0]); i++)
        oMapOperators[asPDFOperators[i].szOpName] = asPDFOperators[i].nArgs;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPDFDataSource::TestCapability( const char * pszCap )

{
    if( bWritable && EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPDFDataSource::GetLayer( int iLayer )

{
    if (iLayer < 0 || iLayer >= nLayers)
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int OGRPDFDataSource::GetLayerCount()
{
    return nLayers;
}

/************************************************************************/
/*                            ExploreTree()                             */
/************************************************************************/

void OGRPDFDataSource::ExploreTree(GDALPDFObject* poObj)
{
    if (poObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFDictionary* poDict = poObj->GetDictionary();

    GDALPDFObject* poS = poDict->Get("S");
    CPLString osS;
    if (poS != NULL && poS->GetType() == PDFObjectType_Name)
    {
        osS = poS->GetName();
    }

    GDALPDFObject* poT = poDict->Get("T");
    CPLString osT;
    if (poT != NULL && poT->GetType() == PDFObjectType_String)
    {
        osT = poT->GetString();
    }

    GDALPDFObject* poK = poDict->Get("K");
    if (poK == NULL)
        return;

    if (poK->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poK->GetArray();
        if (poArray->GetLength() > 0 &&
            poArray->Get(0)->GetType() == PDFObjectType_Dictionary &&
            poArray->Get(0)->GetDictionary()->Get("K") != NULL &&
            poArray->Get(0)->GetDictionary()->Get("K")->GetType() == PDFObjectType_Int)
        {
            CPLString osLayerName;
            if (osT.size())
                osLayerName = osT;
            else
            {
                if (osS.size())
                    osLayerName = osS;
                else
                    osLayerName = CPLSPrintf("Layer%d", nLayers + 1);
            }

            const char* pszWKT = poGDAL_DS->GetProjectionRef();
            OGRSpatialReference* poSRS = NULL;
            if (pszWKT && pszWKT[0] != '\0')
            {
                poSRS = new OGRSpatialReference();
                poSRS->importFromWkt((char**) &pszWKT);
            }

            OGRPDFLayer* poLayer =
                new OGRPDFLayer(this, osLayerName.c_str(), poSRS, wkbUnknown);
            delete poSRS;

            poLayer->Fill(poArray);

            papoLayers = (OGRLayer**)
                CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers] = poLayer;
            nLayers ++;
        }
        else
        {
            for(int i=0;i<poArray->GetLength();i++)
                ExploreTree(poArray->Get(i));
        }
    }
    else if (poK->GetType() == PDFObjectType_Dictionary)
    {
        ExploreTree(poK);
    }
}

/************************************************************************/
/*                        GetGeometryFromMCID()                         */
/************************************************************************/

OGRGeometry* OGRPDFDataSource::GetGeometryFromMCID(int nMCID)
{
    std::map<int,OGRGeometry*>::iterator oMapIter = oMapMCID.find(nMCID);
    if (oMapIter != oMapMCID.end())
        return oMapIter->second;
    else
        return NULL;
}

/************************************************************************/
/*                            GraphicState                              */
/************************************************************************/

class GraphicState
{
    public:
        double adfCM[6];
        double adfStrokeColor[3];
        double adfFillColor[3];

        GraphicState()
        {
            adfCM[0] = 1;
            adfCM[1] = 0;
            adfCM[2] = 0;
            adfCM[3] = 1;
            adfCM[4] = 0;
            adfCM[5] = 0;
            adfStrokeColor[0] = 0.0;
            adfStrokeColor[1] = 0.0;
            adfStrokeColor[2] = 0.0;
            adfFillColor[0] = 1.0;
            adfFillColor[1] = 1.0;
            adfFillColor[2] = 1.0;
        }

        void MultiplyBy(double adfMatrix[6])
        {
            /*
            [ a b 0 ]     [ a' b' 0]     [ aa' + bc'       ab' + bd'       0 ]
            [ c d 0 ]  *  [ c' d' 0]  =  [ ca' + dc'       cb' + dd'       0 ]
            [ e f 1 ]     [ e' f' 1]     [ ea' + fc' + e'  eb' + fd' + f'  1 ]
            */

            double a = adfCM[0];
            double b = adfCM[1];
            double c = adfCM[2];
            double d = adfCM[3];
            double e = adfCM[4];
            double f = adfCM[5];
            double ap = adfMatrix[0];
            double bp = adfMatrix[1];
            double cp = adfMatrix[2];
            double dp = adfMatrix[3];
            double ep = adfMatrix[4];
            double fp = adfMatrix[5];
            adfCM[0] = a*ap + b*cp;
            adfCM[1] = a*bp + b*dp;
            adfCM[2] = c*ap + d*cp;
            adfCM[3] = c*bp + d*dp;
            adfCM[4] = e*ap + f*cp + ep;
            adfCM[5] = e*bp + f*dp + fp;
        }

        void ApplyMatrix(double adfCoords[2])
        {
            double x = adfCoords[0];
            double y = adfCoords[1];

            adfCoords[0] = x * adfCM[0] + y * adfCM[2] + adfCM[4];
            adfCoords[1] = x * adfCM[1] + y * adfCM[3] + adfCM[5];
        }
};

/************************************************************************/
/*                         PDFCoordsToSRSCoords()                       */
/************************************************************************/

void OGRPDFDataSource::PDFCoordsToSRSCoords(double x, double y,
                                            double& X, double &Y)
{
    x = x / dfPageWidth * nXSize;
    y = (1 - y / dfPageHeight) * nYSize;

    X = adfGeoTransform[0] + x * adfGeoTransform[1] + y * adfGeoTransform[2];
    Y = adfGeoTransform[3] + x * adfGeoTransform[4] + y * adfGeoTransform[5];

    if( fabs(X - (int)floor(X + 0.5)) < 1e-8 )
        X = (int)floor(X + 0.5);
    if( fabs(Y - (int)floor(Y + 0.5)) < 1e-8 )
        Y = (int)floor(Y + 0.5);
}

/************************************************************************/
/*                            UnstackTokens()                           */
/************************************************************************/

int OGRPDFDataSource::UnstackTokens(const CPLString& osToken,
                                    std::stack<CPLString>& osTokenStack,
                                    double* adfCoords)
{
    int nArgs = oMapOperators[osToken];
    for(int i=0;i<nArgs;i++)
    {
        if (osTokenStack.empty())
        {
            CPLDebug("PDF", "not enough arguments for %s", osToken.c_str());
            return FALSE;
        }
        adfCoords[nArgs-1-i] = atof(osTokenStack.top());
        osTokenStack.pop();
    }
    return TRUE;
}

/************************************************************************/
/*                         PDFGetCircleCenter()                         */
/************************************************************************/

/* Return the center of a circle, or NULL if it is not recognized */

static OGRPoint* PDFGetCircleCenter(OGRLineString* poLS)
{
    if (poLS == NULL || poLS->getNumPoints() != 5)
        return NULL;

    if (poLS->getY(0) == poLS->getY(2) &&
        poLS->getX(1) == poLS->getX(3) &&
        fabs((poLS->getX(0) + poLS->getX(2)) / 2 - poLS->getX(1)) < EPSILON &&
        fabs((poLS->getY(1) + poLS->getY(3)) / 2 - poLS->getY(0)) < EPSILON)
    {
        return new OGRPoint((poLS->getX(0) + poLS->getX(2)) / 2,
                            (poLS->getY(1) + poLS->getY(3)) / 2);
    }
    return NULL;
}

/************************************************************************/
/*                         PDFGetSquareCenter()                         */
/************************************************************************/

/* Return the center of a square, or NULL if it is not recognized */

static OGRPoint* PDFGetSquareCenter(OGRLineString* poLS)
{
    if (poLS == NULL || poLS->getNumPoints() < 4 || poLS->getNumPoints() > 5)
        return NULL;

    if (poLS->getX(0) == poLS->getX(3) &&
        poLS->getY(0) == poLS->getY(1) &&
        poLS->getX(1) == poLS->getX(2) &&
        poLS->getY(2) == poLS->getY(3) &&
        fabs(fabs(poLS->getX(0) - poLS->getX(1)) - fabs(poLS->getY(0) - poLS->getY(3))) < EPSILON)
    {
        return new OGRPoint((poLS->getX(0) + poLS->getX(1)) / 2,
                            (poLS->getY(0) + poLS->getY(3)) / 2);
    }
    return NULL;
}

/************************************************************************/
/*                        PDFGetTriangleCenter()                        */
/************************************************************************/

/* Return the center of a equilateral triangle, or NULL if it is not recognized */

static OGRPoint* PDFGetTriangleCenter(OGRLineString* poLS)
{
    if (poLS == NULL || poLS->getNumPoints() < 3 || poLS->getNumPoints() > 4)
        return NULL;

    double dfSqD1 = SQUARE(poLS->getX(0) - poLS->getX(1)) + SQUARE(poLS->getY(0) - poLS->getY(1));
    double dfSqD2 = SQUARE(poLS->getX(1) - poLS->getX(2)) + SQUARE(poLS->getY(1) - poLS->getY(2));
    double dfSqD3 = SQUARE(poLS->getX(0) - poLS->getX(2)) + SQUARE(poLS->getY(0) - poLS->getY(2));
    if (fabs(dfSqD1 - dfSqD2) < EPSILON && fabs(dfSqD2 - dfSqD3) < EPSILON)
    {
        return new OGRPoint((poLS->getX(0) + poLS->getX(1) + poLS->getX(2)) / 3,
                            (poLS->getY(0) + poLS->getY(1) + poLS->getY(2)) / 3);
    }
    return NULL;
}

/************************************************************************/
/*                          PDFGetStarCenter()                          */
/************************************************************************/

/* Return the center of a 5-point star, or NULL if it is not recognized */

static OGRPoint* PDFGetStarCenter(OGRLineString* poLS)
{
    if (poLS == NULL || poLS->getNumPoints() < 10 || poLS->getNumPoints() > 11)
        return NULL;

    double dfSqD01 = SQUARE(poLS->getX(0) - poLS->getX(1)) +
                     SQUARE(poLS->getY(0) - poLS->getY(1));
    double dfSqD02 = SQUARE(poLS->getX(0) - poLS->getX(2)) +
                       SQUARE(poLS->getY(0) - poLS->getY(2));
    double dfSqD13 = SQUARE(poLS->getX(1) - poLS->getX(3)) +
                      SQUARE(poLS->getY(1) - poLS->getY(3));
    const double dfSin18divSin126 = 0.38196601125;
    int bOK = fabs(dfSqD13 / dfSqD02 - SQUARE(dfSin18divSin126)) < EPSILON;
    for(int i=1;i<10 && bOK;i++)
    {
        double dfSqDiip1 = SQUARE(poLS->getX(i) - poLS->getX((i+1)%10)) +
                           SQUARE(poLS->getY(i) - poLS->getY((i+1)%10));
        if (fabs(dfSqDiip1 - dfSqD01) > EPSILON)
        {
            bOK = FALSE;
        }
        double dfSqDiip2 = SQUARE(poLS->getX(i) - poLS->getX((i+2)%10)) +
                           SQUARE(poLS->getY(i) - poLS->getY((i+2)%10));
        if ( (i%2) == 1 && fabs(dfSqDiip2 - dfSqD13) > EPSILON )
        {
            bOK = FALSE;
        }
        if ( (i%2) == 0 && fabs(dfSqDiip2 - dfSqD02) > EPSILON )
        {
            bOK = FALSE;
        }
    }
    if (bOK)
    {
        return new OGRPoint((poLS->getX(0) + poLS->getX(2) + poLS->getX(4) +
                             poLS->getX(6) + poLS->getX(8)) / 5,
                            (poLS->getY(0) + poLS->getY(2) + poLS->getY(4) +
                             poLS->getY(6) + poLS->getY(8)) / 5);
    }
    return NULL;
}

/************************************************************************/
/*                           ParseContent()                             */
/************************************************************************/

#define NEW_SUBPATH -99
#define CLOSE_SUBPATH -98
#define FILL_SUBPATH -97

OGRGeometry* OGRPDFDataSource::ParseContent(const char* pszContent,
                                            GDALPDFObject* poResources,
                                            int bInitBDCStack,
                                            int bMatchQ,
                                            std::map<CPLString, OGRPDFLayer*>& oMapPropertyToLayer,
                                            OGRPDFLayer* poCurLayer)
{
    CPLString osToken;
    char ch;
    std::stack<CPLString> osTokenStack;
    int bInString = FALSE;
    int nBDCLevel = 0;
    int nParenthesisLevel = 0;
    int nArrayLevel = 0;
    int nBTLevel = 0;

    int bCollectAllObjects = poResources != NULL && !bInitBDCStack && !bMatchQ;

    GraphicState oGS;
    std::stack<GraphicState> oGSStack;
    std::stack<OGRPDFLayer*> oLayerStack;

    std::vector<double> oCoords;
    int bHasFoundFill = FALSE;
    int bHasMultiPart = FALSE;

    if (bInitBDCStack)
    {
        osTokenStack.push("dummy");
        osTokenStack.push("dummy");
        oLayerStack.push(NULL);
    }

    while((ch = *pszContent) != '\0')
    {
        int bPushToken = FALSE;

        if (!bInString && ch == '%')
        {
            /* Skip comments until end-of-line */
            while((ch = *pszContent) != '\0')
            {
                if (ch == '\r' || ch == '\n')
                    break;
                pszContent ++;
            }
            if (ch == 0)
                break;
        }
        else if (!bInString && (ch == ' ' || ch == '\r' || ch == '\n'))
        {
            bPushToken = TRUE;
        }

        /* Ignore arrays */
        else if (!bInString && osToken.size() == 0 && ch == '[')
        {
            nArrayLevel ++;
        }
        else if (!bInString && nArrayLevel && osToken.size() == 0 && ch == ']')
        {
            nArrayLevel --;
        }

        else if (!bInString && osToken.size() == 0 && ch == '(')
        {
            bInString = TRUE;
            nParenthesisLevel ++;
            osToken += ch;
        }
        else if (bInString && ch == '(')
        {
            nParenthesisLevel ++;
            osToken += ch;
        }
        else if (bInString && ch == ')')
        {
            nParenthesisLevel --;
            osToken += ch;
            if (nParenthesisLevel == 0)
            {
                bInString = FALSE;
                bPushToken = TRUE;
            }
        }
        else if (ch == '<' && pszContent[1] == '<' && osToken.size() == 0)
        {
            int nDictDepth = 0;

            while(*pszContent != '\0')
            {
                if (pszContent[0] == '<' && pszContent[1] == '<')
                {
                    osToken += "<";
                    osToken += "<";
                    nDictDepth ++;
                    pszContent += 2;
                }
                else if (pszContent[0] == '>' && pszContent[1] == '>')
                {
                    osToken += ">";
                    osToken += ">";
                    nDictDepth --;
                    pszContent += 2;
                    if (nDictDepth == 0)
                        break;
                }
                else
                {
                    osToken += *pszContent;
                    pszContent ++;
                }
            }
            if (nDictDepth == 0)
            {
                bPushToken = TRUE;
                pszContent --;
            }
            else
                break;
        }
        else
        {
            osToken += ch;
        }

        pszContent ++;
        if (pszContent[0] == '\0')
            bPushToken = TRUE;

        if (bPushToken && osToken.size())
        {
            if (osToken == "BI")
            {
                while(*pszContent != '\0')
                {
                    if( pszContent[0] == 'E' && pszContent[1] == 'I' && pszContent[2] == ' ' )
                    {
                        break;
                    }
                    pszContent ++;
                }
                if( pszContent[0] == 'E' )
                    pszContent += 3;
                else
                    return NULL;
            }
            else if (osToken == "BDC")
            {
                CPLString osOCGName, osOC;
                for(int i=0;i<2;i++)
                {
                    if (osTokenStack.empty())
                    {
                        CPLDebug("PDF",
                                    "not enough arguments for %s",
                                    osToken.c_str());
                        return NULL;
                    }
                    if (i == 0)
                        osOCGName = osTokenStack.top();
                    else
                        osOC = osTokenStack.top();
                    osTokenStack.pop();
                }
                nBDCLevel ++;

                if( osOC == "/OC" && osOCGName.size() && osOCGName[0] == '/' )
                {
                    std::map<CPLString, OGRPDFLayer*>::iterator oIter =
                        oMapPropertyToLayer.find(osOCGName.c_str() + 1);
                    if( oIter != oMapPropertyToLayer.end() )
                    {
                        poCurLayer = oIter->second;
                        //CPLDebug("PDF", "Cur layer : %s", poCurLayer->GetName());
                    }
                }

                oLayerStack.push(poCurLayer);
                //CPLDebug("PDF", "%s %s BDC", osOC.c_str(), osOCGName.c_str());
            }
            else if (osToken == "EMC")
            {
                //CPLDebug("PDF", "EMC");
                if( !oLayerStack.empty() )
                {
                    oLayerStack.pop();
                    if( !oLayerStack.empty() )
                        poCurLayer = oLayerStack.top();
                    else
                        poCurLayer = NULL;

                    /*if (poCurLayer)
                    {
                        CPLDebug("PDF", "Cur layer : %s", poCurLayer->GetName());
                    }*/
                }
                else
                {
                    CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                    poCurLayer = NULL;
                    //return NULL;
                }

                nBDCLevel --;
                if (nBDCLevel == 0 && bInitBDCStack)
                    break;
            }

            /* Ignore any text stuff */
            else if (osToken == "BT")
                nBTLevel ++;
            else if (osToken == "ET")
            {
                nBTLevel --;
                if (nBTLevel < 0)
                {
                    CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                    return NULL;
                }
            }
            else if (!nArrayLevel && !nBTLevel)
            {
                int bEmitFeature = FALSE;

                if (osToken == "q")
                {
                    oGSStack.push(oGS);
                }
                else if (osToken == "Q")
                {
                    if (oGSStack.empty())
                    {
                        CPLDebug("PDF", "not enough arguments for %s", osToken.c_str());
                        return NULL;
                    }

                    oGS = oGSStack.top();
                    oGSStack.pop();

                    if (oGSStack.empty() && bMatchQ)
                        break;
                }
                else if (osToken == "cm")
                {
                    double adfMatrix[6];
                    for(int i=0;i<6;i++)
                    {
                        if (osTokenStack.empty())
                        {
                            CPLDebug("PDF", "not enough arguments for %s", osToken.c_str());
                            return NULL;
                        }
                        adfMatrix[6-1-i] = atof(osTokenStack.top());
                        osTokenStack.pop();
                    }

                    oGS.MultiplyBy(adfMatrix);
                }
                else if (osToken == "b" || /* closepath, fill, stroke */
                         osToken == "b*"   /* closepath, eofill, stroke */)
                {
                    if (!(oCoords.size() > 0 &&
                          oCoords[oCoords.size() - 2] == CLOSE_SUBPATH &&
                          oCoords[oCoords.size() - 1] == CLOSE_SUBPATH))
                    {
                        oCoords.push_back(CLOSE_SUBPATH);
                        oCoords.push_back(CLOSE_SUBPATH);
                    }
                    oCoords.push_back(FILL_SUBPATH);
                    oCoords.push_back(FILL_SUBPATH);
                    bHasFoundFill = TRUE;

                    bEmitFeature = TRUE;
                }
                else if (osToken == "B" ||  /* fill, stroke */
                         osToken == "B*" || /* eofill, stroke */
                         osToken == "f" ||  /* fill */
                         osToken == "F" ||  /* fill */
                         osToken == "f*"    /* eofill */ )
                {
                    oCoords.push_back(FILL_SUBPATH);
                    oCoords.push_back(FILL_SUBPATH);
                    bHasFoundFill = TRUE;

                    bEmitFeature = TRUE;
                }
                else if (osToken == "h") /* close subpath */
                {
                    if (!(oCoords.size() > 0 &&
                          oCoords[oCoords.size() - 2] == CLOSE_SUBPATH &&
                          oCoords[oCoords.size() - 1] == CLOSE_SUBPATH))
                    {
                        oCoords.push_back(CLOSE_SUBPATH);
                        oCoords.push_back(CLOSE_SUBPATH);
                    }
                }
                else if (osToken == "n") /* new subpath without stroking or filling */
                {
                    oCoords.resize(0);
                }
                else if (osToken == "s") /* close and stroke */
                {
                    if (!(oCoords.size() > 0 &&
                          oCoords[oCoords.size() - 2] == CLOSE_SUBPATH &&
                          oCoords[oCoords.size() - 1] == CLOSE_SUBPATH))
                    {
                        oCoords.push_back(CLOSE_SUBPATH);
                        oCoords.push_back(CLOSE_SUBPATH);
                    }

                    bEmitFeature = TRUE;
                }
                else if (osToken == "S") /* stroke */
                {
                    bEmitFeature = TRUE;
                }
                else if (osToken == "m" || osToken == "l")
                {
                    double adfCoords[2];
                    if (!UnstackTokens(osToken, osTokenStack, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return NULL;
                    }

                    if (osToken == "m")
                    {
                        if (oCoords.size() != 0)
                            bHasMultiPart = TRUE;
                        oCoords.push_back(NEW_SUBPATH);
                        oCoords.push_back(NEW_SUBPATH);
                    }

                    oGS.ApplyMatrix(adfCoords);
                    oCoords.push_back(adfCoords[0]);
                    oCoords.push_back(adfCoords[1]);
                }
                else if (osToken == "c") /* Bezier curve */
                {
                    double adfCoords[6];
                    if (!UnstackTokens(osToken, osTokenStack, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return NULL;
                    }

                    oGS.ApplyMatrix(adfCoords + 4);
                    oCoords.push_back(adfCoords[4]);
                    oCoords.push_back(adfCoords[5]);
                }
                else if (osToken == "v" || osToken == "y") /* Bezier curve */
                {
                    double adfCoords[4];
                    if (!UnstackTokens(osToken, osTokenStack, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return NULL;
                    }

                    oGS.ApplyMatrix(adfCoords + 2);
                    oCoords.push_back(adfCoords[2]);
                    oCoords.push_back(adfCoords[3]);
                }
                else if (osToken == "re") /* Rectangle */
                {
                    double adfCoords[4];
                    if (!UnstackTokens(osToken, osTokenStack, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return NULL;
                    }

                    adfCoords[2] += adfCoords[0];
                    adfCoords[3] += adfCoords[1];

                    oGS.ApplyMatrix(adfCoords);
                    oGS.ApplyMatrix(adfCoords + 2);

                    if (oCoords.size() != 0)
                        bHasMultiPart = TRUE;
                    oCoords.push_back(NEW_SUBPATH);
                    oCoords.push_back(NEW_SUBPATH);
                    oCoords.push_back(adfCoords[0]);
                    oCoords.push_back(adfCoords[1]);
                    oCoords.push_back(adfCoords[2]);
                    oCoords.push_back(adfCoords[1]);
                    oCoords.push_back(adfCoords[2]);
                    oCoords.push_back(adfCoords[3]);
                    oCoords.push_back(adfCoords[0]);
                    oCoords.push_back(adfCoords[3]);
                    oCoords.push_back(CLOSE_SUBPATH);
                    oCoords.push_back(CLOSE_SUBPATH);
                }

                else if (osToken == "Do")
                {
                    if (osTokenStack.empty())
                    {
                        CPLDebug("PDF",
                                 "not enough arguments for %s",
                                 osToken.c_str());
                        return NULL;
                    }

                    CPLString osObjectName = osTokenStack.top();
                    osTokenStack.pop();

                    if (osObjectName[0] != '/')
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return NULL;
                    }

                    if (poResources == NULL)
                    {
                        if (osObjectName.find("/SymImage") == 0)
                        {
                            oCoords.push_back(oGS.adfCM[4] + oGS.adfCM[0] / 2);
                            oCoords.push_back(oGS.adfCM[5] + oGS.adfCM[3] / 2);
                            osToken = "";

                            if( poCurLayer != NULL)
                                bEmitFeature = TRUE;
                            else
                                continue;
                        }
                        else
                        {
                            //CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                            return NULL;
                        }
                    }

                    if( !bEmitFeature )
                    {
                        GDALPDFObject* poXObject =
                            poResources->GetDictionary()->Get("XObject");
                        if (poXObject == NULL ||
                            poXObject->GetType() != PDFObjectType_Dictionary)
                        {
                            CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                            return NULL;
                        }

                        GDALPDFObject* poObject =
                            poXObject->GetDictionary()->Get(osObjectName.c_str() + 1);
                        if (poObject == NULL)
                        {
                            CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                            return NULL;
                        }

                        int bParseStream = TRUE;
                        /* Check if the object is an image. If so, no need to try to parse */
                        /* it. */
                        if (poObject->GetType() == PDFObjectType_Dictionary)
                        {
                            GDALPDFObject* poSubtype = poObject->GetDictionary()->Get("Subtype");
                            if (poSubtype != NULL &&
                                poSubtype->GetType() == PDFObjectType_Name &&
                                poSubtype->GetName() == "Image" )
                            {
                                bParseStream = FALSE;
                            }
                        }

                        if( bParseStream )
                        {
                            GDALPDFStream* poStream = poObject->GetStream();
                            if (!poStream)
                            {
                                CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                                return NULL;
                            }

                            char* pszStr = poStream->GetBytes();
                            OGRGeometry* poGeom = ParseContent(pszStr, NULL, FALSE, FALSE,
                                                            oMapPropertyToLayer, poCurLayer);
                            CPLFree(pszStr);
                            if (poGeom && !bCollectAllObjects)
                                return poGeom;
                            delete poGeom;
                        }
                    }
                }
                else if( osToken == "RG" || osToken == "rg" )
                {
                    double adf[3];
                    for(int i=0;i<3;i++)
                    {
                        if (osTokenStack.empty())
                        {
                            CPLDebug("PDF", "not enough arguments for %s", osToken.c_str());
                            return NULL;
                        }
                        adf[3-1-i] = atof(osTokenStack.top());
                        osTokenStack.pop();
                    }
                    if( osToken == "RG" )
                    {
                        oGS.adfStrokeColor[0] = adf[0];
                        oGS.adfStrokeColor[1] = adf[1];
                        oGS.adfStrokeColor[2] = adf[2];
                    }
                    else
                    {
                        oGS.adfFillColor[0] = adf[0];
                        oGS.adfFillColor[1] = adf[1];
                        oGS.adfFillColor[2] = adf[2];
                    }
                }
                else if (oMapOperators.find(osToken) != oMapOperators.end())
                {
                    int nArgs = oMapOperators[osToken];
                    if (nArgs < 0)
                    {
                        while( !osTokenStack.empty() )
                        {
                            CPLString osTopToken = osTokenStack.top();
                            if (oMapOperators.find(osTopToken) != oMapOperators.end())
                                break;
                            osTokenStack.pop();
                        }
                    }
                    else
                    {
                        for(int i=0;i<nArgs;i++)
                        {
                            if (osTokenStack.empty())
                            {
                                CPLDebug("PDF",
                                        "not enough arguments for %s",
                                        osToken.c_str());
                                return NULL;
                            }
                            osTokenStack.pop();
                        }
                    }
                }
                else
                {
                    //printf("%s\n", osToken.c_str());
                    osTokenStack.push(osToken);
                }

                if( bEmitFeature && poCurLayer != NULL)
                {
                    OGRGeometry* poGeom = BuildGeometry(oCoords, bHasFoundFill, bHasMultiPart);
                    bHasFoundFill = bHasMultiPart = FALSE;
                    if (poGeom)
                    {
                        OGRFeature* poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
                        if( bSetStyle )
                        {
                            if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString ||
                                wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString )
                            {
                                poFeature->SetStyleString(CPLSPrintf("PEN(c:#%02X%02X%02X)",
                                                                    (int)(oGS.adfStrokeColor[0] * 255 + 0.5),
                                                                    (int)(oGS.adfStrokeColor[1] * 255 + 0.5),
                                                                    (int)(oGS.adfStrokeColor[2] * 255 + 0.5)));
                            }
                            else if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                                    wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon )
                            {
                                poFeature->SetStyleString(CPLSPrintf("PEN(c:#%02X%02X%02X);BRUSH(fc:#%02X%02X%02X)",
                                                                    (int)(oGS.adfStrokeColor[0] * 255 + 0.5),
                                                                    (int)(oGS.adfStrokeColor[1] * 255 + 0.5),
                                                                    (int)(oGS.adfStrokeColor[2] * 255 + 0.5),
                                                                    (int)(oGS.adfFillColor[0] * 255 + 0.5),
                                                                    (int)(oGS.adfFillColor[1] * 255 + 0.5),
                                                                    (int)(oGS.adfFillColor[2] * 255 + 0.5)));
                            }
                        }
                        poGeom->assignSpatialReference(poCurLayer->GetSpatialRef());
                        poFeature->SetGeometryDirectly(poGeom);
                        poCurLayer->CreateFeature(poFeature);
                        delete poFeature;
                    }

                    oCoords.resize(0);
                }
            }

            osToken = "";
        }
    }

    if (!osTokenStack.empty())
    {
        while(!osTokenStack.empty())
        {
            CPLDebug("PDF",
                     "Remaing values in stack : %s",
                     osTokenStack.top().c_str());
            osTokenStack.pop();
        }
        return  NULL;
    }

    if (bCollectAllObjects)
        return NULL;

    return BuildGeometry(oCoords, bHasFoundFill, bHasMultiPart);
}

/************************************************************************/
/*                           BuildGeometry()                            */
/************************************************************************/

OGRGeometry* OGRPDFDataSource::BuildGeometry(std::vector<double>& oCoords,
                                             int bHasFoundFill,
                                             int bHasMultiPart)
{
    OGRGeometry* poGeom = NULL;

    if (!oCoords.size())
        return NULL;

    if (oCoords.size() == 2)
    {
        double X, Y;
        PDFCoordsToSRSCoords(oCoords[0], oCoords[1], X, Y);
        poGeom = new OGRPoint(X, Y);
    }
    else if (!bHasFoundFill)
    {
        OGRLineString* poLS = NULL;
        OGRMultiLineString* poMLS = NULL;
        if (bHasMultiPart)
        {
            poMLS = new OGRMultiLineString();
            poGeom = poMLS;
        }

        for(size_t i=0;i<oCoords.size();i+=2)
        {
            if (oCoords[i] == NEW_SUBPATH && oCoords[i+1] == NEW_SUBPATH)
            {
                poLS = new OGRLineString();
                if (poMLS)
                    poMLS->addGeometryDirectly(poLS);
                else
                    poGeom = poLS;
            }
            else if (oCoords[i] == CLOSE_SUBPATH && oCoords[i+1] == CLOSE_SUBPATH)
            {
                if (poLS && poLS->getNumPoints() >= 2 &&
                    !(poLS->getX(0) == poLS->getX(poLS->getNumPoints()-1) &&
                        poLS->getY(0) == poLS->getY(poLS->getNumPoints()-1)))
                {
                    poLS->addPoint(poLS->getX(0), poLS->getY(0));
                }
            }
            else if (oCoords[i] == FILL_SUBPATH && oCoords[i+1] == FILL_SUBPATH)
            {
                /* Should not happen */
            }
            else
            {
                if (poLS)
                {
                    double X, Y;
                    PDFCoordsToSRSCoords(oCoords[i], oCoords[i+1], X, Y);

                    poLS->addPoint(X, Y);
                }
            }
        }

        /* Recognize points as outputed by GDAL (ogr-sym-2 : circle (not filled)) */
        OGRGeometry* poCenter = NULL;
        if (poCenter == NULL && poLS != NULL && poLS->getNumPoints() == 5)
        {
            poCenter = PDFGetCircleCenter(poLS);
        }

        /* Recognize points as outputed by GDAL (ogr-sym-4: square (not filled)) */
        if (poCenter == NULL && poLS != NULL && (poLS->getNumPoints() == 4 || poLS->getNumPoints() == 5))
        {
            poCenter = PDFGetSquareCenter(poLS);
        }

        /* Recognize points as outputed by GDAL (ogr-sym-6: triangle (not filled)) */
        if (poCenter == NULL && poLS != NULL && (poLS->getNumPoints() == 3 || poLS->getNumPoints() == 4))
        {
            poCenter = PDFGetTriangleCenter(poLS);
        }

        /* Recognize points as outputed by GDAL (ogr-sym-8: star (not filled)) */
        if (poCenter == NULL && poLS != NULL && (poLS->getNumPoints() == 10 || poLS->getNumPoints() == 11))
        {
            poCenter = PDFGetStarCenter(poLS);
        }

        if (poCenter == NULL && poMLS != NULL && poMLS->getNumGeometries() == 2)
        {
            OGRLineString* poLS1 = (OGRLineString* )poMLS->getGeometryRef(0);
            OGRLineString* poLS2 = (OGRLineString* )poMLS->getGeometryRef(1);

            /* Recognize points as outputed by GDAL (ogr-sym-0: cross (+) ) */
            if (poLS1->getNumPoints() == 2 && poLS2->getNumPoints() == 2 &&
                poLS1->getY(0) == poLS1->getY(1) &&
                poLS2->getX(0) == poLS2->getX(1) &&
                fabs(fabs(poLS1->getX(0) - poLS1->getX(1)) - fabs(poLS2->getY(0) - poLS2->getY(1))) < EPSILON &&
                fabs((poLS1->getX(0) + poLS1->getX(1)) / 2 - poLS2->getX(0)) < EPSILON &&
                fabs((poLS2->getY(0) + poLS2->getY(1)) / 2 - poLS1->getY(0)) < EPSILON)
            {
                poCenter = new OGRPoint(poLS2->getX(0), poLS1->getY(0));
            }
            /* Recognize points as outputed by GDAL (ogr-sym-1: diagcross (X) ) */
            else if (poLS1->getNumPoints() == 2 && poLS2->getNumPoints() == 2 &&
                     poLS1->getX(0) == poLS2->getX(0) &&
                     poLS1->getY(0) == poLS2->getY(1) &&
                     poLS1->getX(1) == poLS2->getX(1) &&
                     poLS1->getY(1) == poLS2->getY(0) &&
                     fabs(fabs(poLS1->getX(0) - poLS1->getX(1)) - fabs(poLS1->getY(0) - poLS1->getY(1))) < EPSILON)
            {
                poCenter = new OGRPoint((poLS1->getX(0) + poLS1->getX(1)) / 2,
                                        (poLS1->getY(0) + poLS1->getY(1)) / 2);
            }
        }

        if (poCenter)
        {
            delete poGeom;
            poGeom = poCenter;
        }
    }
    else
    {
        OGRLinearRing* poLS = NULL;
        int nPolys = 0;
        OGRGeometry** papoPoly = NULL;

        for(size_t i=0;i<oCoords.size();i+=2)
        {
            if (oCoords[i] == NEW_SUBPATH && oCoords[i+1] == NEW_SUBPATH)
            {
                delete poLS;
                poLS = new OGRLinearRing();
            }
            else if ((oCoords[i] == CLOSE_SUBPATH && oCoords[i+1] == CLOSE_SUBPATH) ||
                        (oCoords[i] == FILL_SUBPATH && oCoords[i+1] == FILL_SUBPATH))
            {
                if (poLS)
                {
                    poLS->closeRings();

                    OGRPoint* poCenter = NULL;

                    if (nPolys == 0 &&
                        poLS &&
                        poLS->getNumPoints() == 5)
                    {
                        /* Recognize points as outputed by GDAL (ogr-sym-3 : circle (filled)) */
                        poCenter = PDFGetCircleCenter(poLS);

                        /* Recognize points as outputed by GDAL (ogr-sym-5: square (filled)) */
                        if (poCenter == NULL)
                            poCenter = PDFGetSquareCenter(poLS);

                        /* ESRI points */
                        if (poCenter == NULL &&
                            oCoords.size() == 14 &&
                            poLS->getY(0) == poLS->getY(1) &&
                            poLS->getX(1) == poLS->getX(2) &&
                            poLS->getY(2) == poLS->getY(3) &&
                            poLS->getX(3) == poLS->getX(0))
                        {
                            poCenter = new OGRPoint((poLS->getX(0) + poLS->getX(1)) / 2,
                                                    (poLS->getY(0) + poLS->getY(2)) / 2);
                        }
                    }
                    /* Recognize points as outputed by GDAL (ogr-sym-7: triangle (filled)) */
                    else if (nPolys == 0 &&
                             poLS &&
                             poLS->getNumPoints() == 4)
                    {
                        poCenter = PDFGetTriangleCenter(poLS);
                    }
                    /* Recognize points as outputed by GDAL (ogr-sym-9: star (filled)) */
                    else if (nPolys == 0 &&
                             poLS &&
                             poLS->getNumPoints() == 11)
                    {
                        poCenter = PDFGetStarCenter(poLS);
                    }

                    if (poCenter)
                    {
                        poGeom = poCenter;
                        break;
                    }

                    if (poLS->getNumPoints() >= 3)
                    {
                        OGRPolygon* poPoly =  new OGRPolygon();
                        poPoly->addRingDirectly(poLS);
                        poLS = NULL;

                        papoPoly = (OGRGeometry**) CPLRealloc(papoPoly, (nPolys + 1) * sizeof(OGRGeometry*));
                        papoPoly[nPolys ++] = poPoly;
                    }
                    else
                    {
                        delete poLS;
                        poLS = NULL;
                    }
                }
            }
            else
            {
                if (poLS)
                {
                    double X, Y;
                    PDFCoordsToSRSCoords(oCoords[i], oCoords[i+1], X, Y);

                    poLS->addPoint(X, Y);
                }
            }
        }

        delete poLS;

        int bIsValidGeometry;
        if (nPolys == 2 &&
            ((OGRPolygon*)papoPoly[0])->getNumInteriorRings() == 0 &&
            ((OGRPolygon*)papoPoly[1])->getNumInteriorRings() == 0)
        {
            OGRLinearRing* poRing0 = ((OGRPolygon*)papoPoly[0])->getExteriorRing();
            OGRLinearRing* poRing1 = ((OGRPolygon*)papoPoly[1])->getExteriorRing();
            if (poRing0->getNumPoints() == poRing1->getNumPoints())
            {
                int bSameRing = TRUE;
                for(int i=0;i<poRing0->getNumPoints();i++)
                {
                    if (poRing0->getX(i) != poRing1->getX(i))
                    {
                        bSameRing = FALSE;
                        break;
                    }
                    if (poRing0->getY(i) != poRing1->getY(i))
                    {
                        bSameRing = FALSE;
                        break;
                    }
                }

                /* Just keep on ring if they are identical */
                if (bSameRing)
                {
                    delete papoPoly[1];
                    nPolys = 1;
                }
            }
        }
        if (nPolys)
        {
            poGeom = OGRGeometryFactory::organizePolygons(
                    papoPoly, nPolys, &bIsValidGeometry, NULL);
        }
        CPLFree(papoPoly);
    }

    return poGeom;
}

/************************************************************************/
/*                          ExploreContents()                           */
/************************************************************************/

void OGRPDFDataSource::ExploreContents(GDALPDFObject* poObj,
                                       GDALPDFObject* poResources)
{
    std::map<CPLString, OGRPDFLayer*> oMapPropertyToLayer;

    if (poObj->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poObj->GetArray();
        for(int i=0;i<poArray->GetLength();i++)
            ExploreContents(poArray->Get(i), poResources);
    }

    if (poObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFStream* poStream = poObj->GetStream();
    if (!poStream)
        return;

    char* pszStr = poStream->GetBytes();
    const char* pszMCID = (const char*) pszStr;
    while((pszMCID = strstr(pszMCID, "/MCID")) != NULL)
    {
        const char* pszBDC = strstr(pszMCID, "BDC");
        if (pszBDC)
        {
            /* Hack for http://www.avenza.com/sites/default/files/spatialpdf/US_County_Populations.pdf */
            /* FIXME: that logic is too fragile. */
            const char* pszStartParsing = pszBDC;
            const char* pszAfterBDC = pszBDC + 3;
            int bMatchQ = FALSE;
            while (pszAfterBDC[0] == ' ' || pszAfterBDC[0] == '\r' || pszAfterBDC[0] == '\n')
                pszAfterBDC ++;
            if (strncmp(pszAfterBDC, "0 0 m", 5) == 0)
            {
                const char* pszLastq = pszBDC;
                while(pszLastq > pszStr && *pszLastq != 'q')
                    pszLastq --;

                if (pszLastq > pszStr && *pszLastq == 'q' &&
                    (pszLastq[-1] == ' ' || pszLastq[-1] == '\r' || pszLastq[-1] == '\n') &&
                    (pszLastq[1] == ' ' || pszLastq[1] == '\r' || pszLastq[1] == '\n'))
                {
                    pszStartParsing = pszLastq;
                    bMatchQ = TRUE;
                }
            }

            int nMCID = atoi(pszMCID + 6);
            if (GetGeometryFromMCID(nMCID) == NULL)
            {
                OGRGeometry* poGeom = ParseContent(pszStartParsing, poResources,
                                                   !bMatchQ, bMatchQ, oMapPropertyToLayer, NULL);
                if( poGeom != NULL )
                {
                    /* Save geometry in map */
                    oMapMCID[nMCID] = poGeom;
                }
            }
        }
        pszMCID += 5;
    }
    CPLFree(pszStr);
}

/************************************************************************/
/*                           PDFSanitizeLayerName()                     */
/************************************************************************/

static
CPLString PDFSanitizeLayerName(const char* pszName)
{
    CPLString osName;
    for(int i=0; pszName[i] != '\0'; i++)
    {
        if (pszName[i] == ' ' || pszName[i] == '.' || pszName[i] == ',')
            osName += "_";
        else
            osName += pszName[i];
    }
    return osName;
}

/************************************************************************/
/*                   ExploreContentsNonStructured()                     */
/************************************************************************/

void OGRPDFDataSource::ExploreContentsNonStructuredInternal(GDALPDFObject* poContents,
                                                            GDALPDFObject* poResources,
                                                            std::map<CPLString, OGRPDFLayer*>& oMapPropertyToLayer)
{
    if (poContents->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poContents->GetArray();
        char* pszConcatStr = NULL;
        int nConcatLen = 0;
        for(int i=0;i<poArray->GetLength();i++)
        {
            GDALPDFObject* poObj = poArray->Get(i);
            if( poObj->GetType() != PDFObjectType_Dictionary)
                break;
            GDALPDFStream* poStream = poObj->GetStream();
            if (!poStream)
                break;
            char* pszStr = poStream->GetBytes();
            int nLen = (int)strlen(pszStr);
            char* pszConcatStrNew = (char*)CPLRealloc(pszConcatStr, nConcatLen + nLen + 1);
            if( pszConcatStrNew == NULL )
            {
                CPLFree(pszStr);
                break;
            }
            pszConcatStr = pszConcatStrNew;
            memcpy(pszConcatStr + nConcatLen, pszStr, nLen+1);
            nConcatLen += nLen;
            CPLFree(pszStr);
        }
        if( pszConcatStr )
            ParseContent(pszConcatStr, poResources, FALSE, FALSE, oMapPropertyToLayer, NULL);
        CPLFree(pszConcatStr);
        return;
    }

    if (poContents->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFStream* poStream = poContents->GetStream();
    if (!poStream)
        return;

    char* pszStr = poStream->GetBytes();
    ParseContent(pszStr, poResources, FALSE, FALSE, oMapPropertyToLayer, NULL);
    CPLFree(pszStr);
}

void OGRPDFDataSource::ExploreContentsNonStructured(GDALPDFObject* poContents,
                                                    GDALPDFObject* poResources)
{
    std::map<CPLString, OGRPDFLayer*> oMapPropertyToLayer;
    if (poResources != NULL &&
        poResources->GetType() == PDFObjectType_Dictionary)
    {
        GDALPDFObject* poProperties =
            poResources->GetDictionary()->Get("Properties");
        if (poProperties != NULL &&
            poProperties->GetType() == PDFObjectType_Dictionary)
        {
            CPLAssert(poGDAL_DS != NULL);
            char** papszLayersWithRef = poGDAL_DS->GetMetadata("LAYERS_WITH_REF");
            char** papszIter = papszLayersWithRef;
            std::map< std::pair<int, int>, OGRPDFLayer *> oMapNumGenToLayer;
            while(papszIter && *papszIter)
            {
                char** papszTokens = CSLTokenizeString(*papszIter);

                if( CSLCount(papszTokens) != 3 ) {
                    CSLDestroy(papszTokens);
                    CPLDebug("PDF", "Ignore '%s', unparsable.", *papszIter);
                    papszIter ++;
                    continue;
                }

                const char* pszLayerName = papszTokens[0];
                int nNum = atoi(papszTokens[1]);
                int nGen = atoi(papszTokens[2]);

                CPLString osSanitizedName(PDFSanitizeLayerName(pszLayerName));

                OGRPDFLayer* poLayer = (OGRPDFLayer*) GetLayerByName(osSanitizedName.c_str());
                if (poLayer == NULL)
                {
                    const char* pszWKT = poGDAL_DS->GetProjectionRef();
                    OGRSpatialReference* poSRS = NULL;
                    if (pszWKT && pszWKT[0] != '\0')
                    {
                        poSRS = new OGRSpatialReference();
                        poSRS->importFromWkt((char**) &pszWKT);
                    }

                    poLayer =
                        new OGRPDFLayer(this, osSanitizedName.c_str(), poSRS, wkbUnknown);
                    delete poSRS;

                    papoLayers = (OGRLayer**)
                        CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
                    papoLayers[nLayers] = poLayer;
                    nLayers ++;
                }

                oMapNumGenToLayer[ std::pair<int,int>(nNum, nGen) ] = poLayer;

                CSLDestroy(papszTokens);
                papszIter ++;
            }

            std::map<CPLString, GDALPDFObject*>& oMap =
                                    poProperties->GetDictionary()->GetValues();
            std::map<CPLString, GDALPDFObject*>::iterator oIter = oMap.begin();
            std::map<CPLString, GDALPDFObject*>::iterator oEnd = oMap.end();

            for(; oIter != oEnd; ++oIter)
            {
                const char* pszKey = oIter->first.c_str();
                GDALPDFObject* poObj = oIter->second;
                if( poObj->GetRefNum() != 0 )
                {
                    std::map< std::pair<int, int>, OGRPDFLayer *>::iterator
                        oIterNumGenToLayer = oMapNumGenToLayer.find(
                            std::pair<int,int>(poObj->GetRefNum(), poObj->GetRefGen()) );
                    if( oIterNumGenToLayer != oMapNumGenToLayer.end() )
                    {
                        oMapPropertyToLayer[pszKey] = oIterNumGenToLayer->second;
                    }
                }
            }
        }
    }

    if( nLayers == 0 )
        return;

    ExploreContentsNonStructuredInternal(poContents,
                                         poResources,
                                         oMapPropertyToLayer);

    /* Remove empty layers */
    int i = 0;
    while(i < nLayers)
    {
        if (papoLayers[i]->GetFeatureCount() == 0)
        {
            delete papoLayers[i];
            if (i < nLayers - 1)
            {
                memmove(papoLayers + i, papoLayers + i + 1,
                        (nLayers - 1 - i) * sizeof(OGRPDFLayer*));
            }
            nLayers --;
        }
        else
            i ++;
    }
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

int OGRPDFDataSource::Open( const char * pszName)
{
    this->pszName = CPLStrdup(pszName);

    poGDAL_DS = GDALPDFOpen(pszName, GA_ReadOnly);
    if (poGDAL_DS == NULL)
        return FALSE;

    const char* pszPageObj = poGDAL_DS->GetMetadataItem("PDF_PAGE_OBJECT");
    if (pszPageObj)
        sscanf(pszPageObj, "%p", &poPageObj);
    if (poPageObj == NULL || poPageObj->GetType() != PDFObjectType_Dictionary)
        return FALSE;

    GDALPDFObject* poMediaBox = poPageObj->GetDictionary()->Get("MediaBox");
    if (poMediaBox == NULL || poMediaBox->GetType() != PDFObjectType_Array ||
        poMediaBox->GetArray()->GetLength() != 4)
        return FALSE;

    if (poMediaBox->GetArray()->Get(2)->GetType() == PDFObjectType_Real)
        dfPageWidth = poMediaBox->GetArray()->Get(2)->GetReal();
    else if (poMediaBox->GetArray()->Get(2)->GetType() == PDFObjectType_Int)
        dfPageWidth = poMediaBox->GetArray()->Get(2)->GetInt();
    else
        return FALSE;

    if (poMediaBox->GetArray()->Get(3)->GetType() == PDFObjectType_Real)
        dfPageHeight = poMediaBox->GetArray()->Get(3)->GetReal();
    else if (poMediaBox->GetArray()->Get(3)->GetType() == PDFObjectType_Int)
        dfPageHeight = poMediaBox->GetArray()->Get(3)->GetInt();
    else
        return FALSE;

    GDALPDFObject* poContents = poPageObj->GetDictionary()->Get("Contents");
    if (poContents == NULL)
        return FALSE;

    if (poContents->GetType() != PDFObjectType_Dictionary &&
        poContents->GetType() != PDFObjectType_Array)
        return FALSE;

    GDALPDFObject* poResources = poPageObj->GetDictionary()->Get("Resources");
    if (poResources == NULL || poResources->GetType() != PDFObjectType_Dictionary)
        return FALSE;
    
    const char* pszCatalog = poGDAL_DS->GetMetadataItem("PDF_CATALOG_OBJECT");
    if (pszCatalog)
        sscanf(pszCatalog, "%p", &poCatalogObj);
    if (poCatalogObj == NULL || poCatalogObj->GetType() != PDFObjectType_Dictionary)
        return FALSE;

    nXSize = poGDAL_DS->GetRasterXSize();
    nYSize = poGDAL_DS->GetRasterYSize();
    poGDAL_DS->GetGeoTransform(adfGeoTransform);


    GDALPDFObject* poStructTreeRoot = poCatalogObj->GetDictionary()->Get("StructTreeRoot");
    if (CSLTestBoolean(CPLGetConfigOption("OGR_PDF_READ_NON_STRUCTURED", "NO")) ||
        poStructTreeRoot == NULL ||
        poStructTreeRoot->GetType() != PDFObjectType_Dictionary)
    {
        ExploreContentsNonStructured(poContents, poResources);
    }
    else
    {
        ExploreContents(poContents, poResources);
        ExploreTree(poStructTreeRoot);
    }

    CleanupIntermediateResources();

    int bEmptyDS = TRUE;
    for(int i=0;i<nLayers;i++)
    {
        if (papoLayers[i]->GetFeatureCount() != 0)
        {
            bEmptyDS = FALSE;
            break;
        }
    }
    if (bEmptyDS)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRPDFDataSource::Create( const char * pszName, char **papszOptions )
{
    this->pszName = CPLStrdup(pszName);
    this->papszOptions = CSLDuplicate(papszOptions);
    bWritable = TRUE;

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPDFDataSource::CreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRLayer* poLayer = new OGRPDFLayer(this, pszLayerName, poSRS, eType);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers] = poLayer;
    nLayers ++;

    return poLayer;
}

/************************************************************************/
/*                            SyncToDisk()                              */
/************************************************************************/

OGRErr OGRPDFDataSource::SyncToDisk()
{
    if (nLayers == 0 || !bModified || !bWritable)
        return OGRERR_NONE;

    bModified = FALSE;

    OGREnvelope sGlobalExtent;
    int bHasExtent = FALSE;
    for(int i=0;i<nLayers;i++)
    {
        OGREnvelope sExtent;
        if (papoLayers[i]->GetExtent(&sExtent) == OGRERR_NONE)
        {
            bHasExtent = TRUE;
            sGlobalExtent.Merge(sExtent);
        }
    }
    if (!bHasExtent ||
        sGlobalExtent.MinX == sGlobalExtent.MaxX ||
        sGlobalExtent.MinY == sGlobalExtent.MaxY)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot compute spatial extent of features");
        return OGRERR_FAILURE;
    }

    PDFCompressMethod eStreamCompressMethod = COMPRESS_DEFLATE;
    const char* pszStreamCompressMethod = CSLFetchNameValue(papszOptions, "STREAM_COMPRESS");
    if (pszStreamCompressMethod)
    {
        if( EQUAL(pszStreamCompressMethod, "NONE") )
            eStreamCompressMethod = COMPRESS_NONE;
        else if( EQUAL(pszStreamCompressMethod, "DEFLATE") )
            eStreamCompressMethod = COMPRESS_DEFLATE;
        else
        {
            CPLError( CE_Warning, CPLE_NotSupported,
                    "Unsupported value for STREAM_COMPRESS.");
        }
    }

    const char* pszGEO_ENCODING =
        CSLFetchNameValueDef(papszOptions, "GEO_ENCODING", "ISO32000");

    double dfDPI = atof(CSLFetchNameValueDef(papszOptions, "DPI", "72"));
    if (dfDPI < 72.0)
        dfDPI = 72.0;

    const char* pszNEATLINE = CSLFetchNameValue(papszOptions, "NEATLINE");

    int nMargin = atoi(CSLFetchNameValueDef(papszOptions, "MARGIN", "0"));

    PDFMargins sMargins;
    sMargins.nLeft = nMargin;
    sMargins.nRight = nMargin;
    sMargins.nTop = nMargin;
    sMargins.nBottom = nMargin;

    const char* pszLeftMargin = CSLFetchNameValue(papszOptions, "LEFT_MARGIN");
    if (pszLeftMargin) sMargins.nLeft = atoi(pszLeftMargin);

    const char* pszRightMargin = CSLFetchNameValue(papszOptions, "RIGHT_MARGIN");
    if (pszRightMargin) sMargins.nRight = atoi(pszRightMargin);

    const char* pszTopMargin = CSLFetchNameValue(papszOptions, "TOP_MARGIN");
    if (pszTopMargin) sMargins.nTop = atoi(pszTopMargin);

    const char* pszBottomMargin = CSLFetchNameValue(papszOptions, "BOTTOM_MARGIN");
    if (pszBottomMargin) sMargins.nBottom = atoi(pszBottomMargin);

    const char* pszExtraImages = CSLFetchNameValue(papszOptions, "EXTRA_IMAGES");
    const char* pszExtraStream = CSLFetchNameValue(papszOptions, "EXTRA_STREAM");
    const char* pszExtraLayerName = CSLFetchNameValue(papszOptions, "EXTRA_LAYER_NAME");

    const char* pszOGRDisplayField = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_FIELD");
    const char* pszOGRDisplayLayerNames = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_LAYER_NAMES");
    int bWriteOGRAttributes = CSLFetchBoolean(papszOptions, "OGR_WRITE_ATTRIBUTES", TRUE);
    const char* pszOGRLinkField = CSLFetchNameValue(papszOptions, "OGR_LINK_FIELD");

    const char* pszOffLayers = CSLFetchNameValue(papszOptions, "OFF_LAYERS");
    const char* pszExclusiveLayers = CSLFetchNameValue(papszOptions, "EXCLUSIVE_LAYERS");

    const char* pszJavascript = CSLFetchNameValue(papszOptions, "JAVASCRIPT");
    const char* pszJavascriptFile = CSLFetchNameValue(papszOptions, "JAVASCRIPT_FILE");

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(pszName, "wb");
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PDF file %s.\n",
                  pszName );
        return OGRERR_FAILURE;
    }

    GDALPDFWriter oWriter(fp);

    double dfRatio = (sGlobalExtent.MaxY - sGlobalExtent.MinY) / (sGlobalExtent.MaxX - sGlobalExtent.MinX);

    int nWidth, nHeight;

    if (dfRatio < 1)
    {
        nWidth = 1024;
        nHeight = nWidth * dfRatio;
    }
    else
    {
        nHeight = 1024;
        nWidth = nHeight / dfRatio;
    }

    GDALDataset* poSrcDS = MEMDataset::Create( "MEM:::", nWidth, nHeight, 0, GDT_Byte, NULL );

    double adfGeoTransform[6];
    adfGeoTransform[0] = sGlobalExtent.MinX;
    adfGeoTransform[1] = (sGlobalExtent.MaxX - sGlobalExtent.MinX) / nWidth;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = sGlobalExtent.MaxY;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = - (sGlobalExtent.MaxY - sGlobalExtent.MinY) / nHeight;

    poSrcDS->SetGeoTransform(adfGeoTransform);

    OGRSpatialReference* poSRS = papoLayers[0]->GetSpatialRef();
    if (poSRS)
    {
        char* pszWKT = NULL;
        poSRS->exportToWkt(&pszWKT);
        poSrcDS->SetProjection(pszWKT);
        CPLFree(pszWKT);
    }

    oWriter.SetInfo(poSrcDS, papszOptions);

    oWriter.StartPage(poSrcDS,
                      dfDPI,
                      pszGEO_ENCODING,
                      pszNEATLINE,
                      &sMargins,
                      eStreamCompressMethod,
                      bWriteOGRAttributes);

    int iObj = 0;
    
    char** papszLayerNames = CSLTokenizeString2(pszOGRDisplayLayerNames,",",0);

    for(int i=0;i<nLayers;i++)
    {
        CPLString osLayerName;
        if (CSLCount(papszLayerNames) < nLayers)
            osLayerName = papoLayers[i]->GetName();
        else
            osLayerName = papszLayerNames[i];

        oWriter.WriteOGRLayer((OGRDataSourceH)this,
                              i,
                              pszOGRDisplayField,
                              pszOGRLinkField,
                              osLayerName,
                              bWriteOGRAttributes,
                              iObj);
    }

    CSLDestroy(papszLayerNames);

    oWriter.EndPage(pszExtraImages,
                    pszExtraStream,
                    pszExtraLayerName,
                    pszOffLayers,
                    pszExclusiveLayers);

    if (pszJavascript)
        oWriter.WriteJavascript(pszJavascript);
    else if (pszJavascriptFile)
        oWriter.WriteJavascriptFile(pszJavascriptFile);

    oWriter.Close();

    delete poSrcDS;

    return OGRERR_NONE;
}
