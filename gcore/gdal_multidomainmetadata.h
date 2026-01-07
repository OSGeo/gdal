/******************************************************************************
 *
 * Name:     gdal_multidomainmetadata.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALMultiDomainMetadata class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALMULTIDOMAINMETADATA_H_INCLUDED
#define GDALMULTIDOMAINMETADATA_H_INCLUDED

#include "cpl_port.h"
#include "cpl_string.h"

#include <map>

/*! @cond Doxygen_Suppress */
typedef struct CPLXMLNode CPLXMLNode;

/*! @endcond */

/************************************************************************/
/*                       GDALMultiDomainMetadata                        */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALMultiDomainMetadata
{
  private:
    CPLStringList aosDomainList{};

    struct Comparator
    {
        bool operator()(const char *a, const char *b) const
        {
            return STRCASECMP(a, b) < 0;
        }
    };

    std::map<const char *, CPLStringList, Comparator> oMetadata{};

  public:
    GDALMultiDomainMetadata();

    /** Copy constructor */
    GDALMultiDomainMetadata(const GDALMultiDomainMetadata &) = default;

    /** Copy assignment operator */
    GDALMultiDomainMetadata &
    operator=(const GDALMultiDomainMetadata &) = default;

    /** Move constructor */
    GDALMultiDomainMetadata(GDALMultiDomainMetadata &&) = default;

    /** Move assignment operator */
    GDALMultiDomainMetadata &operator=(GDALMultiDomainMetadata &&) = default;

    ~GDALMultiDomainMetadata();

    int XMLInit(const CPLXMLNode *psMetadata, int bMerge);
    CPLXMLNode *Serialize() const;

    CSLConstList GetDomainList() const
    {
        return aosDomainList.List();
    }

    char **GetMetadata(const char *pszDomain = "");
    CPLErr SetMetadata(CSLConstList papszMetadata, const char *pszDomain = "");
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") const;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "");

    void Clear();

    inline void clear()
    {
        Clear();
    }
};

//! @endcond

#endif
