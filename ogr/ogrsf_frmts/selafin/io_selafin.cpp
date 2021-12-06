/******************************************************************************
 * Project:  Selafin importer
 * Purpose:  Implementation of functions for reading records in Selafin files
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

#include "io_selafin.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_quad_tree.h"

CPL_CVSID("$Id$")

namespace Selafin {

    const char SELAFIN_ERROR_MESSAGE[]="Error when reading Selafin file\n";

    struct Point {
        int nIndex;
        const Header *poHeader;
    };

    static void GetBoundsFunc( const void *hFeature,CPLRectObj *poBounds )
    {
        const Point *poPoint=(const Point*)hFeature;
        poBounds->minx=poPoint->poHeader->paadfCoords[0][poPoint->nIndex];
        poBounds->maxx=poPoint->poHeader->paadfCoords[0][poPoint->nIndex];
        poBounds->miny=poPoint->poHeader->paadfCoords[1][poPoint->nIndex];
        poBounds->maxy=poPoint->poHeader->paadfCoords[1][poPoint->nIndex];
    }

    static int DumpFeatures( void *pElt,
                             void * /* pUserData */ )
    {
        Point *poPoint=(Point*)pElt;
        delete poPoint;
        return TRUE;
    }

    /****************************************************************/
    /*                         Header                               */
    /****************************************************************/
    Header::Header() :
        nHeaderSize(0),
        nStepSize(0),
        nMinxIndex(-1),
        nMaxxIndex(-1),
        nMinyIndex(-1),
        nMaxyIndex(-1),
        bTreeUpdateNeeded(true),
        nFileSize(0),
        fp(nullptr),
        pszFilename(nullptr),
        pszTitle(nullptr),
        nVar(0),
        papszVariables(nullptr),
        nPoints(0),
        nElements(0),
        nPointsPerElement(0),
        panConnectivity(nullptr),
        poTree(nullptr),
        panBorder(nullptr),
        panStartDate(nullptr),
        nSteps(0),
        nEpsg(0)
    {
        paadfCoords[0] = nullptr;
        paadfCoords[1] = nullptr;
        for( size_t i = 0; i < 7; ++i ) anUnused[i] = 0;
        adfOrigin[0] = 0.0;
        adfOrigin[1] = 0.0;
    }

    Header::~Header() {
        CPLFree(pszFilename);
        CPLFree(pszTitle);
        if( papszVariables!=nullptr )
        {
            for( int i = 0; i < nVar; ++i ) CPLFree(papszVariables[i]);
            CPLFree(papszVariables);
        }
        CPLFree(panConnectivity);
        CPLFree(panBorder);
        if( poTree!=nullptr )
        {
            CPLQuadTreeForeach(poTree,DumpFeatures,nullptr);
            CPLQuadTreeDestroy(poTree);
        }
        CPLFree(panStartDate);
        for( size_t i = 0; i < 2; ++i ) CPLFree(paadfCoords[i]);
        if( fp != nullptr ) VSIFCloseL(fp);
    }

    void Header::setUpdated() {
        nHeaderSize=88+16+nVar*40+12*4+((panStartDate==nullptr)?0:32)+24+(nElements*nPointsPerElement+2)*4+(nPoints+2)*12;
        nStepSize=12+nVar*(nPoints+2)*4;
    }

    int Header::getPosition(int nStep,int nFeature,int nAttribute) const {
        int a=(nFeature!=-1 || nAttribute!=-1)?(12+nAttribute*(nPoints+2)*4+4+nFeature*4):0;
        int b=nStep*nStepSize;
        return nHeaderSize+b+a;
    }

    CPLRectObj *Header::getBoundingBox() const {
        CPLRectObj *poBox=new CPLRectObj;
        poBox->minx=paadfCoords[0][nMinxIndex];
        poBox->maxx=paadfCoords[0][nMaxxIndex];
        poBox->miny=paadfCoords[1][nMinyIndex];
        poBox->maxy=paadfCoords[1][nMaxyIndex];
        return poBox;
    }

    void Header::updateBoundingBox() {
        if (nPoints>0) {
            nMinxIndex=0;
            for (int i=1;i<nPoints;++i) if (paadfCoords[0][i]<paadfCoords[0][nMinxIndex]) nMinxIndex=i;
            nMaxxIndex=0;
            for (int i=1;i<nPoints;++i) if (paadfCoords[0][i]>paadfCoords[0][nMaxxIndex]) nMaxxIndex=i;
            nMinyIndex=0;
            for (int i=1;i<nPoints;++i) if (paadfCoords[1][i]<paadfCoords[1][nMinyIndex]) nMinyIndex=i;
            nMaxyIndex=0;
            for (int i=1;i<nPoints;++i) if (paadfCoords[1][i]>paadfCoords[1][nMaxyIndex]) nMaxyIndex=i;
        }
    }

    int Header::getClosestPoint( const double &dfx, const double &dfy,
                                 const double &dfMax)
    {
        // If there is no quad-tree of the points, build it now
        if (bTreeUpdateNeeded) {
            if (poTree!=nullptr) {
                CPLQuadTreeForeach(poTree,DumpFeatures,nullptr);
                CPLQuadTreeDestroy(poTree);
            }
        }
        if (bTreeUpdateNeeded || poTree==nullptr) {
            bTreeUpdateNeeded=false;
            CPLRectObj *poBB=getBoundingBox();
            poTree=CPLQuadTreeCreate(poBB,GetBoundsFunc);
            delete poBB;
            CPLQuadTreeSetBucketCapacity(poTree,2);
            for (int i=0;i<nPoints;++i) {
                Point *poPoint=new Point;
                poPoint->poHeader=this;
                poPoint->nIndex=i;
                CPLQuadTreeInsert(poTree,(void*)poPoint);
            }
        }
        // Now we can look for the nearest neighbour using this tree
        int nIndex = -1;
        CPLRectObj poObj;
        poObj.minx = dfx-dfMax;
        poObj.maxx = dfx+dfMax;
        poObj.miny=dfy-dfMax;
        poObj.maxy=dfy+dfMax;
        int nFeatureCount = 0;
        void **phResults = CPLQuadTreeSearch(poTree, &poObj, &nFeatureCount);
        if( nFeatureCount <=0 ) return -1;
        double dfMin = dfMax * dfMax;
        for( int i=0;i<nFeatureCount;++i )
        {
            Point *poPoint=(Point*)(phResults[i]);
            double dfa =
                dfx-poPoint->poHeader->paadfCoords[0][poPoint->nIndex];
            dfa *= dfa;
            if (dfa>=dfMin) continue;
            const double dfb =
                dfy-poPoint->poHeader->paadfCoords[1][poPoint->nIndex];
            const double dfc = dfa + dfb * dfb;
            if (dfc<dfMin) {
                dfMin=dfc;
                nIndex=poPoint->nIndex;
            }
        }
        CPLFree(phResults);
        return nIndex;
    }

    void Header::addPoint(const double &dfx,const double &dfy) {
        // We add the point to all the tables
        nPoints++;
        for (size_t i=0;i<2;++i) paadfCoords[i]=(double*)CPLRealloc(paadfCoords[i],sizeof(double)*nPoints);
        paadfCoords[0][nPoints-1]=dfx;
        paadfCoords[1][nPoints-1]=dfy;
        panBorder=(int*)CPLRealloc(panBorder,sizeof(int)*nPoints);
        panBorder[nPoints-1]=0;
        // We update the bounding box
        if (nMinxIndex==-1 || dfx<paadfCoords[0][nMinxIndex]) nMinxIndex=nPoints-1;
        if (nMaxxIndex==-1 || dfx>paadfCoords[0][nMaxxIndex]) nMaxxIndex=nPoints-1;
        if (nMinyIndex==-1 || dfy<paadfCoords[1][nMinyIndex]) nMinyIndex=nPoints-1;
        if (nMaxyIndex==-1 || dfy>paadfCoords[1][nMaxyIndex]) nMaxyIndex=nPoints-1;
        // We update some parameters of the header
        bTreeUpdateNeeded=true;
        setUpdated();
    }

    void Header::removePoint(int nIndex) {
        // We remove the point from all the tables
        nPoints--;
        for (size_t i=0;i<2;++i) {
            for (int j=nIndex;j<nPoints;++j) paadfCoords[i][j]=paadfCoords[i][j+1];
            paadfCoords[i]=(double*)CPLRealloc(paadfCoords[i],sizeof(double)*nPoints);
        }
        for (int j=nIndex;j<nPoints;++j) panBorder[j]=panBorder[j+1];
        panBorder=(int*)CPLRealloc(panBorder,sizeof(int)*nPoints);

        // We must also remove all the elements referencing the deleted feature, otherwise the file will not be consistent any inter
        int nOldElements=nElements;
        for (int i=0;i<nElements;++i) {
            bool bReferencing = false;
            int *panTemp = panConnectivity + i * nPointsPerElement;
            for (int j=0;j<nPointsPerElement;++j) bReferencing |= (panTemp[j]==nIndex+1);
            if (bReferencing) {
                nElements--;
                for (int j=i;j<nElements;++j)
                    for (int k=0;k<nPointsPerElement;++k) panConnectivity[j*nPointsPerElement+k]=panConnectivity[(j+1)*nPointsPerElement+k];
                --i;
            }
        }
        if (nOldElements!=nElements) panConnectivity=(int*)CPLRealloc(panConnectivity,sizeof(int)*nElements*nPointsPerElement);

        // Now we update the bounding box if needed
        if (nPoints==0) {nMinxIndex=-1;nMaxxIndex=-1;nMinyIndex=-1;nMaxyIndex=-1;} else {
            if (nIndex==nMinxIndex) {
                nMinxIndex=0;
                for (int i=1;i<nPoints;++i) if (paadfCoords[0][i]<paadfCoords[0][nMinxIndex]) nMinxIndex=i;
            }
            if (nIndex==nMaxxIndex) {
                nMaxxIndex=0;
                for (int i=1;i<nPoints;++i) if (paadfCoords[0][i]>paadfCoords[0][nMaxxIndex]) nMaxxIndex=i;
            }
            if (nIndex==nMinyIndex) {
                nMinyIndex=0;
                for (int i=1;i<nPoints;++i) if (paadfCoords[1][i]<paadfCoords[1][nMinyIndex]) nMinyIndex=i;
            }
            if (nIndex==nMaxyIndex) {
                nMaxyIndex=0;
                for (int i=1;i<nPoints;++i) if (paadfCoords[1][i]>paadfCoords[1][nMaxyIndex]) nMaxyIndex=i;
            }
        }

        // We update some parameters of the header
        bTreeUpdateNeeded=true;
        setUpdated();
    }

#ifdef notdef
    /****************************************************************/
    /*                         TimeStep                             */
    /****************************************************************/
    TimeStep::TimeStep( int nRecordsP, int nFieldsP ) :
        nFields(nFieldsP)
    {
        papadfData=(double**)VSI_MALLOC2_VERBOSE(sizeof(double*),nFieldsP);
        for (int i=0;i<nFieldsP;++i) papadfData[i]=(double*)VSI_MALLOC2_VERBOSE(sizeof(double),nRecordsP);
    }

    TimeStep::~TimeStep() {
        for (int i=0;i<nFields;++i) CPLFree(papadfData[i]);
        CPLFree(papadfData);
    }

    /****************************************************************/
    /*                       TimeStepList                           */
    /****************************************************************/
    TimeStepList::~TimeStepList() {
        TimeStepList *poFirst=this;
        while( poFirst != 0 )
        {
            TimeStepList *poTmp = poFirst->poNext;
            delete poFirst->poStep;
            delete poFirst;
            poFirst = poTmp;
        }
    }
#endif

    /****************************************************************/
    /*                     General functions                        */
    /****************************************************************/
    int read_integer(VSILFILE *fp,int &nData,bool bDiscard) {
        unsigned char anb[4];
        if (VSIFReadL(anb,1,4,fp)<4) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        };
        if (!bDiscard) {
            memcpy(&nData, anb, 4);
            CPL_MSBPTR32(&nData);
        }
        return 1;
    }

    int write_integer(VSILFILE *fp,int nData) {
        unsigned char anb[4];
        CPL_MSBPTR32(&nData);
        memcpy(anb, &nData, 4);
        if (VSIFWriteL(anb,1,4,fp)<4) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        };
        return 1;
    }

    int read_string(VSILFILE *fp,char *&pszData,vsi_l_offset nFileSize,bool bDiscard) {
        int nLength=0;
        read_integer(fp,nLength);
        if (nLength<=0 || nLength == INT_MAX || static_cast<unsigned>(nLength) > nFileSize) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        }
        if (bDiscard) {
            if (VSIFSeekL(fp,nLength+4,SEEK_CUR)!=0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return 0;
            }
        }
        else {
            pszData=(char*)VSI_MALLOC_VERBOSE(nLength+1);
            if( pszData == nullptr )
            {
                return 0;
            }
            if ((int)VSIFReadL(pszData,1,nLength,fp)<(int)nLength) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                VSIFree(pszData);
                pszData = nullptr;
                return 0;
            }
            pszData[nLength]=0;
            if (VSIFSeekL(fp,4,SEEK_CUR)!=0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                VSIFree(pszData);
                pszData = nullptr;
                return 0;
            }
        }
        return nLength;
    }

    int write_string(VSILFILE *fp,char *pszData,size_t nLength) {
        if (nLength==0) nLength=strlen(pszData);
        if (write_integer(fp,static_cast<int>(nLength))==0) return 0;
        if (VSIFWriteL(pszData,1,nLength,fp)<nLength) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        }
        if (write_integer(fp,static_cast<int>(nLength))==0) return 0;
        return 1;
    }

    int read_intarray(VSILFILE *fp,int *&panData,vsi_l_offset nFileSize,bool bDiscard) {
        int nLength=0;
        read_integer(fp,nLength);
        panData = nullptr;
        if (nLength<0 || static_cast<unsigned>(nLength)/4 > nFileSize) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return -1;
        }
        if (bDiscard) {
            if (VSIFSeekL(fp,nLength+4,SEEK_CUR)!=0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
        }
        else {
            if (nLength==0) panData=nullptr; else {
                panData=(int *)VSI_MALLOC2_VERBOSE(nLength/4,sizeof(int));
                if (panData==nullptr) return -1;
            }
            for (int i=0;i<nLength/4;++i) if (read_integer(fp,panData[i])==0) {
                CPLFree(panData);
                panData = nullptr;
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
            if (VSIFSeekL(fp,4,SEEK_CUR)!=0) {
                CPLFree(panData);
                panData = nullptr;
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
        }
        return nLength/4;
    }

    int write_intarray(VSILFILE *fp,int *panData,size_t nLength) {
        if (write_integer(fp,static_cast<int>(nLength*4))==0) return 0;
        for (size_t i=0;i<nLength;++i) {
            if (write_integer(fp,panData[i])==0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return 0;
            }
        }
        if (write_integer(fp,static_cast<int>(nLength*4))==0) return 0;
        return 1;
    }

    int read_float(VSILFILE *fp, double &dfData, bool bDiscard)
    {
        float dfVal = 0.0;
        if (VSIFReadL(&dfVal,1,4,fp)<4) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        };
        if (!bDiscard) {
            CPL_MSBPTR32(&dfVal);
            dfData=dfVal;
        }
        return 1;
    }

    int write_float(VSILFILE *fp, double dfData) {
        float dfVal=(float)dfData;
        CPL_MSBPTR32(&dfVal);
        if (VSIFWriteL(&dfVal,1,4,fp)<4) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        };
        return 1;
    }

    int read_floatarray(VSILFILE *fp,double **papadfData,vsi_l_offset nFileSize,bool bDiscard) {
        int nLength=0;
        read_integer(fp,nLength);
        if (nLength<0 || static_cast<unsigned>(nLength)/4 > nFileSize) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return -1;
        }
        if (bDiscard) {
            if (VSIFSeekL(fp,nLength+4,SEEK_CUR)!=0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
        }
        else {
            if (nLength==0) *papadfData=nullptr; else {
                *papadfData=(double*)VSI_MALLOC2_VERBOSE(sizeof(double),nLength/4);
                if (*papadfData==nullptr) return -1;
            }
            for (int i=0;i<nLength/4;++i) if (read_float(fp,(*papadfData)[i])==0) {
                CPLFree(*papadfData);
                *papadfData = nullptr;
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
            if (VSIFSeekL(fp,4,SEEK_CUR)!=0) {
                CPLFree(*papadfData);
                *papadfData = nullptr;
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
        }
        return nLength/4;
    }

    int write_floatarray(VSILFILE *fp,double *papadfData,size_t nLength) {
        if (write_integer(fp,static_cast<int>(nLength*4))==0) return 0;
        for (size_t i=0;i<nLength;++i) {
            if (write_float(fp,papadfData[i])==0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return 0;
            }
        }
        if (write_integer(fp,static_cast<int>(nLength*4))==0) return 0;
        return 1;
    }

    void Header::UpdateFileSize()
    {
        VSIFSeekL(fp,0,SEEK_END);
        nFileSize = VSIFTellL(fp);
        VSIRewindL(fp);
    }

    Header *read_header(VSILFILE *fp,const char *pszFilename) {
        // Save the filename
        Header *poHeader=new Header();
        poHeader->fp=fp;
        poHeader->UpdateFileSize();
        poHeader->pszFilename=CPLStrdup(pszFilename);
        int *panTemp = nullptr;
        // Read the title
        int nLength = read_string(fp,poHeader->pszTitle,poHeader->nFileSize);
        if (nLength==0) {
            delete poHeader;
            return nullptr;
        }
        // Read the array of 2 integers, with the number of variables at the first position
        nLength=read_intarray(fp,panTemp,poHeader->nFileSize);
        if (nLength!=2) {
            delete poHeader;
            CPLFree(panTemp);
            return nullptr;
        }
        poHeader->nVar=panTemp[0];
        poHeader->anUnused[0]=panTemp[1];
        CPLFree(panTemp);
        if (poHeader->nVar<0) {
            poHeader->nVar = 0;
            delete poHeader;
            return nullptr;
        }
        if( poHeader->nVar > 1000000 &&
            poHeader->nFileSize / sizeof(int) < static_cast<unsigned>(poHeader->nVar))
        {
            poHeader->nVar = 0;
            delete poHeader;
            return nullptr;
        }
        // For each variable, read its name as a string of 32 characters
        poHeader->papszVariables=(char**)VSI_MALLOC2_VERBOSE(sizeof(char*),poHeader->nVar);
        if( poHeader->nVar > 0 && poHeader->papszVariables == nullptr )
        {
            poHeader->nVar = 0;
            delete poHeader;
            return nullptr;
        }
        for (int i=0;i<poHeader->nVar;++i) {
            nLength=read_string(fp,poHeader->papszVariables[i],poHeader->nFileSize);
            if (nLength==0) {
                poHeader->nVar = i;
                delete poHeader;
                return nullptr;
            }
            // We eliminate quotes in the names of the variables because SQL requests don't seem to appreciate them
            char *pszc=poHeader->papszVariables[i];
            while (*pszc!=0) {
                if (*pszc=='\'') *pszc=' ';
                pszc++;
            }
        }
        // Read an array of 10 integers
        nLength=read_intarray(fp,panTemp,poHeader->nFileSize);
        if (nLength<10) {
            delete poHeader;
            CPLFree(panTemp);
            return nullptr;
        }
        poHeader->anUnused[1]=panTemp[0];
        poHeader->nEpsg=panTemp[1];
        poHeader->adfOrigin[0]=panTemp[2];
        poHeader->adfOrigin[1]=panTemp[3];
        for (size_t i=4;i<9;++i) poHeader->anUnused[i-2]=panTemp[i];
        // If the last integer was 1, read an array of 6 integers with the starting date
        if (panTemp[9]==1) {
            nLength=read_intarray(fp,poHeader->panStartDate,poHeader->nFileSize);
            if (nLength<6) {
                delete poHeader;
                CPLFree(panTemp);
                return nullptr;
            }
        }
        CPLFree(panTemp);
        // Read an array of 4 integers with the number of elements, points and points per element
        nLength=read_intarray(fp,panTemp,poHeader->nFileSize);
        if (nLength<4) {
            delete poHeader;
            CPLFree(panTemp);
            return nullptr;
        }
        poHeader->nElements=panTemp[0];
        poHeader->nPoints=panTemp[1];
        poHeader->nPointsPerElement=panTemp[2];
        if (poHeader->nElements<0 || poHeader->nPoints<0 || poHeader->nPointsPerElement<0 || panTemp[3]!=1) {
            delete poHeader;
            CPLFree(panTemp);
            return nullptr;
        }
        CPLFree(panTemp);
        // Read the connectivity table as an array of nPointsPerElement*nElements integers, and check if all point numbers are valid
        nLength=read_intarray(fp,poHeader->panConnectivity,poHeader->nFileSize);
        if (poHeader->nElements != 0 && nLength/poHeader->nElements != poHeader->nPointsPerElement) {
            delete poHeader;
            return nullptr;
        }
        for (int i=0;i<poHeader->nElements*poHeader->nPointsPerElement;++i) {
            if (poHeader->panConnectivity[i]<=0 || poHeader->panConnectivity[i]>poHeader->nPoints) {
                delete poHeader;
                return nullptr;
            }
        }
        // Read the array of nPoints integers with the border points
        nLength=read_intarray(fp,poHeader->panBorder,poHeader->nFileSize);
        if (nLength!=poHeader->nPoints) {
            delete poHeader;
            return nullptr;
        }
        // Read two arrays of nPoints floats with the coordinates of each point
        for (size_t i=0;i<2;++i) {
            read_floatarray(fp,poHeader->paadfCoords+i,poHeader->nFileSize);
            if (nLength<poHeader->nPoints) {
                delete poHeader;
                return nullptr;
            }
            if( poHeader->nPoints != 0 && poHeader->paadfCoords[i] == nullptr )
            {
                delete poHeader;
                return nullptr;
            }
            for (int j=0;j<poHeader->nPoints;++j) poHeader->paadfCoords[i][j]+=poHeader->adfOrigin[i];
        }
        // Update the boundinx box
        poHeader->updateBoundingBox();
        // Update the size of the header and calculate the number of time steps
        poHeader->setUpdated();
        int nPos=poHeader->getPosition(0);
        if( static_cast<vsi_l_offset>(nPos) > poHeader->nFileSize )
        {
            delete poHeader;
            return nullptr;
        }
        vsi_l_offset nStepsBig = poHeader->nVar != 0 ? (poHeader->nFileSize-nPos)/(poHeader->getPosition(1)-nPos) : 0;
        if( nStepsBig > INT_MAX )
            poHeader->nSteps=INT_MAX;
        else
            poHeader->nSteps= static_cast<int>(nStepsBig);
        return poHeader;
    }

    int write_header(VSILFILE *fp,Header *poHeader) {
        VSIRewindL(fp);
        if (write_string(fp,poHeader->pszTitle,80)==0) return 0;
        int anTemp[10]={0};
        anTemp[0]=poHeader->nVar;
        anTemp[1]=poHeader->anUnused[0];
        if (write_intarray(fp,anTemp,2)==0) return 0;
        for (int i=0;i<poHeader->nVar;++i) if (write_string(fp,poHeader->papszVariables[i],32)==0) return 0;
        anTemp[0]=poHeader->anUnused[1];
        anTemp[1]=poHeader->nEpsg;
        anTemp[2]=(int)poHeader->adfOrigin[0];
        anTemp[3]=(int)poHeader->adfOrigin[1];
        for (size_t i=4;i<9;++i) anTemp[i]=poHeader->anUnused[i-2];
        anTemp[9]=(poHeader->panStartDate!=nullptr)?1:0;
        if (write_intarray(fp,anTemp,10)==0) return 0;
        if (poHeader->panStartDate!=nullptr && write_intarray(fp,poHeader->panStartDate,6)==0) return 0;
        anTemp[0]=poHeader->nElements;
        anTemp[1]=poHeader->nPoints;
        anTemp[2]=poHeader->nPointsPerElement;
        anTemp[3]=1;
        if (write_intarray(fp,anTemp,4)==0) return 0;
        if (write_intarray(fp,poHeader->panConnectivity,poHeader->nElements*poHeader->nPointsPerElement)==0) return 0;
        if (write_intarray(fp,poHeader->panBorder,poHeader->nPoints)==0) return 0;
        double *dfVals = (double*)
            VSI_MALLOC2_VERBOSE(sizeof(double),poHeader->nPoints);
        if (poHeader->nPoints>0 && dfVals==nullptr) return 0;
        for (size_t i=0;i<2;++i) {
            for (int j=0;j<poHeader->nPoints;++j) dfVals[j]=poHeader->paadfCoords[i][j]-poHeader->adfOrigin[i];
            if (write_floatarray(fp,dfVals,poHeader->nPoints)==0) {
                CPLFree(dfVals);
                return 0;
            }
        }
        CPLFree(dfVals);
        return 1;
    }

#ifdef notdef
    int read_step(VSILFILE *fp,const Header *poHeader,TimeStep *&poStep) {
        poStep=new TimeStep(poHeader->nPoints,poHeader->nVar);
        int nLength = 0;
        if (read_integer(fp,nLength)==0 || nLength!=1) {
            delete poStep;
            return 0;
        }
        if (read_float(fp,poStep->dfDate)==0) {
            delete poStep;
            return 0;
        }
        if (read_integer(fp,nLength)==0 || nLength!=1) {
            delete poStep;
            return 0;
        }
        for (int i=0;i<poHeader->nVar;++i) {
            nLength=read_floatarray(fp,&(poStep->papadfData[i]));
            if (nLength!=poHeader->nPoints) {
                delete poStep;
                return 0;
            }
        }
        return 1;
    }

    int write_step(VSILFILE *fp,const Header *poHeader,const TimeStep *poStep) {
        if (write_integer(fp,1)==0) return 0;
        if (write_float(fp,poStep->dfDate)==0) return 0;
        if (write_integer(fp,1)==0) return 0;
        for (int i=0;i<poHeader->nVar;++i) {
            if (write_floatarray(fp,poStep->papadfData[i])==0) return 0;
        }
        return 1;
    }

    int read_steps(VSILFILE *fp,const Header *poHeader,TimeStepList *&poSteps) {
        poSteps=0;
        TimeStepList *poCur,*poNew;
        for (int i=0;i<poHeader->nSteps;++i) {
            poNew=new TimeStepList(0,0);
            if (read_step(fp,poHeader,poNew->poStep)==0) {
                delete poSteps;
                return 0;
            }
            if (poSteps==0) poSteps=poNew; else poCur->poNext=poNew;
            poCur=poNew;
        }
        return 1;
    }

    int write_steps(VSILFILE *fp,const Header *poHeader,const TimeStepList *poSteps) {
        const TimeStepList *poCur=poSteps;
        while (poCur!=0) {
            if (write_step(fp,poHeader,poCur->poStep)==0) return 0;
            poCur=poCur->poNext;
        }
        return 1;
    }
#endif
}
