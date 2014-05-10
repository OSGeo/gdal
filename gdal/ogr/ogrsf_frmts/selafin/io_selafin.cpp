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

namespace Selafin {

    const char SELAFIN_ERROR_MESSAGE[]="Error when reading Selafin file\n";

    struct Point {
        long nIndex;
        const Header *poHeader;
    };

    void GetBoundsFunc(const void *hFeature,CPLRectObj *poBounds) {
        const Point *poPoint=(const Point*)hFeature;
        poBounds->minx=poPoint->poHeader->paadfCoords[0][poPoint->nIndex];
        poBounds->maxx=poPoint->poHeader->paadfCoords[0][poPoint->nIndex];
        poBounds->miny=poPoint->poHeader->paadfCoords[1][poPoint->nIndex];
        poBounds->maxy=poPoint->poHeader->paadfCoords[1][poPoint->nIndex];
    }

    int DumpFeatures(void *pElt,void *pUserData) {
        Point *poPoint=(Point*)pElt;
        delete poPoint;
        return TRUE;
    }


    /****************************************************************/
    /*                         Header                               */
    /****************************************************************/
    Header::Header():nMinxIndex(-1),nMaxxIndex(-1),nMinyIndex(-1),nMaxyIndex(-1),bTreeUpdateNeeded(true),fp(0),pszFilename(0),pszTitle(0),papszVariables(0),nPointsPerElement(0),panConnectivity(0),poTree(0),panBorder(0),panStartDate(0),nEpsg(0) {
        paadfCoords[0]=0;
        paadfCoords[1]=0;
        for (size_t i=0;i<7;++i) anUnused[i]=0;
    } 

    Header::~Header() {
        if (pszFilename!=0) CPLFree(pszFilename);
        if (pszTitle!=0) CPLFree(pszTitle);
        if (papszVariables!=0) {
            for (int i=0;i<nVar;++i) if (papszVariables[i]!=0) CPLFree(papszVariables[i]);
            CPLFree(papszVariables);
        }
        if (panConnectivity!=0) CPLFree(panConnectivity);
        if (panBorder!=0) CPLFree(panBorder);
        if (poTree!=0) {
            CPLQuadTreeForeach(poTree,DumpFeatures,0);
            CPLQuadTreeDestroy(poTree);
        }
        if (panStartDate!=0) CPLFree(panStartDate);
        for (size_t i=0;i<2;++i) if (paadfCoords[i]!=0) CPLFree(paadfCoords[i]);
        if (fp!=0) VSIFCloseL(fp);
    }

    void Header::setUpdated() {
        nHeaderSize=88+16+nVar*40+12*4+((panStartDate==0)?0:32)+24+(nElements*nPointsPerElement+2)*4+(nPoints+2)*12;
        nStepSize=12+nVar*(nPoints+2)*4;
    }

    long Header::getPosition(long nStep,long nFeature,long nAttribute) const {
        long a=(nFeature!=-1 || nAttribute!=-1)?(12+nAttribute*(nPoints+2)*4+4+nFeature*4):0;
        long b=nStep*nStepSize;
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
            for (long i=1;i<nPoints;++i) if (paadfCoords[0][i]<paadfCoords[0][nMinxIndex]) nMinxIndex=i;
            nMaxxIndex=0;
            for (long i=1;i<nPoints;++i) if (paadfCoords[0][i]>paadfCoords[0][nMaxxIndex]) nMaxxIndex=i;
            nMinyIndex=0;
            for (long i=1;i<nPoints;++i) if (paadfCoords[1][i]<paadfCoords[1][nMinyIndex]) nMinyIndex=i;
            nMaxyIndex=0;
            for (long i=1;i<nPoints;++i) if (paadfCoords[1][i]>paadfCoords[1][nMaxyIndex]) nMaxyIndex=i;
        }
    }

    long Header::getClosestPoint(const double &dfx,const double &dfy,const double &dfMax) {
        // If there is no quad-tree of the points, build it now
        if (bTreeUpdateNeeded) {
            if (poTree!=0) {
                CPLQuadTreeForeach(poTree,DumpFeatures,0);
                CPLQuadTreeDestroy(poTree);
            }
        }
        if (bTreeUpdateNeeded || poTree==0) {
            bTreeUpdateNeeded=false;
            CPLRectObj *poBB=getBoundingBox();
            poTree=CPLQuadTreeCreate(poBB,GetBoundsFunc);
            delete poBB;
            CPLQuadTreeSetBucketCapacity(poTree,2);
            for (long i=0;i<nPoints;++i) {
                Point *poPoint=new Point;
                poPoint->poHeader=this;
                poPoint->nIndex=i;
                CPLQuadTreeInsert(poTree,(void*)poPoint);
            }
        }
        // Now we can look for the nearest neighbour using this tree
        long nIndex=-1;
        double dfMin;
        CPLRectObj poObj;
        poObj.minx=dfx-dfMax;
        poObj.maxx=dfx+dfMax;
        poObj.miny=dfy-dfMax;
        poObj.maxy=dfy+dfMax;
        int nFeatureCount;
        void **phResults=CPLQuadTreeSearch(poTree,&poObj,&nFeatureCount);
        if (nFeatureCount<=0) return -1;
        double dfa,dfb,dfc;
        dfMin=dfMax*dfMax;
        for (long i=0;i<nFeatureCount;++i) {
            Point *poPoint=(Point*)(phResults[i]);
            dfa=dfx-poPoint->poHeader->paadfCoords[0][poPoint->nIndex];
            dfa*=dfa;
            if (dfa>=dfMin) continue;
            dfb=dfy-poPoint->poHeader->paadfCoords[1][poPoint->nIndex];
            dfc=dfa+dfb*dfb;
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
        panBorder=(long*)CPLRealloc(panBorder,sizeof(long)*nPoints);
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

    void Header::removePoint(long nIndex) {
        // We remove the point from all the tables
        nPoints--;
        for (size_t i=0;i<2;++i) {
            for (long j=nIndex;j<nPoints;++j) paadfCoords[i][j]=paadfCoords[i][j+1];
            paadfCoords[i]=(double*)CPLRealloc(paadfCoords[i],sizeof(double)*nPoints);
        }
        for (long j=nIndex;j<nPoints;++j) panBorder[j]=panBorder[j+1];
        panBorder=(long*)CPLRealloc(panBorder,sizeof(long)*nPoints);

        // We must also remove all the elements referencing the deleted feature, otherwise the file will not be consistent any longer
        long nOldElements=nElements;
        for (long i=0;i<nElements;++i) {
            bool bReferencing=false;
            long *panTemp=panConnectivity+i*nPointsPerElement;
            for (long j=0;j<nPointsPerElement;++j) bReferencing |= (panTemp[j]==nIndex+1);
            if (bReferencing) {
                nElements--;
                for (long j=i;j<nElements;++j) 
                    for (long k=0;k<nPointsPerElement;++k) panConnectivity[j*nPointsPerElement+k]=panConnectivity[(j+1)*nPointsPerElement+k];
                --i;
            }
        }
        if (nOldElements!=nElements) panConnectivity=(long*)CPLRealloc(panConnectivity,sizeof(long)*nElements*nPointsPerElement);

        // Now we update the bounding box if needed
        if (nPoints==0) {nMinxIndex=-1;nMaxxIndex=-1;nMinyIndex=-1;nMaxyIndex=-1;} else {
            if (nIndex==nMinxIndex) {
                nMinxIndex=0;
                for (long i=1;i<nPoints;++i) if (paadfCoords[0][i]<paadfCoords[0][nMinxIndex]) nMinxIndex=i;
            }
            if (nIndex==nMaxxIndex) {
                nMaxxIndex=0;
                for (long i=1;i<nPoints;++i) if (paadfCoords[0][i]>paadfCoords[0][nMaxxIndex]) nMaxxIndex=i;
            }
            if (nIndex==nMinyIndex) {
                nMinyIndex=0;
                for (long i=1;i<nPoints;++i) if (paadfCoords[1][i]<paadfCoords[1][nMinyIndex]) nMinyIndex=i;
            }
            if (nIndex==nMaxxIndex) {
                nMaxyIndex=0;
                for (long i=1;i<nPoints;++i) if (paadfCoords[1][i]>paadfCoords[1][nMaxyIndex]) nMaxyIndex=i;
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
    TimeStep::TimeStep(long nRecordsP,long nFieldsP):nFields(nFieldsP) {
        papadfData=(double**)VSIMalloc2(sizeof(double*),nFieldsP);
        for (long i=0;i<nFieldsP;++i) papadfData[i]=(double*)VSIMalloc2(sizeof(double),nRecordsP);
    }
    
    TimeStep::~TimeStep() {
        for (long i=0;i<nFields;++i) CPLFree(papadfData[i]);
        CPLFree(papadfData);
    }

    /****************************************************************/
    /*                       TimeStepList                           */
    /****************************************************************/
    TimeStepList::~TimeStepList() {
        TimeStepList *poFirst=this;
        TimeStepList *poTmp;
        while (poFirst!=0) {
            poTmp=poFirst->poNext;
            delete poFirst->poStep;
            delete poFirst;
            poFirst=poTmp;
        }
    }
#endif

    /****************************************************************/
    /*                     General functions                        */
    /****************************************************************/
    int read_integer(VSILFILE *fp,long &nData,bool bDiscard) {
        unsigned char anb[4];
        if (VSIFReadL(anb,1,4,fp)<4) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        };
        if (!bDiscard) {
            nData=0;
            for (size_t i=0;i<4;++i) nData=(nData*0x100)+anb[i];
        }
        return 1;
    }

    int write_integer(VSILFILE *fp,long nData) {
        unsigned char anb[4];
        for (int i=3;i>=0;--i) {
            anb[i]=nData%0x100;
            nData/=0x100;
        }
        if (VSIFWriteL(anb,1,4,fp)<4) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        };
        return 1;
    }
    
    long read_string(VSILFILE *fp,char *&pszData,bool bDiscard) {
        long nLength=0;
        read_integer(fp,nLength);
        if (nLength<=0 || nLength+1<=0) {
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
            pszData=(char*)CPLMalloc(sizeof(char)*(nLength+1));
            if ((long)VSIFReadL(pszData,1,nLength,fp)<(long)nLength) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return 0;
            }
            pszData[nLength]=0;
            if (VSIFSeekL(fp,4,SEEK_CUR)!=0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return 0;
            }
        }
        return nLength;
    }

    int write_string(VSILFILE *fp,char *pszData,size_t nLength) {
        if (nLength==0) nLength=strlen(pszData);
        if (write_integer(fp,nLength)==0) return 0;
        if (VSIFWriteL(pszData,1,nLength,fp)<nLength) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        }
        if (write_integer(fp,nLength)==0) return 0;
        return 1;
    }

    long read_intarray(VSILFILE *fp,long *&panData,bool bDiscard) {
        long nLength=0;
        read_integer(fp,nLength);
        if (nLength<0 || nLength+1<=0) {
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
            if (nLength==0) panData=0; else {
                panData=(long *)VSIMalloc2(nLength/4,sizeof(long));
                if (panData==0) return -1;
            }
            for (long i=0;i<nLength/4;++i) if (read_integer(fp,panData[i])==0) {
                CPLFree(panData);
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
            if (VSIFSeekL(fp,4,SEEK_CUR)!=0) {
                if (panData!=0) CPLFree(panData);
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
        }
        return nLength/4;
    }

    int write_intarray(VSILFILE *fp,long *panData,size_t nLength) {
        if (write_integer(fp,nLength*4)==0) return 0;
        for (size_t i=0;i<nLength;++i) {
            if (write_integer(fp,panData[i])==0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return 0;
            }
        }
        if (write_integer(fp,nLength*4)==0) return 0;
        return 1;
    }

    int read_float(VSILFILE *fp,double &dfData,bool bDiscard) {
        float dfVal;
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

    int write_float(VSILFILE *fp,double dfData) {
        float dfVal=(float)dfData;
        CPL_MSBPTR32(&dfVal);
        if (VSIFWriteL(&dfVal,1,4,fp)<4) {
            CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
            return 0;
        };
        return 1;
    }

    long read_floatarray(VSILFILE *fp,double **papadfData,bool bDiscard) {
        long nLength=0;
        read_integer(fp,nLength);
        if (nLength<0 || nLength+1<=0) {
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
            if (nLength==0) *papadfData=0; else {
                *papadfData=(double*)VSIMalloc2(sizeof(double),nLength/4);
                if (papadfData==0) return -1;
            }
            for (long i=0;i<nLength/4;++i) if (read_float(fp,(*papadfData)[i])==0) {
                CPLFree(papadfData);
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
            if (VSIFSeekL(fp,4,SEEK_CUR)!=0) {
                if (papadfData!=0) CPLFree(papadfData);
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return -1;
            }
        }
        return nLength/4;
    }

    int write_floatarray(VSILFILE *fp,double *papadfData,size_t nLength) {
        if (write_integer(fp,nLength*4)==0) return 0;
        for (size_t i=0;i<nLength;++i) {
            if (write_float(fp,papadfData[i])==0) {
                CPLError(CE_Failure,CPLE_FileIO,"%s",SELAFIN_ERROR_MESSAGE);
                return 0;
            }
        }
        if (write_integer(fp,nLength*4)==0) return 0;
        return 1;
    }

    Header *read_header(VSILFILE *fp,const char *pszFilename) {
        // Get the total file size (used later to estimate the number of time steps)
        long nFileSize;
        VSIFSeekL(fp,0,SEEK_END);
        nFileSize=(long)VSIFTellL(fp);
        VSIRewindL(fp);
        // Save the filename
        long nLength;
        Header *poHeader=new Header();
        poHeader->fp=fp;
        poHeader->pszFilename=CPLStrdup(pszFilename);
        long *panTemp;
        // Read the title
        nLength=read_string(fp,poHeader->pszTitle);
        if (nLength==0) {
            delete poHeader;
            return 0;
        }
        // Read the array of 2 integers, with the number of variables at the first position
        nLength=read_intarray(fp,panTemp);
        if (nLength!=2) {
            delete poHeader;
            CPLFree(panTemp);
            return 0;
        }
        poHeader->nVar=panTemp[0];
        poHeader->anUnused[0]=panTemp[1];
        CPLFree(panTemp);
        if (poHeader->nVar<0) {
            delete poHeader;
            return 0;
        }
        // For each variable, read its name as a string of 32 characters
        poHeader->papszVariables=(char**)VSIMalloc2(sizeof(char*),poHeader->nVar);
        for (long i=0;i<poHeader->nVar;++i) {
            nLength=read_string(fp,poHeader->papszVariables[i]);
            if (nLength==0) {
                delete poHeader;
                return 0;
            }
            // We eliminate quotes in the names of the variables because SQL requests don't seem to appreciate them
            char *pszc=poHeader->papszVariables[i];
            while (*pszc!=0) {
                if (*pszc=='\'') *pszc=' ';
                pszc++;
            }
        }
        // Read an array of 10 integers
        nLength=read_intarray(fp,panTemp);
        if (nLength<10) {
            delete poHeader;
            CPLFree(panTemp);
            return 0;
        }
        poHeader->anUnused[1]=panTemp[0];
        poHeader->nEpsg=panTemp[1];
        poHeader->adfOrigin[0]=panTemp[2];
        poHeader->adfOrigin[1]=panTemp[3];
        for (size_t i=4;i<9;++i) poHeader->anUnused[i-2]=panTemp[i];
        // If the last integer was 1, read an array of 6 integers with the starting date
        if (panTemp[9]==1) {
            nLength=read_intarray(fp,poHeader->panStartDate);
            if (nLength<6) {
                delete poHeader;
                CPLFree(panTemp);
                return 0;
            }
        }
        CPLFree(panTemp);
        // Read an array of 4 integers with the number of elements, points and points per element
        nLength=read_intarray(fp,panTemp);
        if (nLength<4) {
            delete poHeader;
            CPLFree(panTemp);
            return 0;
        }
        poHeader->nElements=panTemp[0];
        poHeader->nPoints=panTemp[1];
        poHeader->nPointsPerElement=panTemp[2];
        if (poHeader->nElements<0 || poHeader->nPoints<0 || poHeader->nPointsPerElement<0 || panTemp[3]!=1) {
            delete poHeader;
            CPLFree(panTemp);
            return 0;
        }
        CPLFree(panTemp);
        // Read the connectivity table as an array of nPointsPerElement*nElements integers, and check if all point numbers are valid
        nLength=read_intarray(fp,poHeader->panConnectivity);
        if (nLength!=poHeader->nElements*poHeader->nPointsPerElement) {
            delete poHeader;
            return 0;
        }
        for (long i=0;i<poHeader->nElements*poHeader->nPointsPerElement;++i) {
            if (poHeader->panConnectivity[i]<=0 || poHeader->panConnectivity[i]>poHeader->nPoints) {
                delete poHeader;
                return 0;
            }
        }
        // Read the array of nPoints integers with the border points
        nLength=read_intarray(fp,poHeader->panBorder);
        if (nLength!=poHeader->nPoints) {
            delete poHeader;
            return 0;
        }
        // Read two arrays of nPoints floats with the coordinates of each point
        for (size_t i=0;i<2;++i) {
            read_floatarray(fp,poHeader->paadfCoords+i);
            if (nLength<poHeader->nPoints) {
                delete poHeader;
                return 0;
            }
            for (long j=0;j<poHeader->nPoints;++j) poHeader->paadfCoords[i][j]+=poHeader->adfOrigin[i];
        }
        // Update the boundinx box
        poHeader->updateBoundingBox();
        // Update the size of the header and calculate the number of time steps
        poHeader->setUpdated();
        long nPos=poHeader->getPosition(0);
        poHeader->nSteps=(nFileSize-nPos)/(poHeader->getPosition(1)-nPos);
        return poHeader;
    }

    int write_header(VSILFILE *fp,Header *poHeader) {
        VSIRewindL(fp);
        if (write_string(fp,poHeader->pszTitle,80)==0) return 0;
        long anTemp[10]={0};
        anTemp[0]=poHeader->nVar;
        anTemp[1]=poHeader->anUnused[0];
        if (write_intarray(fp,anTemp,2)==0) return 0;
        for (long i=0;i<poHeader->nVar;++i) if (write_string(fp,poHeader->papszVariables[i],32)==0) return 0;
        anTemp[0]=poHeader->anUnused[1];
        anTemp[1]=poHeader->nEpsg;
        anTemp[2]=(long)poHeader->adfOrigin[0];
        anTemp[3]=(long)poHeader->adfOrigin[1];
        for (size_t i=4;i<9;++i) anTemp[i]=poHeader->anUnused[i-2];
        anTemp[9]=(poHeader->panStartDate!=0)?1:0;
        if (write_intarray(fp,anTemp,10)==0) return 0;
        if (poHeader->panStartDate!=0 && write_intarray(fp,poHeader->panStartDate,6)==0) return 0;
        anTemp[0]=poHeader->nElements;
        anTemp[1]=poHeader->nPoints;
        anTemp[2]=poHeader->nPointsPerElement;
        anTemp[3]=1;
        if (write_intarray(fp,anTemp,4)==0) return 0;
        if (write_intarray(fp,poHeader->panConnectivity,poHeader->nElements*poHeader->nPointsPerElement)==0) return 0;
        if (write_intarray(fp,poHeader->panBorder,poHeader->nPoints)==0) return 0;
        double *dfVals;
        dfVals=(double*)VSIMalloc2(sizeof(double),poHeader->nPoints);
        if (poHeader->nPoints>0 && dfVals==0) return 0;
        for (size_t i=0;i<2;++i) {
            for (long j=0;j<poHeader->nPoints;++j) dfVals[j]=poHeader->paadfCoords[i][j]-poHeader->adfOrigin[i];
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
        long nLength;
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
        for (long i=0;i<poHeader->nVar;++i) {
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
        for (long i=0;i<poHeader->nVar;++i) {
            if (write_floatarray(fp,poStep->papadfData[i])==0) return 0;
        }
        return 1;
    }

    int read_steps(VSILFILE *fp,const Header *poHeader,TimeStepList *&poSteps) {
        poSteps=0;
        TimeStepList *poCur,*poNew;
        for (long i=0;i<poHeader->nSteps;++i) {
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
