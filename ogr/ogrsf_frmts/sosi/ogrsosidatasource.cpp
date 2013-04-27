/******************************************************************************
 * $Id$
 *
 * Project:  SOSI Data Source
 * Purpose:  Provide SOSI Data to OGR.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
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

/* This is the most common encoding for SOSI files. Let's at least try if
 * it is supported, or generate a meaningful error message.               */
#ifndef CPL_ENC_ISO8859_10
#  define CPL_ENC_ISO8859_10 "ISO8859-10"
#endif

/************************************************************************/
/*                              utility methods                         */
/************************************************************************/

int epsg2sosi (int nEPSG) {
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
                      "(Yet) unsupported coodinate system writing to SOSI file: %i. Defaulting to EPSG:4326/SOSI 84.", nEPSG);
            }
    }
    return nSOSI;
}

int sosi2epsg (int nSOSI) {
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
                      "(Yet) unsupported coodinate system in SOSI-file: %i. Defaulting to EPSG:4326.", nSOSI);
        }
    }
    return nEPSG;
}

/************************************************************************/
/*                              OGRSOSIDataSource()                     */
/************************************************************************/

OGRSOSIDataSource::OGRSOSIDataSource() {
    nLayers = 0;
    
    poFileadm = NULL;
    poBaseadm = NULL;
    papoBuiltGeometries = NULL;
    papoLayers = NULL;
    pszName = NULL;
    poSRS = NULL;
    
    poPolyHeaders = NULL;
    poTextHeaders = NULL;
    poPointHeaders = NULL;
    poCurveHeaders = NULL;
    
    pszEncoding = CPL_ENC_UTF8;
    nMode = MODE_READING;
}

/************************************************************************/
/*                              ~OGRSOSIDataSource()                    */
/************************************************************************/

OGRSOSIDataSource::~OGRSOSIDataSource() {
    if (papoBuiltGeometries != NULL) {
        for (unsigned int i=0; i<nNumFeatures; i++) {
            if (papoBuiltGeometries[i] != NULL) {
                delete papoBuiltGeometries[i];
                papoBuiltGeometries[i] = NULL;
            }
        }
        CPLFree(papoBuiltGeometries);
        papoBuiltGeometries = NULL;
    }

    if (poPolyHeaders  != NULL) delete poPolyHeaders;
    if (poTextHeaders  != NULL) delete poTextHeaders; 
    if (poPointHeaders != NULL) delete poPointHeaders;
    if (poCurveHeaders != NULL) delete poCurveHeaders; 

    if (nMode == MODE_WRITING) {
        if (poFileadm != NULL) LC_CloseSos  (poFileadm, RESET_IDX );
        if (poBaseadm != NULL) LC_CloseBase (poBaseadm, RESET_IDX );
    } else {
        if (poFileadm != NULL) LC_CloseSos  (poFileadm, SAVE_IDX );
        if (poBaseadm != NULL) LC_CloseBase (poBaseadm, SAVE_IDX );
    }
    poFileadm = NULL;
    poBaseadm = NULL;

    if (papoLayers != NULL) {
        for ( int i = 0; i < nLayers; i++ ) {
            delete papoLayers[i];
        }
        CPLFree(papoLayers);
    }
    
    if (poSRS != NULL) poSRS->Release();
    if (pszName != NULL) CPLFree(pszName);
}

static
OGRFeatureDefn *defineLayer(const char *szName, OGRwkbGeometryType szType, S2I *poHeaders) {
    OGRFeatureDefn *poFeatureDefn = new OGRFeatureDefn( szName );
    poFeatureDefn->SetGeomType( szType );
    
    for (unsigned int n=0; n<poHeaders->size(); n++) { /* adding headers in the correct order again */
        for (S2I::iterator i=poHeaders->begin(); i!=poHeaders->end(); i++) {
            if (n==i->second) {
                OGRFieldDefn oFieldTemplate( i->first.c_str(), OFTString );
                poFeatureDefn->AddFieldDefn( &oFieldTemplate );
            }
        }
    }
    return poFeatureDefn;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

int  OGRSOSIDataSource::Open( const char *pszFilename, int bUpdate ) {
    papoBuiltGeometries = NULL;
    poFileadm = NULL;
    poBaseadm = NULL;
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
    if (pszPos != NULL) {
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
     * 	    and prebuild LineString features for later assembly.           *
     * --------------------------------------------------------------------*/

    /* allocate room for one pointer per feature */
    nNumFeatures = poFileadm->lAntGr;
    void* mem = VSIMalloc2(nNumFeatures, sizeof(void*));
    if (mem == NULL) {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Memory allocation for SOSI features failed." );
        return FALSE;
    } else {
        papoBuiltGeometries = (OGRGeometry**)mem;
    }
    for (unsigned int i=0; i<nNumFeatures; i++) papoBuiltGeometries[i] = NULL;

    /* Various iterators and return values used to iterate through SOSI features */
    short          nName, nNumLines;
    long           nNumCoo;
    unsigned short nInfo;
    LC_SNR_ADM	   oSnradm;
    LC_BGR		   oNextSerial;
    LC_BGR		  *poNextSerial;
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
            char *pszPos = strstr(pszUTFLineIter, " "); /* Split header and value */
            if (pszPos != NULL) {
                CPLString osKey = CPLString(std::string(pszUTFLineIter,pszPos)); /* FIXME: clean instantiation of CPLString? */
                CPLString osValue = CPLString(pszPos+1);
                
                oHeaders[osKey]=osValue;          /* Add to header map */
                switch (nName) {             /* Add to header list for the corresponding layer, if it is not */
                case L_FLATE: {            /* in there already */
                    if (poPolyHeaders->find(osKey) == poPolyHeaders->end()) {
                        iH = poPolyHeaders->size();
                        (*poPolyHeaders)[osKey] = iH;
                    }
                    break;
                }
                case L_KURVE: {
                    if (poCurveHeaders->find(osKey) == poCurveHeaders->end()) {
                        iH = poCurveHeaders->size();
                        (*poCurveHeaders)[osKey] = iH;
                    }
                    break;
                }
                case L_PUNKT: {
                    if (poPointHeaders->find(osKey) == poPointHeaders->end()) {
                        iH = poPointHeaders->size();
                        (*poPointHeaders)[osKey] = iH;
                    }
                    break;
                }
                case L_TEKST: {
                    if (poTextHeaders->find(osKey) == poTextHeaders->end()) {
                        iH = poTextHeaders->size();
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
        case L_KURVE: {
            /* Pre-build a line feature. Activate line/curve layer. */
            bCurveLayer = TRUE;
            buildOGRLineString(nNumCoo, oNextSerial.lNr);
            break;
        }
        case L_TEKST: {
            /* Pre-build a text line contour feature. Activate text layer. */
            /* Todo: observe only points 2ff if more than one point is given for follow mode */
            bTextLayer = TRUE;
            buildOGRMultiPoint(nNumCoo, oNextSerial.lNr);
            break;
        }
        case L_HODE: {
            /* Get SRS from SOSI header. */
            unsigned short nMask = LC_TR_ALLT;
            LC_TRANSPAR oTrans;
            if (LC_GetTransEx(&nMask,&oTrans) == UT_FALSE) {
                CPLError( CE_Failure, CPLE_OpenFailed, 
                          "TRANSPAR section not found - No reference system information available.");
                return NULL;
            }
            poSRS = new OGRSpatialReference();

            /* Get coordinate system from SOSI header. */
            int nEPSG = sosi2epsg(oTrans.sKoordsys);
            if (poSRS->importFromEPSG(nEPSG) != OGRERR_NONE) {
				CPLError( CE_Failure, CPLE_OpenFailed, 
                          "OGR could not load coordinate system definition EPSG:%i.", nEPSG);
                return NULL;
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
    int nLayers = 0;
    if (bPolyLayer)  nLayers++;
    if (bCurveLayer) nLayers++;
    if (bPointLayer) nLayers++;
    if (bTextLayer) nLayers++;
    this->nLayers = nLayers;
    /* allocate some memory for up to three layers */
    papoLayers = (OGRSOSILayer **) VSIMalloc2(sizeof(void*), nLayers);

    /* Define each layer, using a proper feature definition, geometry type,
     * and adding every SOSI header encountered in the file as field. */
    S2I::iterator i;
    if (bPolyLayer) {
		OGRFeatureDefn *poFeatureDefn = defineLayer("polygons", wkbPolygon, poPolyHeaders);
        poFeatureDefn->Reference();
        papoLayers[--nLayers] = new OGRSOSILayer( this, poFeatureDefn, poFileadm, poPolyHeaders );
    } else {
        delete poPolyHeaders;
        poPolyHeaders = NULL;
    }
    if (bCurveLayer) {
        OGRFeatureDefn *poFeatureDefn = defineLayer("lines", wkbLineString, poCurveHeaders);
        poFeatureDefn->Reference();
        papoLayers[--nLayers] = new OGRSOSILayer( this, poFeatureDefn, poFileadm, poCurveHeaders );
    } else {
        delete poCurveHeaders;
        poCurveHeaders = NULL;
    }
    if (bPointLayer) {
        OGRFeatureDefn *poFeatureDefn = defineLayer("points", wkbPoint, poPointHeaders);
        poFeatureDefn->Reference();
        papoLayers[--nLayers] = new OGRSOSILayer( this, poFeatureDefn, poFileadm, poPointHeaders );
    } else {
        delete poPointHeaders;
        poPointHeaders = NULL;
    }
    if (bTextLayer) {
        OGRFeatureDefn *poFeatureDefn = defineLayer("text", wkbMultiPoint, poTextHeaders);
        poFeatureDefn->Reference();
        papoLayers[--nLayers] = new OGRSOSILayer( this, poFeatureDefn, poFileadm, poTextHeaders );
    } else {
        delete poTextHeaders;
        poTextHeaders = NULL;
    }
    return TRUE;
}


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
/*                              CreateLayer()                           */
/************************************************************************/

OGRLayer *OGRSOSIDataSource::CreateLayer( const char *pszName, OGRSpatialReference  *poSpatialRef, OGRwkbGeometryType eGType, char **papszOptions ) {
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
    
    } else {
        if (!poSRS->IsSame(poSpatialRef)) {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "SOSI driver does not support different spatial reference systems in one file.");
        }
    }
    
    OGRFeatureDefn *poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eGType );
    OGRSOSILayer *poLayer = new OGRSOSILayer( this, poFeatureDefn, poFileadm, NULL /*poHeaderDefn*/);
    /* todo: where do we delete poLayer and poFeatureDefn? */
    return poLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSOSIDataSource::GetLayer( int iLayer ) {
    if ( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

void OGRSOSIDataSource::buildOGRMultiPoint(int nNumCoo, long iSerial) {
    if (papoBuiltGeometries[iSerial] != NULL) {
        return;
    }

    OGRMultiPoint *poMP = new OGRMultiPoint();

    long i;
    double dfEast = 0, dfNorth = 0;
    for (i=(nNumCoo>1)?2:1; i<=nNumCoo; i++) {
        LC_GetTK(i, &dfEast, &dfNorth);
        OGRPoint poP = OGRPoint(dfEast, dfNorth);
        poMP->addGeometry(&poP); /*poP will be cloned before returning*/
    }
    papoBuiltGeometries[iSerial] = poMP;
}

void OGRSOSIDataSource::buildOGRLineString(int nNumCoo, long iSerial) {
    if (papoBuiltGeometries[iSerial] != NULL) {
        return;
    }

    OGRLineString *poLS = new OGRLineString();
    poLS->setNumPoints(nNumCoo);

    long i;
    double dfEast = 0, dfNorth = 0;
    for (i=1; i<=nNumCoo; i++) {
        LC_GetTK(i, &dfEast, &dfNorth);
        poLS->setPoint(i-1, dfEast, dfNorth);
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

int OGRSOSIDataSource::TestCapability( const char * pszCap ) {
    if (strcmp("CreateLayer",pszCap) == 0) {
        return TRUE; 
    } else {
        CPLDebug( "[TestCapability]","Capability %s not supported by SOSI data source", pszCap);
    }
	return FALSE;
}
