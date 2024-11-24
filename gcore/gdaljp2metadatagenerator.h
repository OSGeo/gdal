/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALJP2Metadata: metadata generator
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, European Union Satellite Centre
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_JP2METADATA_GENERATOR_H_INCLUDED
#define GDAL_JP2METADATA_GENERATOR_H_INCLUDED

#include "cpl_string.h"
#include "cpl_minixml.h"

CPLXMLNode *GDALGMLJP2GenerateMetadata(const CPLString &osTemplateFile,
                                       const CPLString &osSourceFile);

#endif /* GDAL_JP2METADATA_GENERATOR_H_INCLUDED */
