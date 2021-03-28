/******************************************************************************
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset (read vector features)
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_pdf.h"

#include <array>

#define SQUARE(x) ((x)*(x))
#define EPSILON 1e-5

CPL_CVSID("$Id$")

#ifdef HAVE_PDF_READ_SUPPORT

constexpr int BEZIER_STEPS = 10;

/************************************************************************/
/*                        OpenVectorLayers()                            */
/************************************************************************/

int PDFDataset::OpenVectorLayers(GDALPDFDictionary* poPageDict)
{
    if( bHasLoadedLayers )
        return TRUE;
    bHasLoadedLayers = TRUE;

    if( poPageDict == nullptr )
    {
        poPageDict = poPageObj->GetDictionary();
        if ( poPageDict == nullptr )
            return FALSE;
    }

    GetCatalog();
    if( poCatalogObject == nullptr )
        return FALSE;

    GDALPDFObject* poContents = poPageDict->Get("Contents");
    if (poContents == nullptr)
        return FALSE;

    if (poContents->GetType() != PDFObjectType_Dictionary &&
        poContents->GetType() != PDFObjectType_Array)
        return FALSE;

    GDALPDFObject* poResources = poPageDict->Get("Resources");
    if (poResources == nullptr || poResources->GetType() != PDFObjectType_Dictionary)
        return FALSE;

    GDALPDFObject* poStructTreeRoot = poCatalogObject->GetDictionary()->Get("StructTreeRoot");
    if (CPLTestBool(CPLGetConfigOption("OGR_PDF_READ_NON_STRUCTURED", "NO")) ||
        poStructTreeRoot == nullptr ||
        poStructTreeRoot->GetType() != PDFObjectType_Dictionary)
    {
        ExploreContentsNonStructured(poContents, poResources);
    }
    else
    {
        int nDepth = 0;
        int nVisited = 0;
        bool bStop = false;
        ExploreContents(poContents, poResources, nDepth, nVisited, bStop);
        std::set< std::pair<int,int> > aoSetAlreadyVisited;
        ExploreTree(poStructTreeRoot, aoSetAlreadyVisited, 0);
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
    return !bEmptyDS;
}

/************************************************************************/
/*                   CleanupIntermediateResources()                     */
/************************************************************************/

void PDFDataset::CleanupIntermediateResources()
{
    std::map<int,OGRGeometry*>::iterator oMapIter = oMapMCID.begin();
    for( ; oMapIter != oMapMCID.end(); ++oMapIter)
        delete oMapIter->second;
    oMapMCID.erase(oMapMCID.begin(), oMapMCID.end());
}

/************************************************************************/
/*                          InitMapOperators()                          */
/************************************************************************/

typedef struct
{
    char        szOpName[4];
    int         nArgs;
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
    { "Tc", 1},
    { "Td", 2},
    { "TD", 2},
    { "Tf", 1},
    { "Tj", 1},
    { "TJ", 1},
    { "TL", 1},
    { "Tm", 6},
    { "Tr", 1},
    { "Ts", 1},
    { "Tw", 1},
    { "Tz", 1},
    { "v", 4 },
    { "w", 1 },
    { "W", 0 },
    { "W*", 0 },
    { "y", 4 },
    // '
    // "
};

void PDFDataset::InitMapOperators()
{
    for(size_t i=0;i<sizeof(asPDFOperators) / sizeof(asPDFOperators[0]); i++)
        oMapOperators[asPDFOperators[i].szOpName] = asPDFOperators[i].nArgs;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int PDFDataset::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *PDFDataset::GetLayer( int iLayer )

{
    OpenVectorLayers(nullptr);
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int PDFDataset::GetLayerCount()
{
    OpenVectorLayers(nullptr);
    return nLayers;
}

/************************************************************************/
/*                            ExploreTree()                             */
/************************************************************************/

void PDFDataset::ExploreTree(GDALPDFObject* poObj,
                             std::set< std::pair<int,int> > aoSetAlreadyVisited,
                             int nRecLevel)
{
    if (nRecLevel == 16)
        return;

    std::pair<int,int> oObjPair( poObj->GetRefNum().toInt(), poObj->GetRefGen() );
    if( aoSetAlreadyVisited.find( oObjPair ) != aoSetAlreadyVisited.end() )
        return;
    aoSetAlreadyVisited.insert( oObjPair );

    if (poObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFDictionary* poDict = poObj->GetDictionary();

    GDALPDFObject* poS = poDict->Get("S");
    CPLString osS;
    if (poS != nullptr && poS->GetType() == PDFObjectType_Name)
    {
        osS = poS->GetName();
    }

    GDALPDFObject* poT = poDict->Get("T");
    CPLString osT;
    if (poT != nullptr && poT->GetType() == PDFObjectType_String)
    {
        osT = poT->GetString();
    }

    GDALPDFObject* poK = poDict->Get("K");
    if (poK == nullptr)
        return;

    if (poK->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poK->GetArray();
        if (poArray->GetLength() > 0 &&
            poArray->Get(0) &&
            poArray->Get(0)->GetType() == PDFObjectType_Dictionary &&
            poArray->Get(0)->GetDictionary()->Get("K") != nullptr &&
            poArray->Get(0)->GetDictionary()->Get("K")->GetType() == PDFObjectType_Int)
        {
            CPLString osLayerName;
            if (!osT.empty() )
                osLayerName = osT;
            else
            {
                if (!osS.empty() )
                    osLayerName = osS;
                else
                    osLayerName = CPLSPrintf("Layer%d", nLayers + 1);
            }

            auto poSRSOri = GetSpatialRef();
            OGRSpatialReference* poSRS = poSRSOri ? poSRSOri->Clone() : nullptr;
            OGRPDFLayer* poLayer =
                new OGRPDFLayer(this, osLayerName.c_str(), poSRS, wkbUnknown);
            if( poSRS )
                poSRS->Release();

            poLayer->Fill(poArray);

            papoLayers = (OGRLayer**)
                CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers] = poLayer;
            nLayers ++;
        }
        else
        {
            for(int i=0;i<poArray->GetLength();i++)
            {
                auto poSubObj = poArray->Get(i);
                if (poSubObj )
                {
                    ExploreTree(poSubObj, aoSetAlreadyVisited,
                                nRecLevel + 1);
                }
            }
        }
    }
    else if (poK->GetType() == PDFObjectType_Dictionary)
    {
        ExploreTree(poK, aoSetAlreadyVisited, nRecLevel + 1);
    }
}

/************************************************************************/
/*                        GetGeometryFromMCID()                         */
/************************************************************************/

OGRGeometry* PDFDataset::GetGeometryFromMCID(int nMCID)
{
    std::map<int,OGRGeometry*>::iterator oMapIter = oMapMCID.find(nMCID);
    if (oMapIter != oMapMCID.end())
        return oMapIter->second;
    else
        return nullptr;
}

/************************************************************************/
/*                            GraphicState                              */
/************************************************************************/

class GraphicState
{
    public:
        std::array<double,6> adfCM;
        std::array<double,3> adfStrokeColor;
        std::array<double,3> adfFillColor;

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

void PDFDataset::PDFCoordsToSRSCoords(double x, double y,
                                            double& X, double &Y)
{
    x = x / dfPageWidth * nRasterXSize;
    if( bGeoTransformValid )
        y = (1 - y / dfPageHeight) * nRasterYSize;
    else
        y = (y / dfPageHeight) * nRasterYSize;

    X = adfGeoTransform[0] + x * adfGeoTransform[1] + y * adfGeoTransform[2];
    Y = adfGeoTransform[3] + x * adfGeoTransform[4] + y * adfGeoTransform[5];

    if( fabs(X - (int)floor(X + 0.5)) < 1e-8 )
        X = (int)floor(X + 0.5);
    if( fabs(Y - (int)floor(Y + 0.5)) < 1e-8 )
        Y = (int)floor(Y + 0.5);
}

/************************************************************************/
/*                         PDFGetCircleCenter()                         */
/************************************************************************/

/* Return the center of a circle, or NULL if it is not recognized */

static OGRPoint* PDFGetCircleCenter(OGRLineString* poLS)
{
    if (poLS == nullptr || poLS->getNumPoints() != 1 + 4 * BEZIER_STEPS)
        return nullptr;

    if (poLS->getY(0 * BEZIER_STEPS) == poLS->getY(2 * BEZIER_STEPS) &&
        poLS->getX(1 * BEZIER_STEPS) == poLS->getX(3 * BEZIER_STEPS) &&
        fabs((poLS->getX(0 * BEZIER_STEPS) + poLS->getX(2 * BEZIER_STEPS)) / 2 - poLS->getX(1 * BEZIER_STEPS)) < EPSILON &&
        fabs((poLS->getY(1 * BEZIER_STEPS) + poLS->getY(3 * BEZIER_STEPS)) / 2 - poLS->getY(0 * BEZIER_STEPS)) < EPSILON)
    {
        return new OGRPoint((poLS->getX(0 * BEZIER_STEPS) + poLS->getX(2 * BEZIER_STEPS)) / 2,
                            (poLS->getY(1 * BEZIER_STEPS) + poLS->getY(3 * BEZIER_STEPS)) / 2);
    }
    return nullptr;
}

/************************************************************************/
/*                         PDFGetSquareCenter()                         */
/************************************************************************/

/* Return the center of a square, or NULL if it is not recognized */

static OGRPoint* PDFGetSquareCenter(OGRLineString* poLS)
{
    if (poLS == nullptr || poLS->getNumPoints() < 4 || poLS->getNumPoints() > 5)
        return nullptr;

    if (poLS->getX(0) == poLS->getX(3) &&
        poLS->getY(0) == poLS->getY(1) &&
        poLS->getX(1) == poLS->getX(2) &&
        poLS->getY(2) == poLS->getY(3) &&
        fabs(fabs(poLS->getX(0) - poLS->getX(1)) - fabs(poLS->getY(0) - poLS->getY(3))) < EPSILON)
    {
        return new OGRPoint((poLS->getX(0) + poLS->getX(1)) / 2,
                            (poLS->getY(0) + poLS->getY(3)) / 2);
    }
    return nullptr;
}

/************************************************************************/
/*                        PDFGetTriangleCenter()                        */
/************************************************************************/

/* Return the center of a equilateral triangle, or NULL if it is not recognized */

static OGRPoint* PDFGetTriangleCenter(OGRLineString* poLS)
{
    if (poLS == nullptr || poLS->getNumPoints() < 3 || poLS->getNumPoints() > 4)
        return nullptr;

    double dfSqD1 = SQUARE(poLS->getX(0) - poLS->getX(1)) + SQUARE(poLS->getY(0) - poLS->getY(1));
    double dfSqD2 = SQUARE(poLS->getX(1) - poLS->getX(2)) + SQUARE(poLS->getY(1) - poLS->getY(2));
    double dfSqD3 = SQUARE(poLS->getX(0) - poLS->getX(2)) + SQUARE(poLS->getY(0) - poLS->getY(2));
    if (fabs(dfSqD1 - dfSqD2) < EPSILON && fabs(dfSqD2 - dfSqD3) < EPSILON)
    {
        return new OGRPoint((poLS->getX(0) + poLS->getX(1) + poLS->getX(2)) / 3,
                            (poLS->getY(0) + poLS->getY(1) + poLS->getY(2)) / 3);
    }
    return nullptr;
}

/************************************************************************/
/*                          PDFGetStarCenter()                          */
/************************************************************************/

/* Return the center of a 5-point star, or NULL if it is not recognized */

static OGRPoint* PDFGetStarCenter(OGRLineString* poLS)
{
    if (poLS == nullptr || poLS->getNumPoints() < 10 || poLS->getNumPoints() > 11)
        return nullptr;

    double dfSqD01 = SQUARE(poLS->getX(0) - poLS->getX(1)) +
                     SQUARE(poLS->getY(0) - poLS->getY(1));
    double dfSqD02 = SQUARE(poLS->getX(0) - poLS->getX(2)) +
                       SQUARE(poLS->getY(0) - poLS->getY(2));
    double dfSqD13 = SQUARE(poLS->getX(1) - poLS->getX(3)) +
                      SQUARE(poLS->getY(1) - poLS->getY(3));
    const double dfSin18divSin126 = 0.38196601125;
    if( dfSqD02 == 0 )
        return nullptr;
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
    return nullptr;
}

/************************************************************************/
/*                            UnstackTokens()                           */
/************************************************************************/

int PDFDataset::UnstackTokens(const char* pszToken,
                                    int nRequiredArgs,
                                    char aszTokenStack[TOKEN_STACK_SIZE][MAX_TOKEN_SIZE],
                                    int& nTokenStackSize,
                                    double* adfCoords)
{
    if (nTokenStackSize < nRequiredArgs)
    {
        CPLDebug("PDF", "not enough arguments for %s", pszToken);
        return FALSE;
    }
    nTokenStackSize -= nRequiredArgs;
    for(int i=0;i<nRequiredArgs;i++)
    {
        adfCoords[i] = CPLAtof(aszTokenStack[nTokenStackSize+i]);
    }
    return TRUE;
}

/************************************************************************/
/*                           AddBezierCurve()                           */
/************************************************************************/

static void AddBezierCurve(std::vector<double>& oCoords,
                           const double* x0_y0,
                           const double* x1_y1,
                           const double* x2_y2,
                           const double* x3_y3)
{
    double x0 = x0_y0[0];
    double y0 = x0_y0[1];
    double x1 = x1_y1[0];
    double y1 = x1_y1[1];
    double x2 = x2_y2[0];
    double y2 = x2_y2[1];
    double x3 = x3_y3[0];
    double y3 = x3_y3[1];
    for( int i = 1; i < BEZIER_STEPS; i++ )
    {
        const double t = static_cast<double>(i) / BEZIER_STEPS;
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double oneMinust = 1 - t;
        const double oneMinust2 = oneMinust * oneMinust;
        const double oneMinust3 = oneMinust2 * oneMinust;
        const double three_t_oneMinust = 3 * t * oneMinust;
        const double x = oneMinust3 * x0 + three_t_oneMinust * (oneMinust * x1 + t * x2) + t3 * x3;
        const double y = oneMinust3 * y0 + three_t_oneMinust * (oneMinust * y1 + t * y2) + t3 * y3;
        oCoords.push_back(x);
        oCoords.push_back(y);
    }
    oCoords.push_back(x3);
    oCoords.push_back(y3);
}

/************************************************************************/
/*                           ParseContent()                             */
/************************************************************************/

#define NEW_SUBPATH -99
#define CLOSE_SUBPATH -98
#define FILL_SUBPATH -97

OGRGeometry* PDFDataset::ParseContent(const char* pszContent,
                                            GDALPDFObject* poResources,
                                            int bInitBDCStack,
                                            int bMatchQ,
                                            std::map<CPLString, OGRPDFLayer*>& oMapPropertyToLayer,
                                            OGRPDFLayer* poCurLayer)
{

#define PUSH(aszTokenStack, str, strlen) \
    do \
    { \
        if(nTokenStackSize < TOKEN_STACK_SIZE) \
            memcpy(aszTokenStack[nTokenStackSize ++], str, strlen + 1); \
        else \
        { \
            CPLError(CE_Failure, CPLE_AppDefined, \
                     "Max token stack size reached"); \
            return nullptr; \
        }; \
    } while( false )

#define ADD_CHAR(szToken, c) \
    do \
    { \
        if(nTokenSize < MAX_TOKEN_SIZE-1) \
        { \
            szToken[nTokenSize ++ ] = c; \
            szToken[nTokenSize ] = '\0'; \
        } \
        else \
        { \
            CPLError(CE_Failure, CPLE_AppDefined, "Max token size reached");\
            return nullptr; \
        }; \
    } while( false )

    char szToken[MAX_TOKEN_SIZE];
    int nTokenSize = 0;
    char ch;
    char aszTokenStack[TOKEN_STACK_SIZE][MAX_TOKEN_SIZE];
    int nTokenStackSize = 0;
    int bInString = FALSE;
    int nBDCLevel = 0;
    int nParenthesisLevel = 0;
    int nArrayLevel = 0;
    int nBTLevel = 0;

    int bCollectAllObjects = poResources != nullptr && !bInitBDCStack && !bMatchQ;

    GraphicState oGS;
    std::stack<GraphicState> oGSStack;
    std::stack<OGRPDFLayer*> oLayerStack;

    std::vector<double> oCoords;
    int bHasFoundFill = FALSE;
    int bHasMultiPart = FALSE;

    szToken[0] = '\0';

    if (bInitBDCStack)
    {
        PUSH(aszTokenStack, "dummy", 5);
        PUSH(aszTokenStack, "dummy", 5);
        oLayerStack.push(nullptr);
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
        else if (!bInString && nTokenSize == 0 && ch == '[')
        {
            nArrayLevel ++;
        }
        else if (!bInString && nArrayLevel && ch == ']')
        {
            nArrayLevel --;
        }

        else if (!bInString && nTokenSize == 0 && ch == '(')
        {
            bInString = TRUE;
            nParenthesisLevel ++;
            ADD_CHAR(szToken, ch);
        }
        else if (bInString && ch == '(')
        {
            nParenthesisLevel ++;
            ADD_CHAR(szToken, ch);
        }
        else if (bInString && ch == ')')
        {
            nParenthesisLevel --;
            ADD_CHAR(szToken, ch);
            if (nParenthesisLevel == 0)
            {
                bInString = FALSE;
                bPushToken = TRUE;
            }
        }
        else if( bInString && ch == '\\' )
        {
            const auto nextCh = pszContent[1];
            if( nextCh == 'n' )
            {
                ADD_CHAR(szToken, '\n');
                pszContent ++;
            }
            else if( nextCh == 'r' )
            {
                ADD_CHAR(szToken, '\r');
                pszContent ++;
            }
            else if( nextCh == 't' )
            {
                ADD_CHAR(szToken, '\t');
                pszContent ++;
            }
            else if( nextCh == 'b' )
            {
                ADD_CHAR(szToken, '\b');
                pszContent ++;
            }
            else if( nextCh == '(' || nextCh == ')' || nextCh == '\\' )
            {
                ADD_CHAR(szToken, nextCh);
                pszContent ++;
            }
            else if( nextCh >= '0' && nextCh <= '7' &&
                     pszContent[2] >= '0' && pszContent[2] <= '7' &&
                     pszContent[3] >= '0' && pszContent[3] <= '7' )
            {
                ADD_CHAR(szToken,
                         ((nextCh - '\0') * 64 + (pszContent[2] - '\0') * 8 + pszContent[3] - '\0'));
                pszContent += 3;
            }
            else if( nextCh == '\n' )
            {
                if( pszContent[2] == '\r' )
                    pszContent += 2;
                else
                    pszContent ++;
            }
            else if( nextCh == '\r' )
            {
                pszContent ++;
            }
        }
        else if (ch == '<' && pszContent[1] == '<' && nTokenSize == 0)
        {
            int nDictDepth = 0;

            while(*pszContent != '\0')
            {
                if (pszContent[0] == '<' && pszContent[1] == '<')
                {
                    ADD_CHAR(szToken, '<');
                    ADD_CHAR(szToken, '<');
                    nDictDepth ++;
                    pszContent += 2;
                }
                else if (pszContent[0] == '>' && pszContent[1] == '>')
                {
                    ADD_CHAR(szToken, '>');
                    ADD_CHAR(szToken, '>');
                    nDictDepth --;
                    pszContent += 2;
                    if (nDictDepth == 0)
                        break;
                }
                else
                {
                    ADD_CHAR(szToken, *pszContent);
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
            // Do not create too long tokens in arrays, that we will ignore
            // anyway
            if( nArrayLevel == 0 || nTokenSize == 0 )
            {
                ADD_CHAR(szToken, ch);
            }
        }

        pszContent ++;
        if (pszContent[0] == '\0')
            bPushToken = TRUE;

#define EQUAL1(szToken, s) (szToken[0] == s[0] && szToken[1] == '\0')
#define EQUAL2(szToken, s) (szToken[0] == s[0] && szToken[1] == s[1] && szToken[2] == '\0')
#define EQUAL3(szToken, s) (szToken[0] == s[0] && szToken[1] == s[1] && szToken[2] == s[2] && szToken[3] == '\0')

        if (bPushToken && nTokenSize)
        {
            if (EQUAL2(szToken, "BI"))
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
                    return nullptr;
            }
            else if (EQUAL3(szToken, "BDC"))
            {
                if (nTokenStackSize < 2)
                {
                    CPLDebug("PDF",
                                "not enough arguments for %s",
                                szToken);
                    return nullptr;
                }
                nTokenStackSize -= 2;
                const char* pszOC = aszTokenStack[nTokenStackSize];
                const char* pszOCGName = aszTokenStack[nTokenStackSize+1];

                nBDCLevel ++;

                if( EQUAL3(pszOC, "/OC") && pszOCGName[0] == '/' )
                {
                    std::map<CPLString, OGRPDFLayer*>::iterator oIter =
                        oMapPropertyToLayer.find(pszOCGName + 1);
                    if( oIter != oMapPropertyToLayer.end() )
                    {
                        poCurLayer = oIter->second;
                        //CPLDebug("PDF", "Cur layer : %s", poCurLayer->GetName());
                    }
                }

                oLayerStack.push(poCurLayer);
                //CPLDebug("PDF", "%s %s BDC", osOC.c_str(), osOCGName.c_str());
            }
            else if (EQUAL3(szToken, "EMC"))
            {
                //CPLDebug("PDF", "EMC");
                if( !oLayerStack.empty() )
                {
                    oLayerStack.pop();
                    if( !oLayerStack.empty() )
                        poCurLayer = oLayerStack.top();
                    else
                        poCurLayer = nullptr;

                    /*if (poCurLayer)
                    {
                        CPLDebug("PDF", "Cur layer : %s", poCurLayer->GetName());
                    }*/
                }
                else
                {
                    CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                    poCurLayer = nullptr;
                    //return NULL;
                }

                nBDCLevel --;
                if (nBDCLevel == 0 && bInitBDCStack)
                    break;
            }

            /* Ignore any text stuff */
            else if (EQUAL2(szToken, "BT"))
                nBTLevel ++;
            else if (EQUAL2(szToken, "ET"))
            {
                nBTLevel --;
                if (nBTLevel < 0)
                {
                    CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                    return nullptr;
                }
            }
            else if (!nArrayLevel && !nBTLevel)
            {
                int bEmitFeature = FALSE;

                if( szToken[0] < 'A' )
                {
                    PUSH(aszTokenStack, szToken, nTokenSize);
                }
                else if (EQUAL1(szToken, "q"))
                {
                    oGSStack.push(oGS);
                }
                else if (EQUAL1(szToken, "Q"))
                {
                    if (oGSStack.empty())
                    {
                        CPLDebug("PDF", "not enough arguments for %s", szToken);
                        return nullptr;
                    }

                    oGS = oGSStack.top();
                    oGSStack.pop();

                    if (oGSStack.empty() && bMatchQ)
                        break;
                }
                else if (EQUAL2(szToken, "cm"))
                {
                    double adfMatrix[6];
                    if (!UnstackTokens(szToken, 6, aszTokenStack, nTokenStackSize, adfMatrix))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return nullptr;
                    }

                    oGS.MultiplyBy(adfMatrix);
                }
                else if (EQUAL1(szToken, "b") || /* closepath, fill, stroke */
                         EQUAL2(szToken, "b*")   /* closepath, eofill, stroke */)
                {
                    if (!(!oCoords.empty() &&
                          oCoords[oCoords.size() - 2] == CLOSE_SUBPATH &&
                          oCoords.back() == CLOSE_SUBPATH))
                    {
                        oCoords.push_back(CLOSE_SUBPATH);
                        oCoords.push_back(CLOSE_SUBPATH);
                    }
                    oCoords.push_back(FILL_SUBPATH);
                    oCoords.push_back(FILL_SUBPATH);
                    bHasFoundFill = TRUE;

                    bEmitFeature = TRUE;
                }
                else if (EQUAL1(szToken, "B") ||  /* fill, stroke */
                         EQUAL2(szToken, "B*") || /* eofill, stroke */
                         EQUAL1(szToken, "f") ||  /* fill */
                         EQUAL1(szToken, "F") ||  /* fill */
                         EQUAL2(szToken, "f*")    /* eofill */ )
                {
                    oCoords.push_back(FILL_SUBPATH);
                    oCoords.push_back(FILL_SUBPATH);
                    bHasFoundFill = TRUE;

                    bEmitFeature = TRUE;
                }
                else if (EQUAL1(szToken, "h")) /* close subpath */
                {
                    if (!(!oCoords.empty() &&
                          oCoords[oCoords.size() - 2] == CLOSE_SUBPATH &&
                          oCoords.back() == CLOSE_SUBPATH))
                    {
                        oCoords.push_back(CLOSE_SUBPATH);
                        oCoords.push_back(CLOSE_SUBPATH);
                    }
                }
                else if (EQUAL1(szToken, "n")) /* new subpath without stroking or filling */
                {
                    oCoords.resize(0);
                }
                else if (EQUAL1(szToken, "s")) /* close and stroke */
                {
                    if (!(!oCoords.empty() &&
                          oCoords[oCoords.size() - 2] == CLOSE_SUBPATH &&
                          oCoords.back() == CLOSE_SUBPATH))
                    {
                        oCoords.push_back(CLOSE_SUBPATH);
                        oCoords.push_back(CLOSE_SUBPATH);
                    }

                    bEmitFeature = TRUE;
                }
                else if (EQUAL1(szToken, "S")) /* stroke */
                {
                    bEmitFeature = TRUE;
                }
                else if (EQUAL1(szToken, "m") || EQUAL1(szToken, "l"))
                {
                    double adfCoords[2];
                    if (!UnstackTokens(szToken, 2, aszTokenStack, nTokenStackSize, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return nullptr;
                    }

                    if (EQUAL1(szToken, "m"))
                    {
                        if (!oCoords.empty())
                            bHasMultiPart = TRUE;
                        oCoords.push_back(NEW_SUBPATH);
                        oCoords.push_back(NEW_SUBPATH);
                    }

                    oGS.ApplyMatrix(adfCoords);
                    oCoords.push_back(adfCoords[0]);
                    oCoords.push_back(adfCoords[1]);
                }
                else if (EQUAL1(szToken, "c")) /* Bezier curve */
                {
                    double adfCoords[6];
                    if (!UnstackTokens(szToken, 6, aszTokenStack, nTokenStackSize, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return nullptr;
                    }

                    oGS.ApplyMatrix(adfCoords + 0);
                    oGS.ApplyMatrix(adfCoords + 2);
                    oGS.ApplyMatrix(adfCoords + 4);
                    AddBezierCurve(oCoords,
                                   oCoords.empty() ? &adfCoords[0] : &oCoords[oCoords.size()-2],
                                   &adfCoords[0],
                                   &adfCoords[2],
                                   &adfCoords[4]);
                }
                else if (EQUAL1(szToken, "v")) /* Bezier curve */
                {
                    double adfCoords[4];
                    if (!UnstackTokens(szToken, 4, aszTokenStack, nTokenStackSize, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return nullptr;
                    }

                    oGS.ApplyMatrix(adfCoords + 0);
                    oGS.ApplyMatrix(adfCoords + 2);
                    AddBezierCurve(oCoords,
                                   oCoords.empty() ? &adfCoords[0] : &oCoords[oCoords.size()-2],
                                   oCoords.empty() ? &adfCoords[0] : &oCoords[oCoords.size()-2],
                                   &adfCoords[0],
                                   &adfCoords[2]);
                }
                else if (EQUAL1(szToken, "y")) /* Bezier curve */
                {
                    double adfCoords[4];
                    if (!UnstackTokens(szToken, 4, aszTokenStack, nTokenStackSize, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return nullptr;
                    }

                    oGS.ApplyMatrix(adfCoords + 0);
                    oGS.ApplyMatrix(adfCoords + 2);
                    AddBezierCurve(oCoords,
                                   oCoords.empty() ? &adfCoords[0] : &oCoords[oCoords.size()-2],
                                   &adfCoords[0],
                                   &adfCoords[2],
                                   &adfCoords[2]);
                }
                else if (EQUAL2(szToken, "re")) /* Rectangle */
                {
                    double adfCoords[4];
                    if (!UnstackTokens(szToken, 4, aszTokenStack, nTokenStackSize, adfCoords))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return nullptr;
                    }

                    adfCoords[2] += adfCoords[0];
                    adfCoords[3] += adfCoords[1];

                    oGS.ApplyMatrix(adfCoords);
                    oGS.ApplyMatrix(adfCoords + 2);

                    if (!oCoords.empty())
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

                else if (EQUAL2(szToken, "Do"))
                {
                    if (nTokenStackSize == 0)
                    {
                        CPLDebug("PDF",
                                 "not enough arguments for %s",
                                 szToken);
                        return nullptr;
                    }

                    CPLString osObjectName = aszTokenStack[--nTokenStackSize];

                    if (osObjectName[0] != '/')
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return nullptr;
                    }

                    if (poResources == nullptr)
                    {
                        if (osObjectName.find("/SymImage") == 0)
                        {
                            oCoords.push_back(oGS.adfCM[4] + oGS.adfCM[0] / 2);
                            oCoords.push_back(oGS.adfCM[5] + oGS.adfCM[3] / 2);

                            szToken[0] = '\0';
                            nTokenSize = 0;

                            if( poCurLayer != nullptr)
                                bEmitFeature = TRUE;
                            else
                                continue;
                        }
                        else
                        {
                            //CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                            return nullptr;
                        }
                    }

                    if( !bEmitFeature )
                    {
                        GDALPDFObject* poXObject =
                            poResources->GetDictionary()->Get("XObject");
                        if (poXObject == nullptr ||
                            poXObject->GetType() != PDFObjectType_Dictionary)
                        {
                            CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                            return nullptr;
                        }

                        GDALPDFObject* poObject =
                            poXObject->GetDictionary()->Get(osObjectName.c_str() + 1);
                        if (poObject == nullptr)
                        {
                            CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                            return nullptr;
                        }

                        int bParseStream = TRUE;
                        /* Check if the object is an image. If so, no need to try to parse */
                        /* it. */
                        if (poObject->GetType() == PDFObjectType_Dictionary)
                        {
                            GDALPDFObject* poSubtype = poObject->GetDictionary()->Get("Subtype");
                            if (poSubtype != nullptr &&
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
                                return nullptr;
                            }

                            char* pszStr = poStream->GetBytes();
                            if( pszStr )
                            {
                                OGRGeometry* poGeom = ParseContent(pszStr, nullptr, FALSE, FALSE,
                                                                oMapPropertyToLayer, poCurLayer);
                                CPLFree(pszStr);
                                if (poGeom && !bCollectAllObjects)
                                    return poGeom;
                                delete poGeom;
                            }
                        }
                    }
                }
                else if( EQUAL2(szToken, "RG") || EQUAL2(szToken, "rg") )
                {
                    double* padf = ( EQUAL2(szToken, "RG") ) ? &oGS.adfStrokeColor[0] : &oGS.adfFillColor[0];
                    if (!UnstackTokens(szToken, 3, aszTokenStack, nTokenStackSize, padf))
                    {
                        CPLDebug("PDF", "Should not happen at line %d", __LINE__);
                        return nullptr;
                    }
                }
                else if (oMapOperators.find(szToken) != oMapOperators.end())
                {
                    int nArgs = oMapOperators[szToken];
                    if (nArgs < 0)
                    {
                        while( nTokenStackSize != 0 )
                        {
                            CPLString osTopToken = aszTokenStack[--nTokenStackSize];
                            if (oMapOperators.find(osTopToken) != oMapOperators.end())
                                break;
                        }
                    }
                    else
                    {
                        if( nArgs > nTokenStackSize )
                        {
                            CPLDebug("PDF",
                                    "not enough arguments for %s",
                                    szToken);
                            return nullptr;
                        }
                        nTokenStackSize -= nArgs;
                    }
                }
                else
                {
                    PUSH(aszTokenStack, szToken, nTokenSize);
                }

                if( bEmitFeature && poCurLayer != nullptr)
                {
                    OGRGeometry* poGeom = BuildGeometry(oCoords, bHasFoundFill, bHasMultiPart);
                    bHasFoundFill = FALSE;
                    bHasMultiPart = FALSE;
                    if (poGeom)
                    {
                        OGRFeature* poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
                        if( bSetStyle )
                        {
                            OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
                            if( eType == wkbLineString || eType == wkbMultiLineString )
                            {
                                poFeature->SetStyleString(CPLSPrintf("PEN(c:#%02X%02X%02X)",
                                                                    (int)(oGS.adfStrokeColor[0] * 255 + 0.5),
                                                                    (int)(oGS.adfStrokeColor[1] * 255 + 0.5),
                                                                    (int)(oGS.adfStrokeColor[2] * 255 + 0.5)));
                            }
                            else if( eType == wkbPolygon || eType == wkbMultiPolygon )
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
                        CPL_IGNORE_RET_VAL(poCurLayer->CreateFeature(poFeature));
                        delete poFeature;
                    }

                    oCoords.resize(0);
                }
            }

            szToken[0] = '\0';
            nTokenSize = 0;
        }
    }

    if (nTokenStackSize != 0)
    {
        while(nTokenStackSize != 0)
        {
            nTokenStackSize--;
            CPLDebug("PDF",
                     "Remaining values in stack : %s",
                     aszTokenStack[nTokenStackSize]);
        }
        return  nullptr;
    }

    if (bCollectAllObjects)
        return nullptr;

    return BuildGeometry(oCoords, bHasFoundFill, bHasMultiPart);
}

/************************************************************************/
/*                           BuildGeometry()                            */
/************************************************************************/

OGRGeometry* PDFDataset::BuildGeometry(std::vector<double>& oCoords,
                                             int bHasFoundFill,
                                             int bHasMultiPart)
{
    OGRGeometry* poGeom = nullptr;

    if (!oCoords.size())
        return nullptr;

    if (oCoords.size() == 2)
    {
        double X, Y;
        PDFCoordsToSRSCoords(oCoords[0], oCoords[1], X, Y);
        poGeom = new OGRPoint(X, Y);
    }
    else if (!bHasFoundFill)
    {
        OGRLineString* poLS = nullptr;
        OGRMultiLineString* poMLS = nullptr;
        if (bHasMultiPart)
        {
            poMLS = new OGRMultiLineString();
            poGeom = poMLS;
        }

        for(size_t i=0;i<oCoords.size();i+=2)
        {
            if (oCoords[i] == NEW_SUBPATH && oCoords[i+1] == NEW_SUBPATH)
            {
                if (poMLS)
                {
                    poLS = new OGRLineString();
                    poMLS->addGeometryDirectly(poLS);
                }
                else
                {
                    delete poLS;
                    poLS = new OGRLineString();
                    poGeom = poLS;
                }
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

        // Recognize points as written by GDAL (ogr-sym-2 : circle (not filled))
        OGRGeometry* poCenter = nullptr;
        if (poCenter == nullptr && poLS != nullptr && poLS->getNumPoints() == 1 + BEZIER_STEPS * 4 )
        {
            poCenter = PDFGetCircleCenter(poLS);
        }

        // Recognize points as written by GDAL (ogr-sym-4: square (not filled))
        if (poCenter == nullptr && poLS != nullptr && (poLS->getNumPoints() == 4 || poLS->getNumPoints() == 5))
        {
            poCenter = PDFGetSquareCenter(poLS);
        }

        // Recognize points as written by GDAL (ogr-sym-6: triangle (not filled))
        if (poCenter == nullptr && poLS != nullptr && (poLS->getNumPoints() == 3 || poLS->getNumPoints() == 4))
        {
            poCenter = PDFGetTriangleCenter(poLS);
        }

        // Recognize points as written by GDAL (ogr-sym-8: star (not filled))
        if (poCenter == nullptr && poLS != nullptr && (poLS->getNumPoints() == 10 || poLS->getNumPoints() == 11))
        {
            poCenter = PDFGetStarCenter(poLS);
        }

        if (poCenter == nullptr && poMLS != nullptr && poMLS->getNumGeometries() == 2)
        {
            const OGRLineString* poLS1 = poMLS->getGeometryRef(0);
            const OGRLineString* poLS2 = poMLS->getGeometryRef(1);

            // Recognize points as written by GDAL (ogr-sym-0: cross (+) ).
            if (poLS1->getNumPoints() == 2 && poLS2->getNumPoints() == 2 &&
                poLS1->getY(0) == poLS1->getY(1) &&
                poLS2->getX(0) == poLS2->getX(1) &&
                fabs(fabs(poLS1->getX(0) - poLS1->getX(1)) - fabs(poLS2->getY(0) - poLS2->getY(1))) < EPSILON &&
                fabs((poLS1->getX(0) + poLS1->getX(1)) / 2 - poLS2->getX(0)) < EPSILON &&
                fabs((poLS2->getY(0) + poLS2->getY(1)) / 2 - poLS1->getY(0)) < EPSILON)
            {
                poCenter = new OGRPoint(poLS2->getX(0), poLS1->getY(0));
            }
            // Recognize points as written by GDAL (ogr-sym-1: diagcross (X) ).
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
        OGRLinearRing* poLS = nullptr;
        int nPolys = 0;
        OGRGeometry** papoPoly = nullptr;

        for(size_t i=0;i<oCoords.size();i+=2)
        {
            if (oCoords[i] == NEW_SUBPATH && oCoords[i+1] == NEW_SUBPATH)
            {
                if (poLS && poLS->getNumPoints() >= 3)
                {
                    OGRPolygon* poPoly =  new OGRPolygon();
                    poPoly->addRingDirectly(poLS);
                    poLS = nullptr;

                    papoPoly = (OGRGeometry**) CPLRealloc(papoPoly, (nPolys + 1) * sizeof(OGRGeometry*));
                    papoPoly[nPolys ++] = poPoly;
                }
                delete poLS;
                poLS = new OGRLinearRing();
            }
            else if ((oCoords[i] == CLOSE_SUBPATH && oCoords[i+1] == CLOSE_SUBPATH) ||
                        (oCoords[i] == FILL_SUBPATH && oCoords[i+1] == FILL_SUBPATH))
            {
                if (poLS)
                {
                    poLS->closeRings();

                    std::unique_ptr<OGRPoint> poCenter;

                   if (nPolys == 0 &&
                        poLS &&
                        poLS->getNumPoints() == 1 + BEZIER_STEPS * 4 )
                   {
                        // Recognize points as written by GDAL (ogr-sym-3 : circle (filled))
                        poCenter.reset(PDFGetCircleCenter(poLS));
                   }

                    if (nPolys == 0 &&
                        poCenter == nullptr &&
                        poLS &&
                        poLS->getNumPoints() == 5)
                    {
                        // Recognize points as written by GDAL (ogr-sym-5: square (filled))
                        poCenter.reset(PDFGetSquareCenter(poLS));

                        /* ESRI points */
                        if (poCenter == nullptr &&
                            oCoords.size() == 14 &&
                            poLS->getY(0) == poLS->getY(1) &&
                            poLS->getX(1) == poLS->getX(2) &&
                            poLS->getY(2) == poLS->getY(3) &&
                            poLS->getX(3) == poLS->getX(0))
                        {
                            poCenter.reset(new OGRPoint((poLS->getX(0) + poLS->getX(1)) / 2,
                                                    (poLS->getY(0) + poLS->getY(2)) / 2));
                        }
                    }
                    // Recognize points as written by GDAL (ogr-sym-7: triangle (filled))
                    else if (nPolys == 0 &&
                             poLS &&
                             poLS->getNumPoints() == 4)
                    {
                        poCenter.reset(PDFGetTriangleCenter(poLS));
                    }
                    // Recognize points as written by GDAL (ogr-sym-9: star (filled))
                    else if (nPolys == 0 &&
                             poLS &&
                             poLS->getNumPoints() == 11)
                    {
                        poCenter.reset(PDFGetStarCenter(poLS));
                    }

                    if (poCenter)
                    {
                        delete poGeom;
                        poGeom = poCenter.release();
                        break;
                    }

                    if (poLS->getNumPoints() >= 3)
                    {
                        OGRPolygon* poPoly =  new OGRPolygon();
                        poPoly->addRingDirectly(poLS);
                        poLS = nullptr;

                        papoPoly = (OGRGeometry**) CPLRealloc(papoPoly, (nPolys + 1) * sizeof(OGRGeometry*));
                        papoPoly[nPolys ++] = poPoly;
                    }
                    else
                    {
                        delete poLS;
                        poLS = nullptr;
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
            papoPoly[0]->toPolygon()->getNumInteriorRings() == 0 &&
            papoPoly[1]->toPolygon()->getNumInteriorRings() == 0)
        {
            OGRLinearRing* poRing0 = papoPoly[0]->toPolygon()->getExteriorRing();
            OGRLinearRing* poRing1 = papoPoly[1]->toPolygon()->getExteriorRing();
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
                    papoPoly, nPolys, &bIsValidGeometry, nullptr);
        }
        CPLFree(papoPoly);
    }

    return poGeom;
}

/************************************************************************/
/*                          ExploreContents()                           */
/************************************************************************/

void PDFDataset::ExploreContents(GDALPDFObject* poObj,
                                 GDALPDFObject* poResources,
                                 int nDepth,
                                 int& nVisited,
                                 bool& bStop)
{
    std::map<CPLString, OGRPDFLayer*> oMapPropertyToLayer;
    if( nDepth == 10 || nVisited == 1000 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ExploreContents(): too deep exploration or too many items");
        bStop = true;
        return;
    }
    if( bStop )
        return;

    if (poObj->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poObj->GetArray();
        for(int i=0;i<poArray->GetLength();i++)
        {
            GDALPDFObject* poSubObj = poArray->Get(i);
            if( poSubObj )
            {
                nVisited ++;
                ExploreContents(poSubObj, poResources, nDepth + 1, nVisited, bStop);
                if( bStop )
                    return;
            }
        }
    }

    if (poObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFStream* poStream = poObj->GetStream();
    if (!poStream)
        return;

    char* pszStr = poStream->GetBytes();
    if (!pszStr)
        return;

    const char* pszMCID = (const char*) pszStr;
    while((pszMCID = strstr(pszMCID, "/MCID")) != nullptr)
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
            if (STARTS_WITH(pszAfterBDC, "0 0 m"))
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
            if (GetGeometryFromMCID(nMCID) == nullptr)
            {
                OGRGeometry* poGeom = ParseContent(pszStartParsing, poResources,
                                                   !bMatchQ, bMatchQ, oMapPropertyToLayer, nullptr);
                if( poGeom != nullptr )
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
/*                   ExploreContentsNonStructured()                     */
/************************************************************************/

void PDFDataset::ExploreContentsNonStructuredInternal(GDALPDFObject* poContents,
                                                      GDALPDFObject* poResources,
                                                      std::map<CPLString, OGRPDFLayer*>& oMapPropertyToLayer,
                                                      OGRPDFLayer* poSingleLayer)
{
    if (poContents->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poContents->GetArray();
        char* pszConcatStr = nullptr;
        int nConcatLen = 0;
        for(int i=0;i<poArray->GetLength();i++)
        {
            GDALPDFObject* poObj = poArray->Get(i);
            if( poObj == nullptr || poObj->GetType() != PDFObjectType_Dictionary)
                break;
            GDALPDFStream* poStream = poObj->GetStream();
            if (!poStream)
                break;
            char* pszStr = poStream->GetBytes();
            if (!pszStr)
                break;
            int nLen = (int)strlen(pszStr);
            char* pszConcatStrNew = (char*)CPLRealloc(pszConcatStr, nConcatLen + nLen + 1);
            if( pszConcatStrNew == nullptr )
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
            ParseContent(pszConcatStr, poResources, FALSE, FALSE, oMapPropertyToLayer, poSingleLayer);
        CPLFree(pszConcatStr);
        return;
    }

    if (poContents->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFStream* poStream = poContents->GetStream();
    if (!poStream)
        return;

    char* pszStr = poStream->GetBytes();
    if( !pszStr )
        return;
    ParseContent(pszStr, poResources, FALSE, FALSE, oMapPropertyToLayer, poSingleLayer);
    CPLFree(pszStr);
}

void PDFDataset::ExploreContentsNonStructured(GDALPDFObject* poContents,
                                                    GDALPDFObject* poResources)
{
    std::map<CPLString, OGRPDFLayer*> oMapPropertyToLayer;
    if (poResources != nullptr &&
        poResources->GetType() == PDFObjectType_Dictionary)
    {
        GDALPDFObject* poProperties =
            poResources->GetDictionary()->Get("Properties");
        if (poProperties != nullptr &&
            poProperties->GetType() == PDFObjectType_Dictionary)
        {
            std::map< std::pair<int, int>, OGRPDFLayer *> oMapNumGenToLayer;
            for(const auto& oLayerWithref: aoLayerWithRef )
            {
                CPLString osSanitizedName(PDFSanitizeLayerName(oLayerWithref.osName));

                OGRPDFLayer* poLayer = (OGRPDFLayer*) GetLayerByName(osSanitizedName.c_str());
                if (poLayer == nullptr)
                {
                    auto poSRSOri = GetSpatialRef();
                    OGRSpatialReference* poSRS = poSRSOri ? poSRSOri->Clone() : nullptr;
                    poLayer =
                        new OGRPDFLayer(this, osSanitizedName.c_str(), poSRS, wkbUnknown);
                    if( poSRS )
                        poSRS->Release();

                    papoLayers = (OGRLayer**)
                        CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
                    papoLayers[nLayers] = poLayer;
                    nLayers ++;
                }

                oMapNumGenToLayer[ std::pair<int,int>(oLayerWithref.nOCGNum.toInt(), oLayerWithref.nOCGGen) ] = poLayer;
            }

            std::map<CPLString, GDALPDFObject*>& oMap =
                                    poProperties->GetDictionary()->GetValues();
            std::map<CPLString, GDALPDFObject*>::iterator oIter = oMap.begin();
            std::map<CPLString, GDALPDFObject*>::iterator oEnd = oMap.end();

            for(; oIter != oEnd; ++oIter)
            {
                const char* pszKey = oIter->first.c_str();
                GDALPDFObject* poObj = oIter->second;
                if( poObj->GetRefNum().toBool() )
                {
                    std::map< std::pair<int, int>, OGRPDFLayer *>::iterator
                        oIterNumGenToLayer = oMapNumGenToLayer.find(
                            std::pair<int,int>(poObj->GetRefNum().toInt(), poObj->GetRefGen()) );
                    if( oIterNumGenToLayer != oMapNumGenToLayer.end() )
                    {
                        oMapPropertyToLayer[pszKey] = oIterNumGenToLayer->second;
                    }
                }
            }
        }
    }

    OGRPDFLayer* poSingleLayer = nullptr;
    if( nLayers == 0 )
    {
        if( CPLTestBool(CPLGetConfigOption("OGR_PDF_READ_NON_STRUCTURED", "NO")) )
        {
            OGRPDFLayer *poLayer =
                new OGRPDFLayer(this, "content", nullptr, wkbUnknown);
            papoLayers = (OGRLayer**)
                CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers] = poLayer;
            nLayers ++;
            poSingleLayer = poLayer;
        }
        else
        {
            return;
        }
    }

    ExploreContentsNonStructuredInternal(poContents,
                                         poResources,
                                         oMapPropertyToLayer,
                                         poSingleLayer);

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

#endif /* HAVE_PDF_READ_SUPPORT */
