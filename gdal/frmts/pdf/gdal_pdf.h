/******************************************************************************
 * $Id$
 *
 * Project:  PDF Translator
 * Purpose:  Definition of classes for OGR .pdf driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _GDAL_PDF_H_INCLUDED
#define _GDAL_PDF_H_INCLUDED

#ifdef HAVE_POPPLER
/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef bool GBool" */
/* in include/poppler/goo/gtypes.h with the one defined in cpl_port.h */
#define CPL_GBOOL_DEFINED
#include <goo/gtypes.h>
#endif

#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#include "ogr_mem.h"
#include "pdfobject.h"

#include <map>
#include <stack>

/************************************************************************/
/*                             OGRPDFLayer                              */
/************************************************************************/

#if defined(HAVE_POPPLER) || defined(HAVE_PODOFO)

class PDFDataset;

class OGRPDFLayer : public OGRMemLayer
{
    PDFDataset       *poDS;
    int               bGeomTypeSet;
    int               bGeomTypeMixed;

public:
        OGRPDFLayer(PDFDataset* poDS,
                    const char * pszName,
                    OGRSpatialReference *poSRS,
                    OGRwkbGeometryType eGeomType);

    void                Fill( GDALPDFArray* poArray );

    virtual int                 TestCapability( const char * );
};

#endif

/************************************************************************/
/*                          OGRPDFWritableLayer                         */
/************************************************************************/

class PDFWritableVectorDataset;

class OGRPDFWritableLayer : public OGRMemLayer
{
    PDFWritableVectorDataset       *poDS;

public:
        OGRPDFWritableLayer(PDFWritableVectorDataset* poDS,
                    const char * pszName,
                    OGRSpatialReference *poSRS,
                    OGRwkbGeometryType eGeomType);

    virtual int                 TestCapability( const char * );
    virtual OGRErr              CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                            GDALPDFTileDesc                           */
/************************************************************************/

typedef struct
{
    GDALPDFObject* poImage;
    double         adfCM[6];
    double         dfWidth;
    double         dfHeight;
    int            nBands;
} GDALPDFTileDesc;

/************************************************************************/
/* ==================================================================== */
/*                              PDFDataset                              */
/* ==================================================================== */
/************************************************************************/

class PDFRasterBand;
class PDFImageRasterBand;

#ifdef HAVE_POPPLER
class ObjectAutoFree;
#endif

#define MAX_TOKEN_SIZE 256
#define TOKEN_STACK_SIZE 8

#if defined(HAVE_POPPLER) || defined(HAVE_PODOFO)

class PDFDataset : public GDALPamDataset
{
    friend class PDFRasterBand;
    friend class PDFImageRasterBand;

    CPLString    osFilename;
    CPLString    osUserPwd;
    char        *pszWKT;
    double       dfDPI;
    int          bHasCTM;
    double       adfCTM[6];
    double       adfGeoTransform[6];
    int          bGeoTransformValid;
    int          nGCPCount;
    GDAL_GCP    *pasGCPList;
    int          bProjDirty;
    int          bNeatLineDirty;

    GDALMultiDomainMetadata oMDMD;
    int          bInfoDirty;
    int          bXMPDirty;

    int          bUsePoppler;
#ifdef HAVE_POPPLER
    PDFDoc*      poDocPoppler;
#endif
#ifdef HAVE_PODOFO
    PoDoFo::PdfMemDocument* poDocPodofo;
    int          bPdfToPpmFailed;
#endif
    GDALPDFObject* poPageObj;

    int          iPage;

    GDALPDFObject *poImageObj;

    double       dfMaxArea;
    int          ParseLGIDictObject(GDALPDFObject* poLGIDict);
    int          ParseLGIDictDictFirstPass(GDALPDFDictionary* poLGIDict, int* pbIsBestCandidate = NULL);
    int          ParseLGIDictDictSecondPass(GDALPDFDictionary* poLGIDict);
    int          ParseProjDict(GDALPDFDictionary* poProjDict);
    int          ParseVP(GDALPDFObject* poVP, double dfMediaBoxWidth, double dfMediaBoxHeight);
    int          ParseMeasure(GDALPDFObject* poMeasure,
                              double dfMediaBoxWidth, double dfMediaBoxHeight,
                              double dfULX, double dfULY, double dfLRX, double dfLRY);

    int          bTried;
    GByte       *pabyCachedData;
    int          nLastBlockXOff, nLastBlockYOff;

    OGRPolygon*  poNeatLine;

    std::vector<GDALPDFTileDesc> asTiles; /* in the order of the PDF file */
    std::vector<int> aiTiles; /* in the order of blocks */
    int          nBlockXSize;
    int          nBlockYSize;
    int          CheckTiledRaster();

    void         GuessDPI(GDALPDFDictionary* poPageDict, int* pnBands);
    void         FindXMP(GDALPDFObject* poObj);
    void         ParseInfo(GDALPDFObject* poObj);

#ifdef HAVE_POPPLER
    ObjectAutoFree* poCatalogObjectPoppler;
#endif
    GDALPDFObject* poCatalogObject;
    GDALPDFObject* GetCatalog();

#ifdef HAVE_POPPLER
    void         AddLayer(const char* pszLayerName, OptionalContentGroup* ocg);
    void         ExploreLayers(GDALPDFArray* poArray, int nRecLevel, CPLString osTopLayer = "");
    void         FindLayers();
    void         TurnLayersOnOff();
    CPLStringList osLayerList;
    std::map<CPLString, OptionalContentGroup*> oLayerOCGMap;
#endif

    CPLStringList osLayerWithRefList;
    CPLString     FindLayerOCG(GDALPDFDictionary* poPageDict,
                               const char* pszLayerName);
    void          FindLayersGeneric(GDALPDFDictionary* poPageDict);

    int          bUseOCG;

    char       **papszOpenOptions;
    static const char*  GetOption(char** papszOpenOptions,
                                  const char* pszOptionName,
                                  const char* pszDefaultVal);

    int                 nLayers;
    OGRLayer          **papoLayers;

    double              dfPageWidth;
    double              dfPageHeight;
    void                PDFCoordsToSRSCoords(double x, double y,
                                             double& X, double &Y);

    std::map<int,OGRGeometry*> oMapMCID;
    void                CleanupIntermediateResources();

    std::map<CPLString, int> oMapOperators;
    void                InitMapOperators();
    
    int                 bSetStyle;

    void                ExploreTree(GDALPDFObject* poObj, int nRecLevel);
    void                ExploreContents(GDALPDFObject* poObj, GDALPDFObject* poResources);

    void                ExploreContentsNonStructuredInternal(GDALPDFObject* poContents,
                                                             GDALPDFObject* poResources,
                                                             std::map<CPLString, OGRPDFLayer*>& oMapPropertyToLayer);
    void                ExploreContentsNonStructured(GDALPDFObject* poObj, GDALPDFObject* poResources);

    int                 UnstackTokens(const char* pszToken,
                                      int nRequiredArgs,
                                      char aszTokenStack[TOKEN_STACK_SIZE][MAX_TOKEN_SIZE],
                                      int& nTokenStackSize,
                                      double* adfCoords);
    OGRGeometry*        ParseContent(const char* pszContent,
                                     GDALPDFObject* poResources,
                                     int bInitBDCStack,
                                     int bMatchQ,
                                     std::map<CPLString, OGRPDFLayer*>& oMapPropertyToLayer,
                                     OGRPDFLayer* poCurLayer);
    OGRGeometry*        BuildGeometry(std::vector<double>& oCoords,
                                      int bHasFoundFill,
                                      int bHasMultiPart);

    int                 OpenVectorLayers(GDALPDFDictionary* poPageDict);

  public:
                 PDFDataset();
    virtual     ~PDFDataset();

    virtual const char* GetProjectionRef();
    virtual CPLErr GetGeoTransform( double * );

    virtual CPLErr      SetProjection(const char* pszWKTIn);
    virtual CPLErr      SetGeoTransform(double* padfGeoTransform);

    virtual char      **GetMetadataDomainList();
    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    CPLErr ReadPixels( int nReqXOff, int nReqYOff,
                       int nReqXSize, int nReqYSize,
                       int nPixelSpace,
                       int nLineSpace,
                       int nBandSpace,
                       GByte* pabyData );

    virtual int                 GetLayerCount();
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );

    OGRGeometry        *GetGeometryFromMCID(int nMCID);

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                         PDFRasterBand                                */
/* ==================================================================== */
/************************************************************************/

class PDFRasterBand : public GDALPamRasterBand
{
    friend class PDFDataset;

    CPLErr IReadBlockFromTile( int, int, void * );

  public:

                PDFRasterBand( PDFDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};

#endif /*  defined(HAVE_POPPLER) || defined(HAVE_PODOFO) */

/************************************************************************/
/*                          PDFWritableDataset                          */
/************************************************************************/

class PDFWritableVectorDataset : public GDALDataset
{
        char**              papszOptions;

        int                 nLayers;
        OGRLayer          **papoLayers;

        int                 bModified;

    public:
                            PDFWritableVectorDataset();
                           ~PDFWritableVectorDataset();

        virtual OGRLayer*           ICreateLayer( const char * pszLayerName,
                                                OGRSpatialReference *poSRS,
                                                OGRwkbGeometryType eType,
                                                char ** papszOptions );

        virtual OGRErr              SyncToDisk();

        virtual int                 GetLayerCount();
        virtual OGRLayer*           GetLayer( int );

        virtual int                 TestCapability( const char * );

        static GDALDataset* Create( const char * pszName,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszOptions );

        void                SetModified() { bModified = TRUE; }
};

GDALDataset* GDALPDFOpen(const char* pszFilename, GDALAccess eAccess);
CPLString PDFSanitizeLayerName(const char* pszName);

#endif /* ndef _GDAL_PDF_H_INCLUDED */
