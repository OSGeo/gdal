/**********************************************************************
 * $Id$
 *
 * Name:     cpl_auto_close.h
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL Auto Close handling
 * Author:   Liu Yimin, ymwh@foxmail.com
 *
 **********************************************************************
 * Copyright (c) 2018, Liu Yimin
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_AUTO_CLOSE_H_INCLUDED
#define CPL_AUTO_CLOSE_H_INCLUDED

#if defined(__cplusplus)
#include <type_traits>

/************************************************************************/
/*                           CPLAutoClose                               */
/************************************************************************/

/**
 * The class use the destructor to automatically close the resource.
 * Example:
 *     GDALDatasetH hDset = GDALOpen(path,GA_ReadOnly);
 *     CPLAutoClose<GDALDatasetH,void(*)(void*)>
 * autoclosehDset(hDset,GDALClose); Or: GDALDatasetH hDset =
 * GDALOpen(path,GA_ReadOnly); CPL_AUTO_CLOSE_WARP(hDset,GDALClose);
 */
template <typename _Ty, typename _Dx> class CPLAutoClose
{
    static_assert(!std::is_const<_Ty>::value && std::is_pointer<_Ty>::value,
                  "_Ty must is pointer type,_Dx must is function type");

  private:
    _Ty &m_ResourcePtr;
    _Dx m_CloseFunc;

  private:
    CPLAutoClose(const CPLAutoClose &) = delete;
    void operator=(const CPLAutoClose &) = delete;

  public:
    /**
     * @brief Constructor.
     * @param ptr Pointer to the resource object.
     * @param dt  Resource release(close) function.
     */
    explicit CPLAutoClose(_Ty &ptr, _Dx dt)
        : m_ResourcePtr(ptr), m_CloseFunc(dt)
    {
    }

    /**
     * @brief Destructor.
     */
    ~CPLAutoClose()
    {
        if (m_ResourcePtr && m_CloseFunc)
            m_CloseFunc(m_ResourcePtr);
    }
};

#define CPL_AUTO_CLOSE_WARP(hObject, closeFunc)                                \
    CPLAutoClose<decltype(hObject), decltype(closeFunc) *>                     \
        tAutoClose##hObject(hObject, closeFunc)

#endif /* __cplusplus */

#endif /* CPL_AUTO_CLOSE_H_INCLUDED */
