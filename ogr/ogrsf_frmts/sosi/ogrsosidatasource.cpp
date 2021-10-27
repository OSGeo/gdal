/******************************************************************************
 *
 * Project:  SOSI Data Source
 * Purpose:  Provide SOSI Data to OGR.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_sosi.h"
#include <map>
#include <math.h>

CPL_CVSID("$Id$")

/* This is the most common encoding for SOSI files. Let's at least try if
 * it is supported, or generate a meaningful error message.               */
#ifndef CPL_ENC_ISO8859_10
#  define CPL_ENC_ISO8859_10 "ISO8859-10"
#endif

#ifdef WRITE_SUPPORT
/************************************************************************/
/*                              utility methods                         */
/************************************************************************/

static int epsg2sosi (int nEPSG) {
    int nSOSI = 23;
    switch (nEPSG) {
        case 27391: /* NGO 1984 Axis I-VIII */
        case 27392:
        case 27393:
        case 27394:
        case 27395:
        case 27396:
        case 27397:
        case 27398: {
            nSOSI = nEPSG - 27390;
            break;
        }
        case 3043: /* UTM ZONE 31-36 */
        case 3044:
        case 3045:
        case 3046:
        case 3047:
        case 3048: {
            nSOSI = nEPSG - 3022;
            break;
        }
        case 23031: /* UTM ZONE 31-36 / ED50 */
        case 23032:
        case 23033:
        case 23034:
        case 23035:
        case 23036: {
            nSOSI = nEPSG - 23000;
            break;
        }
        case 4326: { /* WSG84 */
            nSOSI = 84;
            break;
        }
        default: {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "(Yet) unsupported coordinate system writing to SOSI "
                      "file: %i. Defaulting to EPSG:4326/SOSI 84.", nEPSG);
            }
    }
    return nSOSI;
}
#endif

static int sosi2epsg (int nSOSI) {
    int nEPSG = 4326;
    switch (nSOSI) {
        case 1: /* NGO 1984 Axis I-VIII */
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8: {
            nEPSG = 27390+nSOSI;
            break;
        }
        case 21: /* UTM ZONE 31-36 */
        case 22:
        case 23:
        case 24:
        case 25:
        case 26: {
            nEPSG = 3022+nSOSI;
            break;
        }
        case 31: /* UTM ZONE 31-36 / ED50 */
        case 32:
        case 33:
        case 34:
        case 35:
        case 36: {
            nEPSG = 23000+nSOSI;
            break;
        }
        case 84: { /* WSG84 */
            nEPSG = 4326;
            break;
        }
        default: {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "(Yet) unsupported coordinate system in SOSI-file: %i. "
                      "Defaulting to EPSG:4326.", nSOSI);
        }
    }
    return nEPSG;
}

/************************************************************************/
/*                              OGRSOSIDataSource()                     */
/************************************************************************/

OGRSOSIDataSource::OGRSOSIDataSource() {
    nLayers = 0;

    poFileadm = nullptr;
    poBaseadm = nullptr;
    papoBuiltGeometries = nullptr;
    papoLayers = nullptr;
    pszName = nullptr;
    poSRS = nullptr;

    poPolyHeaders = nullptr;
    poTextHeaders = nullptr;
    poPointHeaders = nullptr;
    poCurveHeaders = nullptr;

    pszEncoding = CPL_ENC_UTF8;
    nNumFeatures = 0;

    nMode = MODE_READING;
}

/************************************************************************/
/*                              ~OGRSOSIDataSource()                    */
/************************************************************************/

OGRSOSIDataSource::~OGRSOSIDataSource() {
    if (papoBuiltGeometries != nullptr) {
        for (unsigned int i=0; i<nNumFeatures; i++) {
            if (papoBuiltGeometries[i] != nullptr) {
                delete papoBuiltGeometries[i];
                papoBuiltGeometries[i] = nullptr;
            }
        }
        CPLFree(papoBuiltGeometries);
        papoBuiltGeometries = nullptr;
    }

    if (poPolyHeaders  != nullptr) delete poPolyHeaders;
    if (poTextHeaders  != nullptr) delete poTextHeaders;
    if (poPointHeaders != nullptr) delete poPointHeaders;
    if (poCurveHeaders != nullptr) delete poCurveHeaders;

    if (nMode == MODE_WRITING) {
        if (poFileadm != nullptr) LC_CloseSos  (poFileadm, RESET_IDX );
        if (poBaseadm != nullptr) LC_CloseBase (poBaseadm, RESET_IDX );
    } else {
        if (poFileadm != nullptr) LC_CloseSos  (poFileadm, SAVE_IDX );
        if (poBaseadm != nullptr) LC_CloseBase (poBaseadm, SAVE_IDX );
    }
    poFileadm = nullptr;
    poBaseadm = nullptr;

    if (papoLayers != nullptr) {
        for ( int i = 0; i < nLayers; i++ ) {
            delete papoLayers[i];
        }
        CPLFree(papoLayers);
    }

    if (poSRS != nullptr) poSRS->Release();
    if (pszName != nullptr) CPLFree(pszName);
}

static
OGRFeatureDefn *defineLayer(const char *szName, OGRwkbGeometryType szType, S2I *poHeaders, S2I **ppoHeadersNew) {
    OGRFeatureDefn *poFeatureDefn = new OGRFeatureDefn( szName );
    poFeatureDefn->SetGeomType( szType );
    S2I* poHeadersNew  = *ppoHeadersNew;

    for (S2I::iterator i=poHeaders->begin(); i!=poHeaders->end(); ++i) {
                OGRSOSIDataType* poType = SOSIGetType(i->first);
                OGRSOSISimpleDataType* poElements = poType->getElements();
                for (int k=0; k<poType->getElementCount(); k++) {
                    if (strcmp(poElements[k].GetName(),"")==0) continue;
                    OGRFieldDefn oFieldTemplate( poElements[k].GetName(), poElements[k].GetType() );
                    (*poHeadersNew)[CPLString(poElements[k].GetName())] = poFeatureDefn->GetFieldCount();
                    poFeatureDefn->AddFieldDefn( &oFieldTemplate );
                }
    }
    return poFeatureDefn;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

int  OGRSOSIDataSource::Open( const char *pszFilename, int bUpdate ) {
    papoBuiltGeometries = nullptr;
    poFileadm = nullptr;
    poBaseadm = nullptr;
    char *pszPos;

    if ( bUpdate ) {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Update access not supported by the SOSI driver." );
        return FALSE;
    }

    /* Check that the file exists otherwise HO_TestSOSI() emits an error */
    VSIStatBuf sStat;
    if( VSIStat(pszFilename, &sStat) != 0 )
        return FALSE;

    pszName = CPLStrdup( pszFilename );
    /* We ignore any layer parameters for now. */
    pszPos = strchr(pszName, ',');
    if (pszPos != nullptr) {
        pszPos[0] = '\0';
    }

    /* Confirm that we are dealing with a SOSI file. Used also by data
     * format auto-detection in some ogr utilities. */
    UT_INT64 nEnd = 0;
    int bIsSosi = HO_TestSOSI ( pszName, &nEnd );
    if ( bIsSosi == UT_FALSE ) {
        return FALSE; /* No error message: This is used by file format auto-detection */
    }

    short nStatus = 0, nDetStatus = 0; /* immediate status, detailed status */

    /* open index base and sosi file */
    poBaseadm = LC_OpenBase(LC_BASE);
    nStatus   = LC_OpenSos(pszName, LC_BASE_FRAMGR, LC_GML_IDX, LC_INGEN_STATUS,
                           &poFileadm, &nDetStatus);
    if ( nStatus == UT_FALSE ) {
        char *pszErrorMessage;
        LC_StrError(nDetStatus, &pszErrorMessage);
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "File %s could not be opened by SOSI Driver: %s", pszName, pszErrorMessage );
        return FALSE;
    }

    /* --------------------------------------------------------------------*
     *      Prefetch all the information needed to determine layers        *
     *      and prebuild LineString features for later assembly.           *
     * --------------------------------------------------------------------*/

    /* allocate room for one pointer per feature */
    nNumFeatures = static_cast<unsigned int>(poFileadm->lAntGr);
    papoBuiltGeometries = static_cast<OGRGeometry**>(
        VSI_MALLOC2_VERBOSE(nNumFeatures, sizeof(OGRGeometry*)));
    if (papoBuiltGeometries == nullptr) {
        nNumFeatures = 0;
        return FALSE;
    }
    for (unsigned int i=0; i<nNumFeatures; i++) papoBuiltGeometries[i] = nullptr;

    /* Various iterators and return values used to iterate through SOSI features */
    short          nName, nNumLines;
    long           nNumCoo;
    unsigned short nInfo;
    LC_SNR_ADM oSnradm;
    LC_BGR   oNextSerial;
    LC_BGR  *poNextSerial;
    poNextSerial =&oNextSerial;

    bool bPointLayer = FALSE; /* Initialize four layers for the different geometry types */
    bool bCurveLayer = FALSE;
    bool bPolyLayer  = FALSE;
    bool bTextLayer  = FALSE;
    poPolyHeaders  = new S2I();
    poPointHeaders = new S2I();
    poCurveHeaders = new S2I();
    poTextHeaders  = new S2I();

    LC_SBSn(&oSnradm, poFileadm, 0, nNumFeatures); /* Set FYBA search limits  */
    LC_InitNextBgr(poNextSerial);

    /* Prebuilding simple features and extracting layer information. */
    while (LC_NextBgr(poNextSerial,LC_FRAMGR)) {
        /* Fetch next group information */
        nName = LC_RxGr(poNextSerial, LES_OPTIMALT, &nNumLines, &nNumCoo, &nInfo);

        S2S oHeaders;
        S2S::iterator iHeaders;
        int iH;
        /* Extract all strings from group header. */
        for (short i=1; i<=nNumLines; i++) {
            char *pszLine = LC_GetGi(i);      /* Get one header line */
            if ((pszLine[0] == ':')||(pszLine[0] == '(')) continue;  /* If we have a continued REF line, skip it. */
            if (pszLine[0] == '!') continue;  /* If we have a comment line, skip it. */

            char *pszUTFLine = CPLRecode(pszLine, pszEncoding, CPL_ENC_UTF8); /* switch to UTF encoding here, if it is known. */
            char *pszUTFLineIter = pszUTFLine;
            while (pszUTFLineIter[0] == '.') pszUTFLineIter++; /* Skipping the dots at the beginning of a SOSI line */
            char *pszPos2 = strstr(pszUTFLineIter, " "); /* Split header and value */
            if (pszPos2 != nullptr) {
                CPLString osKey = CPLString(std::string(pszUTFLineIter,pszPos2)); /* FIXME: clean instantiation of CPLString? */
                CPLString osValue = CPLString(pszPos2+1);

                oHeaders[osKey]=osValue;          /* Add to header map */
                switch (nName) {             /* Add to header list for the corresponding layer, if it is not */
                case L_FLATE: {            /* in there already */
                    if (poPolyHeaders->find(osKey) == poPolyHeaders->end()) {
                        iH = static_cast<int>(poPolyHeaders->size());
                        (*poPolyHeaders)[osKey] = iH;
                    }
                    break;
                }
                case L_KURVE:
                case L_LINJE:
                case L_BUEP:  {    /* FIXME: maybe not use the same headers for both */
                    if (poCurveHeaders->find(osKey) == poCurveHeaders->end()) {
                        iH = static_cast<int>(poCurveHeaders->size());
                        (*poCurveHeaders)[osKey] = iH;
                    }
                    break;
                }
                case L_PUNKT:
                case L_SYMBOL: {
                    if (poPointHeaders->find(osKey) == poPointHeaders->end()) {
                        iH = static_cast<int>(poPointHeaders->size());
                        (*poPointHeaders)[osKey] = iH;
                    }
                    break;
                }
                case L_TEKST: {
                    if (poTextHeaders->find(osKey) == poTextHeaders->end()) {
                        iH = static_cast<int>(poTextHeaders->size());
                        (*poTextHeaders)[osKey] = iH;
                    }
                    break;
                }
                }
            }
            CPLFree(pszUTFLine);
        }

        /* Feature-specific tasks */
        switch (nName) {
        case L_PUNKT: {
            /* Pre-build a point feature. Activate point layer. */
            bPointLayer = TRUE;
            buildOGRPoint(oNextSerial.lNr);
            break;
        }
        case L_FLATE: {
            /* Activate polygon layer. */
            bPolyLayer = TRUE;
            /* cannot build geometries that reference others yet */
            break;
        }
        case L_KURVE:
        case L_LINJE: {
            /* Pre-build a line feature. Activate line/curve layer. */
            bCurveLayer = TRUE;
            buildOGRLineString(static_cast<int>(nNumCoo), oNextSerial.lNr);
            break;
        }
        case L_BUEP: {
            /* Pre-build a line feature as interpolation from an arc. Activate line/curve layer. */
            bCurveLayer = TRUE;
            buildOGRLineStringFromArc(oNextSerial.lNr);
            break;
        }
        case L_TEKST: {
            /* Pre-build a text line contour feature. Activate text layer. */
            /* Todo: observe only points 2ff if more than one point is given for follow mode */
            bTextLayer = TRUE;
            buildOGRMultiPoint(static_cast<int>(nNumCoo), oNextSerial.lNr);
            break;
        }
        case L_HODE: {
            /* Get SRS from SOSI header. */
            unsigned short nMask = LC_TR_ALLT;
            LC_TRANSPAR oTrans;
            if (LC_GetTransEx(&nMask,&oTrans) == UT_FALSE) {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "TRANSPAR section not found - No reference system information available.");
                return FALSE;
            }
            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            /* Get coordinate system from SOSI header. */
            int nEPSG = sosi2epsg(oTrans.sKoordsys);
            if (poSRS->importFromEPSG(nEPSG) != OGRERR_NONE) {
              CPLError( CE_Failure, CPLE_OpenFailed,
                        "OGR could not load coordinate system definition EPSG:%i.", nEPSG);
                return FALSE;
            }

            /* Get character encoding from SOSI header. */
            iHeaders = oHeaders.find("TEGNSETT");
            if (iHeaders != oHeaders.end()) {
                CPLString osLine = iHeaders->second;
                if (osLine.compare("ISO8859-1")==0) {
                    pszEncoding = CPL_ENC_ISO8859_1;
                } else if (osLine.compare("ISO8859-10")==0) {
                    pszEncoding = CPL_ENC_ISO8859_10;
                } else if (osLine.compare("UTF-8")==0) {
                    pszEncoding = CPL_ENC_UTF8;
                }
            }

            break;
        }
        default: {
            break;
        }
        }
    }

    /* -------------------------------------------------------------------- *
     *      Create a corresponding layers. One per geometry type            *
     * -------------------------------------------------------------------- */
    int l_nLayers = 0;
    if (bPolyLayer)  l_nLayers++;
    if (bCurveLayer) l_nLayers++;
    if (bPointLayer) l_nLayers++;
    if (bTextLayer) l_nLayers++;
    this->nLayers = l_nLayers;
    /* allocate some memory for up to three layers */
    papoLayers = (OGRSOSILayer **) VSI_MALLOC2_VERBOSE(sizeof(void*), l_nLayers);
    if( papoLayers == nullptr )
    {
        this->nLayers = 0;
        return FALSE;
    }

    /* Define each layer, using a proper feature definition, geometry type,
     * and adding every SOSI header encountered in the file as field. */
    if (bPolyLayer) {
        S2I * poHeadersNew = new S2I();
        OGRFeatureDefn *poFeatureDefn = defineLayer("polygons", wkbPolygon, poPolyHeaders, &poHeadersNew);
        delete poPolyHeaders;
        poPolyHeaders = poHeadersNew;
        poFeatureDefn->Reference();
        papoLayers[--l_nLayers] = new OGRSOSILayer( this, poFeatureDefn, poFileadm, poPolyHeaders );
    } else {
        delete poPolyHeaders;
        poPolyHeaders = nullptr;
    }
    if (bCurveLayer) {
        S2I * poHeadersNew = new S2I();
        OGRFeatureDefn *poFeatureDefn = defineLayer("lines", wkbLineString, poCurveHeaders, &poHeadersNew);
        delete poCurveHeaders;
        poCurveHeaders = poHeadersNew;
        poFeatureDefn->Reference();
        papoLayers[--l_nLayers] = new OGRSOSILayer( this, poFeatureDefn, poFileadm, poCurveHeaders );
    } else {
        delete poCurveHeaders;
        poCurveHeaders = nullptr;
    }
    if (bPointLayer) {
        S2I * poHeadersNew = new S2I();
        OGRFeatureDefn *poFeatureDefn = defineLayer("points", wkbPoint, poPointHeaders, &poHeadersNew);
        delete poPointHeaders;
        poPointHeaders = poHeadersNew;
        poFeatureDefn->Reference();
        papoLayers[--l_nLayers] = new OGRSOSILayer( this, poFeatureDefn, poFileadm, poPointHeaders );
    } else {
        delete poPointHeaders;
        poPointHeaders = nullptr;
    }
    if (bTextLayer) {
        S2I * poHeadersNew = new S2I();
        OGRFeatureDefn *poFeatureDefn = defineLayer("text", wkbMultiPoint, poTextHeaders, &poHeadersNew);
        delete poTextHeaders;
        poTextHeaders = poHeadersNew;
        poFeatureDefn->Reference();
        papoLayers[--l_nLayers] = new OGRSOSILayer( this, poFeatureDefn, poFileadm, poTextHeaders );
    } else {
        delete poTextHeaders;
        poTextHeaders = nullptr;
    }
    return TRUE;
}

#ifdef WRITE_SUPPORT
/************************************************************************/
/*                              Create()                                */
/************************************************************************/

int  OGRSOSIDataSource::Create( const char *pszFilename ) {
    short nStatus;
    short nDetStatus;

    poBaseadm = LC_OpenBase(LC_KLADD);
    nStatus   = LC_OpenSos(pszFilename, LC_SEKV_SKRIV, LC_NY_IDX, LC_INGEN_STATUS,
                           &poFileadm, &nDetStatus);
    if (nStatus == UT_FALSE) {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Could not open SOSI file for writing (Status %i).", nDetStatus );
        return FALSE;
    }

    LC_NyttHode(); /* Create new file header, will be written to file when all
                      header information elements are set. */

    return TRUE;
}

/************************************************************************/
/*                             ICreateLayer()                           */
/************************************************************************/

OGRLayer *OGRSOSIDataSource::ICreateLayer( const char *pszNameIn,
                                           OGRSpatialReference  *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           CPL_UNUSED char **papszOptions ) {
    /* SOSI does not really support layers - so let's first see that the global settings are consistent */
    if (poSRS == NULL) {
        if (poSpatialRef!=NULL) {
            poSRS = poSpatialRef;
            poSRS->Reference();

            const char *pszKoosys = poSRS->GetAuthorityCode("PROJCS");
            if (pszKoosys == NULL) {
                OGRErr err = poSRS->AutoIdentifyEPSG();
                if (err == OGRERR_UNSUPPORTED_SRS) {
                    CPLError( CE_Failure, CPLE_OpenFailed,
                        "Could not identify EPSG code for spatial reference system");
                    return NULL;
                }
                pszKoosys = poSRS->GetAuthorityCode("PROJCS");
            }

            if (pszKoosys != NULL) {
                int nKoosys = epsg2sosi(atoi(pszKoosys));
                CPLDebug( "[CreateLayer]","Projection set to SOSI %i", nKoosys);
                LC_PutTrans(nKoosys,0,0,0.01,0.01,0.01);
            } else {
                pszKoosys = poSRS->GetAuthorityCode("GEOGCS");
                if (pszKoosys != NULL) {
                    int nKoosys = epsg2sosi(atoi(pszKoosys));
                   LC_PutTrans(nKoosys,0,0,0.01,0.01,0.01);
                } else {
                    CPLError( CE_Failure, CPLE_OpenFailed,
                        "Could not retrieve EPSG code for spatial reference system");
                    return NULL;
                }
            }
        }
        LC_WsGr(poFileadm); /* Writing the header here! */
    }
    else
    {
        if (!poSRS->IsSame(poSpatialRef)) {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "SOSI driver does not support different spatial reference systems in one file.");
        }
    }

    OGRFeatureDefn *poFeatureDefn = new OGRFeatureDefn( pszNameIn );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eGType );
    OGRSOSILayer *poLayer = new OGRSOSILayer( this, poFeatureDefn, poFileadm, NULL /*poHeaderDefn*/);
    /* todo: where do we delete poLayer and poFeatureDefn? */
    return poLayer;
}
#endif

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSOSIDataSource::GetLayer( int iLayer ) {
    if ( iLayer < 0 || iLayer >= nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}

void OGRSOSIDataSource::buildOGRMultiPoint(int nNumCoo, long iSerial) {
    if (papoBuiltGeometries[iSerial] != nullptr) {
        return;
    }

    OGRMultiPoint *poMP = new OGRMultiPoint();

    long i;
    double dfEast = 0.0;
    double dfNorth = 0.0;
    for (i=(nNumCoo>1)?2:1; i<=nNumCoo; i++) {
        LC_GetTK(i, &dfEast, &dfNorth);
        OGRPoint poP = OGRPoint(dfEast, dfNorth);
        poMP->addGeometry(&poP); /*poP will be cloned before returning*/
    }
    papoBuiltGeometries[iSerial] = poMP;
}

void OGRSOSIDataSource::buildOGRLineString(int nNumCoo, long iSerial) {
    if (papoBuiltGeometries[iSerial] != nullptr) {
        return;
    }

    OGRLineString *poLS = new OGRLineString();
    poLS->setNumPoints(nNumCoo);

    int i;
    double dfEast = 0, dfNorth = 0;
    for (i=1; i<=nNumCoo; i++) {
        LC_GetTK(i, &dfEast, &dfNorth);
        poLS->setPoint(i-1, dfEast, dfNorth);
    }
    papoBuiltGeometries[iSerial] = poLS;
}

static double sqr(double x) { return x * x; }

void OGRSOSIDataSource::buildOGRLineStringFromArc(long iSerial) {
    if (papoBuiltGeometries[iSerial] != nullptr) {
        return;
    }

    OGRLineString *poLS = new OGRLineString();

    /* fetch reference points on circle (easting, northing) */
    double e1 = 0, e2 = 0, e3 = 0;
    double n1 = 0, n2 = 0, n3 = 0;
    LC_GetTK(1, &e1, &n1);
    LC_GetTK(2, &e2, &n2);
    LC_GetTK(3, &e3, &n3);

    /* helper constants */
    double p12  = (e1 * e1 - e2 * e2 + n1 * n1 - n2 * n2) / 2;
    double p13  = (e1 * e1 - e3 * e3 + n1 * n1 - n3 * n3) / 2;

    double dE12 = e1 - e2;
    double dE13 = e1 - e3;
    double dN12 = n1 - n2;
    double dN13 = n1 - n3;

    /* center of the circle */
    double cE = (dN13 * p12 - dN12 * p13) / (dE12 * dN13 - dN12 * dE13);
    double cN = (dE13 * p12 - dE12 * p13) / (dN12 * dE13 - dE12 * dN13);

    /* radius of the circle */
    double r = sqrt(sqr(e1 - cE) + sqr(n1 - cN));

    /* angles of points A and B (1 and 3) */
    double th1 = atan2(n1 - cN, e1 - cE);
    double th3 = atan2(n3 - cN, e3 - cE);

    /* interpolation step in radians */
    double dth = th3 - th1;
    if (dth < 0) {dth  += 2 * M_PI;}
    if (dth > M_PI) {
      dth = - 2*M_PI + dth;
    }
    int    npt = (int)(ARC_INTERPOLATION_FULL_CIRCLE * dth / 2*M_PI);
    if (npt < 0) npt=-npt;
    if (npt < 3) npt=3;
    poLS->setNumPoints(npt);
    dth = dth / (npt-1);

    for (int i=0; i<npt; i++) {
        const double dfEast  = cE + r * cos(th1 + dth * i);
        const double dfNorth = cN + r * sin(th1 + dth * i);
        if (dfEast != dfEast) { /* which is a wonderful property of nans */
          CPLError( CE_Warning, CPLE_AppDefined,
                    "Calculated %lf for point %d of %d in curve %li.", dfEast, i, npt, iSerial);
        }
        poLS->setPoint(i, dfEast, dfNorth);
    }
    papoBuiltGeometries[iSerial] = poLS;
}

void OGRSOSIDataSource::buildOGRPoint(long iSerial) {
    double dfEast = 0, dfNorth = 0;
    LC_GetTK(1, &dfEast, &dfNorth);
    papoBuiltGeometries[iSerial] = new OGRPoint(dfEast, dfNorth);
}

/************************************************************************/
/*                              TestCapability()                        */
/************************************************************************/

int OGRSOSIDataSource::TestCapability( CPL_UNUSED const char * pszCap ) {
#ifdef WRITE_SUPPORT
    if (strcmp("CreateLayer",pszCap) == 0) {
        return TRUE;
    }
#endif
    return FALSE;
}
