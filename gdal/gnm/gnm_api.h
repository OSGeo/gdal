/******************************************************************************
 * $Id$
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM C API.
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
 ****************************************************************************/

#ifndef GNM_API
#define GNM_API

#include "gnm.h"

CPL_C_START

typedef void *GNMNetworkH;
typedef void *GNMGenericNetworkH;

const char CPL_DLL * CPL_STDCALL GNMGetName (GNMNetworkH hNet);

int CPL_DLL CPL_STDCALL GNMGetVersion(GNMNetworkH hNet);

CPLErr CPL_DLL CPL_STDCALL GNMDisconnectAll (GNMNetworkH hNet);

OGRFeatureH CPL_DLL CPL_STDCALL GNMGetFeatureByGlobalFID (GNMNetworkH hNet, GNMGFID nGFID);

OGRLayerH CPL_DLL CPL_STDCALL GNMGetPath (GNMNetworkH hNet, GNMGFID nStartFID,
                              GNMGFID nEndFID, GNMGraphAlgorithmType eAlgorithm,
                              char** papszOptions);



CPLErr CPL_DLL CPL_STDCALL GNMConnectFeatures (GNMGenericNetworkH hNet,
                                               GNMGFID nSrcFID, GNMGFID nTgtFID,
                                               GNMGFID nConFID, double dfCost,
                                               double dfInvCost,
                                               GNMDirection eDir);

CPLErr CPL_DLL CPL_STDCALL GNMDisconnectFeatures (GNMGenericNetworkH hNet,
                                                  GNMGFID nSrcFID, GNMGFID nTgtFID,
                                                  GNMGFID nConFID);

CPLErr CPL_DLL CPL_STDCALL GNMDisconnectFeaturesWithId(GNMGenericNetworkH hNet,
                                                       GNMGFID nFID);


CPLErr CPL_DLL CPL_STDCALL GNMReconnectFeatures (GNMGenericNetworkH hNet,
                                                 GNMGFID nSrcFID, GNMGFID nTgtFID,
                                                 GNMGFID nConFID, double dfCost,
                                                 double dfInvCost, GNMDirection eDir);

CPLErr CPL_DLL CPL_STDCALL GNMCreateRule (GNMGenericNetworkH hNet,
                                          const char *pszRuleStr);

CPLErr CPL_DLL CPL_STDCALL GNMDeleteAllRules(GNMGenericNetworkH hNet);

CPLErr CPL_DLL CPL_STDCALL GNMDeleteRule(GNMGenericNetworkH hNet,
                                         const char *pszRuleStr);

char CPL_DLL ** CPL_STDCALL GNMGetRules(GNMGenericNetworkH hNet);

CPLErr CPL_DLL CPL_STDCALL GNMConnectPointsByLines (GNMGenericNetworkH hNet,
                                                    char **papszLayerList,
                                                    double dfTolerance,
                                                    double dfCost,
                                                    double dfInvCost,
                                                    GNMDirection eDir);

CPLErr CPL_DLL CPL_STDCALL GNMChangeBlockState (GNMGenericNetworkH hNet,
                                                GNMGFID nFID, bool bIsBlock);

CPLErr CPL_DLL CPL_STDCALL GNMChangeAllBlockState (GNMGenericNetworkH hNet,
                                                   int bIsBlock);

CPL_C_END

#endif // GNM_API
