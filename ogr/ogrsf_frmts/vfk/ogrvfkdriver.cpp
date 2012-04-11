/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVFKDriver class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include "ogr_vfk.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRVFKDriver()                             */
/************************************************************************/
OGRVFKDriver::~OGRVFKDriver()
{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/
const char *OGRVFKDriver::GetName()
{
    return "VFK";
}

/*
  \brief Open existing data source
  
  \param pszFilename data source name to be open
  \param pUpdate non-zero for update, zero for read-only

  \return pointer to OGRDataSource instance
  \return NULL on failure
*/
OGRDataSource *OGRVFKDriver::Open(const char * pszFilename,
                                  int bUpdate)
{
    OGRVFKDataSource *poDS;

    if (bUpdate)
        return NULL;
    
    poDS = new OGRVFKDataSource();

    if(!poDS->Open(pszFilename, TRUE) || poDS->GetLayerCount() == 0) {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/*!
  \brief Test driver capability

  \param pszCap capability

  \return TRUE on success
  \return False on failure
*/
int OGRVFKDriver::TestCapability(const char *pszCap)
{
    return FALSE;
}

/*!
  \brief Register VFK driver
*/
void RegisterOGRVFK()
{
    if (!GDAL_CHECK_VERSION("OGR/VFK driver"))
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(new OGRVFKDriver);
}
