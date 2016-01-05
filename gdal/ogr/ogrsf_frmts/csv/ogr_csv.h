/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Definition of classes for OGR .csv driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004,  Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_CSV_H_INCLUDED
#define OGR_CSV_H_INCLUDED

#include "ogrsf_frmts.h"

typedef enum
{
    OGR_CSV_GEOM_NONE,
    OGR_CSV_GEOM_AS_WKT,
    OGR_CSV_GEOM_AS_SOME_GEOM_FORMAT,
    OGR_CSV_GEOM_AS_XYZ,
    OGR_CSV_GEOM_AS_XY,
    OGR_CSV_GEOM_AS_YX,
} OGRCSVGeometryFormat;

class OGRCSVDataSource;

char **OGRCSVReadParseLineL( VSILFILE * fp, char chDelimiter,
                             int bDontHonourStrings = FALSE,
                             int bKeepLeadingAndClosingQuotes = FALSE,
                             int bMergeDelimiter = FALSE);

typedef enum
{
    CREATE_FIELD_DO_NOTHING,
    CREATE_FIELD_PROCEED,
    CREATE_FIELD_ERROR
} OGRCSVCreateFieldAction;

void OGRCSVDriverRemoveFromMap(const char* pszName, GDALDataset* poDS);

/************************************************************************/
/*                             OGRCSVLayer                              */
/************************************************************************/

class OGRCSVLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    VSILFILE           *fpCSV;

    int                 nNextFID;

    int                 bHasFieldNames;

    OGRFeature *        GetNextUnfilteredFeature();

    int                 bNew;
    int                 bInWriteMode;
    int                 bUseCRLF;
    int                 bNeedRewindBeforeRead;
    OGRCSVGeometryFormat eGeometryFormat;

    char*               pszFilename;
    int                 bCreateCSVT;
    int                 bWriteBOM;
    char                chDelimiter;

    int                 nCSVFieldCount;
    int*                panGeomFieldIndex;
    int                 bFirstFeatureAppendedDuringSession;
    int                 bHiddenWKTColumn;

    /*http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm specific */
    int                 iNfdcLongitudeS;
    int                 iNfdcLatitudeS;
    int                 bDontHonourStrings;

    int                 iLongitudeField;
    int                 iLatitudeField;
    int                 iZField;
    CPLString           osXField;
    CPLString           osYField;
    CPLString           osZField;

    int                 bIsEurostatTSV;
    int                 nEurostatDims;

    GIntBig             nTotalFeatures;

    char              **AutodetectFieldTypes(char** papszOpenOptions, int nFieldCount);

    int                 bWarningBadTypeOrWidth;
    int                 bKeepSourceColumns;
    int                 bKeepGeomColumns;

    int                 bMergeDelimiter;

    int                 bEmptyStringNull;

    char              **GetNextLineTokens();

    static int          Matches(const char* pszFieldName, char** papszPossibleNames);

  public:
    OGRCSVLayer( const char *pszName, VSILFILE *fp, const char *pszFilename,
                 int bNew, int bInWriteMode, char chDelimiter );
   ~OGRCSVLayer();
   
    const char*         GetFilename() const { return pszFilename; }
    char                GetDelimiter() const { return chDelimiter; }
    int                 GetCRLF() const { return bUseCRLF; }
    int                 GetCreateCSVT() const { return bCreateCSVT; }
    int                 GetWriteBOM() const { return bWriteBOM; }
    OGRCSVGeometryFormat GetGeometryFormat() const { return eGeometryFormat; }
    int                 HasHiddenWKTColumn() const { return bHiddenWKTColumn; }
    GIntBig             GetTotalFeatureCount() const { return nTotalFeatures; }
    const CPLString&    GetXField() const { return osXField; }
    const CPLString&    GetYField() const { return osYField; }
    const CPLString&    GetZField() const { return osZField; }

    void                BuildFeatureDefn( const char* pszNfdcGeomField = NULL,
                                          const char* pszGeonamesGeomFieldPrefix = NULL,
                                          char** papszOpenOptions = NULL );

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    virtual OGRFeature* GetFeature( GIntBig nFID );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    static OGRCSVCreateFieldAction PreCreateField( OGRFeatureDefn* poFeatureDefn,
                                    OGRFieldDefn *poNewField, int bApproxOK );
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                         int bApproxOK = TRUE );

    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );

    void                SetCRLF(int);
    void                SetWriteGeometry(OGRwkbGeometryType eGType,
                                         OGRCSVGeometryFormat eGeometryFormat,
                                         const char* pszGeomCol = NULL);
    void                SetCreateCSVT(int bCreateCSVT);
    void                SetWriteBOM(int bWriteBOM);

    virtual GIntBig     GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      SyncToDisk();

    OGRErr              WriteHeader();
};

/************************************************************************/
/*                           OGRCSVDataSource                           */
/************************************************************************/

class OGRCSVDataSource : public OGRDataSource
{
    char                *pszName;

    OGRLayer          **papoLayers;
    int                 nLayers;

    int                 bUpdate;

    CPLString           osDefaultCSVName;

    int                 bEnableGeometryFields;

  public:
                        OGRCSVDataSource();
                        ~OGRCSVDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate, int bForceAccept,
                              char** papszOpenOptions = NULL );
    int                 OpenTable( const char * pszFilename,
                                   char** papszOpenOptions,
                                   const char* pszNfdcRunwaysGeomField = NULL,
                                   const char* pszGeonamesGeomFieldPrefix = NULL);

    const char          *GetName() { return pszName; }

    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer   *ICreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );

    virtual OGRErr      DeleteLayer(int);

    int                 TestCapability( const char * );

    void                CreateForSingleFile( const char* pszDirname,
                                             const char *pszFilename );

    void                EnableGeometryFields() { bEnableGeometryFields = TRUE; }

    static CPLString    GetRealExtension(CPLString osFilename);
};

#endif /* ndef OGR_CSV_H_INCLUDED */
