/******************************************************************************
 * $Id$
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
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

#include "ogr_elastic.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         ~OGRElasticDriver()                          */
/************************************************************************/

OGRElasticDriver::~OGRElasticDriver() {
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRElasticDriver::GetName() {
    return "ElasticSearch";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRElasticDriver::Open(CPL_UNUSED const char * pszFilename,
                                      CPL_UNUSED int bUpdate) {
    return NULL;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRElasticDriver::CreateDataSource(const char * pszName,
        char **papszOptions) {
    OGRElasticDataSource *poDS = new OGRElasticDataSource();

    if (!poDS->Create(pszName, papszOptions)) {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRElasticDriver::TestCapability(const char * pszCap) {
    if (EQUAL(pszCap, ODrCCreateDataSource))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          RegisterOGRElastic()                        */
/************************************************************************/

void RegisterOGRElastic() {
    if (!GDAL_CHECK_VERSION("OGR/Elastic Search driver"))
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(new OGRElasticDriver);
}

