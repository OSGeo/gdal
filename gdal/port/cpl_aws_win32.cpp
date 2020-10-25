/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Fetch Windows UUID using WMI COM interface.
 * Author:   rprinceley@esri.com
 *
 **********************************************************************
 * Copyright (c) 2020, Robin Princeley <rprinceley@esri.com>
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

#include "cpl_port.h"

#if defined(WIN32) && defined(_MSC_VER)

#define _WIN32_DCOM
#include <iostream>
#include <mutex>

#include <comdef.h>
#include <Wbemidl.h>
#include <windows.h>
#include <comutil.h>
#include <atlbase.h>

#include "cpl_string.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$")

static CPLString osWindowsProductUUID;

static void FetchUUIDFunc(void*)
{
    HRESULT hResult = CoInitializeEx(0, COINIT_MULTITHREADED);
    if( FAILED(hResult) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to initialize COM library, HRESULT = %d", hResult);
        return;
    }
    else
    {
        CComPtr<IWbemLocator> poLocator;
        hResult = poLocator.CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER);
        if( FAILED(hResult) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to create instance of IWbemLocator, HRESULT = %d", hResult);
            goto com_cleanup;
        }

        CComPtr<IWbemServices> pSvc;
        hResult = poLocator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
        if( FAILED(hResult) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to create connect to WMI server, HRESULT = %d", hResult);
            goto com_cleanup;
        }

        CPLDebug("CPLFetchWindowsProductUUID", "Connected to ROOT\\CIMV2 WMI namespace");

        hResult = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL,
                                    RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        if( FAILED(hResult) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to set proxy blanket, HRESULT = %d", hResult);
            goto com_cleanup;
        }

        CComPtr<IEnumWbemClassObject> pEnumerator;
        hResult = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT UUID FROM Win32_ComputerSystemProduct"),
                                  WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
        if( FAILED(hResult) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Query for UUID in Win32_ComputerSystemProduct failed, HRESULT = %d", hResult);
            goto com_cleanup;
        }

        if( pEnumerator )
        {
            CComPtr<IWbemClassObject> poClassObject;
            ULONG uReturn = 0;
            hResult = pEnumerator->Next(WBEM_INFINITE, 1, &poClassObject, &uReturn);
            if( SUCCEEDED(hResult) && uReturn )
            {
                CComVariant poVariant;
                hResult = poClassObject->Get(L"UUID", 0, &poVariant, 0, 0);
                if( SUCCEEDED(hResult) )
                {
                    bstr_t bString(poVariant.bstrVal);
                    if( bString.length() )
                        osWindowsProductUUID = static_cast<const char*>(bString);
                }
            }
        }
    }

    if( !osWindowsProductUUID.empty() )
        CPLDebug("CPLFetchWindowsProductUUID", "Succeeded in querying UUID from WMI.");

com_cleanup:
    CoUninitialize();
}

bool CPLFetchWindowsProductUUID(CPLString &osStr);

bool CPLFetchWindowsProductUUID(CPLString &osStr)
{
    static std::mutex gMutex;
    std::lock_guard<std::mutex> oGuard(gMutex);
    static bool bAttemptedGUID = false;

    if( !bAttemptedGUID )
    {
        // All COM work is in a different thread to avoid potential problems:
        // the calling thread may or may not have already initialized COM,
        // and the threading model may differ.
        auto pThread = CPLCreateJoinableThread(FetchUUIDFunc, nullptr);
        CPLJoinThread(pThread);
        bAttemptedGUID = true;
    }

    osStr = osWindowsProductUUID;
    return !osWindowsProductUUID.empty();
}

#endif /* defined(WIN32) && defined(_MSC_VER) */
