// SPDX-License-Identifier: MIT
// Copyright 1999-2003, Daniel Morissette
// Copyright (c) 1999-2001, Frank Warmerdam
// Implementation translation between MIF CoordSys format, and
// and OGRSpatialRef format.

#ifndef OGRMITABSPATIALREF_H_INCLUDED
#define OGRMITABSPATIALREF_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include "cpl_port.h"

class OGRSpatialReference;

/*---------------------------------------------------------------------
 * TABProjInfo
 * struct used to store the projection parameters from the .MAP header
 *--------------------------------------------------------------------*/
typedef struct TABProjInfo_t
{
    GByte nProjId;  // See MapInfo Ref. Manual, App. F and G
    GByte nEllipsoidId;
    GByte nUnitsId;
    double adProjParams[7];  // params in same order as in .MIF COORDSYS

    GInt16 nDatumId;      // Datum Id added in MapInfo 7.8+ (.map V500)
    double dDatumShiftX;  // Before that, we had to always lookup datum
    double dDatumShiftY;  // parameters to establish datum id
    double dDatumShiftZ;
    double adDatumParams[5];

    // Affine parameters only in .map version 500 and up
    GByte nAffineFlag;  // 0=No affine param, 1=Affine params
    GByte nAffineUnits;
    double dAffineParamA;  // Affine params
    double dAffineParamB;
    double dAffineParamC;
    double dAffineParamD;
    double dAffineParamE;
    double dAffineParamF;
} TABProjInfo;

OGRSpatialReference CPL_DLL *
TABFileGetSpatialRefFromTABProj(const TABProjInfo &sTABProj);

int CPL_DLL
TABFileGetTABProjFromSpatialRef(const OGRSpatialReference *poSpatialRef,
                                TABProjInfo &sTABProj, int &nParamCount);

OGRSpatialReference CPL_DLL *MITABCoordSys2SpatialRef(const char *pszCoordSys);

char CPL_DLL *MITABSpatialRef2CoordSys(const OGRSpatialReference *poSR);

bool CPL_DLL MITABExtractCoordSysBounds(const char *pszCoordSys, double &dXMin,
                                        double &dYMin, double &dXMax,
                                        double &dYMax);

int CPL_DLL MITABCoordSys2TABProjInfo(const char *pszCoordSys,
                                      TABProjInfo *psProj);

/*---------------------------------------------------------------------
 * The following are used for coordsys bounds lookup
 *--------------------------------------------------------------------*/

bool CPL_DLL MITABLookupCoordSysBounds(TABProjInfo *psCS, double &dXMin,
                                       double &dYMin, double &dXMax,
                                       double &dYMax,
                                       bool bOnlyUserTable = false);
int CPL_DLL MITABLoadCoordSysTable(const char *pszFname);
void CPL_DLL MITABFreeCoordSysTable();

/*! @endcond */

#endif
