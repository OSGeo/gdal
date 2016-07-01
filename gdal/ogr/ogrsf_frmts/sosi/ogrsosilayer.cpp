/******************************************************************************
 * $Id$
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSILayer.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

/************************************************************************/
/*                           OGRSOSILayer()                             */
/************************************************************************/

OGRSOSILayer::OGRSOSILayer( OGRSOSIDataSource *poPar, OGRFeatureDefn *poFeatDefn, LC_FILADM *poFil, std::map<CPLString, unsigned int> *poHeadDefn) {
    poParent      = poPar;
    poFileadm     = poFil;
    poFeatureDefn = poFeatDefn;
    poHeaderDefn  = poHeadDefn;
    nNextFID      = 0;
    poNextSerial  = NULL;

    SetDescription( poFeatureDefn->GetName() );
    if( poFeatureDefn->GetGeomFieldCount() > 0 )
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poParent->poSRS);

    ResetReading();
}

/************************************************************************/
/*                           ~OGRSOSILayer()                            */
/************************************************************************/
OGRSOSILayer::~OGRSOSILayer() {
	poFeatureDefn->Release();
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/
OGRFeatureDefn *OGRSOSILayer::GetLayerDefn() {
    return poFeatureDefn;
}

#ifdef WRITE_SUPPORT
/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/
OGRErr OGRSOSILayer::CreateField (OGRFieldDefn *poField, CPL_UNUSED int bApproxOK) {
    poFeatureDefn->AddFieldDefn( poField );
    return OGRERR_NONE; /* We'll just gladly accept any "field" we find */
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/
OGRErr OGRSOSILayer::ICreateFeature(OGRFeature *poFeature) {
    //short nNavn;
    long nSerial;

    const char *pszSosi = NULL;
    switch (poFeatureDefn->GetGeomType()) {
        case wkbPoint: {
            pszSosi = ".PUNKT";
            break;
        }
        case wkbLineString: {
            pszSosi = ".KURVE";
            break;
        }
        case wkbPolygon: {
            pszSosi = ".FLATE";
            break;
        }
        default: {
            CPLError( CE_Warning, CPLE_AppDefined, "Unknown geometry type in CreateFeature.");
            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }
    }
    /*nNavn = */ LC_NyGr(poFileadm, (char *)pszSosi, &oNextSerial, &nSerial);
    /* === WIP - Work in progress === */
    /* PutGI for all headers */
    char pszGi[255];
    for (int i=0;i<poFeature->GetFieldCount();i++) {
		int n = snprintf (pszGi, 255, "%s", poFeature->GetFieldDefnRef(i)->GetNameRef());
		if (n<255) {
			/*int m = */snprintf (pszGi + (n-1), 255-n, "%s", poFeature->GetFieldAsString(i));
			/* check overflow */
		}
		LC_PutGi(i+2, pszGi); /* should add headers too */
	}
	//LC_OppdaterEndret(0);
    /* PutTK for all coords */
    /* ... */
    /* === /WIP - Work in progress === */
    LC_WsGr(poFileadm); /* Writing the header here! */
    return OGRERR_NONE;
}
#endif

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/
OGRFeature *OGRSOSILayer::GetNextFeature() {
    short nName, nNumLines;
    long  nNumCoo;
    unsigned short nInfo;

    /* iterate through the SOSI groups*/
    while (LC_NextBgr(poNextSerial,LC_FRAMGR)) {
        nName = LC_RxGr(&oNextSerial, LES_OPTIMALT, &nNumLines, &nNumCoo, &nInfo);

        S2S oHeaders;
        S2S::iterator iHeaders;
        /* extract reference strings from group header */
        CPLString osKey, osValue;
        for (short i=1; i<=nNumLines; i++) {
            char *pszLine = LC_GetGi(i);
            if (pszLine[0] == '!') continue;                 /* If we have a comment line, skip it. */
            if ((pszLine[0] == ':')||(pszLine[0] == '(')) {  /* if we have a continued REF line... */
                osValue.append(CPLString(pszLine));            /* append to previous line.           */
                oHeaders.insert(std::pair<CPLString,CPLString>(osKey,osValue));
                continue;
            }
            while (pszLine[0] == '.') pszLine++; /* skipping the dots at the beginning of a SOSI line */
            char *pszUTFLine = CPLRecode(pszLine, poParent->pszEncoding, CPL_ENC_UTF8); /* switch to UTF encoding here */
            char *pszPos = strstr(pszUTFLine, " ");
            if (pszPos != NULL) {
                osKey = CPLString(std::string(pszUTFLine,pszPos));
                osValue = CPLString(pszPos+1);
                oHeaders.insert(std::pair<CPLString,CPLString>(osKey,osValue));
            }
            CPLFree(pszUTFLine);
        }

        /* get Feature from fyba, according to feature definition */
        OGRGeometry *poGeom = NULL;
        OGRwkbGeometryType oGType = wkbUnknown;

        switch (nName) {
        case INGEN_GRUPPE: {  /* No group */
            CPLDebug( "[GetNextFeature]", "Could not load further groups - FYBA reported INGEN_GRUPPE.");
            break;
        }
        case L_FLATE: {  /* Area */
            oGType = wkbPolygon;
            OGRLinearRing *poOuter = new OGRLinearRing();  /* Initialize a new closed polygon */
            long nRefNr;
            unsigned char nRefStatus;
            long nRefCount;
            bool correct = true;
            LC_GRF_STATUS oGrfStat;

            // Iterate through all objects that constitute this area.
            LC_InitGetRefFlate(&oGrfStat);
            nRefCount = LC_GetRefFlate(&oGrfStat, GRF_YTRE, &nRefNr, &nRefStatus, 1);
            while (nRefCount > 0) {
                if (poParent->papoBuiltGeometries[nRefNr] == NULL) {
                    // This should not happen under normal operation.
                    CPLError( CE_Warning, CPLE_AppDefined, "Feature %li referenced by %li, but it was not initialized. Geometry may be broken.", nRefNr, oNextSerial.lNr);
                    correct = false;
                    //return NULL;
                    break;
                }
                OGRGeometry *geom = poParent->papoBuiltGeometries[nRefNr];
                if (geom->getGeometryType() == wkbLineString) {
                  OGRLineString *poCurve = (OGRLineString*)geom;
                  if (nRefStatus == LC_MED_DIG) {         /* clockwise */
                    poOuter->addSubLineString(poCurve);
                  } else if (nRefStatus == LC_MOT_DIG) {  /* counter-clockwise */
                      poOuter->addSubLineString(poCurve,poCurve->getNumPoints()-1,0);
                  } else {
                      CPLError( CE_Failure, CPLE_OpenFailed, "Internal error: GRF_*_OY encountered.");
                      return NULL;
                  }
                } else {
                    CPLError( CE_Warning, CPLE_AppDefined, "Element %li composed of non-linestrings (REF %li of type %i). Ignored.", oNextSerial.lNr, nRefNr, geom->getGeometryType());
                }
                nRefCount = LC_GetRefFlate(&oGrfStat, GRF_YTRE, &nRefNr, &nRefStatus, 1);
            }

            if (correct) {
              OGRPolygon *poLy = new OGRPolygon();
              poOuter->closeRings();
              poLy->addRingDirectly(poOuter);

              OGRLinearRing *poInner = 0;
              nRefCount = LC_GetRefFlate(&oGrfStat, GRF_INDRE, &nRefNr, &nRefStatus, 1);
              while (nRefCount > 0) {
                  if (nRefNr == -1) {
                    if (poInner && (poInner->getNumPoints()>2)) {   /* If this is not the first polygon, terminate and add the last */
                          poInner->closeRings();
                          poLy->addRingDirectly(poInner);
                    }
                    poInner = new OGRLinearRing();  /* Initialize a new closed polygon */
                  } else {
                    if (poParent->papoBuiltGeometries[nRefNr] == NULL) { /* this shouldn't happen under normal operation */
                        CPLError( CE_Fatal, CPLE_AppDefined, "Feature %li referenced by %li, but it was not initialized.", nRefNr, oNextSerial.lNr);
                        return NULL;
                    }
                    OGRGeometry *geom = poParent->papoBuiltGeometries[nRefNr];
                    if (geom->getGeometryType() == wkbLineString) {
                      OGRLineString *poCurve = (OGRLineString*)geom;
                      if (nRefStatus == LC_MED_DIG) {         /* clockwise */
                        poInner->addSubLineString(poCurve);
                      } else if (nRefStatus == LC_MOT_DIG) {  /* counter-clockwise */
                          poInner->addSubLineString(poCurve,poCurve->getNumPoints()-1,0);
                      } else {
                          CPLError( CE_Failure, CPLE_OpenFailed, "Internal error: GRF_*_OY encountered.");
                          return NULL;
                      }
                    } else {
                        CPLError( CE_Warning, CPLE_AppDefined, "Element %li composed of non-linestrings (REF %li of type %i). Ignored.", oNextSerial.lNr, nRefNr, geom->getGeometryType());
                    }
                  }
                  nRefCount = LC_GetRefFlate(&oGrfStat, GRF_INDRE, &nRefNr, &nRefStatus, 1);
              }
              poGeom = poLy;
            }
            break;
        }
        case L_KURVE:    /* curve */
        case L_LINJE:    /* curve, not simplifyable */
        case L_BUEP:  {  /* curve, interpolated from circular arc */
            oGType = wkbLineString;

            OGRLineString *poCurve = (OGRLineString*)(poParent->papoBuiltGeometries[oNextSerial.lNr]);
            if (poCurve == NULL) {
                CPLError( CE_Fatal, CPLE_AppDefined, "Curve %li was not initialized.", oNextSerial.lNr);
                return NULL;
            }
            poGeom = poCurve->clone();
            break;
        }
        case L_TEKST: {  /* text */
            oGType = wkbMultiPoint;

            OGRMultiPoint *poMP = (OGRMultiPoint*)(poParent->papoBuiltGeometries[oNextSerial.lNr]);
            if (poMP == NULL) {
                CPLError( CE_Fatal, CPLE_AppDefined, "Tekst %li was not initialized.", oNextSerial.lNr);
                return NULL;
            }
            poGeom = poMP->clone();
            break;
        }
        case L_SYMBOL: {
            //CPLError( CE_Warning, CPLE_OpenFailed, "Geometry of type SYMBOL treated as point (PUNKT).");
        }
        case L_PUNKT: {  /* point */
            oGType = wkbPoint;
            OGRPoint *poPoint = (OGRPoint*)(poParent->papoBuiltGeometries[oNextSerial.lNr]);
            if (poPoint == NULL) {
                CPLError( CE_Fatal, CPLE_AppDefined, "Point %li was not initialized.", oNextSerial.lNr);
                return NULL;
            }
            poGeom = poPoint->clone();
            break;
        }
        case L_DEF:    /* skip user definitions and headers here */
        case L_HODE: {
            break;
        }
        default: {     /* complain a bit about anything else that is not implemented */
            CPLError( CE_Failure, CPLE_OpenFailed, "Unrecognized geometry of type %i.", nName);
            break;
        }
        }

        if (poGeom == NULL) continue;                         /* skipping L_HODE and unrecognized groups */
        if (oGType != poFeatureDefn->GetGeomType()) {
            if (poGeom != NULL) delete poGeom;
            continue; /* skipping features that are not the correct geometry */
        }

        OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

        /* set all headers found in this group - we export everything, just in case */
        for (iHeaders = oHeaders.begin(); iHeaders != oHeaders.end(); iHeaders++) {
            OGRSOSIDataType *poType = SOSIGetType(iHeaders->first);
            OGRSOSISimpleDataType *poElements = poType->getElements();

            const char *pszLine = iHeaders->second.c_str();
            char** tokens = CSLTokenizeString(iHeaders->second.c_str());

            for (int k=0; k<poType->getElementCount(); k++) {
                if (tokens[k] == 0) break;

                if (strcmp(poElements[k].GetName(),"")==0) continue;
                int iHNr = poHeaderDefn->find(poElements[k].GetName())->second;
                if (iHNr == -1) {
	    			CPLError( CE_Warning, CPLE_AppDefined, "Could not find field definition for %s.", poElements[k].GetName());
                    continue;
    			}
                OGRFieldType nType = poElements[k].GetType();
                switch (nType) {
                  case OFTInteger: {
                    poFeature->SetField( iHNr, SOSITypeToInt(tokens[k]));
                    break;
                  }
                  case OFTDate: {
                    int date[3];
                    SOSITypeToDate(tokens[k], date);
                    poFeature->SetField( iHNr, date[0], date[1], date[2]);
                    break;
                  }
                  case OFTDateTime: {
                    int date[6];
                    SOSITypeToDateTime(tokens[k], date);
                    if (date[0]>0)
                      poFeature->SetField( iHNr, date[0], date[1], date[2], date[3], date[4], static_cast<float>(date[5]), 1);
                    break;
                  }
                  case OFTReal: {
                    poFeature->SetField( iHNr, SOSITypeToReal(tokens[k]));
                    break;
                  }
                  default: {
                    if ((k==0)&&((pszLine[0] == '\'')||(pszLine[0] == '\"'))) { /* If the value is quoted, ignore these */
                        int nLen = strlen(pszLine);
                        char *pszNline = (char*)CPLMalloc(nLen-1);
                        strncpy(pszNline, pszLine+1, nLen-2);
                        pszNline[nLen-2] = '\0';
                        poFeature->SetField( iHNr, pszNline);
                        CPLFree(pszNline);
                    } else {
                        poFeature->SetField( iHNr, tokens[k]);
                    }
                    break;
                  }
                }
          }

          CSLDestroy(tokens);
        }

        if( poGeom != NULL )
            poGeom->assignSpatialReference(poParent->poSRS);

        poFeature->SetGeometryDirectly( poGeom );
        poFeature->SetFID( nNextFID++ );

        /* Loop until we have a feature that matches the definition */
        if ( (m_poFilterGeom == NULL || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == NULL || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;
        delete poFeature;
    }
    return NULL;
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/
void OGRSOSILayer::ResetReading() {
    LC_SBSn(&oSnradm, poFileadm, 0, poFileadm->lAntGr); /* set FYBA Search limits */
    poNextSerial = &oNextSerial;
    LC_InitNextBgr(poNextSerial);
    nNextFID = 0;
}

/************************************************************************/
/*                              TestCapability()                        */
/************************************************************************/

int OGRSOSILayer::TestCapability( const char * pszCap ) {

    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
#if 0
    if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;
    else
#endif
        return FALSE;
}
