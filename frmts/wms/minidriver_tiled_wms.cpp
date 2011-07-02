/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of the OnEarth Tiled WMS minidriver.
 *           http://onearth.jpl.nasa.gov/tiled.html
 * Author:   Lucian Plesea (Lucian dot Pleasea at jpl.nasa.gov)
 *           Adam Nowacki
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
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

#include "stdinc.h"

CPP_GDALWMSMiniDriverFactory(TiledWMS)

/************************************************************************/
/*                           SearchXMLSiblings()                        */
/************************************************************************/

/*
 * \brief Search for a sibling of the root node with a given name.
 *
 * Searches only the next siblings of the node passed in for the named element or attribute.
 * If the first character of the pszElement is '=', the search includes the psRoot node
 * 
 * @param psRoot the root node to search.  This should be a node of type
 * CXT_Element.  NULL is safe.
 *
 * @param pszElement the name of the element or attribute to search for.
 *
 *
 * @return The first matching node or NULL on failure. 
 */

static CPLXMLNode *SearchXMLSiblings( CPLXMLNode *psRoot, const char *pszElement )

{
    if( psRoot == NULL || pszElement == NULL )
        return NULL;

    // If the strings starts with '=', include the current node
    if (pszElement[0]=='=') {
	if (EQUAL(psRoot->pszValue,pszElement+1))
	    return psRoot;
	else return SearchXMLSiblings(psRoot,pszElement+1);
    }

    // Only search the siblings, starting with psRoot->psNext
    for (psRoot=psRoot->psNext;psRoot!=NULL;psRoot=psRoot->psNext) {
	if ( (psRoot->eType == CXT_Element ||
              psRoot->eType == CXT_Attribute)
	     && EQUAL(pszElement,psRoot->pszValue) )
            return psRoot;
    }

    return NULL;
}

/************************************************************************/
/*                        SearchLeafGroupName()                         */
/************************************************************************/

/*
 * \brief Search for a leaf TileGroup node by name.
 *
 * @param psRoot the root node to search.  This should be a node of type
 * CXT_Element.  NULL is safe.
 *
 * @param pszElement the name of the TileGroup to search for.
 *
 * @return The XML node of the matching TileGroup or NULL on failure.
 */

static CPLXMLNode *SearchLeafGroupName( CPLXMLNode *psRoot, const char *name )

{
    CPLXMLNode *ret=NULL;

    if( psRoot == NULL || name == NULL ) return NULL;

    // Has to be a leaf TileGroup with the right name
    if (NULL==CPLSearchXMLNode(psRoot->psChild,"=TiledGroup"))
    {
        if (EQUAL(name,CPLGetXMLValue(psRoot,"Name","")))
        {
            return psRoot;
        }
        else
        { // Try a sibling
            return SearchLeafGroupName(psRoot->psNext,name);
        }
    }
    else
    { // Is metagroup, try children then siblings
        ret=SearchLeafGroupName(psRoot->psChild,name);
        if (NULL!=ret) return ret;
        return SearchLeafGroupName(psRoot->psNext,name);
    }
}

/************************************************************************/
/*                             BandInterp()                             */
/************************************************************************/

/*
 * \brief Utility function to calculate color band interpretation.
 * Only handles Gray, GrayAlpha, RGB and RGBA, based on total band count
 *
 * @param nbands is the total number of bands in the image
 *
 * @param band is the band number, starting with 1
 *
 * @return GDALColorInterp of the band
 */

static GDALColorInterp BandInterp(int nbands, int band) {
    switch (nbands) {
      case 1: return GCI_GrayIndex;
      case 2: return ((band==1)?GCI_GrayIndex:GCI_AlphaBand);
      case 3: // RGB
      case 4: // RBGA
        if (band<3)
            return ((band==1)?GCI_RedBand:GCI_GreenBand);
        return ((band==3)?GCI_BlueBand:GCI_AlphaBand);
      default:
        return GCI_Undefined;
    }
}

/************************************************************************/
/*                              FindBbox()                              */
/************************************************************************/

/*
 * \brief Utility function to find the position of the bbox parameter value 
 * within a request string.  The search for the bbox is case insensitive
 *
 * @param in, the string to search into
 *
 * @return The position from the begining of the string or -1 if not found
 */

static int FindBbox(CPLString in) {

    size_t pos = in.ifind("&bbox=");
    if (pos == std::string::npos)
        return -1;
    else
        return (int)pos + 6;
}

/************************************************************************/
/*                         FindChangePattern()                          */
/************************************************************************/

/*
 * \brief Utility function to pick the right request pattern based on
 * the change request list
 *
 * @param cdata, the list of possible requests, white space separated
 * @param substs, the list of substitutions
 * @param ret The best match request
 */

void FindChangePattern( char *cdata,char **substs, CPLString &ret) {
    char **papszTokens=CSLTokenizeString2(cdata," \t\n\r",
                                          CSLT_STRIPLEADSPACES|CSLT_STRIPENDSPACES);

    int matchcount=CSLCount(substs);
    for (int j=0;j<CSLCount(papszTokens);j++)
    {
        int thiscount=0;
        CPLString this_string=papszTokens[j];
        for (int i=0;i<matchcount;i++) {
            char *key = NULL;
            CPLParseNameValue(substs[i],&key);
            if (key)
            {
                if (std::string::npos!=this_string.find(key,0))
                    thiscount++;
                CPLFree(key);
            }
        }
        if (thiscount==matchcount) {
            ret=papszTokens[j];
            break;
        }
    }

    // if no match is found, return first string
    if (ret.empty()) ret=papszTokens[0];
    CSLDestroy(papszTokens);
}

GDALWMSMiniDriver_TiledWMS::GDALWMSMiniDriver_TiledWMS() {
    m_requests = NULL;
    m_substs = NULL;
}

GDALWMSMiniDriver_TiledWMS::~GDALWMSMiniDriver_TiledWMS() {
    CSLDestroy(m_requests);
    CSLDestroy(m_substs);
}


// Returns the scale of a WMS request as compared to the base resolution
double GDALWMSMiniDriver_TiledWMS::Scale(const char *request) {
    int bbox=FindBbox(request);
    if (bbox<0) return 0;
    double x,y,X,Y;
    sscanf(request+bbox,"%lf,%lf,%lf,%lf",&x,&y,&X,&Y);
    return (m_data_window.m_x1-m_data_window.m_x0)/(X-x)*m_bsx/m_data_window.m_sx;
}


// Finds, extracts, and returns the highest resolution request string from a list, starting at item i
void GDALWMSMiniDriver_TiledWMS::GetLowestScale(char **& list,int i, CPLString &req) {
    req="";
    double scale=-1;
    int position=-1;
    while (NULL!=list[i]) {
	double tscale=Scale(list[i]);
	if (tscale>=scale) {
	    scale=tscale;
	    position=i;
	}
	i++;
    }
    if (position>-1) {
        req=list[position];
        list = CSLRemoveStrings(list,position,1,NULL);
    }
}


CPLErr GDALWMSMiniDriver_TiledWMS::Initialize(CPLXMLNode *config) {
    CPLErr ret = CE_None;
    CPLXMLNode *tileServiceConfig=NULL;
    CPLHTTPResult *psResult=NULL;
    CPLXMLNode *TG=NULL;

    char **requests=NULL;
    char **substs=NULL;

    for (int once=1;once;once--) { // Something to break out of
        // Parse info from the service

        m_end_url = CPLGetXMLValue(config,"AdditionalArgs","");
        m_base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (m_base_url.empty()) {
            CPLError(ret=CE_Failure, CPLE_AppDefined, "GDALWMS, WMS mini-driver: ServerURL missing.");
            break;
        }

        m_tiledGroupName = CPLGetXMLValue(config, "TiledGroupName", "");
        if (m_tiledGroupName.empty()) {
            CPLError(ret=CE_Failure, CPLE_AppDefined, "GDALWMS, Tiled WMS: TiledGroupName missing.");
            break;
        }

        // Change strings, key is an attribute, value is the value of the Change node
        // Multiple substitutions are possible
        TG=CPLSearchXMLNode(config, "Change");
        while(TG!=NULL) {
            CPLString name=CPLGetXMLValue(TG,"key","");
            if (!name.empty()) {
                CPLString value=CPLGetXMLValue(TG,"","");
                substs=CSLSetNameValue(substs,name,value);
            } else {
                CPLError(ret=CE_Failure, CPLE_AppDefined, "GDALWMS, Tiled WMS: Syntax error in configuration file.\n"
                         "Change element needs a non-empty \"key\" attribute");
                break;
            }
            TG=SearchXMLSiblings(TG,"Change");
        }
        if (ret!=CE_None) break;

        CPLString getTileServiceUrl = m_base_url + "request=GetTileService";
        psResult = CPLHTTPFetch(getTileServiceUrl, NULL);

        if (NULL==psResult) {
            CPLError(ret=CE_Failure, CPLE_AppDefined, 
                     "GDALWMS, Tiled WMS: Can't use GDAL HTTP, no curl support.");
            break;
        }

        if ((psResult->nStatus!=0)||(NULL==psResult->pabyData)||('\0'==psResult->pabyData[0])) {
            CPLError(ret=CE_Failure, CPLE_AppDefined,
                     "GDALWMS, Tiled WMS: Can't get server response to GetTileService.");
            break;
        }

        if (NULL==(tileServiceConfig=CPLParseXMLString((const char*)psResult->pabyData))) {
            CPLError(ret=CE_Failure,CPLE_AppDefined, "GDALWMS, Tiled WMS: Error parsing the GetTileService response.");
            break;
        }
    
        m_base_url=CPLGetXMLValue(tileServiceConfig,"TiledPatterns.OnlineResource.xlink:href","");
        if (m_base_url.empty()) {
            CPLError(ret=CE_Failure,CPLE_AppDefined, "GDALWMS, Tiled WMS: Can't locate OnlineResource in the server response.");
            break;
        }
    
        if (NULL==(TG=CPLSearchXMLNode(tileServiceConfig, "TiledPatterns"))) {
            CPLError(ret=CE_Failure,CPLE_AppDefined, 
                     "GDALWMS, Tiled WMS: Can't locate TiledPatterns in server response.");
            break;
        }
    
        if (NULL==(TG=SearchLeafGroupName(TG->psChild,m_tiledGroupName))) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,
                     "GDALWMS, Tiled WMS: Can't locate TiledGroup in server response.");
            break;
        }

        if (0>(m_bands_count=atoi(CPLGetXMLValue(TG, "Bands", "3")))) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,
                     "GDALWMS, Tiled WMS: Invalid number of bands in server response");
            break;
        }
        if (!GDALCheckBandCount(m_bands_count, FALSE))
        {
            ret = CE_Failure;
            break;
        }

        m_parent_dataset->WMSSetBandsCount(m_bands_count);
        m_parent_dataset->WMSSetDataType(GDALGetDataTypeByName(CPLGetXMLValue(TG, "DataType", "Byte")));
        m_projection_wkt=CPLGetXMLValue(TG, "Projection","");

        // Bounding box for the group itself
        CPLXMLNode *latlonbbox = CPLSearchXMLNode(TG, "LatLonBoundingBox");
        if (NULL==latlonbbox) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,
                     "GDALWMS, Tiled WMS: Can't locate the LatLonBoundingBox in server response.");
            break;
        }

        m_data_window.m_x0=atof(CPLGetXMLValue(latlonbbox,"minx","0"));
        m_data_window.m_x1=atof(CPLGetXMLValue(latlonbbox,"maxx","-1"));
        m_data_window.m_y0=atof(CPLGetXMLValue(latlonbbox,"maxy","0"));
        m_data_window.m_y1=atof(CPLGetXMLValue(latlonbbox,"miny","-1"));

        if ((m_data_window.m_x1-m_data_window.m_x0)<0) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,
                     "GDALWMS, Tiled WMS: Coordinate order in boundingbox problem in server response.");
            break;
        }

        m_overview_count=0;
        CPLXMLNode *Pattern=TG->psChild;

        m_bsx=m_bsy=-1;
        m_data_window.m_sx=m_data_window.m_sy=0;

        for (int once=1;once;once--) { // Something to break out of
            while ((NULL!=Pattern)&&(NULL!=(Pattern=SearchXMLSiblings(Pattern,"=TilePattern")))) {
                int mbsx,mbsy;

                CPLString request;
                FindChangePattern(Pattern->psChild->pszValue,substs,request);

                char **papszTokens=CSLTokenizeString2(request,"&",0);

                mbsx=atoi(CSLFetchNameValue(papszTokens,"WIDTH"));
                mbsy=atoi(CSLFetchNameValue(papszTokens,"HEIGHT"));
                if (m_projection_wkt.empty()) {
                    const char* pszSRS = CSLFetchNameValue(papszTokens,"SRS");
                    m_projection_wkt = (pszSRS) ? pszSRS : "";
                    if (!m_projection_wkt.empty())
                        m_projection_wkt=ProjToWKT(m_projection_wkt);
                }

                if (-1==m_bsx) m_bsx=mbsx;
                if (-1==m_bsy) m_bsy=mbsy;
                if ((m_bsy!=mbsy)||(m_bsy!=mbsy)) {
                    CPLError(ret=CE_Failure,CPLE_AppDefined,
                             "GDALWMS, Tiled WMS: Tileset uses different block sizes.");
                    m_overview_count=0;
                    CSLDestroy(papszTokens);
                    break;
                }

                const char* pszBBOX = CSLFetchNameValue(papszTokens,"BBOX");
                if (pszBBOX == NULL)
                {
                    CPLError(ret=CE_Failure,CPLE_AppDefined,
                        "GDALWMS, Tiled WMS: BBOX parameter not found in server response.");
                    CSLDestroy(papszTokens);
                    break;
                }

                double x,y,X,Y;
                if (sscanf(pszBBOX,"%lf,%lf,%lf,%lf",&x,&y,&X,&Y) != 4)
                {
                    CPLError(ret=CE_Failure,CPLE_AppDefined,
                        "GDALWMS, Tiled WMS: Invalid value for BBOX parameter in server response.");
                    CSLDestroy(papszTokens);
                    break;
                }
                int sx=static_cast<int>((m_data_window.m_x1-m_data_window.m_x0)/(X-x)*m_bsx);
                int sy=static_cast<int>(fabs((m_data_window.m_y1-m_data_window.m_y0)/(Y-y)*m_bsy));
                if (sx>m_data_window.m_sx) m_data_window.m_sx=sx;
                if (sy>m_data_window.m_sy) m_data_window.m_sy=sy;
                CSLDestroy(papszTokens);

                // Only use overlays where the top coordinate is within a pixel from the top of coverage
                double pix_off,temp;
                pix_off=m_bsy*modf(fabs((Y-m_data_window.m_y0)/(Y-y)),&temp);
                if ((pix_off<1)||((m_bsy-pix_off)<1)) {
                    requests=CSLAddString(requests,request);
                    m_overview_count++;
                } else
                    CPLError(CE_Warning,CPLE_AppDefined,
                             "GDALWMS, Tiled WMS: Overlay size %dX%d can't be used due to alignment",sx,sy);

                Pattern=Pattern->psNext;

            }

            // The tlevel is needed, the tx and ty are not used by this minidriver
            m_data_window.m_tlevel = 0;
            m_data_window.m_tx = 0;
            m_data_window.m_ty = 0;

            // Make sure the parent_dataset values are set before creating the bands
            m_parent_dataset->WMSSetBlockSize(m_bsx,m_bsy);
            m_parent_dataset->WMSSetRasterSize(m_data_window.m_sx,m_data_window.m_sy);

            m_parent_dataset->WMSSetDataWindow(m_data_window);
            m_parent_dataset->WMSSetOverviewCount(m_overview_count);
            m_parent_dataset->WMSSetClamp(false);

            // Ready for the Rasterband creation
            int i;
            for (i=0;i<m_overview_count;i++) {
                CPLString request="";
                GetLowestScale(requests,i,request);
                double scale=Scale(request);

                if (i == 0)
                {
                    if (fabs(scale-1.0) >1e-6)
                    {
                        CPLError(ret=CE_Failure,CPLE_AppDefined,
                         "GDALWMS, Tiled WMS: Did not get expected scale : %.15f", scale);
                        break;
                    }
                }

                // Prepare the request and insert it back into the list
                int startBbox=FindBbox(request);
                int BboxSize=request.find_first_of("&",startBbox);
                request.replace(startBbox,BboxSize,"${GDAL_BBOX}");
                requests = CSLInsertString(requests,i,request);

                // Create the Rasterband or overview
                for (int j = 1; j <= m_bands_count; j++) {
                    if (i == 0) {
                        GDALWMSRasterBand *band=new GDALWMSRasterBand(m_parent_dataset, j, scale);
                        band->SetColorInterpretation(BandInterp(m_bands_count,j));
                        m_parent_dataset->mSetBand(j, band);
                    } else
                        m_parent_dataset->mGetBand(j)->AddOverview(scale);
                }
            }

            if (i != m_overview_count)
                break;

            if ((m_overview_count==0)||(m_bsx<1)||(m_bsy<1)) {
                CPLError(ret=CE_Failure,CPLE_AppDefined,
                         "GDALWMS, Tiled WMS: No usable TilePattern elements found");
                break;
            }
        }
    }

    m_requests=requests;
    m_substs=substs;

    if (tileServiceConfig) CPLDestroyXMLNode(tileServiceConfig);
    if (psResult) CPLHTTPDestroyResult(psResult);

    return ret;
}

void GDALWMSMiniDriver_TiledWMS::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
    caps->m_capabilities_version = 1;
    caps->m_has_arb_overviews = 0;
    caps->m_has_image_request = 1;
    caps->m_has_tiled_image_requeset = 1;
    caps->m_max_overview_count = 32;
}


// not called
void GDALWMSMiniDriver_TiledWMS::ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri) {
}

void GDALWMSMiniDriver_TiledWMS::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
    *url = m_base_url;
    URLAppend(url,CSLGetField(m_requests,-tiri.m_level));
    URLSearchAndReplace(url,"${GDAL_BBOX}","%013.8f,%013.8f,%013.8f,%013.8f",
                        iri.m_x0,iri.m_y1,iri.m_x1,iri.m_y0);
    if (m_substs!=NULL) {
	for (int i=0;i<CSLCount(m_substs);i++) {
	    char *k;
	    const char *v=CPLParseNameValue(m_substs[i],&k);
	    URLSearchAndReplace(url,k,"%s",v);
	    VSIFree(k);
	}
    }
    URLAppend(url,m_end_url);
}

const char *GDALWMSMiniDriver_TiledWMS::GetProjectionInWKT() {
    return m_projection_wkt.c_str();
}

