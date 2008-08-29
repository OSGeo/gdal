/******************************************************************************
 * $Id: $
 *
 * Name:     oci_wrapper.h
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Limited wrapper for OCI (Oracle Call Interfaces)
 * Author:   Ivan Lucena [ivan.lucena@pmldnet.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena
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

#ifndef _ORCL_WRAP_H_INCLUDED
#define _ORCL_WRAP_H_INCLUDED

#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"

#include <oci.h>

#define ID_SEPARATORS   ",/@:"

/***************************************************************************/
/*                            Data type conversion table record type       */
/***************************************************************************/

struct OW_CellDepth {
    const char*     pszValue;
    GDALDataType    eDataType;
};

/***************************************************************************/
/*                            Free with NULL test                          */
/*                                                                         */
/* NOTE - mloskot: Calling free() or delete on null pointers is perfectly  */
/* valid, so these tests are redundant.                                    */
/***************************************************************************/

#define ObjFree_nt(p)    if(p) { delete p; p = NULL; }
#define CPLFree_nt(p)    if(p) { CPLFree(p); p = NULL; }
#define CSLFree_nt(p)    if(p) { CSLDestroy(p); p = NULL; }
#define XMLFree_nt(p)    if(p) { CPLDestroyXMLNode(p); p = NULL; }

/***************************************************************************/
/*                            OCI Error check                              */
/***************************************************************************/

bool CheckError( sword nStatus, OCIError* hError );

/***************************************************************************/
/*                            Auxiliar functions                           */
/***************************************************************************/

const GDALDataType  OWGetDataType( const char* pszCellDepth );
const char*         OWSetDataType( const GDALDataType eType );
int                 OWParseServerVersion( const char* pszText );
int                 OWParseEPSG( const char* pszText );
bool                OWIsNumeric( const char *pszText );
const char*         OWReplaceToken( const char* pszBaseString, 
                        char cToken, 
                        const char* pszOWReplaceToken );

/***************************************************************************/
/*                            Arbitrary limits                             */
/***************************************************************************/

#define OWCODE      64
#define OWNAME      512
#define OWTEXT      1024

/***************************************************************************/
/*                                  TYPES                                  */
/***************************************************************************/

#define TYPE_OWNER                  "MDSYS"
#define SDO_GEOMETRY                TYPE_OWNER".SDO_GEOMETRY"
#define SDO_COLORMAP                TYPE_OWNER".SDO_GEOR_COLORMAP"
#define SDO_GEORASTER               TYPE_OWNER".SDO_GEORASTER"
#define SDO_NUMBER_ARRAY            TYPE_OWNER".SDO_NUMBER_ARRAY"

/***************************************************************************/
/*                   USER DEFINED (actualy Oracle's) types                 */
/***************************************************************************/

typedef OCIRef SDO_GEORASTER_ref;
typedef OCIRef SDO_GEOMETRY_ref;
typedef OCIRef SDO_POINT_TYPE_ref;
typedef OCIRef SDO_GEOR_COLORMAP_ref;

struct sdo_point_type
{
    OCINumber x;
    OCINumber y;
    OCINumber z;
};

typedef struct sdo_point_type sdo_point_type;

typedef OCIArray sdo_elem_info_array;
typedef OCIArray sdo_ordinate_array;
typedef OCIArray SDO_NUMBER_ARRAY_TYPE;

struct sdo_geometry
{
    OCINumber       sdo_gtype;
    OCINumber       sdo_srid;
    sdo_point_type  sdo_point;
    OCIArray*       sdo_elem_info;
    OCIArray*       sdo_ordinates;
};
typedef struct sdo_geometry SDO_GEOMETRY_TYPE;

struct sdo_point_type_ind
{
    OCIInd      _atomic;
    OCIInd      x;
    OCIInd      y;
    OCIInd      z;
};
typedef struct sdo_point_type_ind sdo_point_type_ind;

struct sdo_geometry_ind
{
    OCIInd      _atomic;
    OCIInd      sdo_gtype;
    OCIInd      sdo_srid;
    struct      sdo_point_type_ind sdo_point;
    OCIInd      sdo_elem_info;
    OCIInd      sdo_ordinates;
};
typedef struct SDO_GEOMETRY_ind SDO_GEOMETRY_ind;

struct sdo_georaster
{
    OCINumber          rastertype;
    SDO_GEOMETRY_TYPE  spatialextent;
    OCIString*         rasterdatatable;
    OCINumber          rasterid;
    void*              metadata;
};
typedef struct sdo_georaster SDO_GEORASTER_TYPE;

struct sdo_georaster_ind
{
    OCIInd            _atomic;
    OCIInd            rastertype;
    sdo_geometry_ind  spatialextent;
    OCIInd            rasterdatatable;
    OCIInd            rasterid;
    OCIInd            metadata;
};
typedef struct sdo_georaster_ind SDO_GEORASTER_ind;

struct sdo_geor_colormap
{
    OCIArray* CellValue;
    OCIArray* Red;
    OCIArray* Green;
    OCIArray* Blue;
    OCIArray* Alpha;
};

typedef struct sdo_geor_colormap SDO_GEOR_COLORMAP;

struct sdo_geor_colormap_ind
{
    OCIInd _atomic;
    OCIInd CellValue;
    OCIInd Red;
    OCIInd Green;
    OCIInd Blue;
    OCIInd Alpha;
};

typedef struct sdo_geor_colormap_ind SDO_GEOR_COLORMAP_ind;

/***************************************************************************/
/*                            Oracle class wrappers                        */
/***************************************************************************/

class OWConnection;
class OWStatement;

//  ---------------------------------------------------------------------------
//  OWConnection
//  ---------------------------------------------------------------------------

class OWConnection 
{
    friend class OWStatement;

public:

                        OWConnection( 
                            const char* pszUserIn,
                            const char* pszPasswordIn,
                            const char* pszServerIn );
    virtual            ~OWConnection();

private:

    OCIEnv*             hEnv;
    OCIError*           hError;
    OCISvcCtx*          hSvcCtx;
    OCIDescribe*        hDescribe;

    int                 nVersion;

    bool                bSuceeded;

    char*               pszUser; 
    char*               pszPassword; 
    char*               pszServer;

    OCIType*            hNumArrayTDO;
    OCIType*            hGeometryTDO;
    OCIType*            hGeoRasterTDO;
    OCIType*            hColormapTDO;

public:

    OWStatement*        CreateStatement( const char* pszStatementIn );
    OCIParam*           GetDescription( char* pszTableName );
    bool                GetNextField( 
                            OCIParam* phTable, 
                            int nIndex,
                            const char* pszName, 
                            const char* pszType,
                            const char* pszSize );

    void                CreateType( sdo_geometry** pphData );
    void                DestroyType( sdo_geometry** pphData );
    OCIType*            DescribeType( char *pszTypeName );

    bool                Succed() { return bSuceeded; };

    char*               GetUser() { return pszUser; };
    char*               GetPassword() { return pszPassword; };
    char*               GetServer() { return pszServer; };
    int                 GetVersion () { return nVersion; };
};

/***************************************************************************/
/*                           OWStatement                              */
/***************************************************************************/

class OWStatement 
{

public:

                        OWStatement( OWConnection* pConnection,
                            const char* pszStatementIn );
    virtual            ~OWStatement();

private:

    OWConnection*       poConnect;
    OCIStmt*            hStmt;
    OCIError*           hError;

    int                 nNextCol;
    int                 nNextBnd;

    ub4                 nStmtMode;

    char*               pszStatement;

public:

    bool                Execute( int nRows = 0 );
    bool                Fetch( int nRows = 1 );
    unsigned int        nFetchCount;

    int                 GetInteger( OCINumber* ppoData );
    double              GetDouble( OCINumber* ppoData );
    char*               GetString( OCIString* ppoData );

    void                Define( int* pnData );
    void                Bind( int* pnData );
    void                Bind( double* pnData );
    void                Define( double* pnData );
    void                Define( char* pszData, int nSize = OWNAME );
    void                Bind( char* pszData, int nSize = OWNAME );
    void                Define( OCILobLocator** pphLocator, 
                            bool bBLOB = false);
    void                Define( OCIArray** pphData );
    void                Define( sdo_georaster** pphData );
    void                Define( sdo_geometry** pphData );
    void                Define( sdo_geor_colormap** pphData );
    void                Define( OCILobLocator** pphLocator, 
                            int nIterations );
    void                BindName( char* pszName, int* pnData );
    void                BindName( char* pszName, char* pszData, 
                            int nSize = OWNAME );
    void                BindName( char* pszName, 
                            OCILobLocator** pphLocator );
    static void         Free( OCILobLocator** ppphLocator,
                            int nCount );
    bool                ReadBlob( OCILobLocator* phLocator, 
                            void* pBuffer, int nSize );
    char*               ReadClob( OCILobLocator* phLocator );
    bool                WriteBlob( OCILobLocator* phLocator, 
                            void* pBuffer, int nSize );
    int                 GetElement( OCIArray** ppoData, 
                            int nIndex, int* pnResult );
    double              GetElement( OCIArray** ppoData, 
                            int nIndex, double* pdfResult );
};

#endif /* ifndef _ORCL_WRAP_H_INCLUDED */
