/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Common services for DGNv8/DWG drivers
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_TEIGHA_H
#define OGR_TEIGHA_H

class OGRDWGServices;
class OGRDGNV8Services;

bool OGRTEIGHAInitialize();
void OGRTEIGHADeinitialize();
OGRDWGServices *OGRDWGGetServices();
OGRDGNV8Services *OGRDGNV8GetServices();

#endif  // OGR_TEIGHA_H
