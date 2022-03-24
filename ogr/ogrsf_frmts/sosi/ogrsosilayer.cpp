/******************************************************************************
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSILayer.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include <memory>
#include <stdio.h>
#include <string.h>


CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRSOSILayer()                             */
/************************************************************************/

OGRSOSILayer::OGRSOSILayer( OGRSOSIDataSource *poPar, OGRFeatureDefn *poFeatDefn, LC_FILADM *poFil, std::map<CPLString, unsigned int> *poHeadDefn) {
    poParent      = poPar;
    poFileadm     = poFil;
    poFeatureDefn = poFeatDefn;
    poHeaderDefn  = poHeadDefn;
    nNextFID      = 0;
    poNextSerial  = nullptr;

    SetDescription( poFeatureDefn->GetName() );
    if( poFeatureDefn->GetGeomFieldCount() > 0 )
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poParent->poSRS);

    OGRSOSILayer::ResetReading();
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
    // LC_OppdaterEndret(0);
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

    // Used to limit the number of limit the fields to be appended.
    // Default is that duplicates will appended if no appendFieldsMap parameter is sent.
    std::map<std::string,std::string> appendFieldsMap;

    // Get appendFieldsMap and update appendFieldsMap if present;
    // The input must on this format

    // -oo appendFieldsMap="BEITEBRUKERID:,&FIELD2test: &FIELD3test:;"
    // With the example above the field BEITEBRUKERID append character will be ':', for FIELD2test it will space and for FIELD3test it will be ;

    // -oo appendFieldsMap="BEITEBRUKERID&FIELD2test&FIELD3test"
    // With the example above the append character will be ',' for fields in the list

    const char * appendFieldsMapInput = CSLFetchNameValue(poParent->GetOpenOptions(),"appendFieldsMap");
    CPLStringList aosTokens(CSLTokenizeString2(appendFieldsMapInput, "&", 0));
    for( int i = 0; i < aosTokens.size(); i++ )
    {
        std::string filedsAndDelimStr = aosTokens[i];
        std::size_t found = filedsAndDelimStr.find(":");
        std::string appfieldName = filedsAndDelimStr;
        std::string appfieldDelimiter = ",";
        if (found < filedsAndDelimStr.length()) {
            appfieldName = filedsAndDelimStr.substr(0,found);
            appfieldDelimiter = filedsAndDelimStr.substr(found+1);
        }
        appendFieldsMap.insert(std::pair<std::string,std::string>(appfieldName,appfieldDelimiter));
    }


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
            if (pszPos != nullptr) {
                osKey = CPLString(std::string(pszUTFLine,pszPos));
                // Check if this oskey is used before in this feature
                if (oHeaders.count(osKey) > 0 && appendFieldsMap.count(osKey.c_str()) > 0) {
                    // get old osvalue so we can append the next value
                    CPLString newAppendOsValue = oHeaders[osKey];

                    // append split character
                    newAppendOsValue.append(appendFieldsMap[osKey]);

                    // append new value
                    newAppendOsValue.append(CPLString(pszPos+1));

                    // the new value
                    oHeaders[osKey]= newAppendOsValue;

                    // printf ("Append value for %s is %s \n", osKey.c_str(), newAppendOsValue.c_str());
                } else {
                    osValue = CPLString(pszPos+1);
                    oHeaders.insert(std::pair<CPLString,CPLString>(osKey,osValue));
                }
            }
            CPLFree(pszUTFLine);
        }

        /* get Feature from fyba, according to feature definition */
        OGRGeometry *poGeom = nullptr;
        OGRwkbGeometryType oGType = wkbUnknown;

        switch (nName) {
        case INGEN_GRUPPE: {  /* No group */
            CPLDebug( "[GetNextFeature]", "Could not load further groups - FYBA reported INGEN_GRUPPE.");
            break;
        }
        case L_FLATE: {  /* Area */
            oGType = wkbPolygon;
            auto poOuter = cpl::make_unique<OGRLinearRing>();  /* Initialize a new closed polygon */
            long nRefNr;
            unsigned char nRefStatus;
            long nRefCount;
            bool correct = true;
            LC_GRF_STATUS oGrfStat;

            // Iterate through all objects that constitute this area.
            LC_InitGetRefFlate(&oGrfStat);
            nRefCount = LC_GetRefFlate(&oGrfStat, GRF_YTRE, &nRefNr, &nRefStatus, 1);
            while (nRefCount > 0) {
                if (poParent->papoBuiltGeometries[nRefNr] == nullptr) {
                    // This should not happen under normal operation.
                    CPLError( CE_Warning, CPLE_AppDefined, "Feature %li referenced by %li, but it was not initialized. Geometry may be broken.", nRefNr, oNextSerial.lNr);
                    correct = false;
                    //return NULL;
                    break;
                }
                OGRGeometry *geom = poParent->papoBuiltGeometries[nRefNr];
                if (geom->getGeometryType() == wkbLineString) {
                  OGRLineString *poCurve = geom->toLineString();
                  if (nRefStatus == LC_MED_DIG) {         /* clockwise */
                    poOuter->addSubLineString(poCurve);
                  } else if (nRefStatus == LC_MOT_DIG) {  /* counter-clockwise */
                      poOuter->addSubLineString(poCurve,poCurve->getNumPoints()-1,0);
                  } else {
                      CPLError( CE_Failure, CPLE_OpenFailed, "Internal error: GRF_*_OY encountered.");
                      return nullptr;
                  }
                } else {
                    CPLError( CE_Warning, CPLE_AppDefined, "Element %li composed of non-linestrings (REF %li of type %i). Ignored.", oNextSerial.lNr, nRefNr, geom->getGeometryType());
                }
                nRefCount = LC_GetRefFlate(&oGrfStat, GRF_YTRE, &nRefNr, &nRefStatus, 1);
            }

            if (correct) {
              auto poLy = cpl::make_unique<OGRPolygon>();
              poOuter->closeRings();
              poLy->addRingDirectly(poOuter.release());

              std::unique_ptr<OGRLinearRing> poInner;
              nRefCount = LC_GetRefFlate(&oGrfStat, GRF_INDRE, &nRefNr, &nRefStatus, 1);
              while (nRefCount > 0) {
                  if (nRefNr == -1) {
                    if (poInner && poInner->getNumPoints()>2) {   /* If this is not the first polygon, terminate and add the last */
                      poInner->closeRings();
                      poLy->addRingDirectly(poInner.release());
                    }
                    poInner.reset(new OGRLinearRing());  /* Initialize a new closed polygon */
                  } else {
                    if (poParent->papoBuiltGeometries[nRefNr] == nullptr) { /* this shouldn't happen under normal operation */
                        CPLError( CE_Fatal, CPLE_AppDefined, "Feature %li referenced by %li, but it was not initialized.", nRefNr, oNextSerial.lNr);
                        return nullptr;
                    }
                    OGRGeometry *geom = poParent->papoBuiltGeometries[nRefNr];
                    if (geom->getGeometryType() == wkbLineString) {
                      OGRLineString *poCurve = geom->toLineString();
                      if (poInner && nRefStatus == LC_MED_DIG) {         /* clockwise */
                        poInner->addSubLineString(poCurve);
                      } else if (poInner && nRefStatus == LC_MOT_DIG) {  /* counter-clockwise */
                          poInner->addSubLineString(poCurve,poCurve->getNumPoints()-1,0);
                      } else {
                          CPLError( CE_Failure, CPLE_OpenFailed, "Internal error: GRF_*_OY encountered.");
                          return nullptr;
                      }
                    } else {
                        CPLError( CE_Warning, CPLE_AppDefined, "Element %li composed of non-linestrings (REF %li of type %i). Ignored.", oNextSerial.lNr, nRefNr, geom->getGeometryType());
                    }
                  }
                  nRefCount = LC_GetRefFlate(&oGrfStat, GRF_INDRE, &nRefNr, &nRefStatus, 1);
              }
              poGeom = poLy.release();
            }
            break;
        }
        case L_KURVE:    /* curve */
        case L_LINJE:    /* curve, not simplifyable */
        case L_BUEP:  {  /* curve, interpolated from circular arc */
            oGType = wkbLineString;
            if (poParent->papoBuiltGeometries[oNextSerial.lNr] == nullptr ||
                poParent->papoBuiltGeometries[oNextSerial.lNr]->getGeometryType() != wkbLineString ) {
                // This should not happen under normal operation.
                CPLError( CE_Warning, CPLE_AppDefined, "Curve or line %li may have a broken geometry", oNextSerial.lNr);
                //return NULL;
                break;
            }
            OGRLineString *poCurve = poParent->papoBuiltGeometries[oNextSerial.lNr]->toLineString();
            poGeom = poCurve->clone();
            break;
        }
        case L_TEKST: {  /* text */
            oGType = wkbMultiPoint;
            if (poParent->papoBuiltGeometries[oNextSerial.lNr] == nullptr ||
                poParent->papoBuiltGeometries[oNextSerial.lNr]->getGeometryType() != wkbMultiPoint ) {
                // This should not happen under normal operation.
                CPLError( CE_Warning, CPLE_AppDefined, "Text point %li may have a broken geometry", oNextSerial.lNr);
                //return NULL;
                break;
            }
            OGRMultiPoint *poMP = poParent->papoBuiltGeometries[oNextSerial.lNr]->toMultiPoint();
            poGeom = poMP->clone();
            break;
        }
        case L_SYMBOL: {
            //CPLError( CE_Warning, CPLE_OpenFailed, "Geometry of type SYMBOL treated as point (PUNKT).");
            CPL_FALLTHROUGH
        }
        case L_PUNKT: {  /* point */
            oGType = wkbPoint;
            if (poParent->papoBuiltGeometries[oNextSerial.lNr] == nullptr ||
                poParent->papoBuiltGeometries[oNextSerial.lNr]->getGeometryType() != wkbPoint ) {
                // This should not happen under normal operation.
                CPLError( CE_Warning, CPLE_AppDefined, "Point or symbol %li may have a broken geometry", oNextSerial.lNr);
                //return NULL;
                break;
            }
            OGRPoint *poPoint = poParent->papoBuiltGeometries[oNextSerial.lNr]->toPoint();
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

        if (poGeom == nullptr) continue;                         /* skipping L_HODE and unrecognized groups */
        if (oGType != poFeatureDefn->GetGeomType()) {
            delete poGeom;
            continue; /* skipping features that are not the correct geometry */
        }

        OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

        /* set all headers found in this group - we export everything, just in case */
        for (iHeaders = oHeaders.begin(); iHeaders != oHeaders.end(); ++iHeaders) {
            OGRSOSIDataType *poType = SOSIGetType(iHeaders->first);
            OGRSOSISimpleDataType *poElements = poType->getElements();

            const char *pszLine = iHeaders->second.c_str();
            char** tokens = CSLTokenizeString(iHeaders->second.c_str());

            for (int k=0; k<poType->getElementCount(); k++) {
                if (tokens[k] == nullptr) break;

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
                        int nLen = static_cast<int>(strlen(pszLine));
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

        poGeom->assignSpatialReference(poParent->poSRS);

        poFeature->SetGeometryDirectly( poGeom );
        poFeature->SetFID( nNextFID++ );

        /* Loop until we have a feature that matches the definition */
        if ( (m_poFilterGeom == nullptr || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;
        delete poFeature;
    }
    return nullptr;
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
