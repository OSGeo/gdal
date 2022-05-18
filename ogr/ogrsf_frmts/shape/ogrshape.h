/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the Shapefile driver to implement
 *           integration with OGR.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGRSHAPE_H_INCLUDED
#define OGRSHAPE_H_INCLUDED

#ifdef RENAME_INTERNAL_SHAPELIB_SYMBOLS
#include "gdal_shapelib_symbol_rename.h"
#endif

#include "ogrsf_frmts.h"
#include "shapefil.h"
#include "shp_vsi.h"
#include "ogrlayerpool.h"
#include <set>
#include <vector>

/* Was limited to 255 until OGR 1.10, but 254 seems to be a more */
/* conventional limit (http://en.wikipedia.org/wiki/Shapefile, */
/* http://www.clicketyclick.dk/databases/xbase/format/data_types.html, */
/* #5052 ) */
#define OGR_DBF_MAX_FIELD_WIDTH 254

/* ==================================================================== */
/*      Functions from Shape2ogr.cpp.                                   */
/* ==================================================================== */
OGRFeature *SHPReadOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                               OGRFeatureDefn * poDefn, int iShape,
                               SHPObject *psShape, const char *pszSHPEncoding );
OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape, SHPObject *psShape );
OGRFeatureDefn *SHPReadOGRFeatureDefn( const char * pszName,
                                       SHPHandle hSHP, DBFHandle hDBF,
                                       const char *pszSHPEncoding,
                                       int bAdjustType );
OGRErr SHPWriteOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                           OGRFeatureDefn *poFeatureDefn,
                           OGRFeature *poFeature, const char *pszSHPEncoding,
                           bool* pbTruncationWarningEmitted,
                           bool bRewind );

/************************************************************************/
/*                         OGRShapeGeomFieldDefn                        */
/************************************************************************/

class OGRShapeGeomFieldDefn final: public OGRGeomFieldDefn
{
    CPL_DISALLOW_COPY_ASSIGN(OGRShapeGeomFieldDefn)

    char* pszFullName = nullptr;
    mutable bool  bSRSSet = false;
    mutable CPLString osPrjFile{};

    public:
        OGRShapeGeomFieldDefn( const char* pszFullNameIn,
                               OGRwkbGeometryType eType,
                               int bSRSSetIn, OGRSpatialReference *poSRSIn) :
            OGRGeomFieldDefn("", eType),
            pszFullName(CPLStrdup(pszFullNameIn)),
            bSRSSet(CPL_TO_BOOL(bSRSSetIn))
        {
            SetSpatialRef(poSRSIn);
        }

        virtual ~OGRShapeGeomFieldDefn()
        {
            CPLFree(pszFullName);
        }

        OGRSpatialReference* GetSpatialRef() const override;
        void SetSRSSet() { bSRSSet = true; }

        const CPLString& GetPrjFilename() const { return osPrjFile; }
        void SetPrjFilename(const std::string& osFilename) { osPrjFile = osFilename; }
};

/************************************************************************/
/*                            OGRShapeLayer                             */
/************************************************************************/

class OGRShapeDataSource;

class OGRShapeLayer final: public OGRAbstractProxiedLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRShapeLayer)

    OGRShapeDataSource  *poDS;

    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;
    int                 nTotalShapeCount;

    char                *pszFullName;

    SHPHandle           hSHP;
    DBFHandle           hDBF;

    bool                bUpdateAccess;

    OGRwkbGeometryType  eRequestedGeomType;
    int                 ResetGeomType( int nNewType );

    bool                ScanIndices();

    GIntBig            *panMatchingFIDs;
    int                 iMatchingFID;
    void                ClearMatchingFIDs();

    OGRGeometry        *m_poFilterGeomLastValid;
    int                 nSpatialFIDCount;
    int                *panSpatialFIDs;
    void                ClearSpatialFIDs();

    bool                bHeaderDirty;
    bool                bSHPNeedsRepack;
    bool                bCheckedForQIX;
    SHPTreeDiskHandle   hQIX;
    bool                CheckForQIX();

    bool                bCheckedForSBN;
    SBNSearchHandle     hSBN;
    bool                CheckForSBN();

    bool                bSbnSbxDeleted;

    CPLString           ConvertCodePage( const char * );
    CPLString           osEncoding{};

    bool                bTruncationWarningEmitted;

    bool                bHSHPWasNonNULL; // Must try to reopen a .shp?
    bool                bHDBFWasNonNULL; // Must try to reopen a .dbf
    // Current state of opening of file descriptor to .shp and .dbf.

    typedef enum
    {
        FD_OPENED,
        FD_CLOSED,
        FD_CANNOT_REOPEN
    } FileDescriptorState;
    FileDescriptorState eFileDescriptorsState;

    bool                TouchLayer();
    bool                ReopenFileDescriptors();

    bool                bResizeAtClose;

    void                TruncateDBF();

    bool                bCreateSpatialIndexAtClose;
    bool                bRewindOnWrite;

    bool                m_bAutoRepack;
    typedef enum
    {
        YES,
        NO,
        MAYBE
    } NormandyState; /* French joke. "Peut'et' ben que oui, peut'et' ben que non." Sorry :-) */
    NormandyState       m_eNeedRepack;

    // Set of field names (in upper case). Built and invalidated when convenient
    std::set<CPLString> m_oSetUCFieldName{};

    bool                StartUpdate( const char* pszOperation );

    void                CloseUnderlyingLayer() override;

// WARNING: Each of the below public methods should start with a call to
// TouchLayer() and test its return value, so as to make sure that
// the layer is properly re-opened if necessary.

  public:
    OGRErr              CreateSpatialIndex( int nMaxDepth );
    OGRErr              DropSpatialIndex();
    OGRErr              Repack();
    OGRErr              RecomputeExtent();
    OGRErr              ResizeDBF();

    void                SetResizeAtClose( bool bFlag )
        { bResizeAtClose = bFlag; }

    const char         *GetFullName() { return pszFullName; }
    void                UpdateFollowingDeOrRecompression();

    OGRFeature *        FetchShape( int iShapeId );
    int                 GetFeatureCountWithSpatialFilterOnly();

                        OGRShapeLayer( OGRShapeDataSource* poDSIn,
                                       const char * pszName,
                                       SHPHandle hSHP, DBFHandle hDBF,
                                       const OGRSpatialReference *poSRS, bool bSRSSet,
                                       bool bUpdate,
                                       OGRwkbGeometryType eReqType,
                                       char ** papszCreateOptions = nullptr);
    virtual            ~OGRShapeLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;
    OGRErr              SetNextByIndex( GIntBig nIndex ) override;

    OGRFeature         *GetFeature( GIntBig nFeatureId ) override;
    OGRErr              ISetFeature( OGRFeature *poFeature ) override;
    OGRErr              DeleteFeature( GIntBig nFID ) override;
    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    OGRErr              SyncToDisk() override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    GIntBig             GetFeatureCount( int ) override;
    OGRErr              GetExtent( OGREnvelope *psExtent, int bForce ) override;
    OGRErr              GetExtent( int iGeomField, OGREnvelope *psExtent,
                                   int bForce ) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    OGRErr              CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    OGRErr              DeleteField( int iField ) override;
    OGRErr              ReorderFields( int* panMap ) override;
    OGRErr              AlterFieldDefn( int iField,
                                        OGRFieldDefn* poNewFieldDefn,
                                        int nFlags ) override;
    OGRErr              AlterGeomFieldDefn( int iGeomField,
                                            const OGRGeomFieldDefn* poNewGeomFieldDefn,
                                            int nFlagsIn ) override;

    int                 TestCapability( const char * ) override;
    void                SetSpatialFilter( OGRGeometry * ) override;
    void                SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    OGRErr              SetAttributeFilter( const char * ) override;

    OGRErr              Rename(const char* pszNewName) override;

    void                AddToFileList( CPLStringList& oFileList );
    void                CreateSpatialIndexAtClose( int bFlag )
        { bCreateSpatialIndexAtClose = CPL_TO_BOOL(bFlag); }
    void                SetModificationDate( const char* pszStr );
    void                SetAutoRepack(bool b) { m_bAutoRepack = b; }
    void                SetWriteDBFEOFChar(bool b);
};

/************************************************************************/
/*                          OGRShapeDataSource                          */
/************************************************************************/

class OGRShapeDataSource final: public OGRDataSource
{
    OGRShapeLayer     **papoLayers;
    int                 nLayers;
    char                *pszName;
    bool                bDSUpdate;
    bool                bSingleFileDataSource;
    OGRLayerPool       *poPool;

    std::vector<CPLString> oVectorLayerName{};

    bool                b2GBLimit;
    bool                m_bIsZip = false;
    bool                m_bSingleLayerZip = false;
    CPLString           m_osTemporaryUnzipDir{};
    CPLMutex           *m_poRefreshLockFileMutex = nullptr;
    CPLCond            *m_poRefreshLockFileCond = nullptr;
    VSILFILE           *m_psLockFile = nullptr;
    CPLJoinableThread  *m_hRefreshLockFileThread = nullptr;
    bool                m_bExitRefreshLockFileThread = false;
    double              m_dfRefreshLockDelay = 0;

    std::vector<CPLString> GetLayerNames() const;
    void                AddLayer( OGRShapeLayer* poLayer );
    static void         RefreshLockFile(void* _self);
    void                RemoveLockFile();
    bool                RecompressIfNeeded(const std::vector<CPLString>& layerNames);

    CPL_DISALLOW_COPY_ASSIGN(OGRShapeDataSource)

  public:
                        OGRShapeDataSource();
    virtual            ~OGRShapeDataSource();

    OGRLayerPool       *GetPool() const { return poPool; }

    bool                Open( GDALOpenInfo* poOpenInfo, bool bTestOpen,
                              bool bForceSingleFileDataSource = false );
    bool                OpenFile( const char *, bool bUpdate );
    bool                OpenZip( GDALOpenInfo* poOpenInfo,
                                 const char* pszOriFilename );
    bool                CreateZip(const char* pszOriFilename );

    const char         *GetName() override { return pszName; }

    int                 GetLayerCount() override;
    OGRLayer           *GetLayer( int ) override;
    OGRLayer           *GetLayerByName( const char * ) override;

    OGRLayer           *ICreateLayer( const char *,
                                       OGRSpatialReference * = nullptr,
                                       OGRwkbGeometryType = wkbUnknown,
                                       char ** = nullptr ) override;

    OGRLayer           *ExecuteSQL( const char *pszStatement,
                                     OGRGeometry *poSpatialFilter,
                                     const char *pszDialect ) override;

    int                 TestCapability( const char * ) override;
    OGRErr              DeleteLayer( int iLayer ) override;

    char              **GetFileList() override;

    void                 SetLastUsedLayer( OGRShapeLayer* poLayer );
    void                 UnchainLayer( OGRShapeLayer* poLayer );

    bool                 UncompressIfNeeded();

    SHPHandle            DS_SHPOpen( const char * pszShapeFile,
                                     const char * pszAccess );
    DBFHandle            DS_DBFOpen( const char * pszDBFFile,
                                     const char * pszAccess );
    char               **GetOpenOptions() { return papszOpenOptions; }

    static const char* const* GetExtensionsForDeletion();
    bool                 IsZip() const { return m_bIsZip; }
    CPLString            GetVSIZipPrefixeDir() const { return CPLString("/vsizip/{") + pszName + '}'; }
    const CPLString&     GetTemporaryUnzipDir() const { return m_osTemporaryUnzipDir; }

    static bool          CopyInPlace( VSILFILE* fpTarget,
                                      const CPLString& osSourceFilename );
};

#endif /* ndef OGRSHAPE_H_INCLUDED */
