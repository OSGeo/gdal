/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  WMSMiniDriverManager implementation.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 *
 * Copyright (c) 2007, Adam Nowacki
 *               2016, Lucian Plesea
 *
 * A single global MiniDriverManager exists, containing factories for all possible
 * types of WMS minidrivers.  Minidriver object factories get registered in wmsdriver.cpp,
 * during the WMS driver registration with GDAL
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

#include "wmsdriver.h"

CPL_CVSID("$Id$")

class WMSMiniDriverManager {
public:
    WMSMiniDriverManager() {}
    ~WMSMiniDriverManager() { erase(); }

public:
    void Register(WMSMiniDriverFactory *mdf);
    // Clean up the minidriver factories
    void erase();
    WMSMiniDriverFactory *Find(const CPLString &name);

protected:
    std::vector<WMSMiniDriverFactory *> m_mdfs;
};

// Called by WMS driver deregister, also by destructor
void WMSMiniDriverManager::erase() {
    for (size_t i = 0; i < m_mdfs.size(); i++)
        delete m_mdfs[i];
    m_mdfs.clear();
}

WMSMiniDriverFactory *WMSMiniDriverManager::Find(const CPLString &name) {
    for (size_t i = 0; i < m_mdfs.size(); i++)
        if (EQUAL(name.c_str(), m_mdfs[i]->m_name))
            return m_mdfs[i];
    return NULL;
}

void WMSMiniDriverManager::Register(WMSMiniDriverFactory *mdf) {
    // Prevent duplicates
    if (!Find(mdf->m_name))
        m_mdfs.push_back(mdf);
    else // Register takes ownership of factories, so it removes the duplicate
        delete mdf;
}

// global object containing minidriver factories
static WMSMiniDriverManager g_mini_driver_manager;

// If a matching factory is found in the global minidriver manager, it returns a new minidriver object
WMSMiniDriver *NewWMSMiniDriver(const CPLString &name) {
    const WMSMiniDriverFactory *factory = g_mini_driver_manager.Find(name);
    if (factory == NULL) return NULL;
    return factory->New();
}

// Registers a minidriver factory with the global minidriver manager
void WMSRegisterMiniDriverFactory(WMSMiniDriverFactory *mdf) {
    g_mini_driver_manager.Register(mdf);
}

void WMSDeregisterMiniDrivers(CPL_UNUSED GDALDriver *) {
    g_mini_driver_manager.erase();
}
