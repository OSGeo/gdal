/******************************************************************************
 *
 * Name:     gdal_majorobject.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALMajorObject class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALMAJOROBJECT_H_INCLUDED
#define GDALMAJOROBJECT_H_INCLUDED

#include "cpl_port.h"
#include "cpl_string.h"

#include "gdal_fwd.h"  // for GDALMajorObjectH
#include "gdal_multidomainmetadata.h"

//! @cond Doxygen_Suppress
#define GMO_VALID 0x0001
#define GMO_IGNORE_UNIMPLEMENTED 0x0002
#define GMO_SUPPORT_MD 0x0004
#define GMO_SUPPORT_MDMD 0x0008
#define GMO_MD_DIRTY 0x0010
#define GMO_PAM_CLASS 0x0020

//! @endcond

/* ******************************************************************** */
/*                           GDALMajorObject                            */
/*                                                                      */
/*      Base class providing metadata, description and other            */
/*      services shared by major objects.                               */
/* ******************************************************************** */

/** Object with metadata. */
class CPL_DLL GDALMajorObject
{
  protected:
    //! @cond Doxygen_Suppress
    int nFlags;  // GMO_* flags.
    CPLString sDescription{};
    GDALMultiDomainMetadata oMDMD{};

    //! @endcond

    char **BuildMetadataDomainList(char **papszList, int bCheckNonEmpty,
                                   ...) CPL_NULL_TERMINATED;

    /** Copy constructor */
    GDALMajorObject(const GDALMajorObject &) = default;

    /** Copy assignment operator */
    GDALMajorObject &operator=(const GDALMajorObject &) = default;

    /** Move constructor */
    GDALMajorObject(GDALMajorObject &&) = default;

    /** Move assignment operator */
    GDALMajorObject &operator=(GDALMajorObject &&) = default;

  public:
    GDALMajorObject();
    virtual ~GDALMajorObject();

    int GetMOFlags() const;
    void SetMOFlags(int nFlagsIn);

    virtual const char *GetDescription() const;
    virtual void SetDescription(const char *);

    virtual char **GetMetadataDomainList();

    virtual CSLConstList GetMetadata(const char *pszDomain = "");
    virtual CPLErr SetMetadata(CSLConstList papszMetadata,
                               const char *pszDomain = "");
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "");
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "");

    /** Convert a GDALMajorObject* to a GDALMajorObjectH.
     */
    static inline GDALMajorObjectH ToHandle(GDALMajorObject *poMajorObject)
    {
        return static_cast<GDALMajorObjectH>(poMajorObject);
    }

    /** Convert a GDALMajorObjectH to a GDALMajorObject*.
     */
    static inline GDALMajorObject *FromHandle(GDALMajorObjectH hMajorObject)
    {
        return static_cast<GDALMajorObject *>(hMajorObject);
    }
};

#endif
