/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  HDF5 convenience functions.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GH5_CONVENIENCE_H_INCLUDED_
#define GH5_CONVENIENCE_H_INCLUDED_

#include "hdf5_api.h"

#include "cpl_string.h"
#include "gdal.h"

#include <stdint.h>

/* release 1.6.3 or 1.6.4 changed the type of count in some api functions */

#if H5_VERS_MAJOR == 1 && H5_VERS_MINOR <= 6 &&                                \
    (H5_VERS_MINOR < 6 || H5_VERS_RELEASE < 3)
#define H5OFFSET_TYPE hssize_t
#else
#define H5OFFSET_TYPE hsize_t
#endif

bool GH5_FetchAttribute(hid_t loc_id, const char *pszName, CPLString &osResult,
                        bool bReportError = false);
bool GH5_FetchAttribute(hid_t loc_id, const char *pszName, double &dfResult,
                        bool bReportError = false);
GDALDataType GH5_GetDataType(hid_t TypeID);
constexpr unsigned VARIABLE_LENGTH = UINT32_MAX;
bool GH5_CreateAttribute(hid_t loc_id, const char *pszAttrName, hid_t TypeID,
                         unsigned nMaxLen = 0);
bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName,
                        const char *pszValue);
bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName, double dfValue);
bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName, int nValue);
bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName, unsigned nValue);

/************************************************************************/
/*                                h5check()                             */
/************************************************************************/

#ifdef DEBUG
template <class T> static T h5check(T ret, const char *filename, int line)
{
    if (ret < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "HDF5 API failed at %s:%d",
                 filename, line);
    }
    return ret;
}

#define H5_CHECK(x) h5check(x, __FILE__, __LINE__)
#else
#define H5_CHECK(x) (x)
#endif

/************************************************************************/
/*                              GH5_HIDBaseHolder                       */
/************************************************************************/

template <herr_t (*closeFunc)(hid_t)> struct GH5_HIDBaseHolder /* non final */
{
    inline hid_t get() const
    {
        return m_hid;
    }

    inline operator bool() const
    {
        return m_hid >= 0;
    }

    inline operator hid_t() const
    {
        return m_hid;
    }

    inline ~GH5_HIDBaseHolder()
    {
        clear();
    }

    inline void reset(hid_t hid)
    {
        clear();
        m_hid = hid;
    }

    inline bool clear()
    {
        const bool ret = m_hid < 0 || H5_CHECK(closeFunc(m_hid)) >= 0;
        m_hid = -1;
        return ret;
    }

  protected:
    inline explicit GH5_HIDBaseHolder(hid_t hid) : m_hid(hid)
    {
    }

    hid_t m_hid = -1;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GH5_HIDBaseHolder)
};

struct GH5_HIDFileHolder : public GH5_HIDBaseHolder<H5Fclose>
{
    inline explicit GH5_HIDFileHolder(hid_t hid = -1) : GH5_HIDBaseHolder(hid)
    {
    }
};

struct GH5_HIDGroupHolder : public GH5_HIDBaseHolder<H5Gclose>
{
    inline explicit GH5_HIDGroupHolder(hid_t hid = -1) : GH5_HIDBaseHolder(hid)
    {
    }
};

struct GH5_HIDTypeHolder : public GH5_HIDBaseHolder<H5Tclose>
{
    inline explicit GH5_HIDTypeHolder(hid_t hid = -1) : GH5_HIDBaseHolder(hid)
    {
    }
};

struct GH5_HIDSpaceHolder : public GH5_HIDBaseHolder<H5Sclose>
{
    inline explicit GH5_HIDSpaceHolder(hid_t hid = -1) : GH5_HIDBaseHolder(hid)
    {
    }
};

struct GH5_HIDDatasetHolder : public GH5_HIDBaseHolder<H5Dclose>
{
    inline explicit GH5_HIDDatasetHolder(hid_t hid = -1)
        : GH5_HIDBaseHolder(hid)
    {
    }
};

struct GH5_HIDParametersHolder : public GH5_HIDBaseHolder<H5Pclose>
{
    inline explicit GH5_HIDParametersHolder(hid_t hid = -1)
        : GH5_HIDBaseHolder(hid)
    {
    }
};

// Silence "HDF5-DIAG: Error detected in HDF5" messages coming from libhdf4
struct GH5_libhdf5_error_silencer
{
    H5E_auto2_t old_func = nullptr;
    void *old_data = nullptr;

    GH5_libhdf5_error_silencer()
    {
        H5Eget_auto2(H5E_DEFAULT, &old_func, &old_data);
        H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);
    }

    ~GH5_libhdf5_error_silencer()
    {
        H5Eset_auto2(H5E_DEFAULT, old_func, old_data);
    }

    CPL_DISALLOW_COPY_ASSIGN(GH5_libhdf5_error_silencer)
};

#endif /* ndef GH5_CONVENIENCE_H_INCLUDED_ */
