/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR DWG/DXF Driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.5  2005/11/18 21:26:25  fwarmerdam
 * added geometry collection type support
 *
 * Revision 1.4  2005/11/15 23:23:38  fwarmerdam
 * update extents, preliminary point support
 *
 * Revision 1.3  2005/11/15 15:02:42  fwarmerdam
 * Added ODRX_USING_HEAP_OPERATORS() definition for OGRServices.
 *
 * Revision 1.2  2005/11/10 21:31:48  fwarmerdam
 * preliminary version
 *
 * Revision 1.1  2005/11/07 04:43:05  fwarmerdam
 * New
 *
 */

#ifndef _OGR_DWG_H_INCLUDED
#define _OGR_DWG_H_INLLUDED

#include "ogrsf_frmts.h"
#include "cpl_string.h"

#include "OdaCommon.h"
#include "DbDatabase.h"
#include "DbAudit.h"

#include "StaticRxObject.h"
#include "DbHostAppServices.h"
#include "Gs/Gs.h"
#include "OdFileBuf.h"
//#include "ExSystemServices.h"
//#include "ExHostAppServices.h"
#include "DynamicLinker.h"

#include "RxDynamicModule.h"
#include "DbViewport.h"

#ifdef notdef
class MyServices : public ExSystemServices, public ExHostAppServices
{
protected:
  ODRX_USING_HEAP_OPERATORS(ExSystemServices);
};
#endif

/************************************************************************/
/*                              OGRServices                             */
/************************************************************************/

class OGRServices : public OdDbHostAppServices, public OdDbSystemServices

{
protected:
  ODRX_USING_HEAP_OPERATORS(OdDbSystemServices);

public:
    virtual bool ttfFileNameByDescriptor(const OdTtfDescriptor& descr, 
                                         OdString& fileName) 
        { return false; }

    virtual OdHatchPatternManager* patternManager() 
        { return NULL; }

    virtual OdGsDevicePtr gsBitmapDevice() { return OdGsDevicePtr(); }

    OdDbDatabasePtr readFile(const OdChar* fileName,
                             bool bAllowCPConversion = false,
                             bool bPartial = false,
                             Oda::FileShareMode shmode = Oda::kShareDenyNo,
                             const OdPassword& password = OdPassword()) {
        OdDbDatabasePtr pRes;
        pRes = OdDbHostAppServices::readFile(fileName, bAllowCPConversion, 
                                             bPartial, shmode, password);
        return pRes;
    }

    virtual OdStreamBufPtr createFile(
        const OdChar* pcFilename,                   // file name
        Oda::FileAccessMode nDesiredAccess,
        Oda::FileShareMode nShareMode,
        Oda::FileCreationDisposition nCreationDisposition)

        {
            OdSmartPtr<OdBaseFileBuf> pFile;
            
            if(pcFilename && pcFilename[0])
            {
                if (nDesiredAccess == Oda::kFileRead)
                {
                    pFile = OdRdFileBuf::createObject();
                }
                else
                {
                    pFile = OdWrFileBuf::createObject();
                }

                pFile->open(pcFilename, nShareMode, nDesiredAccess, nCreationDisposition);
            }
            else
            {
                throw OdError(eNoFileName);
            }
            return OdStreamBufPtr(pFile);
        }

    OdCodePageId systemCodePage() const 
        { return CP_UNDEFINED; }

    void setSystemCodePage(OdCodePageId id) 
        { CPLDebug( "DWG", "setSystemCodePage" ); }

    OdString formatMessage(unsigned int code, va_list* argList = 0)
        { 
            static const OdChar* message[] =
                {
#define OD_ERROR_DEF(cod, desc)  desc,
#include "ErrorDefs.h"
#undef OD_ERROR_DEF

#define OD_MESSAGE_DEF(cod, desc) desc,
#include "MessageDefs.h"
#undef OD_MESSAGE_DEF
                    ""// DummyLastMassage
                };
            OdString msg;
            if (argList)
                msg.formatV(message[code], *argList);
            else 
                msg = message[code];
            return msg;
        }

    virtual bool accessFile(const OdChar* pcFilename, int mode)
        {  
            VSIStatBufL sStatBuf;
            if( VSIStatL( pcFilename, &sStatBuf ) == 0 )
                return true;
            else
                return false;
        }

    long getFileCTime(const OdChar* name)
        {
            VSIStatBufL sStatBuf;
            if( VSIStatL( name, &sStatBuf ) == 0 )
                return sStatBuf.st_ctime;
            else
                return 0;
        }

    long getFileMTime(const OdChar* name)
        {
            VSIStatBufL sStatBuf;
            if( VSIStatL( name, &sStatBuf ) == 0 )
                return sStatBuf.st_mtime;
            else
                return 0;
        }

    OdInt64 getFileSize(const OdChar* name)
        {
            VSIStatBufL sStatBuf;
            if( VSIStatL( name, &sStatBuf ) == 0 )
                return sStatBuf.st_size;
            else
                return 0;
        }
};

/************************************************************************/
/*                         OGRWritableDWGLayer                          */
/************************************************************************/

class OGRWritableDWGDataSource;

class OGRWritableDWGLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;
    OdDbObjectId        hLayerId;
    OdDbDatabasePtr     pDb;

    OGRWritableDWGDataSource    *poDS;

    char              **papszOptions;

    OGRErr       WriteEntity( OGRGeometry * );

  public:
                        OGRWritableDWGLayer( const char *pszLayerName, 
                                             char **papszOptions, 
                                             OGRWritableDWGDataSource *poDS );
    virtual             ~OGRWritableDWGLayer();

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int         TestCapability( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                       OGRWritableDWGDataSource                       */
/************************************************************************/

class OGRWritableDWGDataSource : public OGRDataSource
{
    friend class OGRWritableDWGLayer;

    OGRWritableDWGLayer **papoLayers;
    int                 nLayers;

    CPLString           osFilename;
    CPLString           osOutClass;

    char              **papszOptions;

    OdDbDatabasePtr     pDb;
    OdDbViewportPtr 	pVp;
    OdDbViewportPtr 	pVm;

    OdDbBlockTableRecordPtr pPs;  // paper space
    OdDbBlockTableRecordPtr pMs;  // model space
    
    OdStaticRxObject<OGRServices> svcs;

    OGREnvelope         sExtent;
    void                ExtendExtent( OGRGeometry * );

  public:
                        OGRWritableDWGDataSource( const char *pszOutClass );
                        ~OGRWritableDWGDataSource();

    int                 Create( const char *pszFilename, char **papszOptions );

    const char          *GetName();
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRDWGDriver                              */
/************************************************************************/

class OGRDWGDriver : public OGRSFDriver
{
    CPLString    osOutClass;

  public:
    		OGRDWGDriver( const char * );
                ~OGRDWGDriver();

    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszFilename,
                                             char **papszOptions = NULL );

    int                 TestCapability( const char * );
};


#endif /* ndef _OGR_DWG_H_INCLUDED */


