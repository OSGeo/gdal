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

#ifndef _OCI_WRAPPER_H_INCLUDED
#define _OCI_WRAPPER_H_INCLUDED

// GDAL supporting types

#include "gdal.h"
#include "gdal_priv.h"

// Oracle Class Interface

#include <oci.h>

/***************************************************************************/
/*                            Data type conversion table record type       */
/***************************************************************************/

struct OW_CellDepth {
    const char*     pszValue;
    GDALDataType    eDataType;
};

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
const char*         OWParseSDO_GEOR_INIT( const char* pszInsert, int nField );
const char*         OWReplaceString( const char* pszBaseString,
                        const char* pszToken,
                        const char* pszStopToken,
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
#define SDO_GEORASTER               TYPE_OWNER".SDO_GEORASTER"
#define SDO_PC                      TYPE_OWNER".SDO_PC"
#define SDO_NUMBER_ARRAY            TYPE_OWNER".SDO_NUMBER_ARRAY"
#define SDO_ORDINATE_ARRAY          TYPE_OWNER".SDO_ORDINATE_ARRAY"
#define SDO_ELEM_INFO_ARRAY         TYPE_OWNER".SDO_ELEM_INFO_ARRAY"

#define OW_XMLNS        "xmlns=\"http://xmlns.oracle.com/spatial/georaster\""

/***************************************************************************/
/*                   USER DEFINED (actualy Oracle's) types                 */
/***************************************************************************/

typedef OCIRef SDO_GEORASTER_ref;
typedef OCIRef SDO_GEOMETRY_ref;
typedef OCIRef SDO_POINT_TYPE_ref;

typedef OCIArray sdo_elem_info_array;
typedef OCIArray sdo_ordinate_array;
typedef OCIArray SDO_NUMBER_ARRAY_TYPE;

/***************************************************************************/
/*                            Point type                                   */
/***************************************************************************/

struct sdo_point_type
{
    OCINumber x;
    OCINumber y;
    OCINumber z;
};

typedef struct sdo_point_type sdo_point_type;

struct sdo_point_type_ind
{
    OCIInd      _atomic;
    OCIInd      x;
    OCIInd      y;
    OCIInd      z;
};

typedef struct sdo_point_type_ind sdo_point_type_ind;

/***************************************************************************/
/*                            Geometry type                                */
/***************************************************************************/

struct sdo_geometry
{
    OCINumber       sdo_gtype;
    OCINumber       sdo_srid;
    sdo_point_type  sdo_point;
    OCIArray*       sdo_elem_info;
    OCIArray*       sdo_ordinates;
};

typedef struct sdo_geometry SDO_GEOMETRY_TYPE;

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

/***************************************************************************/
/*                            GeoRaster type                               */
/***************************************************************************/

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

/***************************************************************************/
/*                            Point Cloud type                             */
/***************************************************************************/

struct sdo_mbr
{
   OCIArray*            lower_left;
   OCIArray*            upper_right;
};
typedef struct sdo_mbr SDO_MBR_TYPE;

struct sdo_mbr_ind
{
   OCIInd               _atomic;
   OCIInd               lower_left;
   OCIInd               upper_right;
};
typedef struct sdo_mbr_ind SDO_MBR_ind;

struct sdo_orgscl_type
{
   SDO_MBR_TYPE         extent;
   OCIArray*            scale;
   OCIArray*            ord_cmp_type;
};
typedef struct sdo_orgscl_type SDO_ORGSCL_TYPE;

struct sdo_orgscl_type_ind
{
   OCIInd               _atomic;
   SDO_MBR_ind          extent;
   OCIInd               scale;
   OCIInd               ord_cmp_type;
};
typedef struct sdo_orgscl_type_ind SDO_ORGSCL_TYPE_ind;

struct sdo_pc
{
    OCIString*          base_table;
    OCIString*          base_column;
    OCINumber           pc_id;
    OCIString*          blk_table;
    OCIString*          ptn_params;
    SDO_GEOMETRY_TYPE   pc_geometry;
    OCINumber           pc_tol;
    OCINumber           pc_tot_dimensions;
    SDO_ORGSCL_TYPE     pc_domain;
    OCIString*          pc_val_attr_tables;
    void*               pc_other_attrs;
};
typedef struct sdo_pc SDO_PC_TYPE;

struct sdo_pc_ind
{
    OCIInd              _atomic;
    OCIInd              base_table;
    OCIInd              base_column;
    OCIInd              pc_id;
    OCIInd              blk_table;
    OCIInd              ptn_params;
    sdo_geometry_ind    pc_geometry;
    OCIInd              pc_tol;
    OCIInd              pc_tot_dimensions;
    OCIInd              pc_domain;
    OCIInd              pc_val_attr_tables;
    OCIInd              pc_other_attrs;
};
typedef struct sdo_pc_ind SDO_PC_ind;

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
    OCIServer*          hServer;
    OCISession*         hSession;
    OCIDescribe*        hDescribe;

    int                 nVersion;
    sb4                 nCharSize;

    bool                bSuceeeded;

    char*               pszUser;
    char*               pszPassword;
    char*               pszServer;

    OCIType*            hNumArrayTDO;
    OCIType*            hGeometryTDO;
    OCIType*            hGeoRasterTDO;
    OCIType*            hPCTDO;
    OCIType*            hElemArrayTDO;
    OCIType*            hOrdnArrayTDO;

public:

    OWStatement*        CreateStatement( const char* pszStatement );
    OCIParam*           GetDescription( char* pszTable );
    bool                GetNextField(
                            OCIParam* phTable,
                            int nIndex,
                            char* pszName,
                            int* pnType,
                            int* pnSize,
                            int* pnPrecision,
                            signed short* pnScale );

    void                CreateType( sdo_geometry** pphData );
    void                DestroyType( sdo_geometry** pphData );
    void                CreateType( OCIArray** phData , OCIType* type);
    void                DestroyType( OCIArray** phData );
    OCIType*            DescribeType( char *pszTypeName );

    bool                Succeeded() { return bSuceeeded; };

    char*               GetUser() { return pszUser; };
    char*               GetPassword() { return pszPassword; };
    char*               GetServer() { return pszServer; };
    int                 GetVersion () { return nVersion; };
    sb4                 GetCharSize () { return nCharSize; };

    OCIType*            GetGeometryType() { return hGeometryTDO; }
    OCIType*            GetGeoRasterType() { return hGeoRasterTDO; }
    OCIType*            GetElemInfoType() {return hElemArrayTDO; }
    OCIType*            GetOrdinateType() {return hOrdnArrayTDO; }

    bool                Commit(); // OCITransCommit()
    bool                StartTransaction(); //  //OCITransStart()
    bool                EndTransaction() {return Commit(); }

};

/***************************************************************************/
/*                           OWStatement                                   */
/***************************************************************************/

class OWStatement
{

public:

                        OWStatement( OWConnection* poConnect, 
                            const char* pszStatement );
    virtual            ~OWStatement();

private:

    OWConnection*       poConnection;
    OCIStmt*            hStmt;
    OCIError*           hError;

    int                 nNextCol;
    int                 nNextBnd;

    ub4                 nStmtMode;

public:

    bool                Execute( int nRows = 1 );
    bool                Fetch( int nRows = 1 );
    unsigned int        nFetchCount;

    int                 GetInteger( OCINumber* ppoData );
    double              GetDouble( OCINumber* ppoData );
    char*               GetString( OCIString* ppoData );

    void                Bind( int* pnData );
    void                Bind( long* pnData );
    void                Bind( double* pnData );
    void                Bind( char* pData, long nData );
    void                Bind( sdo_geometry** pphData );
    void                Bind( OCILobLocator** pphLocator );
    void                Bind( OCIArray** pphData, OCIType* type );
    void                Bind( char* pszData, int nSize = OWNAME );
    void                Define( int* pnData );
    void                Define( long* pnData );
    void                Define( double* pnData );
    void                Define( char* pszData, int nSize = OWNAME );
    void                Define( OCILobLocator** pphLocator);
    void                Define( OCIArray** pphData );
    void                Define( sdo_georaster** pphData );
    void                Define( sdo_geometry** pphData );
    void                Define( sdo_pc** pphData );
    void                Define( OCILobLocator** pphLocator, long nIterations );
    void                BindName( const char* pszName, int* pnData );
    void                BindName( const char* pszName, double* pnData );
    void                BindName( const char* pszName, char* pszData,
                            int nSize = OWNAME );
    void                BindName( const char* pszName,
                            OCILobLocator** pphLocator );
    void                BindArray( void* pData, long nSize = 1);
    static void         Free( OCILobLocator** ppphLocator,
                            int nCount );
    unsigned long       ReadBlob( OCILobLocator* phLocator,
                            void* pBuffer, int nSize );
    char*               ReadCLob( OCILobLocator* phLocator );
    bool                WriteBlob( OCILobLocator* phLocator,
                            void* pBuffer, int nSize );
    int                 GetElement( OCIArray** ppoData,
                            int nIndex, int* pnResult );
    double              GetElement( OCIArray** ppoData,
                            int nIndex, double* pdfResult );
    void                AddElement( OCIArray* ppoData,
                            int nValue );
    void                AddElement( OCIArray* ppoData,
                            double dfValue );
};

#endif /* ifndef _ORCL_WRAP_H_INCLUDED */
