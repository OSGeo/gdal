/******************************************************************************
 * Project:  Selafin importer
 * Purpose:  Definition of classes for OGR driver for Selafin files.
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

#ifndef _OGR_SELAFIN_H_INCLUDED
#define _OGR_SELAFIN_H_INCLUDED

#include "io_selafin.h"
#include "ogrsf_frmts.h"

class OGRSelafinDataSource;

typedef enum {POINTS,ELEMENTS,ALL} SelafinTypeDef;

/************************************************************************/
/*                             Range                                    */
/************************************************************************/
class Range {
    private:
        typedef struct List {
            SelafinTypeDef eType;
            long nMin,nMax;
            List *poNext;
            List():poNext(0) {}
            List(SelafinTypeDef eTypeP,long nMinP,long nMaxP,List *poNextP):eType(eTypeP),nMin(nMinP),nMax(nMaxP),poNext(poNextP) {}
        } List;
        List *poVals,*poActual;
        long nMaxValue;
        static void sortList(List *&poList,List *poEnd=0);
        static void deleteList(List *poList);
    public:
        Range():poVals(0),poActual(0),nMaxValue(0) {}
        void setRange(const char *pszStr);
        ~Range();
        void setMaxValue(long nMaxValueP);
        bool contains(SelafinTypeDef eType,long nValue) const;
        size_t getSize() const;
};


/************************************************************************/
/*                             OGRSelafinLayer                          */
/************************************************************************/

class OGRSelafinLayer : public OGRLayer {
    private:
        SelafinTypeDef eType;
        int bUpdate;
        long nStepNumber;
        Selafin::Header *poHeader;
        OGRFeatureDefn *poFeatureDefn;
        OGRSpatialReference *poSpatialRef;
        long nCurrentId;
    public:
        OGRSelafinLayer( const char *pszLayerNameP, int bUpdateP,OGRSpatialReference *poSpatialRefP,Selafin::Header *poHeaderP,int nStepNumberP,SelafinTypeDef eTypeP);
        ~OGRSelafinLayer();
        OGRSpatialReference *GetSpatialRef() {return poSpatialRef;}
        long GetStepNumber() {return nStepNumber;}
        OGRFeature *GetNextFeature();
        OGRFeature *GetFeature(GIntBig nFID);
        void ResetReading();
        OGRErr SetNextByIndex(GIntBig nIndex);
        OGRFeatureDefn *GetLayerDefn() {return poFeatureDefn;}
        int TestCapability(const char *pszCap);
        GIntBig GetFeatureCount(int bForce=TRUE);
        OGRErr GetExtent(OGREnvelope *psExtent,int bForce=TRUE);
        OGRErr ISetFeature(OGRFeature *poFeature);
        OGRErr ICreateFeature(OGRFeature *poFeature);
        OGRErr CreateField(OGRFieldDefn *poField,int bApproxOK=TRUE);
        OGRErr DeleteField(int iField);
        OGRErr ReorderFields(int *panMap);
        OGRErr AlterFieldDefn(int iField,OGRFieldDefn *poNewFieldDefn,int nFlags);
        OGRErr DeleteFeature(GIntBig nFID);
};

/************************************************************************/
/*                           OGRSelafinDataSource                       */
/************************************************************************/

class OGRSelafinDataSource : public OGRDataSource {
    private:
        char *pszName;
        char *pszLockName;
        OGRSelafinLayer **papoLayers;
        Range poRange;
        int nLayers;
        int bUpdate;
        Selafin::Header *poHeader;
        CPLString osDefaultSelafinName;
        OGRSpatialReference *poSpatialRef;
        int TakeLock(const char *pszFilename);
        void ReleaseLock();
    public:
        OGRSelafinDataSource();
        ~OGRSelafinDataSource();
        int Open(const char * pszFilename, int bUpdate, int bCreate);
        int OpenTable(const char * pszFilename);
        const char *GetName() { return pszName; }
        int GetLayerCount() { return nLayers; }
        OGRLayer *GetLayer( int );
        virtual OGRLayer *ICreateLayer( const char *pszName, OGRSpatialReference *poSpatialRefP = NULL, OGRwkbGeometryType eGType = wkbUnknown, char ** papszOptions = NULL );
        virtual OGRErr DeleteLayer(int); 
        int TestCapability( const char * );
        void SetDefaultSelafinName( const char *pszName ) { osDefaultSelafinName = pszName; }
};

#endif /* ndef _OGR_SELAFIN_H_INCLUDED */
