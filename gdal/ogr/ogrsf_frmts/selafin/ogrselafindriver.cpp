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
/*                     OGRSelafinDriverIdentify()                       */
/************************************************************************/

static int OGRSelafinDriverIdentify( GDALOpenInfo* poOpenInfo ) {

    if( poOpenInfo->fpL != NULL )
    {
        if( poOpenInfo->nHeaderBytes < 84 + 8 )
            return FALSE;
        if (poOpenInfo->pabyHeader[0]!=0 || poOpenInfo->pabyHeader[1]!=0 ||
            poOpenInfo->pabyHeader[2]!=0 || poOpenInfo->pabyHeader[3]!=0x50)
            return FALSE;

        if (poOpenInfo->pabyHeader[84+0]!=0 || poOpenInfo->pabyHeader[84+1]!=0 ||
            poOpenInfo->pabyHeader[84+2]!=0 || poOpenInfo->pabyHeader[84+3]!=0x50 ||
            poOpenInfo->pabyHeader[84+4]!=0 || poOpenInfo->pabyHeader[84+5]!=0 ||
            poOpenInfo->pabyHeader[84+6]!=0 || poOpenInfo->pabyHeader[84+7]!=8)
            return FALSE;

        return TRUE;
    }
    return -1;
}

/************************************************************************/
/*                      OGRSelafinDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGRSelafinDriverOpen( GDALOpenInfo* poOpenInfo ) {

    if( OGRSelafinDriverIdentify(poOpenInfo) == 0 )
        return NULL;
    
    OGRSelafinDataSource *poDS = new OGRSelafinDataSource();
    if( !poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update, FALSE) ) {
        delete poDS;
        poDS = NULL;
    }
    return poDS;
}

/************************************************************************/
/*                       OGRSelafinDriverCreate()                       */
/************************************************************************/

static GDALDataset *OGRSelafinDriverCreate( const char * pszName, int nXSize, int nYSize,
                                     int nBands, GDALDataType eDT, char **papszOptions ) {
    // First, ensure there isn't any such file yet.
    VSIStatBufL sStatBuf;
    if (strcmp(pszName, "/dev/stdout") == 0) pszName = "/vsistdout/";
    if( VSIStatL( pszName, &sStatBuf ) == 0 ) {
        CPLError(CE_Failure, CPLE_AppDefined,"It seems a file system object called '%s' already exists.",pszName);
        return NULL;
    }
    // Parse options
    const char *pszTemp=CSLFetchNameValue(papszOptions,"TITLE");
    char pszTitle[81];
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
    strncpy(pszTitle+72,"SERAPHIN",9);
    bool bError=false;
    if (Selafin::write_string(fp,pszTitle,80)==0) bError=true;
    long pnTemp[10]={0};
    if (Selafin::write_intarray(fp,pnTemp,2)==0) bError=true;
    if (pnDate[0]>=0) pnTemp[9]=1;
    if (Selafin::write_intarray(fp,pnTemp,10)==0) bError=true;
    if (pnDate[0]>=0) {
        if (Selafin::write_intarray(fp,pnTemp,6)==0) bError=true;
    }
    pnTemp[3]=1;
    if (Selafin::write_intarray(fp,pnTemp,4)==0) bError=true;
    if (Selafin::write_intarray(fp,pnTemp,0)==0) bError=true;
    if (Selafin::write_intarray(fp,pnTemp,0)==0) bError=true;
    if (Selafin::write_floatarray(fp,0,0)==0) bError=true;
    if (Selafin::write_floatarray(fp,0,0)==0) bError=true;
    VSIFCloseL(fp);
    if (bError) {
        CPLError(CE_Failure, CPLE_AppDefined,"Error writing to file %s.",pszName);
        return NULL;
    }
    // Force it to open as a datasource
    OGRSelafinDataSource *poDS = new OGRSelafinDataSource();
    if( !poDS->Open( pszName, TRUE, TRUE ) ) {
        delete poDS;
        return NULL;
    }
    return poDS;
}

/************************************************************************/
/*                      OGRSelafinDriverDelete()                        */
/************************************************************************/
static CPLErr OGRSelafinDriverDelete( const char *pszFilename ) {
    if( CPLUnlinkTree( pszFilename ) == 0 ) return CE_None;
    else return CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRSelafin()                       */
/************************************************************************/
void RegisterOGRSelafin() {
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "Selafin" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "Selafin" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "Selafin" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Selafin" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_selafin.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='TITLE' type='string' description='Title of the datasource, stored in the Selafin file. The title must not hold more than 72 characters.'/>"
"  <Option name='DATE' type='string' description='Starting date of the simulation. Each layer in a Selafin file is characterized by a date, counted in seconds since a reference date. This option allows to provide the reference date. The format of this field must be YYYY-MM-DD_hh:mm:ss'/>"
"</CreationOptionList>");
        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='DATE' type='float' description='Date of the time step, in seconds, relative to the starting date of the simulation.'/>"
"</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRSelafinDriverOpen;
        poDriver->pfnIdentify = OGRSelafinDriverIdentify;
        poDriver->pfnCreate = OGRSelafinDriverCreate;
        poDriver->pfnDelete = OGRSelafinDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

