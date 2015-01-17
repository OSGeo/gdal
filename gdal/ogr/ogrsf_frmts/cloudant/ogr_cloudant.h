/******************************************************************************
 * $Id$
 *
 * Project:  Cloudant Translator
 * Purpose:  Definition of classes for OGR Cloudant driver.
 * Author:   Norman Barker, norman at cloudant com
 *           Based on the CouchDB driver
 *
 ******************************************************************************
 * Copyright (c) 2014, Norman Barker <norman at cloudant com>
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

#ifndef _OGR_CLOUDANT_H_INCLUDED
#define _OGR_CLOUDANT_H_INCLUDED

#include "ogr_couchdb.h"

typedef enum
{
    CLOUDANT_TABLE_LAYER
} CloudantLayerType;

class OGRCloudantDataSource;

/************************************************************************/
/*                      OGRCloudantTableLayer                         */
/************************************************************************/

class OGRCloudantTableLayer : public OGRCouchDBTableLayer
{
    int                       bHasStandardSpatial;
    const char*               pszSpatialView;
    char*                     pszSpatialDDoc;

    protected:
            virtual int               GetFeaturesToFetch() { 
               return atoi(CPLGetConfigOption("CLOUDANT_PAGE_SIZE", "200"));
            }

            virtual int               RunSpatialFilterQueryIfNecessary();
            virtual void              GetSpatialView();
            virtual void              WriteMetadata();
            virtual void              LoadMetadata();
            
    public:
            OGRCloudantTableLayer(OGRCloudantDataSource* poDS,
                                 const char* pszName);
            ~OGRCloudantTableLayer();
};

/************************************************************************/
/*                         OGRCloudantDataSource                        */
/************************************************************************/

class OGRCloudantDataSource : public OGRCouchDBDataSource
{
  protected:
            OGRLayer*    OpenDatabase(const char* pszLayerName = NULL);
  public:
                        OGRCloudantDataSource();
                        ~OGRCloudantDataSource();
    virtual int Open( const char * pszFilename, int bUpdateIn);                       
    virtual OGRLayer   *ICreateLayer( const char *pszName,
             OGRSpatialReference *poSpatialRef = NULL,
             OGRwkbGeometryType eGType = wkbUnknown,
             char ** papszOptions = NULL );
};

/************************************************************************/
/*                           OGRCloudantDriver                          */
/************************************************************************/

class OGRCloudantDriver : public OGRCouchDBDriver
{
  public:
                ~OGRCloudantDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual OGRDataSource*      CreateDataSource( const char * pszName,
                                                  char **papszOptions );
    virtual int                 TestCapability( const char * );

};

#endif /* ndef _OGR_CLOUDANT_H_INCLUDED */
