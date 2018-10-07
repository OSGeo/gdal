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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
 *     CPLAutoClose<GDALDatasetH,void(*)(void*)> autoclosehDset(hDset,GDALClose);
 * Or:
 *     GDALDatasetH hDset = GDALOpen(path,GA_ReadOnly);
 *     CPL_AUTO_CLOSE_WARP(hDset,GDALClose);
 */
template<typename _Ty,typename _Dx>
class CPLAutoClose {
    static_assert( !std::is_const<_Ty>::value && std::is_pointer<_Ty>::value,
                    "_Ty must is pointer type,_Dx must is function type");
    private:
    _Ty& m_ResourcePtr;
    _Dx  m_CloseFunc;
    private:
    CPLAutoClose(const CPLAutoClose&) = delete;
    void operator=(const CPLAutoClose&) = delete;
    public:
        /**
         * @brief Constructor.
         * @param ptr Pointer to the resource object.
         * @param dt  Resource release(close) function.
         */
        explicit CPLAutoClose(_Ty& ptr,_Dx dt) :
            m_ResourcePtr(ptr),
            m_CloseFunc(dt)
        {}
        /**
         * @brief Destructor.
         */
        ~CPLAutoClose()
        {
            if(m_ResourcePtr && m_CloseFunc)
              m_CloseFunc(m_ResourcePtr);
        }
};

#define CPL_AUTO_CLOSE_WARP(hObject,closeFunc) \
    CPLAutoClose<decltype(hObject),decltype(closeFunc)*> tAutoClose##hObject(hObject,closeFunc)

#endif /* __cplusplus */

#endif /* CPL_AUTO_CLOSE_H_INCLUDED */
