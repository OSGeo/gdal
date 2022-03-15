/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Common services for DGNv8/DWG drivers
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_multiproc.h"

#include "ogr_dgnv8.h"
#include "ogr_dwg.h"
#include "ogrteigha.h"

extern "C" void CPL_DLL RegisterOGRDWG_DGNV8();

static CPLMutex* hMutex = nullptr;
static bool bInitialized = false;
static bool bInitSuccess = false;

// We used to define the 2 below objects as static in the global scope
// However with ODA 2022 on Linux, when OGRTEIGHADeinitialize() is not called,
// on unloading of the ODA libraries, a crash occurred when freeing a singleton
// inside one of the ODA libraries. It turns out that if the 2 below objects
// are kept alive, the crash doesn't occur.
struct OGRODAServicesWrapper
{
    OdStaticRxObject<OGRDWGServices> oDWGServices;
    OdStaticRxObject<OGRDGNV8Services> oDGNServices;
};

static OGRODAServicesWrapper* poServicesWrapper = nullptr;

/************************************************************************/
/*                        OGRTEIGHAErrorHandler()                       */
/************************************************************************/

static void OGRTEIGHAErrorHandler( OdResult oResult )

{
    CPLError( CE_Failure, CPLE_AppDefined,
              "GeError:%ls",
              OdError(oResult).description().c_str() );
}

/************************************************************************/
/*                        OGRTEIGHAInitialize()                         */
/************************************************************************/

#ifndef _TOOLKIT_IN_DLL_
// Define module map for statically linked modules:
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdDgnModule);
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(BitmapModule);
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdRecomputeDimBlockModule);
ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(ModelerModule);
ODRX_BEGIN_STATIC_MODULE_MAP()
    ODRX_DEFINE_STATIC_APPMODULE(L"TG_Db", OdDgnModule)
    ODRX_DEFINE_STATIC_APPMODULE(OdWinBitmapModuleName, BitmapModule)
    ODRX_DEFINE_STATIC_APPMODULE(OdRecomputeDimBlockModuleName, OdRecomputeDimBlockModule)
    ODRX_DEFINE_STATIC_APPMODULE(OdModelerGeometryModuleName, ModelerModule)
ODRX_END_STATIC_MODULE_MAP()
#endif

bool OGRTEIGHAInitialize()
{
    CPLMutexHolderD(&hMutex);
    if( bInitialized )
        return bInitSuccess;

#ifndef _TOOLKIT_IN_DLL_
    //Additional static modules initialization
    ODRX_INIT_STATIC_MODULE_MAP();
#endif

    bInitialized = true;

    OdGeContext::gErrorFunc = OGRTEIGHAErrorHandler;

    try
    {
        poServicesWrapper = new OGRODAServicesWrapper();

        odInitialize(&poServicesWrapper->oDWGServices);
        poServicesWrapper->oDWGServices.disableOutput( true );

        /********************************************************************/
        /* Find the data file and and initialize the character mapper       */
        /********************************************************************/
        OdString iniFile = poServicesWrapper->oDWGServices.findFile(OD_T("adinit.dat"));
        if (!iniFile.isEmpty())
            OdCharMapper::initialize(iniFile);

        odrxInitialize(&poServicesWrapper->oDGNServices);
        poServicesWrapper->oDGNServices.disableProgressMeterOutput( true );

        ::odrxDynamicLinker()->loadModule(L"TG_Db", false);

        bInitSuccess = true;
    }
    catch ( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An exception occurred in OGRTEIGHAInitialize(): %s", e.what());
    }
    catch ( ... )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An exception occurred in OGRTEIGHAInitialize()");
    }
    return bInitSuccess;
}

/************************************************************************/
/*                        OGRDWGGetServices()                           */
/************************************************************************/

OGRDWGServices* OGRDWGGetServices()
{
    return poServicesWrapper ? &poServicesWrapper->oDWGServices : nullptr;
}

/************************************************************************/
/*                        OGRDGNV8GetServices()                         */
/************************************************************************/

OGRDGNV8Services* OGRDGNV8GetServices()
{
    return poServicesWrapper ? &poServicesWrapper->oDGNServices : nullptr;
}

/************************************************************************/
/*                       OGRTEIGHADeinitialize()                        */
/************************************************************************/

void OGRTEIGHADeinitialize()
{
    if( bInitSuccess )
    {
        odUninitialize();
        odrxUninitialize();
    }
    delete poServicesWrapper;
    poServicesWrapper = nullptr;
    bInitialized = false;
    bInitSuccess = false;
    if( hMutex != nullptr )
        CPLDestroyMutex(hMutex);
    hMutex = nullptr;
}

/************************************************************************/
/*                       RegisterOGRDWG_DGNV8()                         */
/*                                                                      */
/* Entry point for the plugin                                           */
/************************************************************************/

void RegisterOGRDWG_DGNV8()

{
    RegisterOGRDWG();
    RegisterOGRDGNV8();
}
