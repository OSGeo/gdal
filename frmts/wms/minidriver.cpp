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
 * A single global MiniDriverManager exists, containing factories for all
 *possible types of WMS minidrivers.  Minidriver object factories get registered
 *in wmsdriver.cpp, during the WMS driver registration with GDAL
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmsdriver.h"

class WMSMiniDriverManager
{
  public:
    WMSMiniDriverManager()
    {
    }

    ~WMSMiniDriverManager()
    {
        erase();
    }

  public:
    void Register(WMSMiniDriverFactory *mdf);
    // Clean up the minidriver factories
    void erase();
    WMSMiniDriverFactory *Find(const CPLString &name);

  protected:
    std::vector<WMSMiniDriverFactory *> m_mdfs;
};

// Called by WMS driver deregister, also by destructor
void WMSMiniDriverManager::erase()
{
    for (size_t i = 0; i < m_mdfs.size(); i++)
        delete m_mdfs[i];
    m_mdfs.clear();
}

WMSMiniDriverFactory *WMSMiniDriverManager::Find(const CPLString &name)
{
    for (size_t i = 0; i < m_mdfs.size(); i++)
        if (EQUAL(name.c_str(), m_mdfs[i]->m_name))
            return m_mdfs[i];
    return nullptr;
}

void WMSMiniDriverManager::Register(WMSMiniDriverFactory *mdf)
{
    // Prevent duplicates
    if (!Find(mdf->m_name))
        m_mdfs.push_back(mdf);
    else  // Register takes ownership of factories, so it removes the duplicate
        delete mdf;
}

// global object containing minidriver factories
static WMSMiniDriverManager g_mini_driver_manager;

// If a matching factory is found in the global minidriver manager, it returns a
// new minidriver object
WMSMiniDriver *NewWMSMiniDriver(const CPLString &name)
{
    const WMSMiniDriverFactory *factory = g_mini_driver_manager.Find(name);
    if (factory == nullptr)
        return nullptr;
    return factory->New();
}

// Registers a minidriver factory with the global minidriver manager
void WMSRegisterMiniDriverFactory(WMSMiniDriverFactory *mdf)
{
    g_mini_driver_manager.Register(mdf);
}

void WMSDeregisterMiniDrivers(CPL_UNUSED GDALDriver *)
{
    g_mini_driver_manager.erase();
}
