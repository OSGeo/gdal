/******************************************************************************
 * Project:  Selafin importer
 * Purpose:  Implementation of OGR driver for Selafin files.
 * Author:   François Hissel, francois.hissel@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2014,  François Hissel <francois.hissel@gmail.com>
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

#include "ogr_selafin.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "io_selafin.h"

/************************************************************************/
/*                        ~OGRSelafinDriver()                           */
/************************************************************************/

OGRSelafinDriver::~OGRSelafinDriver() { }

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRSelafinDriver::GetName() {
    return "Selafin";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRSelafinDriver::Open( const char * pszFilename, int bUpdate ) {
    OGRSelafinDataSource *poDS = new OGRSelafinDataSource();
    if( !poDS->Open(pszFilename, bUpdate) ) {
        delete poDS;
        poDS = NULL;
    }
    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRSelafinDriver::CreateDataSource( const char * pszName, char **papszOptions ) {
    // First, ensure there isn't any such file yet.
    VSIStatBufL sStatBuf;
    if (strcmp(pszName, "/dev/stdout") == 0) pszName = "/vsistdout/";
    if( VSIStatL( pszName, &sStatBuf ) == 0 ) {
        CPLError(CE_Failure, CPLE_AppDefined,"It seems a file system object called '%s' already exists.",pszName);
        return NULL;
    }
    // Parse options
    const char *pszTemp=CSLFetchNameValue(papszOptions,"TITLE");
    char pszTitle[80];
    long pnDate[6]={-1,0};
    if (pszTemp!=0) strncpy(pszTitle,pszTemp,72); else memset(pszTitle,' ',72);
    pszTemp=CSLFetchNameValue(papszOptions,"DATE");
    if (pszTemp!=0) {
        const char* pszErrorMessage="Wrong format for date parameter: must be \"%%Y-%%m-%%d_%%H:%%M:%%S\", ignored";
        const char *pszc=pszTemp;
        pnDate[0]=atoi(pszTemp);
        if (pnDate[0]<=0) CPLError(CE_Warning, CPLE_AppDefined,"%s",pszErrorMessage); else {
            if (pnDate[0]<100) pnDate[0]+=2000; 
        }
        while (*pszc!=0 && *pszc!='-') ++pszc;
        pnDate[1]=atoi(pszc);
        if (pnDate[1]<0 || pnDate[1]>12) CPLError(CE_Warning, CPLE_AppDefined,"%s",pszErrorMessage);
        while (*pszc!=0 && *pszc!='_') ++pszc;
        pnDate[2]=atoi(pszc);
        if (pnDate[2]<0 || pnDate[2]>59) CPLError(CE_Warning, CPLE_AppDefined,"%s",pszErrorMessage);
        while (*pszc!=0 && *pszc!='_') ++pszc;
        pnDate[3]=atoi(pszc);
        if (pnDate[3]<0 || pnDate[3]>23) CPLError(CE_Warning, CPLE_AppDefined,"%s",pszErrorMessage);
        while (*pszc!=0 && *pszc!=':') ++pszc;
        pnDate[4]=atoi(pszc);
        if (pnDate[4]<0 || pnDate[4]>59) CPLError(CE_Warning, CPLE_AppDefined,"%s",pszErrorMessage);
        while (*pszc!=0 && *pszc!=':') ++pszc;
        pnDate[5]=atoi(pszc);
        if (pnDate[5]<0 || pnDate[5]>59) CPLError(CE_Warning, CPLE_AppDefined,"%s",pszErrorMessage);
    }
    // Create the skeleton of a Selafin file
    VSILFILE *fp=VSIFOpenL(pszName,"wb");
    if (fp==NULL) {
        CPLError(CE_Failure, CPLE_AppDefined,"Unable to open %s with write access.",pszName);
        return NULL;
    }
    strncpy(pszTitle+72,"SERAPHIN",8);
    Selafin::write_string(fp,pszTitle,80);
    long pnTemp[10]={0};
    Selafin::write_intarray(fp,pnTemp,2);
    if (pnDate[0]>=0) pnTemp[9]=1;
    Selafin::write_intarray(fp,pnTemp,10);
    if (pnDate[0]>=0) Selafin::write_intarray(fp,pnTemp,6);
    pnTemp[3]=1;
    Selafin::write_intarray(fp,pnTemp,4);
    Selafin::write_intarray(fp,pnTemp,0);
    Selafin::write_intarray(fp,pnTemp,0);
    Selafin::write_floatarray(fp,0,0);
    Selafin::write_floatarray(fp,0,0);
    VSIFCloseL(fp);
    // Force it to open as a datasource
    OGRSelafinDataSource *poDS = new OGRSelafinDataSource();
    if( !poDS->Open( pszName, TRUE) ) {
        delete poDS;
        return NULL;
    }
    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSelafinDriver::TestCapability( const char * pszCap ) {
    if( EQUAL(pszCap,ODrCCreateDataSource) ) return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) ) return TRUE;
    else return FALSE;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/
OGRErr OGRSelafinDriver::DeleteDataSource( const char *pszFilename ) {
    if( CPLUnlinkTree( pszFilename ) == 0 ) return OGRERR_NONE;
    else return OGRERR_FAILURE;
}

/************************************************************************/
/*                           RegisterOGRSelafin()                       */
/************************************************************************/
void RegisterOGRSelafin() {
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRSelafinDriver );
}

