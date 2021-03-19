/******************************************************************************
 * Project:  Selafin importer
 * Purpose:  Implementation of OGRSelafinDataSource class.
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
#include "cpl_vsi_virtual.h"
#include "cpl_vsi.h"
#include "io_selafin.h"

#include <algorithm>
#include <ctime>

CPL_CVSID("$Id$")

/************************************************************************/
/*                          Range                                       */
/************************************************************************/
Range::~Range() {
    deleteList(poVals);
    deleteList(poActual);
}

void Range::deleteList(Range::List *poList) {
    if (poList==nullptr) return;
    Range::List *pol=poList;
    while (pol!=nullptr) {
        poList=poList->poNext;
        delete pol;
        pol=poList;
    }
}

void Range::setRange(const char *pszStr) {
    deleteList(poVals);
    deleteList(poActual);
    poVals=nullptr;
    Range::List *poEnd=nullptr;
    if (pszStr==nullptr || pszStr[0]!='[') {
        CPLError( CE_Warning, CPLE_IllegalArg, "Invalid range specified\n");
        return;
    }
    const char *pszc=pszStr;
    char *psze = nullptr;
    SelafinTypeDef eType;
    while (*pszc!=0 && *pszc!=']') {
        pszc++;
        if (*pszc=='p' || *pszc=='P') {
            eType=POINTS;
            pszc++;
        } else if (*pszc=='e' || *pszc=='E') {
            eType=ELEMENTS;
            pszc++;
        } else eType=ALL;

        int nMin = 0;
        if (*pszc!=':') {
            nMin=(int)strtol(pszc,&psze,10);
            if (*psze!=':' && *psze!=',' && *psze!=']') {
                CPLError( CE_Warning, CPLE_IllegalArg, "Invalid range specified\n");
                deleteList(poVals);
                poVals=nullptr;
                return;
            }
            pszc=psze;
        }
        int nMax = -1;
        if (*pszc==':') {
            ++pszc;
            if (*pszc != ',' && *pszc !=']') {
                nMax=(int)strtol(pszc,&psze,10);
                if (*psze!=',' && *psze!=']') {
                    CPLError( CE_Warning, CPLE_IllegalArg, "Invalid range specified\n");
                    deleteList(poVals);
                    poVals=nullptr;
                    return;
                }
                pszc=psze;
            }
        } else nMax=nMin;
        Range::List *poNew = nullptr;
        if (eType!=ALL) poNew=new Range::List(eType,nMin,nMax,nullptr); else poNew=new Range::List(POINTS,nMin,nMax,new Range::List(ELEMENTS,nMin,nMax,nullptr));
        if (poVals==nullptr) {
            poVals=poNew;
            poEnd=poNew;
        } else {
            poEnd->poNext=poNew;
            poEnd=poNew;
        }
        if (poEnd->poNext!=nullptr) poEnd=poEnd->poNext;
    }
    if (*pszc!=']') {
        CPLError( CE_Warning, CPLE_IllegalArg, "Invalid range specified\n");
        deleteList(poVals);
        poVals=nullptr;
    }
}

bool Range::contains(SelafinTypeDef eType,int nValue) const {
    if (poVals==nullptr) return true;
    Range::List *poCur=poActual;
    while (poCur!=nullptr) {
        if (poCur->eType==eType && nValue>=poCur->nMin && nValue<=poCur->nMax) return true;
        poCur=poCur->poNext;
    }
    return false;
}

void Range::sortList(Range::List *&poList,Range::List *poEnd) {
    if (poList==nullptr || poList==poEnd) return;
    Range::List *pol=poList;
    Range::List *poBefore=nullptr;
    Range::List *poBeforeEnd=nullptr;
    // poList plays the role of the pivot value. Values greater and smaller are sorted on each side of it.
    // The order relation here is POINTS ranges first, then sorted by nMin value.
    while (pol->poNext!=poEnd) {
        if ((pol->eType==ELEMENTS && (pol->poNext->eType==POINTS || pol->poNext->nMin<pol->nMin)) || (pol->eType==POINTS && pol->poNext->eType==POINTS && pol->poNext->nMin<pol->nMin)) {
            if (poBefore==nullptr) {
                poBefore=pol->poNext;
                poBeforeEnd=poBefore;
            } else {
                poBeforeEnd->poNext=pol->poNext;
                poBeforeEnd=poBeforeEnd->poNext;
            }
            pol->poNext=pol->poNext->poNext;
        } else pol=pol->poNext;
    }
    if (poBefore!=nullptr) poBeforeEnd->poNext=poList;
    // Now, poList is well placed. We do the same for the sublists before and after poList
    Range::sortList(poBefore,poList);
    Range::sortList(poList->poNext,poEnd);
    // Finally, we restore the right starting point of the list
    if (poBefore!=nullptr) poList=poBefore;
}

void Range::setMaxValue(int nMaxValueP) {
    nMaxValue=nMaxValueP;
    if (poVals==nullptr) return;
    // We keep an internal private copy of the list where the range is "resolved", that is simplified to a union of disjoint intervals
    deleteList(poActual);
    poActual=nullptr;
    Range::List *pol=poVals;
    Range::List *poActualEnd=nullptr;
    int nMinT,nMaxT;
    while (pol!=nullptr) {
        if (pol->nMin<0) nMinT=pol->nMin+nMaxValue; else nMinT=pol->nMin;
        if (pol->nMin<0) pol->nMin=0;
        if (pol->nMin>=nMaxValue) pol->nMin=nMaxValue-1;
        if (pol->nMax<0) nMaxT=pol->nMax+nMaxValue; else nMaxT=pol->nMax;
        if (pol->nMax<0) pol->nMax=0;
        if (pol->nMax>=nMaxValue) pol->nMax=nMaxValue-1;
        if (nMaxT<nMinT) continue;
        if (poActual==nullptr) {
            poActual=new Range::List(pol->eType,nMinT,nMaxT,nullptr);
            poActualEnd=poActual;
        } else {
            poActualEnd->poNext=new Range::List(pol->eType,nMinT,nMaxT,nullptr);
            poActualEnd=poActualEnd->poNext;
        }
        pol=pol->poNext;
    }
    sortList(poActual);
    // Now we merge successive ranges when they intersect or are consecutive
    if (poActual!=nullptr) {
        pol=poActual;
        while (pol->poNext!=nullptr) {
            if (pol->poNext->eType==pol->eType && pol->poNext->nMin<=pol->nMax+1) {
                if (pol->poNext->nMax>pol->nMax) pol->nMax=pol->poNext->nMax;
                poActualEnd=pol->poNext->poNext;
                delete pol->poNext;
                pol->poNext=poActualEnd;
            } else pol=pol->poNext;
        }
    }
}

size_t Range::getSize() const {
    if (poVals==nullptr) return nMaxValue*2;
    Range::List *pol=poActual;
    size_t nSize=0;
    while (pol!=nullptr) {
        nSize+=(pol->nMax-pol->nMin+1);
        pol=pol->poNext;
    }
    return nSize;
}

/************************************************************************/
/*                          OGRSelafinDataSource()                      */
/************************************************************************/

OGRSelafinDataSource::OGRSelafinDataSource() :
    pszName(nullptr),
    papoLayers(nullptr),
    nLayers(0),
    bUpdate(false),
    poHeader(nullptr),
    poSpatialRef(nullptr)
{}

/************************************************************************/
/*                         ~OGRSelafinDataSource()                      */
/************************************************************************/

OGRSelafinDataSource::~OGRSelafinDataSource() {
#ifdef DEBUG_VERBOSE
    CPLDebug("Selafin", "~OGRSelafinDataSource(%s)", pszName);
#endif
    for( int i = 0; i < nLayers; i++ ) delete papoLayers[i];
    CPLFree( papoLayers );
    CPLFree( pszName );
    delete poHeader;
    if (poSpatialRef!=nullptr) poSpatialRef->Release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSelafinDataSource::TestCapability( const char * pszCap ) {
    if( EQUAL(pszCap,ODsCCreateLayer) ) return TRUE;
    else if( EQUAL(pszCap,ODsCDeleteLayer) ) return TRUE;
    else if( EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) ) return FALSE;
    else return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSelafinDataSource::GetLayer( int iLayer ) {
    if( iLayer < 0 || iLayer >= nLayers ) return nullptr;
    else return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
int OGRSelafinDataSource::Open( const char * pszFilename, int bUpdateIn,
                                int bCreate )
{
    // Check if a range is set and extract it and the filename.
    const char *pszc = pszFilename;
    if (*pszFilename==0) return FALSE;
    while (*pszc) ++pszc;
    if (*(pszc-1)==']') {
        --pszc;
        while (pszc!=pszFilename && *pszc!='[') pszc--;
        if (pszc==pszFilename) return FALSE;
        poRange.setRange(pszc);
    }
    pszName = CPLStrdup( pszFilename );
    pszName[pszc-pszFilename]=0;
    bUpdate = CPL_TO_BOOL(bUpdateIn);
    if (bCreate && EQUAL(pszName, "/vsistdout/")) return TRUE;
    /* For writable /vsizip/, do nothing more */
    if (bCreate && STARTS_WITH(pszName, "/vsizip/")) return TRUE;
    CPLString osFilename(pszName);
    // Determine what sort of object this is.
    VSIStatBufL sStatBuf;
    if (VSIStatExL( osFilename, &sStatBuf, VSI_STAT_NATURE_FLAG ) != 0) return FALSE;

    // Is this a single Selafin file?
    if (VSI_ISREG(sStatBuf.st_mode)) return OpenTable( pszName );

    // Is this a single a ZIP file with only a Selafin file inside ?
    if( STARTS_WITH(osFilename, "/vsizip/") && VSI_ISREG(sStatBuf.st_mode) ) {
        char** papszFiles = VSIReadDir(osFilename);
        if (CSLCount(papszFiles) != 1) {
            CSLDestroy(papszFiles);
            return FALSE;
        }
        osFilename = CPLFormFilename(osFilename, papszFiles[0], nullptr);
        CSLDestroy(papszFiles);
        return OpenTable( osFilename );
    }

#ifdef notdef
    // Otherwise it has to be a directory.
    if( !VSI_ISDIR(sStatBuf.st_mode) ) return FALSE;

    // Scan through for entries which look like Selafin files
    int nNotSelafinCount = 0, i;
    char **papszNames = VSIReadDir( osFilename );
    for( i = 0; papszNames != NULL && papszNames[i] != NULL; i++ ) {
        CPLString oSubFilename = CPLFormFilename( osFilename, papszNames[i], NULL );
        if( EQUAL(papszNames[i],".") || EQUAL(papszNames[i],"..") ) continue;
        if( VSIStatL( oSubFilename, &sStatBuf ) != 0 || !VSI_ISREG(sStatBuf.st_mode) ) {
            nNotSelafinCount++;
            continue;
        }
        if( !OpenTable( oSubFilename ) ) {
            CPLDebug("Selafin", "Cannot open %s", oSubFilename.c_str());
            nNotSelafinCount++;
            continue;
        }
    }
    CSLDestroy( papszNames );

    // We presume that this is indeed intended to be a Selafin datasource if over half the files were Selafin files.
    return nNotSelafinCount < nLayers;
#else
    return FALSE;
#endif
}

/************************************************************************/
/*                              OpenTable()                             */
/************************************************************************/
int OGRSelafinDataSource::OpenTable(const char * pszFilename) {
#ifdef DEBUG_VERBOSE
    CPLDebug("Selafin", "OpenTable(%s,%i)",
             pszFilename, static_cast<int>(bUpdate));
#endif
    // Open the file
    VSILFILE *fp = nullptr;
    if( bUpdate )
    {
        fp = VSIFOpenExL( pszFilename, "rb+", true );
    }
    else
    {
        fp = VSIFOpenExL( pszFilename, "rb", true );
    }

    if( fp == nullptr ) {
        CPLError( CE_Warning, CPLE_OpenFailed, "Failed to open %s.", VSIGetLastErrorMsg() );
        return FALSE;
    }
    if( !bUpdate && strstr(pszFilename, "/vsigzip/") == nullptr && strstr(pszFilename, "/vsizip/") == nullptr ) fp = (VSILFILE*) VSICreateBufferedReaderHandle((VSIVirtualHandle*)fp);

    // Quickly check if the file is in Selafin format, before actually starting to read to make it faster
    char szBuf[9];
    VSIFReadL(szBuf,1,4,fp);
    if (szBuf[0]!=0 || szBuf[1]!=0 || szBuf[2]!=0 || szBuf[3]!=0x50) {
        VSIFCloseL(fp);
        return FALSE;
    }
    VSIFSeekL(fp,84,SEEK_SET);
    VSIFReadL(szBuf,1,8,fp);
    if (szBuf[0]!=0 || szBuf[1]!=0 || szBuf[2]!=0 || szBuf[3]!=0x50 || szBuf[4]!=0 || szBuf[5]!=0 || szBuf[6]!=0 || szBuf[7]!=8) {
        VSIFCloseL(fp);
        return FALSE;
    }
    /* VSIFSeekL(fp,76,SEEK_SET);
    VSIFReadL(szBuf,1,8,fp);
    if (STRNCASECMP(szBuf,"Seraphin",8)!=0 && STRNCASECMP(szBuf,"Serafin",7)!=0) {
        VSIFCloseL(fp);
        return FALSE;
    } */

    // Get layer base name
    CPLString osBaseLayerName = CPLGetBasename(pszFilename);
    CPLString osExt = CPLGetExtension(pszFilename);
    if( STARTS_WITH(pszFilename, "/vsigzip/") && EQUAL(osExt, "gz") ) {
        size_t nPos=std::string::npos;
        if (strlen(pszFilename)>3) nPos=osExt.find_last_of('.',strlen(pszFilename)-4);
        if (nPos!=std::string::npos) {
            osExt=osExt.substr(nPos+1,strlen(pszFilename)-4-nPos);
            osBaseLayerName=osBaseLayerName.substr(0,nPos);
        } else {
            osExt="";
        }
    }

    // Read header of file to get common information for all layers
    // poHeader now owns fp
    poHeader=Selafin::read_header(fp,pszFilename);
    if (poHeader==nullptr) {
        CPLError( CE_Failure, CPLE_OpenFailed, "Failed to open %s, wrong format.\n", pszFilename);
        return FALSE;
    }
    if (poHeader->nEpsg!=0) {
        poSpatialRef=new OGRSpatialReference();
        poSpatialRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSpatialRef->importFromEPSG(poHeader->nEpsg)!=OGRERR_NONE) {
            CPLError( CE_Warning, CPLE_AppDefined, "EPSG %d not found. Could not set datasource SRS.\n", poHeader->nEpsg);
            delete poSpatialRef;
            poSpatialRef=nullptr;
        }
    }

    // To prevent int overflow in poRange.getSize() call where we do
    // nSteps * 2
    if( poHeader->nSteps >= INT_MAX / 2 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Invalid nSteps value" );
        return FALSE;
    }

    // Create two layers for each selected time step: one for points, the other for elements
    poRange.setMaxValue(poHeader->nSteps);
    const int nNewLayers = static_cast<int>(poRange.getSize());
    if (EQUAL(pszFilename, "/vsistdin/")) osBaseLayerName = "layer";
    CPLString osLayerName;
    papoLayers = (OGRSelafinLayer **) CPLRealloc(papoLayers, sizeof(void*) * (nLayers+nNewLayers));
    for (size_t j=0;j<2;++j) {
        SelafinTypeDef eType=(j==0)?POINTS:ELEMENTS;
        for (int i=0;i<poHeader->nSteps;++i) {
            if (poRange.contains(eType,i)) {
                char szTemp[30] = {};
                double dfTime = 0.0;
                if( VSIFSeekL(fp, poHeader->getPosition(i)+4, SEEK_SET)!=0 ||
                    Selafin::read_float(fp, dfTime)==0 )
                {
                    CPLError( CE_Failure, CPLE_OpenFailed, "Failed to open %s, wrong format.\n", pszFilename);
                    return FALSE;
                }
                if (poHeader->panStartDate==nullptr) snprintf(szTemp,29,"%d",i); else {
                    struct tm sDate;
                    memset(&sDate, 0, sizeof(sDate));
                    sDate.tm_year=std::max(poHeader->panStartDate[0], 0) - 1900;
                    sDate.tm_mon=std::max(poHeader->panStartDate[1], 1) - 1;
                    sDate.tm_mday=poHeader->panStartDate[2];
                    sDate.tm_hour=poHeader->panStartDate[3];
                    sDate.tm_min=poHeader->panStartDate[4];
                    double dfSec=poHeader->panStartDate[5]+dfTime;
                    if( dfSec >= 0 && dfSec < 60 )
                        sDate.tm_sec=static_cast<int>(dfSec);
                    mktime(&sDate);
                    strftime(szTemp,29,"%Y_%m_%d_%H_%M_%S",&sDate);
                }
                if (eType==POINTS) osLayerName=osBaseLayerName+"_p"+szTemp; else osLayerName=osBaseLayerName+"_e"+szTemp;
                papoLayers[nLayers++] =
                    new OGRSelafinLayer( osLayerName, bUpdate, poSpatialRef,
                                         poHeader, i, eType);
                //poHeader->nRefCount++;
            }
        }
    }

    // Free allocated variables and exit
    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRSelafinDataSource::ICreateLayer( const char *pszLayerName, OGRSpatialReference *poSpatialRefP, OGRwkbGeometryType eGType, char ** papszOptions  ) {
    CPLDebug("Selafin","CreateLayer(%s,%s)",pszLayerName,(eGType==wkbPoint)?"wkbPoint":"wkbPolygon");
    // Verify we are in update mode.
    if ( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.  "
                  "New layer %s cannot be created.",
                  pszName, pszLayerName );
        return nullptr;
    }
    // Check that new layer is a point or polygon layer
    if( eGType != wkbPoint )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess, "Selafin format can only handle %s layers whereas input is %s\n.", OGRGeometryTypeToName(wkbPoint),OGRGeometryTypeToName(eGType));
        return nullptr;
    }
    // Parse options
    const char *pszTemp=CSLFetchNameValue(papszOptions,"DATE");
    const double dfDate = pszTemp != nullptr ? CPLAtof(pszTemp) : 0.0;
    // Set the SRS of the datasource if this is the first layer
    if (nLayers==0 && poSpatialRefP!=nullptr) {
        poSpatialRef=poSpatialRefP;
        poSpatialRef->Reference();
        const char* szEpsg=poSpatialRef->GetAttrValue("GEOGCS|AUTHORITY",1);
        int nEpsg=0;
        if (szEpsg!=nullptr) nEpsg=(int)strtol(szEpsg,nullptr,10);
        if (nEpsg==0) {
            CPLError(CE_Warning,CPLE_AppDefined,"Could not find EPSG code for SRS. The SRS won't be saved in the datasource.");
        } else {
            poHeader->nEpsg=nEpsg;
        }
    }
    // Create the new layer in the Selafin file by adding a "time step" at the end
    // Beware, as the new layer shares the same header, it automatically contains the same number of features and fields as the existing ones. This may not be intuitive for the user.
    if (VSIFSeekL(poHeader->fp,0,SEEK_END)!=0) return nullptr;
    if (Selafin::write_integer(poHeader->fp,4)==0 ||
        Selafin::write_float(poHeader->fp,dfDate)==0 ||
        Selafin::write_integer(poHeader->fp,4)==0) {
        CPLError( CE_Failure, CPLE_FileIO, "Could not write to Selafin file %s.\n",pszName);
        return nullptr;
    }
    double *pdfValues=nullptr;
    if (poHeader->nPoints>0)
    {
        pdfValues=(double*)VSI_MALLOC2_VERBOSE(sizeof(double),poHeader->nPoints);
        if( pdfValues == nullptr )
            return nullptr;
    }
    for (int i=0;i<poHeader->nVar;++i) {
        if (Selafin::write_floatarray(poHeader->fp,pdfValues,poHeader->nPoints)==0) {
            CPLError( CE_Failure, CPLE_FileIO, "Could not write to Selafin file %s.\n",pszName);
            CPLFree(pdfValues);
            return nullptr;
        }
    }
    CPLFree(pdfValues);
    VSIFFlushL(poHeader->fp);
    poHeader->nSteps++;
    // Create two layers as usual, one for points and one for elements
    nLayers+=2;
    papoLayers = (OGRSelafinLayer **) CPLRealloc(papoLayers, sizeof(void*) * nLayers);
    CPLString szName=pszLayerName;
    CPLString szNewLayerName=szName+"_p";
    papoLayers[nLayers-2] =
        new OGRSelafinLayer( szNewLayerName, bUpdate, poSpatialRef, poHeader,
                             poHeader->nSteps-1, POINTS );
    szNewLayerName=szName+"_e";
    papoLayers[nLayers-1] =
        new OGRSelafinLayer( szNewLayerName, bUpdate, poSpatialRef, poHeader,
                             poHeader->nSteps-1, ELEMENTS );
    return papoLayers[nLayers-2];
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/
OGRErr OGRSelafinDataSource::DeleteLayer( int iLayer ) {
    // Verify we are in update mode.
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.  "
                  "Layer %d cannot be deleted.\n", pszName, iLayer );
        return OGRERR_FAILURE;
    }
    if( iLayer < 0 || iLayer >= nLayers ) {
        CPLError( CE_Failure, CPLE_AppDefined, "Layer %d not in legal range of 0 to %d.", iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }
    // Delete layer in file. Here we don't need to create a copy of the file because we only update values and it can't get corrupted even if the system crashes during the operation
    const int nNum = papoLayers[iLayer]->GetStepNumber();
    double *dfValues=nullptr;
    for( int i = nNum; i < poHeader->nSteps - 1; ++i )
    {
        double dfTime = 0.0;
        if (VSIFSeekL(poHeader->fp,poHeader->getPosition(i+1)+4,SEEK_SET)!=0 ||
            Selafin::read_float(poHeader->fp, dfTime) == 0 ||
            VSIFSeekL(poHeader->fp,poHeader->getPosition(i)+4,SEEK_SET)!=0 ||
            Selafin::write_float(poHeader->fp, dfTime) == 0)
        {
            CPLError( CE_Failure, CPLE_FileIO, "Could not update Selafin file %s.\n",pszName);
            return OGRERR_FAILURE;
        }
        for (int j=0;j<poHeader->nVar;++j)
        {
            if (VSIFSeekL(poHeader->fp,poHeader->getPosition(i+1)+12,SEEK_SET)!=0 ||
                Selafin::read_floatarray(poHeader->fp,&dfValues,poHeader->nFileSize) !=poHeader->nPoints ||
                VSIFSeekL(poHeader->fp,poHeader->getPosition(i)+12,SEEK_SET)!=0 ||
                Selafin::write_floatarray(poHeader->fp,dfValues,poHeader->nPoints)==0) {
                CPLError( CE_Failure, CPLE_FileIO, "Could not update Selafin file %s.\n",pszName);
                CPLFree(dfValues);
                return OGRERR_FAILURE;
            }
            CPLFree(dfValues);
            dfValues = nullptr;
        }
    }
    // Delete all layers with the same step number in layer list. Usually there are two of them: one for points and one for elements, but we can't rely on that because of possible layer filtering specifications
    for (int i=0;i<nLayers;++i) {
        if (papoLayers[i]->GetStepNumber()==nNum) {
            delete papoLayers[i];
            nLayers--;
            for (int j=i;j<nLayers;++j) papoLayers[j]=papoLayers[j+1];
            --i;
        }
    }
    return OGRERR_NONE;
}
