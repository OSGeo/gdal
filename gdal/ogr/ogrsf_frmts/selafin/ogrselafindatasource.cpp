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

#include <ctime>

/************************************************************************/
/*                          Range                                       */
/************************************************************************/
Range::~Range() {
    deleteList(poVals);
    deleteList(poActual);
}

void Range::deleteList(Range::List *poList) {
    if (poList==NULL) return;
    Range::List *pol=poList;
    while (pol!=NULL) {
        poList=poList->poNext;
        delete pol;
        pol=poList;
    }
}

void Range::setRange(const char *pszStr) {
    deleteList(poVals);
    deleteList(poActual);
    poVals=NULL;
    Range::List *poEnd=NULL;
    if (pszStr==NULL || pszStr[0]!='[') {
        CPLError( CE_Warning, CPLE_IllegalArg, "Invalid range specified\n");
        return;
    }
    const char *pszc=pszStr;
    char *psze;
    int nMin,nMax;
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
        if (*pszc==':') {
            nMin=0;
        } else {
            nMin=(int)strtol(pszc,&psze,10);
            if (*psze!=':' && *psze!=',' && *psze!=']') {
                CPLError( CE_Warning, CPLE_IllegalArg, "Invalid range specified\n");
                deleteList(poVals);
                poVals=NULL;
                return;
            }
            pszc=psze;
        }
        if (*pszc==':') {
            ++pszc;
            if (*pszc==',' || *pszc==']') {
                nMax=-1;
            } else {
                nMax=(int)strtol(pszc,&psze,10);
                if (*psze!=',' && *psze!=']') {
                    CPLError( CE_Warning, CPLE_IllegalArg, "Invalid range specified\n");
                    deleteList(poVals);
                    poVals=NULL;
                    return;
                }
                pszc=psze;
            }
        } else nMax=nMin;
        Range::List *poNew;
        if (eType!=ALL) poNew=new Range::List(eType,nMin,nMax,NULL); else poNew=new Range::List(POINTS,nMin,nMax,new Range::List(ELEMENTS,nMin,nMax,NULL));
        if (poVals==NULL) {
            poVals=poNew;
            poEnd=poNew;
        } else {
            poEnd->poNext=poNew;
            poEnd=poNew;
        }
        if (poEnd->poNext!=NULL) poEnd=poEnd->poNext;
    }
    if (*pszc!=']') {
        CPLError( CE_Warning, CPLE_IllegalArg, "Invalid range specified\n");
        deleteList(poVals);
        poVals=NULL;
    }
}

bool Range::contains(SelafinTypeDef eType,int nValue) const {
    if (poVals==NULL) return true;
    Range::List *poCur=poActual;
    while (poCur!=NULL) {
        if (poCur->eType==eType && nValue>=poCur->nMin && nValue<=poCur->nMax) return true;
        poCur=poCur->poNext;
    }
    return false;
}

void Range::sortList(Range::List *&poList,Range::List *poEnd) {
    if (poList==NULL || poList==poEnd) return;
    Range::List *pol=poList;
    Range::List *poBefore=NULL;
    Range::List *poBeforeEnd=NULL;
    // poList plays the role of the pivot value. Values greater and smaller are sorted on each side of it.
    // The order relation here is POINTS ranges first, then sorted by nMin value.
    while (pol->poNext!=poEnd) {
        if ((pol->eType==ELEMENTS && (pol->poNext->eType==POINTS || pol->poNext->nMin<pol->nMin)) || (pol->eType==POINTS && pol->poNext->eType==POINTS && pol->poNext->nMin<pol->nMin)) {
            if (poBefore==NULL) {
                poBefore=pol->poNext;
                poBeforeEnd=poBefore;
            } else {
                poBeforeEnd->poNext=pol->poNext;
                poBeforeEnd=poBeforeEnd->poNext;
            }
            pol->poNext=pol->poNext->poNext;
        } else pol=pol->poNext;
    }
    if (poBefore!=NULL) poBeforeEnd->poNext=poList;
    // Now, poList is well placed. We do the same for the sublists before and after poList
    Range::sortList(poBefore,poList);
    Range::sortList(poList->poNext,poEnd);
    // Finally, we restore the right starting point of the list
    if (poBefore!=NULL) poList=poBefore;
}

void Range::setMaxValue(int nMaxValueP) {
    nMaxValue=nMaxValueP;
    if (poVals==NULL) return;
    // We keep an internal private copy of the list where the range is "resolved", that is simplified to a union of disjoint intervals
    deleteList(poActual);
    poActual=NULL;
    Range::List *pol=poVals;
    Range::List *poActualEnd=NULL;
    int nMinT,nMaxT;
    while (pol!=NULL) {
        if (pol->nMin<0) nMinT=pol->nMin+nMaxValue; else nMinT=pol->nMin;
        if (pol->nMin<0) pol->nMin=0;
        if (pol->nMin>=nMaxValue) pol->nMin=nMaxValue-1;
        if (pol->nMax<0) nMaxT=pol->nMax+nMaxValue; else nMaxT=pol->nMax;
        if (pol->nMax<0) pol->nMax=0;
        if (pol->nMax>=nMaxValue) pol->nMax=nMaxValue-1;
        if (nMaxT<nMinT) continue;
        if (poActual==NULL) {
            poActual=new Range::List(pol->eType,nMinT,nMaxT,NULL);
            poActualEnd=poActual;
        } else {
            poActualEnd->poNext=new Range::List(pol->eType,nMinT,nMaxT,NULL);
            poActualEnd=poActualEnd->poNext;
        }
        pol=pol->poNext;
    }
    sortList(poActual);
    // Now we merge successive ranges when they intersect or are consecutive
    if (poActual!=NULL) {
        pol=poActual;
        while (pol->poNext!=NULL) {
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
    if (poVals==NULL) return nMaxValue*2;
    Range::List *pol=poActual;
    size_t nSize=0;
    while (pol!=NULL) {
        nSize+=(pol->nMax-pol->nMin+1);
        pol=pol->poNext;
    }
    return nSize;
}

/************************************************************************/
/*                          OGRSelafinDataSource()                      */
/************************************************************************/

OGRSelafinDataSource::OGRSelafinDataSource() :
    pszName(NULL),
    pszLockName(NULL),
    papoLayers(NULL),
    nLayers(0),
    bUpdate(FALSE),
    poHeader(NULL),
    poSpatialRef(NULL)
{ }

/************************************************************************/
/*                         ~OGRSelafinDataSource()                      */
/************************************************************************/

OGRSelafinDataSource::~OGRSelafinDataSource() {
    //CPLDebug("Selafin","~OGRSelafinDataSource(%s)",pszName);
    for( int i = 0; i < nLayers; i++ ) delete papoLayers[i];
    CPLFree( papoLayers );
    CPLFree( pszName );
    ReleaseLock();
    delete poHeader;
    if (poSpatialRef!=NULL) poSpatialRef->Release();
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
    if( iLayer < 0 || iLayer >= nLayers ) return NULL;
    else return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
int OGRSelafinDataSource::Open(const char * pszFilename, int bUpdateIn, int bCreate) {
    // Check if a range is set and extract it and the filename
    const char *pszc=pszFilename;
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
    bUpdate = bUpdateIn;
    if (bCreate && EQUAL(pszName, "/vsistdout/")) return TRUE;
    /* For writable /vsizip/, do nothing more */
    if (bCreate && STARTS_WITH(pszName, "/vsizip/")) return TRUE;
    CPLString osFilename(pszName);
    CPLString osBaseFilename = CPLGetFilename(pszName);
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
        osFilename = CPLFormFilename(osFilename, papszFiles[0], NULL);
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
/*                              TakeLock()                              */
/************************************************************************/
int OGRSelafinDataSource::TakeLock(CPL_UNUSED const char *pszFilename) {
#ifdef notdef
    // Ideally, we should implement a locking mechanism for Selafin layers because different layers share the same header and file on disk. If two layers are modified at the same time,
    // this would most likely result in data corruption. However, locking the data source causes other problems in QGis which always tries to open the datasource twice, a first time
    // in Update mode, and the second time for the layer inside in Read-only mode (because the lock is taken), and layers therefore can't be changed in QGis.
    // For now, this procedure is deactivated and a warning message is issued when a datasource is opened in update mode.
    //CPLDebug("Selafin","TakeLock(%s)",pszFilename);
    if (pszLockName!=0) CPLFree(pszLockName);
    VSILFILE *fpLock;
    size_t nLen=strlen(pszFilename)+4;
    pszLockName=(char*)CPLMalloc(sizeof(char)*nLen);
    CPLStrlcpy(pszLockName,pszFilename,nLen-3);
    CPLStrlcat(pszLockName,"~~~",nLen);
    fpLock=VSIFOpenL(pszLockName,"rb+");
    // This is not thread-safe but I'm not quite sure how to open a file in exclusive mode and in a portable way
    if (fpLock!=NULL) {
        VSIFCloseL(fpLock);
        return 0;
    }
    fpLock=VSIFOpenL(pszLockName,"wb");
    VSIFCloseL(fpLock);
#endif
    return 1;
}

/************************************************************************/
/*                            ReleaseLock()                             */
/************************************************************************/
void OGRSelafinDataSource::ReleaseLock() {
#ifdef notdef
    if (pszLockName==0) return;
    VSIUnlink(pszLockName);
    CPLFree(pszLockName);
#endif
}

/************************************************************************/
/*                              OpenTable()                             */
/************************************************************************/
int OGRSelafinDataSource::OpenTable(const char * pszFilename) {
    //CPLDebug("Selafin","OpenTable(%s,%i)",pszFilename,bUpdate);
    // Open the file
    VSILFILE * fp;
    if( bUpdate ) {
        // We have to implement this locking feature for write access because the same file may hold several layers, and some programs (like QGIS) open each layer in a single datasource,
        // so the same file might be opened several times for write access
        if (TakeLock(pszFilename)==0) {
            CPLError(CE_Failure,CPLE_OpenFailed,"Failed to open %s for write access, lock file found %s.",pszFilename,pszLockName);
            return FALSE;
        }
        fp = VSIFOpenL( pszFilename, "rb+" );
    } else fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == NULL ) {
        CPLError( CE_Warning, CPLE_OpenFailed, "Failed to open %s, %s.", pszFilename, VSIStrerror( errno ) );
        return FALSE;
    }
    if( !bUpdate && strstr(pszFilename, "/vsigzip/") == NULL && strstr(pszFilename, "/vsizip/") == NULL ) fp = (VSILFILE*) VSICreateBufferedReaderHandle((VSIVirtualHandle*)fp);

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
    poHeader=Selafin::read_header(fp,pszFilename);
    if (poHeader==NULL) {
        VSIFCloseL(fp);
        CPLError( CE_Failure, CPLE_OpenFailed, "Failed to open %s, wrong format.\n", pszFilename);
        return FALSE;
    }
    if (poHeader->nEpsg!=0) {
        poSpatialRef=new OGRSpatialReference();
        if (poSpatialRef->importFromEPSG(poHeader->nEpsg)!=OGRERR_NONE) {
            CPLError( CE_Warning, CPLE_AppDefined, "EPSG %d not found. Could not set datasource SRS.\n", poHeader->nEpsg);
            delete poSpatialRef;
            poSpatialRef=NULL;
        }
    }

    // Create two layers for each selected time step: one for points, the other for elements
    int nNewLayers;
    poRange.setMaxValue(poHeader->nSteps);
    nNewLayers=static_cast<int>(poRange.getSize());
    if (EQUAL(pszFilename, "/vsistdin/")) osBaseLayerName = "layer";
    CPLString osLayerName;
    papoLayers = (OGRSelafinLayer **) CPLRealloc(papoLayers, sizeof(void*) * (nLayers+nNewLayers));
    for (size_t j=0;j<2;++j) {
        SelafinTypeDef eType=(j==0)?POINTS:ELEMENTS;
        for (int i=0;i<poHeader->nSteps;++i) {
            if (poRange.contains(eType,i)) {
                char szTemp[30];
                double dfTime;
                if (VSIFSeekL(fp,poHeader->getPosition(i)+4,SEEK_SET)!=0 || Selafin::read_float(fp,dfTime)==0) {
                    VSIFCloseL(fp);
                    CPLError( CE_Failure, CPLE_OpenFailed, "Failed to open %s, wrong format.\n", pszFilename);
                    return FALSE;
                }
                if (poHeader->panStartDate==NULL) snprintf(szTemp,29,"%d",i); else {
                    struct tm sDate;
                    memset(&sDate, 0, sizeof(sDate));
                    sDate.tm_year=poHeader->panStartDate[0]-1900;
                    sDate.tm_mon=poHeader->panStartDate[1]-1;
                    sDate.tm_mday=poHeader->panStartDate[2];
                    sDate.tm_hour=poHeader->panStartDate[3];
                    sDate.tm_min=poHeader->panStartDate[4];
                    sDate.tm_sec=poHeader->panStartDate[5]+(int)dfTime;
                    mktime(&sDate);
                    strftime(szTemp,29,"%Y_%m_%d_%H_%M_%S",&sDate);
                }
                if (eType==POINTS) osLayerName=osBaseLayerName+"_p"+szTemp; else osLayerName=osBaseLayerName+"_e"+szTemp;
                papoLayers[nLayers++] = new OGRSelafinLayer( osLayerName, bUpdate, poSpatialRef, poHeader,i,eType);
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
    if (!bUpdate) {
        CPLError( CE_Failure, CPLE_NoWriteAccess, "Data source %s opened read-only.\n" "New layer %s cannot be created.\n", pszName, pszLayerName );
        return NULL;
    }
    // Check that new layer is a point or polygon layer
    if (eGType!=wkbPoint) {
        CPLError( CE_Failure, CPLE_NoWriteAccess, "Selafin format can only handle %s layers whereas input is %s\n.", OGRGeometryTypeToName(wkbPoint),OGRGeometryTypeToName(eGType));
        return NULL;
    }
    // Parse options
    double dfDate;
    const char *pszTemp=CSLFetchNameValue(papszOptions,"DATE");
    if (pszTemp!=NULL) dfDate=CPLAtof(pszTemp); else dfDate=0.0;
    // Set the SRS of the datasource if this is the first layer
    if (nLayers==0 && poSpatialRefP!=NULL) {
        poSpatialRef=poSpatialRefP;
        poSpatialRef->Reference();
        const char* szEpsg=poSpatialRef->GetAttrValue("GEOGCS|AUTHORITY",1);
        int nEpsg=0;
        if (szEpsg!=NULL) nEpsg=(int)strtol(szEpsg,NULL,10);
        if (nEpsg==0) {
            CPLError(CE_Warning,CPLE_AppDefined,"Could not find EPSG code for SRS. The SRS won't be saved in the datasource.");
        } else {
            poHeader->nEpsg=nEpsg;
        }
    }
    // Create the new layer in the Selafin file by adding a "time step" at the end
    // Beware, as the new layer shares the same header, it automatically contains the same number of features and fields as the existing ones. This may not be intuitive for the user.
    if (VSIFSeekL(poHeader->fp,0,SEEK_END)!=0) return NULL;
    if (Selafin::write_integer(poHeader->fp,4)==0 ||
        Selafin::write_float(poHeader->fp,dfDate)==0 ||
        Selafin::write_integer(poHeader->fp,4)==0) {
        CPLError( CE_Failure, CPLE_FileIO, "Could not write to Selafin file %s.\n",pszName);
        return NULL;
    }
    double *pdfValues=NULL;
    if (poHeader->nPoints>0)
    {
        pdfValues=(double*)VSI_MALLOC2_VERBOSE(sizeof(double),poHeader->nPoints);
        if( pdfValues == NULL )
            return NULL;
    }
    for (int i=0;i<poHeader->nVar;++i) {
        if (Selafin::write_floatarray(poHeader->fp,pdfValues,poHeader->nPoints)==0) {
            CPLError( CE_Failure, CPLE_FileIO, "Could not write to Selafin file %s.\n",pszName);
            CPLFree(pdfValues);
            return NULL;
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
    papoLayers[nLayers-2] = new OGRSelafinLayer( szNewLayerName, bUpdate, poSpatialRef, poHeader,poHeader->nSteps-1,POINTS);
    szNewLayerName=szName+"_e";
    papoLayers[nLayers-1] = new OGRSelafinLayer( szNewLayerName, bUpdate, poSpatialRef, poHeader,poHeader->nSteps-1,ELEMENTS);
    return papoLayers[nLayers-2];
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/
OGRErr OGRSelafinDataSource::DeleteLayer( int iLayer ) {
    // Verify we are in update mode.
    if( !bUpdate ) {
        CPLError( CE_Failure, CPLE_NoWriteAccess, "Data source %s opened read-only.\n" "Layer %d cannot be deleted.\n", pszName, iLayer );
        return OGRERR_FAILURE;
    }
    if( iLayer < 0 || iLayer >= nLayers ) {
        CPLError( CE_Failure, CPLE_AppDefined, "Layer %d not in legal range of 0 to %d.", iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }
    // Delete layer in file. Here we don't need to create a copy of the file because we only update values and it can't get corrupted even if the system crashes during the operation
    int nNum=papoLayers[iLayer]->GetStepNumber();
    double dfTime;
    double *dfValues=NULL;
    int nTemp;
    for (int i=nNum;i<poHeader->nSteps-1;++i) {
        if (VSIFSeekL(poHeader->fp,poHeader->getPosition(i+1)+4,SEEK_SET)!=0 ||
            Selafin::read_float(poHeader->fp,dfTime)==0 ||
            VSIFSeekL(poHeader->fp,poHeader->getPosition(i)+4,SEEK_SET)!=0 ||
            Selafin::write_float(poHeader->fp,dfTime)==0) {
            CPLError( CE_Failure, CPLE_FileIO, "Could not update Selafin file %s.\n",pszName);
            return OGRERR_FAILURE;
        }
        for (int j=0;j<poHeader->nVar;++j) {
            if (VSIFSeekL(poHeader->fp,poHeader->getPosition(i+1)+12,SEEK_SET)!=0 ||
                (nTemp=Selafin::read_floatarray(poHeader->fp,&dfValues)) !=poHeader->nPoints ||
                VSIFSeekL(poHeader->fp,poHeader->getPosition(i)+12,SEEK_SET)!=0 ||
                Selafin::write_floatarray(poHeader->fp,dfValues,poHeader->nPoints)==0) {
                CPLError( CE_Failure, CPLE_FileIO, "Could not update Selafin file %s.\n",pszName);
                CPLFree(dfValues);
                return OGRERR_FAILURE;
            }
            CPLFree(dfValues);
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
