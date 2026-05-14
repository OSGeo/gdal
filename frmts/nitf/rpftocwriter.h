/******************************************************************************
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Creates A.TOC RPF index
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 **********************************************************************
 * Copyright (c) 2026, T-Kartor
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RPFTOC_WRITER_INCLUDED
#define RPFTOC_WRITER_INCLUDED

#include <string>

bool RPFTOCCreate(const std::string &osInputDirectory,
                  const std::string &osOutputFilename,
                  const char chIndexClassification, const int nScale,
                  const std::string &osProducerID,
                  const std::string &osProducerName,
                  const std::string &osSecurityCountryCode,
                  bool bDoNotCreateIfNoFrame);

#endif
