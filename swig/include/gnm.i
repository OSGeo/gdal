/******************************************************************************
 * $Id$
 *
 * Project:  GNM Core SWIG Interface declarations.
 * Purpose:  GNM declarations.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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
 *****************************************************************************/

#ifndef FROM_GDAL_I
%include "exception.i"
#endif
%include constraints.i

#if defined(SWIGCSHARP)
%module Gnm
#elif defined(SWIGPYTHON)
%module (package="osgeo") gnm
#else
%module gnm
#endif

#ifdef SWIGCSHARP
%include swig_csharp_extensions.i
#endif

#ifndef SWIGJAVA
%feature ("compactdefaultargs");
#endif

%{
#include <iostream>
using namespace std;

#define CPL_SUPRESS_CPLUSPLUS

#include "gdal.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"
#include "gnm_api.h"

typedef void GDALMajorObjectShadow;
typedef void GNMNetworkShadow;
typedef void GNMGenericNetworkShadow;

#ifdef DEBUG
typedef struct OGRSpatialReferenceHS OSRSpatialReferenceShadow;
typedef struct OGRDriverHS OGRDriverShadow;
typedef struct OGRDataSourceHS OGRDataSourceShadow;
typedef struct OGRLayerHS OGRLayerShadow;
typedef struct OGRFeatureHS OGRFeatureShadow;
typedef struct OGRFeatureDefnHS OGRFeatureDefnShadow;
typedef struct OGRGeometryHS OGRGeometryShadow;
typedef struct OGRCoordinateTransformationHS OSRCoordinateTransformationShadow;
typedef struct OGRCoordinateTransformationHS OGRCoordinateTransformationShadow;
typedef struct OGRFieldDefnHS OGRFieldDefnShadow;
#else
typedef void OSRSpatialReferenceShadow;
typedef void OGRDriverShadow;
typedef void OGRDataSourceShadow;
typedef void OGRLayerShadow;
typedef void OGRFeatureShadow;
typedef void OGRFeatureDefnShadow;
typedef void OGRGeometryShadow;
typedef void OSRCoordinateTransformationShadow;
typedef void OGRFieldDefnShadow;
#endif
typedef struct OGRStyleTableHS OGRStyleTableShadow;
typedef struct OGRGeomFieldDefnHS OGRGeomFieldDefnShadow;
%}

#if defined(SWIGPYTHON)
%include gnm_python.i
#elif defined(SWIGCSHARP)
//%include gnm_csharp.i
#elif defined(SWIGJAVA)
%include gnm_java.i
#else
%include gdal_typemaps.i
#endif

#define FROM_OGR_I
%import ogr.i


typedef int GNMDirection;
typedef int CPLErr;

//************************************************************************
//
// Define the MajorObject object
//
//************************************************************************
#if defined(SWIGPYTHON)
%{
#include "gdal.h"
%}
#define FROM_PYTHON_OGR_I
%include MajorObject.i
#undef FROM_PYTHON_OGR_I
#else /* defined(SWIGPYTHON) */
%import MajorObject.i
#endif /* defined(SWIGPYTHON) */

%feature("autodoc");

// Redefine AlgorithmType
%rename (GraphAlgorithm) GNMGraphAlgorithmType;
typedef enum
{
    GATDijkstraShortestPath = 1,
    GATKShortestPath = 2,
    GATConnectedComponents = 3
} GNMGraphAlgorithmType;

#define GNMGFID GIntBig
#define GNM_EDGE_DIR_BOTH       0   // bidirectional
#define GNM_EDGE_DIR_SRCTOTGT   1   // from source to target
#define GNM_EDGE_DIR_TGTTOSRC   2   // from target to source

#ifndef SWIGJAVA
%inline %{
  GNMNetworkShadow* CastToNetwork(GDALMajorObjectShadow* base) {
      return (GNMNetworkShadow*)GNMCastToNetwork((GDALMajorObjectH)base);
  }
%}

%inline %{
  GNMGenericNetworkShadow* CastToGenericNetwork(GDALMajorObjectShadow* base) {
      return (GNMGenericNetworkShadow*)GNMCastToGenericNetwork((GDALMajorObjectH)base);
  }
%}
#endif

/************************************************************************/
/*                       GNMNetworkShadow                               */
/************************************************************************/

%rename (Network) GNMNetworkShadow;

class GNMNetworkShadow : public GDALMajorObjectShadow
{
    GNMNetworkShadow(){}

    public:

    %extend
    {
        ~GNMNetworkShadow()
        {
            if ( GDALDereferenceDataset( self ) <= 0 ) {
              GDALClose(self);
            }
        }

#ifndef SWIGJAVA
        %apply SWIGTYPE *DISOWN {OGRLayerShadow *layer};
        void ReleaseResultSet(OGRLayerShadow *layer){
            GDALDatasetReleaseResultSet(self, layer);
        }
        %clear OGRLayerShadow *layer;
#endif

        int GetVersion()
        {
            return GNMGetVersion(self);
        }

        char const *GetName()
        {
            return GNMGetName(self);
        }

        %newobject GetFeatureByGlobalFID;
        OGRFeatureShadow *GetFeatureByGlobalFID (GNMGFID GFID)
        {
            return GNMGetFeatureByGlobalFID(self, GFID);
        }

        %newobject GetPath;
        #ifndef SWIGJAVA
        %feature( "kwargs" ) GetPath;
        #endif
        OGRLayerShadow *GetPath (GNMGFID nStartFID, GNMGFID nEndFID,
                                 GNMGraphAlgorithmType eAlgorithm,
                                 char **options = 0)
        {
            return GNMGetPath(self, nStartFID, nEndFID, eAlgorithm, options);
        }

        CPLErr DisconnectAll() {
            return GNMDisconnectAll( self );
        }

        char const *GetProjection() {
            return GDALGetProjectionRef( self );
        }

        char const *GetProjectionRef() {
            return GDALGetProjectionRef( self );
        }

        %apply (char **CSL) {char **};
        char **GetFileList() {
            return GDALGetFileList( self );
        }
        %clear char **;

        /* Note that datasources own their layers */
        #ifndef SWIGJAVA
        %feature( "kwargs" ) CreateLayer;
        #endif
        OGRLayerShadow *CreateLayer(const char* name,
              OSRSpatialReferenceShadow* srs=NULL,
              OGRwkbGeometryType geom_type=wkbUnknown,
              char** options=0) {
            OGRLayerShadow* layer = (OGRLayerShadow*) GDALDatasetCreateLayer( self,
                                      name,
                                      srs,
                                      geom_type,
                                      options);
            return layer;
        }

        #ifndef SWIGJAVA
        %feature( "kwargs" ) CopyLayer;
        #endif
        %apply Pointer NONNULL {OGRLayerShadow *src_layer};
        OGRLayerShadow *CopyLayer(OGRLayerShadow *src_layer,
            const char* new_name,
            char** options=0) {
            OGRLayerShadow* layer = (OGRLayerShadow*) GDALDatasetCopyLayer( self,
                                                      src_layer,
                                                      new_name,
                                                      options);
            return layer;
        }

        OGRErr DeleteLayer(int index){
            return GDALDatasetDeleteLayer(self, index);
        }

        int GetLayerCount() {
            return GDALDatasetGetLayerCount(self);
        }

        #ifdef SWIGJAVA
        OGRLayerShadow *GetLayerByIndex( int index ) {
        #else
        OGRLayerShadow *GetLayerByIndex( int index=0) {
        #endif
        OGRLayerShadow* layer = (OGRLayerShadow*) GDALDatasetGetLayer(self,
                                                                      index);
            return layer;
        }

        OGRLayerShadow *GetLayerByName( const char* layer_name) {
            OGRLayerShadow* layer =
                  (OGRLayerShadow*) GDALDatasetGetLayerByName(self, layer_name);
            return layer;
        }

        bool TestCapability(const char * cap) {
            return (GDALDatasetTestCapability(self, cap) > 0);
        }

        #ifndef SWIGJAVA
        %feature( "kwargs" ) StartTransaction;
        #endif
        OGRErr StartTransaction(int force = FALSE)
        {
            return GDALDatasetStartTransaction(self, force);
        }

        OGRErr CommitTransaction()
        {
            return GDALDatasetCommitTransaction(self);
        }

        OGRErr RollbackTransaction()
        {
            return GDALDatasetRollbackTransaction(self);
        }

    }
};

/************************************************************************/
/*                   GNMGenericNetworkShadow                            */
/************************************************************************/

%rename (GenericNetwork) GNMGenericNetworkShadow;

class GNMGenericNetworkShadow : public GNMNetworkShadow
{
    GNMGenericNetworkShadow(){}

    public:

    %extend
    {
        ~GNMGenericNetworkShadow()
        {
            if ( GDALDereferenceDataset( self ) <= 0 ) {
              GDALClose(self);
            }
        }

        CPLErr ConnectFeatures (GNMGFID nSrcFID, GNMGFID nTgtFID,
                                GNMGFID nConFID, double dfCost,
                                double dfInvCost,
                                GNMDirection eDir) {
            return GNMConnectFeatures(self, nSrcFID, nTgtFID,
                                              nConFID, dfCost, dfInvCost, eDir);
        }

        CPLErr DisconnectFeatures (GNMGFID nSrcFID, GNMGFID nTgtFID,
                                   GNMGFID nConFID) {
            return GNMDisconnectFeatures(self, nSrcFID, nTgtFID,
                                                           nConFID);
        }

        CPLErr DisconnectFeaturesWithId(GNMGFID nFID) {
            return GNMDisconnectFeaturesWithId(self, nFID);
        }

        CPLErr ReconnectFeatures (GNMGFID nSrcFID, GNMGFID nTgtFID, GNMGFID nConFID,
                                  double dfCost, double dfInvCost,
                                  GNMDirection eDir) {
            return GNMReconnectFeatures(self, nSrcFID, nTgtFID, nConFID, dfCost, dfInvCost, eDir);
        }

        %apply Pointer NONNULL {const char * pszRuleStr};
        CPLErr CreateRule (const char *pszRuleStr) {
            return GNMCreateRule(self, pszRuleStr);
        }
        %clear const char * pszRuleStr;

        CPLErr DeleteAllRules() {
            return GNMDeleteAllRules(self);
        }

        %apply Pointer NONNULL {const char * pszRuleStr};
        CPLErr DeleteRule(const char *pszRuleStr) {
            return GNMDeleteRule(self, pszRuleStr);
        }
        %clear const char * pszRuleStr;

        %apply (char **CSL) {char **};
        char** GetRules() {
            return GNMGetRules(self);
        }
        %clear char **;

        #ifndef SWIGJAVA
        %feature( "kwargs" ) ConnectPointsByLines;
        #endif
        %apply (char **options) { char ** papszLayerList };
        CPLErr ConnectPointsByLines (char **papszLayerList,
                                         double dfTolerance,
                                         double dfCost,
                                         double dfInvCost,
                                         GNMDirection eDir) {
            return GNMConnectPointsByLines(self, papszLayerList, dfTolerance, dfCost, dfInvCost, eDir);
        }
        %clear char **papszLayerList;

        CPLErr ChangeBlockState (GNMGFID nFID, bool bIsBlock) {
            return GNMChangeBlockState(self, nFID, bIsBlock);
        }

        CPLErr ChangeAllBlockState (bool bIsBlock = false) {
            return GNMChangeAllBlockState(self, bIsBlock);
        }
    }
};
