/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of the OnEarth Tiled WMS minidriver.
 *           http://onearth.jpl.nasa.gov/tiled.html
 * Author:   Lucian Plesea (Lucian dot Plesea at jpl.nasa.gov)
 *           Adam Nowacki
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "wmsdriver.h"
#include "minidriver_tiled_wms.h"

CPP_GDALWMSMiniDriverFactory(TiledWMS)

static char SIG[]="GDAL_WMS TiledWMS: ";

/*
 *\brief Read a number from an xml element
 */

static double getXMLNum(CPLXMLNode *poRoot, const char *pszPath, const char *pszDefault)
{
    return CPLAtof(CPLGetXMLValue(poRoot,pszPath,pszDefault));
}

/*
 *\brief Read a ColorEntry XML node, return a GDALColorEntry structure
 *
 */

static GDALColorEntry GetXMLColorEntry(CPLXMLNode *p)
{
    GDALColorEntry ce;
    ce.c1= static_cast<short>(getXMLNum(p,"c1","0"));
    ce.c2= static_cast<short>(getXMLNum(p,"c2","0"));
    ce.c3= static_cast<short>(getXMLNum(p,"c3","0"));
    ce.c4= static_cast<short>(getXMLNum(p,"c4","255"));
    return ce;
}


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

    // If the strings starts with '=', skip it and test the root
    // If not, start testing with the next sibling
    if (pszElement[0]=='=')
        pszElement++;
    else
        psRoot=psRoot->psNext;

    for (;psRoot!=NULL;psRoot=psRoot->psNext)
    {
        if ((psRoot->eType == CXT_Element ||
             psRoot->eType == CXT_Attribute)
             && EQUAL(pszElement,psRoot->pszValue))
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
    if (NULL==SearchXMLSiblings(psRoot->psChild,"TiledGroup"))
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
 * \brief Build the right request pattern based on the change request list
 * It only gets called on initialization
 * @param cdata, possible request strings, white space separated
 * @param substs, the list of substitutions to be applied
 * @param keys, the list of available substitution keys
 * @param ret The return value, a matching request or an empty string
 */

static void FindChangePattern( char *cdata,char **substs, char **keys, CPLString &ret)
{
    char **papszTokens=CSLTokenizeString2(cdata," \t\n\r",
                                           CSLT_STRIPLEADSPACES|CSLT_STRIPENDSPACES);
    ret.clear();

    int matchcount=CSLCount(substs);
    int keycount=CSLCount(keys);
    if (keycount<matchcount)
    {
        CSLDestroy(papszTokens);
        return;
    }

    // A valid string has only the keys in the substs list and none other
    for (int j=0;j<CSLCount(papszTokens);j++)
    {
        ret=papszTokens[j];  // The target string
        bool matches=true;

        for (int k=0;k<keycount;k++)
        {
            const char *key=keys[k];
            int sub_number=CSLPartialFindString(substs,key);
            if (sub_number!=-1)
            { // It is a listed match
                // But is the match for the key position?
                char *found_key=NULL;
                const char *found_value=CPLParseNameValue(substs[sub_number],&found_key);
                if (found_key!=NULL && EQUAL(found_key,key))
                {  // Should exits in the request
                    if (std::string::npos==ret.find(key))
                    {
                        matches=false;
                        break;
                    }
                    // Execute the substitution on the "ret" string
                    URLSearchAndReplace(&ret,key,"%s",found_value);
                }
                if (found_key!=NULL) CPLFree(found_key);
            }
            else
            {  // Key not in the subst list, should not match
                if (std::string::npos!=ret.find(key))
                {
                    matches=false;
                    break;
                }
            }
        } // Key loop
        if (matches)
        {
            CSLDestroy(papszTokens);
            return;  // We got the string ready, all keys accounted for and substs applied
        }
    }
    ret.clear();
    CSLDestroy(papszTokens);
}

GDALWMSMiniDriver_TiledWMS::GDALWMSMiniDriver_TiledWMS() {
    m_requests = NULL;
}

GDALWMSMiniDriver_TiledWMS::~GDALWMSMiniDriver_TiledWMS() {
    CSLDestroy(m_requests);
}


// Returns the scale of a WMS request as compared to the base resolution
double GDALWMSMiniDriver_TiledWMS::Scale(const char *request) {
    int bbox=FindBbox(request);
    if (bbox<0) return 0;
    double x,y,X,Y;
    CPLsscanf(request+bbox,"%lf,%lf,%lf,%lf",&x,&y,&X,&Y);
    return (m_data_window.m_x1-m_data_window.m_x0)/(X-x)*m_bsx/m_data_window.m_sx;
}


// Finds, extracts, and returns the highest resolution request string from a list, starting at item i
CPLString GDALWMSMiniDriver_TiledWMS::GetLowestScale(char **& list,int i)
{
    CPLString req;
    double scale=-1;
    int position=-1;
    while (NULL!=list[i])
    {
        double tscale=Scale(list[i]);
        if (tscale>=scale)
        {
            scale=tscale;
            position=i;
        }
        i++;
    }
    if (position>-1)
    {
        req=list[position];
        list = CSLRemoveStrings(list,position,1,NULL);
    }
    return req;
}

/*
 *\Brief Initialize minidriver with info from the server
 */

CPLErr GDALWMSMiniDriver_TiledWMS::Initialize(CPLXMLNode *config)
{
    CPLErr ret = CE_None;
    CPLXMLNode *tileServiceConfig=NULL;
    CPLHTTPResult *psResult=NULL;
    CPLXMLNode *TG=NULL;

    char **requests=NULL;
    char **substs=NULL;
    char **keys=NULL;

    for (int once=1;once;once--) { // Something to break out of
        // Parse info from the service

        m_end_url = CPLGetXMLValue(config,"AdditionalArgs","");
        m_base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (m_base_url.empty()) {
            CPLError(ret=CE_Failure, CPLE_AppDefined, "%s ServerURL missing.",SIG);
            break;
        }

        CPLString tiledGroupName (CPLGetXMLValue(config, "TiledGroupName", ""));
        if (tiledGroupName.empty()) {
            CPLError(ret=CE_Failure, CPLE_AppDefined, "%s TiledGroupName missing.",SIG);
            break;
        }

        // Change strings, key is an attribute, value is the value of the Change node
        // Multiple substitutions are possible
        TG=CPLSearchXMLNode(config, "Change");
        while(TG!=NULL) {
            CPLString name=CPLGetXMLValue(TG,"key","");
            if (name.empty()) {
                CPLError(ret=CE_Failure, CPLE_AppDefined,
                    "%s Change element needs a non-empty \"key\" attribute",SIG);
                break;
            }
            substs=CSLSetNameValue(substs,name,CPLGetXMLValue(TG,"",""));
            TG=SearchXMLSiblings(TG,"Change");
        }
        if (ret!=CE_None) break;

        CPLString getTileServiceUrl = m_base_url + "request=GetTileService";
        psResult = CPLHTTPFetch(getTileServiceUrl, NULL);

        if (NULL==psResult) {
            CPLError(ret=CE_Failure, CPLE_AppDefined, "%s Can't use HTTP", SIG);
            break;
        }

        if ((psResult->nStatus!=0)||(NULL==psResult->pabyData)||('\0'==psResult->pabyData[0])) {
            CPLError(ret=CE_Failure, CPLE_AppDefined, "%s Server response error on GetTileService.",SIG);
            break;
        }

        if (NULL==(tileServiceConfig=CPLParseXMLString((const char*)psResult->pabyData))) {
            CPLError(ret=CE_Failure,CPLE_AppDefined, "%s Error parsing the GetTileService response.",SIG);
            break;
        }

        if (NULL==(TG=CPLSearchXMLNode(tileServiceConfig, "TiledPatterns"))) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,
                "%s Can't locate TiledPatterns in server response.",SIG);
            break;
        }

        // Get the global base_url and bounding box, these can be overwritten at the tileGroup level
        // They are just pointers into existing structures, cleanup is not required
        const char *global_base_url=CPLGetXMLValue(tileServiceConfig,"TiledPatterns.OnlineResource.xlink:href","");
        CPLXMLNode *global_latlonbbox=CPLGetXMLNode(tileServiceConfig, "TiledPatterns.LatLonBoundingBox");
        CPLXMLNode *global_bbox=CPLGetXMLNode(tileServiceConfig, "TiledPatterns.BoundingBox");

        if (NULL==(TG=SearchLeafGroupName(TG->psChild,tiledGroupName))) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,
                "%s Can't locate TiledGroup ""%s"" in server response.",SIG,
                tiledGroupName.c_str());
            break;
        }

        int band_count=atoi(CPLGetXMLValue(TG, "Bands", "3"));

        if (!GDALCheckBandCount(band_count, FALSE)) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,"%s%s",SIG,
                "Invalid number of bands in server response");
            break;
        }

        // Collect all keys defined by this tileset
        if (NULL!=CPLGetXMLNode(TG,"Key")) {
            CPLXMLNode *node=CPLGetXMLNode(TG,"Key");
                while (NULL!=node) {
                    const char *val=CPLGetXMLValue(node,NULL,NULL);
                    if (val) keys=CSLAddString(keys,val);
                    node=SearchXMLSiblings(node,"Key");
                }
        }

       // Data values are attributes, they include NoData Min and Max
       if (0!=CPLGetXMLNode(TG,"DataValues")) {
           const char *nodata=CPLGetXMLValue(TG,"DataValues.NoData",NULL);
           if (nodata!=NULL) m_parent_dataset->WMSSetNoDataValue(nodata);
           const char *min=CPLGetXMLValue(TG,"DataValues.min",NULL);
           if (min!=NULL) m_parent_dataset->WMSSetMinValue(min);
           const char *max=CPLGetXMLValue(TG,"DataValues.max",NULL);
           if (max!=NULL) m_parent_dataset->WMSSetMaxValue(max);
       }

        m_parent_dataset->WMSSetBandsCount(band_count);
        m_parent_dataset->WMSSetDataType(GDALGetDataTypeByName(CPLGetXMLValue(TG, "DataType", "Byte")));
        m_projection_wkt=CPLGetXMLValue(TG, "Projection","");

        m_base_url=CPLGetXMLValue(TG,"OnlineResource.xlink:href",global_base_url);
        if (m_base_url[0]=='\0') {
            CPLError(ret=CE_Failure,CPLE_AppDefined, "%s%s",SIG,
                "Can't locate OnlineResource in the server response.");
            break;
        }

        // Bounding box, local, global, local lat-lon, global lat-lon, in this order
        CPLXMLNode *bbox = CPLGetXMLNode(TG, "BoundingBox");
        if (NULL==bbox) bbox=global_bbox;
        if (NULL==bbox) bbox=CPLGetXMLNode(TG, "LatLonBoundingBox");
        if (NULL==bbox) bbox=global_latlonbbox;

        if (NULL==bbox) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,"%s%s",SIG,
                "Can't locate the LatLonBoundingBox in server response.");
            break;
        }

        m_data_window.m_x0=CPLAtof(CPLGetXMLValue(bbox,"minx","0"));
        m_data_window.m_x1=CPLAtof(CPLGetXMLValue(bbox,"maxx","-1"));
        m_data_window.m_y0=CPLAtof(CPLGetXMLValue(bbox,"maxy","0"));
        m_data_window.m_y1=CPLAtof(CPLGetXMLValue(bbox,"miny","-1"));

        if ((m_data_window.m_x1-m_data_window.m_x0)<0) {
            CPLError(ret=CE_Failure,CPLE_AppDefined,"%s%s", SIG,
                "Coordinate order in BBox, problem in server response");
            break;
        }

        // Is there a palette?
        //
        // Format is
        // <Palette>
        //   <Size>N</Size> : Optional
        //   <Model>RGBA|RGB|CMYK|HSV|HLS|L</Model> :mandatory
        //   <Entry idx=i c1=v1 c2=v2 c3=v3 c4=v4/> :Optional
        //   <Entry .../>
        // </Palette>
        // the idx attribute is optional, it autoincrements
        // The entries are actually vertices, interpolation takes place inside
        // The palette starts initialized with zeros
        // HSV and HLS are the similar, with c2 and c3 swapped
        // RGB or RGBA are same
        //

        GDALColorTable *poColorTable=NULL;

        if ((band_count==1) && CPLGetXMLNode(TG,"Palette")) {

            CPLXMLNode *node=CPLGetXMLNode(TG,"Palette");

            int entries=static_cast<int>(getXMLNum(node,"Size","255"));
            GDALPaletteInterp eInterp=GPI_RGB;

            CPLString pModel=CPLGetXMLValue(node,"Model","RGB");
            if (!pModel.empty() && pModel.find("RGB")!=std::string::npos)
                eInterp=GPI_RGB;
            else {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "%s Palette Model %s is unknown, use RGB or RGBA",
                    SIG, pModel.c_str());
                return CE_Failure;
            }

            if ((entries>0)&&(entries<257)) {
                int start_idx, end_idx;
                GDALColorEntry ce_start={0,0,0,255},ce_end={0,0,0,255};

                // Create it and initialize it to nothing
                poColorTable = new GDALColorTable(eInterp);
                poColorTable->CreateColorRamp(0,&ce_start,entries-1,&ce_end);
                // Read the values
                CPLXMLNode *p=CPLGetXMLNode(node,"Entry");
                if (p) {
                    // Initialize the first entry
                    start_idx=static_cast<int>(getXMLNum(p,"idx","0"));
                    ce_start=GetXMLColorEntry(p);
                    if (start_idx<0) {
                        CPLError(CE_Failure, CPLE_AppDefined,
                            "%s Palette index %d not allowed",SIG,start_idx);
                        delete poColorTable;
                        return CE_Failure;
                    }
                    poColorTable->SetColorEntry(start_idx,&ce_start);
                    while (NULL!=(p=SearchXMLSiblings(p,"Entry"))) {
                        // For every entry, create a ramp
                        ce_end=GetXMLColorEntry(p);
                        end_idx=static_cast<int>(getXMLNum(p,"idx",CPLString().FormatC(start_idx+1).c_str()));
                        if ((end_idx<=start_idx)||(start_idx>=entries)) {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                "%s Index Error at index %d",SIG,end_idx);
                            delete poColorTable;
                            return CE_Failure;
                        }
                        poColorTable->CreateColorRamp(start_idx,&ce_start,
                            end_idx,&ce_end);
                        ce_start=ce_end;
                        start_idx=end_idx;
                    }
                }
                m_parent_dataset->SetColorTable(poColorTable);
            } else {
                CPLError(CE_Failure, CPLE_AppDefined,"%s Palette definition error",SIG);
                return CE_Failure;
            }
        }

        int overview_count=0;
        CPLXMLNode *Pattern=TG->psChild;

        m_bsx=m_bsy=-1;
        m_data_window.m_sx=m_data_window.m_sy=0;

        for (int once=1;once;once--) { // Something to break out of
            while ((NULL!=Pattern)&&(NULL!=(Pattern=SearchXMLSiblings(Pattern,"=TilePattern")))) {
                int mbsx,mbsy;

                CPLString request;
                FindChangePattern(Pattern->psChild->pszValue,substs,keys,request);

                char **papszTokens=CSLTokenizeString2(request,"&",0);

                const char* pszWIDTH = CSLFetchNameValue(papszTokens,"WIDTH");
                const char* pszHEIGHT = CSLFetchNameValue(papszTokens,"HEIGHT");
                if (pszWIDTH == NULL || pszHEIGHT == NULL)
                {
                    CPLError(ret=CE_Failure,CPLE_AppDefined,"%s%s",SIG,
                        "Cannot find width and/or height parameters.");
                    overview_count=0;
                    CSLDestroy(papszTokens);
                    break;
                }

                mbsx=atoi(pszWIDTH);
                mbsy=atoi(pszHEIGHT);
                if (m_projection_wkt.empty()) {
                    m_projection_wkt = CSLFetchNameValueDef(papszTokens,"SRS", "");
                    if (!m_projection_wkt.empty())
                        m_projection_wkt=ProjToWKT(m_projection_wkt);
                }

                if (-1==m_bsx) m_bsx=mbsx;
                if (-1==m_bsy) m_bsy=mbsy;
                if ((m_bsy!=mbsy)||(m_bsy!=mbsy)) {
                    CPLError(ret=CE_Failure,CPLE_AppDefined,"%s%s",SIG,
                        "Tileset uses different block sizes.");
                    overview_count=0;
                    CSLDestroy(papszTokens);
                    break;
                }

                double x,y,X,Y;
                if (CPLsscanf(CSLFetchNameValueDef(papszTokens,"BBOX", ""),"%lf,%lf,%lf,%lf",&x,&y,&X,&Y)!=4)
                {
                    CPLError(ret=CE_Failure,CPLE_AppDefined,
                        "%s Error parsing BBOX, pattern %d\n",SIG,overview_count+1);
                    CSLDestroy(papszTokens);
                    break;
                }
                // Pick the largest size
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
                    overview_count++;
                } else
                    CPLError(CE_Warning,CPLE_AppDefined,
                    "%s Overlay size %dX%d can't be used due to alignment",SIG,sx,sy);

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
            //m_parent_dataset->WMSSetOverviewCount(overview_count);
            m_parent_dataset->WMSSetClamp(false);

            // Ready for the Rasterband creation
            for (int i=0;i<overview_count;i++) {
                CPLString request=GetLowestScale(requests,i);
                double scale=Scale(request);

                // Base scale should be very close to 1
                if ((0==i)&&(fabs(scale-1) > 1e-6)) {
                    CPLError(ret=CE_Failure,CPLE_AppDefined,"%s%s",SIG,
                        "Base resolution pattern missing.");
                    break;
                }

                // Prepare the request and insert it back into the list
                // Find returns an answer relative to the original string start!
                size_t startBbox=FindBbox(request);
                size_t endBbox=request.find('&',startBbox);
                if (endBbox==std::string::npos) endBbox=request.size();
                request.replace(startBbox,endBbox-startBbox,"${GDAL_BBOX}");
                requests = CSLInsertString(requests,i,request);

                // Create the Rasterband or overview
                for (int j = 1; j <= band_count; j++) {
                    if (i!=0)
                        m_parent_dataset->mGetBand(j)->AddOverview(scale);
                    else { // Base resolution
                        GDALWMSRasterBand *band=new
                            GDALWMSRasterBand(m_parent_dataset,j,1);
                        if (poColorTable!=NULL) band->SetColorInterpretation(GCI_PaletteIndex);
                        else band->SetColorInterpretation(BandInterp(band_count,j));
                        m_parent_dataset->mSetBand(j, band);
                    };
                }
            }
            if ((overview_count==0)||(m_bsx<1)||(m_bsy<1)) {
                CPLError(ret=CE_Failure,CPLE_AppDefined,
                    "%s No usable TilePattern elements found",SIG);
                break;
            }
        }
    }

    CSLDestroy(keys);
    CSLDestroy(substs);
    if (tileServiceConfig) CPLDestroyXMLNode(tileServiceConfig);
    if (psResult) CPLHTTPDestroyResult(psResult);

    m_requests=requests;
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
void GDALWMSMiniDriver_TiledWMS::ImageRequest(CPL_UNUSED CPLString *url,
                                              CPL_UNUSED const GDALWMSImageRequestInfo &iri) {
}

void GDALWMSMiniDriver_TiledWMS::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
    *url = m_base_url;
    URLAppend(url,CSLGetField(m_requests,-tiri.m_level));
    URLSearchAndReplace(url,"${GDAL_BBOX}","%013.8f,%013.8f,%013.8f,%013.8f",
                        iri.m_x0,iri.m_y1,iri.m_x1,iri.m_y0);
    URLAppend(url,m_end_url);
}

const char *GDALWMSMiniDriver_TiledWMS::GetProjectionInWKT() {
    return m_projection_wkt.c_str();
}
