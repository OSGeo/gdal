/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Reader
 * Purpose:  Private Declarations for Reader code.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _CPL_ILI1READERP_H_INCLUDED
#define _CPL_ILI1READERP_H_INCLUDED

#include "ili1reader.h"
#include "ogr_ili1.h"
#include "iom/iom.h"


class ILI1Reader;
class OGRILI1Layer;

/************************************************************************/
/*                              ILI1Reader                              */
/************************************************************************/

class ILI1Reader : public IILI1Reader
{
private:
    FILE         *fpItf;
    int          nLayers;
    OGRILI1Layer **papoLayers;
    OGRILI1Layer *curLayer;
    OGRILI1Layer *metaLayer;
    double       arcIncr;
    char         codeBlank;
    char         codeUndefined;
    char         codeContinue;

public:
                 ILI1Reader();
                ~ILI1Reader();

    void         SetArcDegrees(double arcDegrees);
    int          OpenFile( const char *pszFilename );
    int          ReadModel( const char *pszModelFilename );
    int          ReadFeatures();
    int          ReadTable(const char *layername);
    void         ReadGeom(char **stgeom, OGRwkbGeometryType eType, OGRFeature *feature);
    char         **ReadParseLine();

    void         AddLayer( OGRILI1Layer * poNewLayer );
    int          AddIliGeom(OGRFeature *feature, int iField, long fpos);
    OGRILI1Layer *GetLayer( int );
    OGRILI1Layer *GetLayerByName( const char* );
    int          GetLayerCount();

    int          HasMultiplePointGeom(const char* layername);
    char*        GetPointLayerName(const char* layername, char* newlayername);
    const char*  GetLayerNameString(const char* topicname, const char* tablename);
    const char*  GetLayerName(IOM_BASKET model, IOM_OBJECT table);
    void         AddCoord(OGRILI1Layer* layer, IOM_BASKET model, IOM_OBJECT modelele, IOM_OBJECT typeobj);
    void         AddEnumTable(OGRILI1Layer* layer, IOM_BASKET model, IOM_OBJECT enumeration);
    OGRILI1Layer* AddGeomTable(const char* datalayername, const char* geomname, OGRwkbGeometryType eType);
    void         AddField(OGRILI1Layer* layer, IOM_BASKET model, IOM_OBJECT obj);
    unsigned int GetCoordDim(IOM_BASKET model, IOM_OBJECT typeobj);
};


#endif
