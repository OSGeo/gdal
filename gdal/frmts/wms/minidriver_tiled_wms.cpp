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
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

CPL_CVSID("$Id$")

static const char SIG[] = "GDAL_WMS TiledWMS: ";

/*
 *\brief Read a number from an xml element
 */

static double getXMLNum(CPLXMLNode *poRoot, const char *pszPath, const char *pszDefault)
{
    return CPLAtof(CPLGetXMLValue(poRoot, pszPath, pszDefault));
}

/*
 *\brief Read a ColorEntry XML node, return a GDALColorEntry structure
 *
 */

static GDALColorEntry GetXMLColorEntry(CPLXMLNode *p)
{
    GDALColorEntry ce;
    ce.c1 = static_cast<short>(getXMLNum(p, "c1", "0"));
    ce.c2 = static_cast<short>(getXMLNum(p, "c2", "0"));
    ce.c3 = static_cast<short>(getXMLNum(p, "c3", "0"));
    ce.c4 = static_cast<short>(getXMLNum(p, "c4", "255"));
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

static CPLXMLNode *SearchXMLSiblings(CPLXMLNode *psRoot, const char *pszElement)

{
    if (psRoot == nullptr || pszElement == nullptr)
        return nullptr;

    // If the strings starts with '=', skip it and test the root
    // If not, start testing with the next sibling
    if (pszElement[0] == '=')
        pszElement++;
    else
        psRoot = psRoot->psNext;

    for (; psRoot != nullptr; psRoot = psRoot->psNext)
    {
        if ((psRoot->eType == CXT_Element ||
            psRoot->eType == CXT_Attribute)
            && EQUAL(pszElement, psRoot->pszValue))
            return psRoot;
    }
    return nullptr;
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

static CPLXMLNode *SearchLeafGroupName(CPLXMLNode *psRoot, const char *name)

{
    CPLXMLNode *ret = nullptr;

    if (psRoot == nullptr || name == nullptr) return nullptr;

    // Has to be a leaf TileGroup with the right name
    if (nullptr == SearchXMLSiblings(psRoot->psChild, "TiledGroup"))
    {
        if (EQUAL(name, CPLGetXMLValue(psRoot, "Name", "")))
        {
            return psRoot;
        }
        else
        { // Try a sibling
            return SearchLeafGroupName(psRoot->psNext, name);
        }
    }
    else
    { // Is metagroup, try children then siblings
        ret = SearchLeafGroupName(psRoot->psChild, name);
        if (nullptr != ret) return ret;
        return SearchLeafGroupName(psRoot->psNext, name);
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
    case 2: return band == 1 ? GCI_GrayIndex : GCI_AlphaBand;
    case 3: // RGB
    case 4: // RBGA
        if (band < 3)
            return band == 1 ? GCI_RedBand : GCI_GreenBand;
        return band == 3 ? GCI_BlueBand : GCI_AlphaBand;
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
 * @return The position from the beginning of the string or -1 if not found
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

static void FindChangePattern(char *cdata, char **substs, char **keys, CPLString &ret)
{
    char **papszTokens = CSLTokenizeString2(cdata, " \t\n\r",
        CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
    ret.clear();

    int matchcount = CSLCount(substs);
    int keycount = CSLCount(keys);
    if (keycount < matchcount)
    {
        CSLDestroy(papszTokens);
        return;
    }

    // A valid string has only the keys in the substs list and none other
    for (int j = 0; j < CSLCount(papszTokens); j++)
    {
        ret = papszTokens[j];  // The target string
        bool matches = true;

        for (int k = 0; k < keycount && keys != nullptr; k++)
        {
            const char *key = keys[k];
            int sub_number = CSLPartialFindString(substs, key);
            if (sub_number != -1)
            { // It is a listed match
                // But is the match for the key position?
                char *found_key = nullptr;
                const char *found_value = CPLParseNameValue(substs[sub_number], &found_key);
                if (found_key != nullptr && EQUAL(found_key, key))
                {  // Should exits in the request
                    if (std::string::npos == ret.find(key))
                    {
                        matches = false;
                        CPLFree(found_key);
                        break;
                    }
                    // Execute the substitution on the "ret" string
                    URLSearchAndReplace(&ret, key, "%s", found_value);
                }
                if (found_key != nullptr) CPLFree(found_key);
            }
            else
            {  // Key not in the subst list, should not match
                if (std::string::npos != ret.find(key))
                {
                    matches = false;
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

WMSMiniDriver_TiledWMS::WMSMiniDriver_TiledWMS() :
m_requests(nullptr),
m_bsx(0),
m_bsy(0)
{}

WMSMiniDriver_TiledWMS::~WMSMiniDriver_TiledWMS() {
    CSLDestroy(m_requests);
}

// Returns the scale of a WMS request as compared to the base resolution
double WMSMiniDriver_TiledWMS::Scale(const char *request) const {
    int bbox = FindBbox(request);
    if (bbox < 0) return 0;
    double x, y, X, Y;
    CPLsscanf(request + bbox, "%lf,%lf,%lf,%lf", &x, &y, &X, &Y);
    return (m_data_window.m_x1 - m_data_window.m_x0) / (X - x)*m_bsx / m_data_window.m_sx;
}

// Finds, extracts, and returns the highest resolution request string from a list, starting at item i
CPLString WMSMiniDriver_TiledWMS::GetLowestScale(char **& list, int i) const
{
    CPLString req;
    double scale = -1;
    int position = -1;
    while (nullptr != list[i])
    {
        double tscale = Scale(list[i]);
        if (tscale >= scale)
        {
            scale = tscale;
            position = i;
        }
        i++;
    }
    if (position > -1)
    {
        req = list[position];
        list = CSLRemoveStrings(list, position, 1, nullptr);
    }
    return req;
}

/*
 *\Brief Initialize minidriver with info from the server
 */

CPLErr WMSMiniDriver_TiledWMS::Initialize(CPLXMLNode *config, CPL_UNUSED char **OpenOptions)
{
    CPLErr ret = CE_None;
    CPLXMLNode *tileServiceConfig = nullptr;
    CPLXMLNode *TG = nullptr;

    char **requests = nullptr;
    char **substs = nullptr;
    char **keys = nullptr;

    try { // Parse info from the WMS Service node
//        m_end_url = CPLGetXMLValue(config, "AdditionalArgs", "");
        m_base_url = CPLGetXMLValue(config, "ServerURL", "");

        if (m_base_url.empty())
            throw CPLOPrintf("%s ServerURL missing.", SIG);

        CPLString tiledGroupName(CPLGetXMLValue(config, "TiledGroupName", ""));
        if (tiledGroupName.empty())
            throw CPLOPrintf("%s TiledGroupName missing.", SIG);

        // Change strings, key is an attribute, value is the value of the Change node
        // Multiple substitutions are possible
        TG = CPLSearchXMLNode(config, "Change");
        while (TG != nullptr) {
            CPLString name = CPLGetXMLValue(TG, "key", "");
            if (name.empty())
                throw CPLOPrintf("%s Change element needs a non-empty \"key\" attribute", SIG);
            substs = CSLSetNameValue(substs, name, CPLGetXMLValue(TG, "", ""));
            TG = SearchXMLSiblings(TG, "Change");
        }

        // Part of WMS config
        const char *psconfig = CPLGetXMLValue(config, "Configuration", "");

        CPLString buffer;

        if (strlen(psconfig) != 0) { // Probably XML encoded because it is XML itself
            buffer = psconfig; // The copy will be replaced by the decoded result
            psconfig = WMSUtilDecode(buffer, CPLGetXMLValue(config, "Configuration.encoding", ""));
        }
        else { // Not inline, use the WMSdriver to fetch the server config
            CPLString getTileServiceUrl = m_base_url + "request=GetTileService";

            // This returns a string managed by the cfg cache
            psconfig = GDALWMSDataset::GetServerConfig(getTileServiceUrl,
                const_cast<char **>(m_parent_dataset->GetHTTPRequestOpts()));

            if (psconfig == nullptr)
                throw CPLOPrintf("%s HTTP failure", SIG);
        }

        // psconfig contains the GetTileService return now
        if (nullptr == (tileServiceConfig = CPLParseXMLString(psconfig)))
            throw CPLOPrintf("%s Error parsing the GetTileService response", SIG);

        if (nullptr == (TG = CPLSearchXMLNode(tileServiceConfig, "TiledPatterns")))
            throw CPLOPrintf("%s Can't locate TiledPatterns in server response.", SIG);

        // Get the global base_url and bounding box, these can be overwritten at the tileGroup level
        // They are just pointers into existing structures, cleanup is not required
        const char *global_base_url = CPLGetXMLValue(tileServiceConfig, "TiledPatterns.OnlineResource.xlink:href", "");
        CPLXMLNode *global_latlonbbox = CPLGetXMLNode(tileServiceConfig, "TiledPatterns.LatLonBoundingBox");
        CPLXMLNode *global_bbox = CPLGetXMLNode(tileServiceConfig, "TiledPatterns.BoundingBox");

        if (nullptr == (TG = SearchLeafGroupName(TG->psChild, tiledGroupName)))
            throw CPLOPrintf("%s No TiledGroup ""%s"" in server response.", SIG, tiledGroupName.c_str());

        int band_count = atoi(CPLGetXMLValue(TG, "Bands", "3"));

        if (!GDALCheckBandCount(band_count, FALSE))
            throw CPLOPrintf("%s Invalid number of bands in server response", SIG);

        // Collect all keys defined by this tileset
        if (nullptr != CPLGetXMLNode(TG, "Key")) {
            CPLXMLNode *node = CPLGetXMLNode(TG, "Key");
            while (nullptr != node) {
                const char *val = CPLGetXMLValue(node, nullptr, nullptr);
                if (val) keys = CSLAddString(keys, val);
                node = SearchXMLSiblings(node, "Key");
            }
        }

        // Data values are attributes, they include NoData Min and Max
        if (nullptr != CPLGetXMLNode(TG, "DataValues")) {
            const char *nodata = CPLGetXMLValue(TG, "DataValues.NoData", nullptr);
            if (nodata != nullptr) {
                m_parent_dataset->WMSSetNoDataValue(nodata);
                m_parent_dataset->SetTileOO("@NDV", nodata);
            }
            const char *min = CPLGetXMLValue(TG, "DataValues.min", nullptr);
            if (min != nullptr) m_parent_dataset->WMSSetMinValue(min);
            const char *max = CPLGetXMLValue(TG, "DataValues.max", nullptr);
            if (max != nullptr) m_parent_dataset->WMSSetMaxValue(max);
        }

        m_parent_dataset->WMSSetBandsCount(band_count);
        GDALDataType dt = GDALGetDataTypeByName(CPLGetXMLValue(TG, "DataType", "Byte"));
        m_parent_dataset->WMSSetDataType(dt);
        if (dt != GDT_Byte)
            m_parent_dataset->SetTileOO("@DATATYPE", GDALGetDataTypeName(dt));
        m_projection_wkt = CPLGetXMLValue(TG, "Projection", "");

        m_base_url = CPLGetXMLValue(TG, "OnlineResource.xlink:href", global_base_url);
        if (m_base_url[0] == '\0')
            throw CPLOPrintf("%s Can't locate OnlineResource in the server response", SIG);

        // Bounding box, local, global, local lat-lon, global lat-lon, in this order
        CPLXMLNode *bbox = CPLGetXMLNode(TG, "BoundingBox");
        if (nullptr == bbox) bbox = global_bbox;
        if (nullptr == bbox) bbox = CPLGetXMLNode(TG, "LatLonBoundingBox");
        if (nullptr == bbox) bbox = global_latlonbbox;

        if (nullptr == bbox)
            throw CPLOPrintf("%s Can't locate the LatLonBoundingBox in server response", SIG);

        m_data_window.m_x0 = CPLAtof(CPLGetXMLValue(bbox, "minx", "0"));
        m_data_window.m_x1 = CPLAtof(CPLGetXMLValue(bbox, "maxx", "-1"));
        m_data_window.m_y0 = CPLAtof(CPLGetXMLValue(bbox, "maxy", "0"));
        m_data_window.m_y1 = CPLAtof(CPLGetXMLValue(bbox, "miny", "-1"));

        if ((m_data_window.m_x1 - m_data_window.m_x0) < 0)
            throw CPLOPrintf("%s Coordinate order in BBox problem in server response", SIG);

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

        GDALColorTable *poColorTable = nullptr;

        if ((band_count == 1) && CPLGetXMLNode(TG, "Palette")) {
            CPLXMLNode *node = CPLGetXMLNode(TG, "Palette");

            int entries = static_cast<int>(getXMLNum(node, "Size", "255"));
            GDALPaletteInterp eInterp = GPI_RGB;

            CPLString pModel = CPLGetXMLValue(node, "Model", "RGB");
            if (!pModel.empty() && pModel.find("RGB") != std::string::npos)
                eInterp = GPI_RGB;
            else
                throw CPLOPrintf("%s Palette Model %s is unknown, use RGB or RGBA", SIG, pModel.c_str());

            if ((entries < 1) || (entries > 256))
                throw CPLOPrintf("%s Palette definition error", SIG);

            // Create it and initialize it to nothing
            try {
                int start_idx;
                int end_idx;
                GDALColorEntry ce_start = { 0, 0, 0, 255 };
                GDALColorEntry ce_end = { 0, 0, 0, 255 };

                poColorTable = new GDALColorTable(eInterp);
                poColorTable->CreateColorRamp(0, &ce_start, entries - 1, &ce_end);
                // Read the values
                CPLXMLNode *p = CPLGetXMLNode(node, "Entry");
                if (p) {
                    // Initialize the first entry
                    start_idx = static_cast<int>(getXMLNum(p, "idx", "0"));
                    ce_start = GetXMLColorEntry(p);

                    if (start_idx < 0)
                        throw CPLOPrintf("%s Palette index %d not allowed", SIG, start_idx);

                    poColorTable->SetColorEntry(start_idx, &ce_start);
                    while (nullptr != (p = SearchXMLSiblings(p, "Entry"))) {
                        // For every entry, create a ramp
                        ce_end = GetXMLColorEntry(p);
                        end_idx = static_cast<int>(getXMLNum(p, "idx", CPLOPrintf("%d", start_idx + 1)));
                        if ((end_idx <= start_idx) || (start_idx >= entries))
                            throw CPLOPrintf("%s Index Error at index %d", SIG, end_idx);

                        poColorTable->CreateColorRamp(start_idx, &ce_start, end_idx, &ce_end);
                        ce_start = ce_end;
                        start_idx = end_idx;
                    }
                }

                // Dataset has ownership
                m_parent_dataset->SetColorTable(poColorTable);
            }
            catch (const CPLString &) {
                delete poColorTable;
                throw;
            }
        } // If palette

        int overview_count = 0;
        CPLXMLNode *Pattern = TG->psChild;

        m_bsx = -1;
        m_bsy = -1;
        m_data_window.m_sx = 0;
        m_data_window.m_sy = 0;

        while ((nullptr != Pattern) && (nullptr != (Pattern = SearchXMLSiblings(Pattern, "=TilePattern")))) {
            int mbsx, mbsy, sx, sy;
            double x, y, X, Y;

            CPLString request;
            FindChangePattern(Pattern->psChild->pszValue, substs, keys, request);

            char **papszTokens = CSLTokenizeString2(request, "&", 0);

            try {
                const char* pszWIDTH = CSLFetchNameValue(papszTokens, "WIDTH");
                const char* pszHEIGHT = CSLFetchNameValue(papszTokens, "HEIGHT");
                if (pszWIDTH == nullptr || pszHEIGHT == nullptr)
                    throw CPLOPrintf("%s Cannot find width or height parameters in %s", SIG, request.c_str());

                mbsx = atoi(pszWIDTH);
                mbsy = atoi(pszHEIGHT);
                if (m_projection_wkt.empty()) {
                    m_projection_wkt = CSLFetchNameValueDef(papszTokens, "SRS", "");
                    if (!m_projection_wkt.empty())
                        m_projection_wkt = ProjToWKT(m_projection_wkt);
                }

                if (-1 == m_bsx) m_bsx = mbsx;
                if (-1 == m_bsy) m_bsy = mbsy;
                if ((m_bsx != mbsx) || (m_bsy != mbsy))
                    throw CPLOPrintf("%s Tileset uses different block sizes", SIG);

                if (CPLsscanf(CSLFetchNameValueDef(papszTokens, "BBOX", ""), "%lf,%lf,%lf,%lf", &x, &y, &X, &Y) != 4)
                    throw CPLOPrintf("%s Error parsing BBOX, pattern %d\n", SIG, overview_count + 1);

                // Pick the largest size
                sx = static_cast<int>((m_data_window.m_x1 - m_data_window.m_x0) / (X - x)*m_bsx);
                sy = static_cast<int>(fabs((m_data_window.m_y1 - m_data_window.m_y0) / (Y - y)*m_bsy));
                if (sx > m_data_window.m_sx) m_data_window.m_sx = sx;
                if (sy > m_data_window.m_sy) m_data_window.m_sy = sy;
            }
            catch (const CPLString &) {
                CSLDestroy(papszTokens);
                throw;
            }

            CSLDestroy(papszTokens);

            // Only use overlays where the top coordinate is within a pixel from the top of coverage
            double pix_off, temp;
            pix_off = m_bsy * modf(fabs((Y - m_data_window.m_y0) / (Y - y)), &temp);
            if ((pix_off < 1) || ((m_bsy - pix_off) < 1)) {
                requests = CSLAddString(requests, request);
                overview_count++;
            }
            else
                CPLError(CE_Warning, CPLE_AppDefined, "%s Overlay size %dX%d can't be used due to alignment", SIG, sx, sy);

            Pattern = Pattern->psNext;
        } // Search for matching TilePattern

        // The tlevel is needed, the tx and ty are not used by this minidriver
        m_data_window.m_tlevel = 0;
        m_data_window.m_tx = 0;
        m_data_window.m_ty = 0;

        // Make sure the parent_dataset values are set before creating the bands
        m_parent_dataset->WMSSetBlockSize(m_bsx, m_bsy);
        m_parent_dataset->WMSSetRasterSize(m_data_window.m_sx, m_data_window.m_sy);

        m_parent_dataset->WMSSetDataWindow(m_data_window);
        //m_parent_dataset->WMSSetOverviewCount(overview_count);
        m_parent_dataset->WMSSetClamp(false);

        // Ready for the Rasterband creation
        for (int i = 0; i < overview_count; i++) {
            CPLString request = GetLowestScale(requests, i);
            double scale = Scale(request);

            // Base scale should be very close to 1
            if ((0 == i) && (fabs(scale - 1) > 1e-6))
                throw CPLOPrintf("%s Base resolution pattern missing", SIG);

            // Prepare the request and insert it back into the list
            // Find returns an answer relative to the original string start!
            size_t startBbox = FindBbox(request);
            size_t endBbox = request.find('&', startBbox);
            if (endBbox == std::string::npos)
                endBbox = request.size();
            request.replace(startBbox, endBbox - startBbox, "${GDAL_BBOX}");
            requests = CSLInsertString(requests, i, request);

            // Create the Rasterband or overview
            for (int j = 1; j <= band_count; j++) {
                if (i != 0)
                    m_parent_dataset->mGetBand(j)->AddOverview(scale);
                else { // Base resolution
                    GDALWMSRasterBand *band = new
                        GDALWMSRasterBand(m_parent_dataset, j, 1);
                    if (poColorTable != nullptr) band->SetColorInterpretation(GCI_PaletteIndex);
                    else band->SetColorInterpretation(BandInterp(band_count, j));
                    m_parent_dataset->mSetBand(j, band);
                }
            }
        }

        if ((overview_count == 0) || (m_bsx < 1) || (m_bsy < 1))
            throw CPLOPrintf("%s No usable TilePattern elements found", SIG);

        // Do we need to embed the server configuration in the prototype XML
        if (CPLGetXMLValue(config, "Configuration", nullptr) == nullptr) {
            // Get the proposed XML, it will exist at this point
            CPLXMLNode *cfg_root = CPLParseXMLString(m_parent_dataset->GetMetadataItem("XML", "WMS"));

            if (cfg_root && CPLGetXMLNode(cfg_root, "Cache")) { // Only if there is a cache node

                char *server_xml = CPLSerializeXMLTree(tileServiceConfig);

                int len = static_cast<int>(strlen(server_xml));
                char *encoded_server_xml = CPLEscapeString(server_xml, len, CPLES_XML);
                CPLFree(server_xml);

                // It doesn't have a Service.Configuration element, safe to add one
                CPLXMLNode *service_node = CPLGetXMLNode(cfg_root, "Service");
                CPLXMLNode *scfg = CPLCreateXMLElementAndValue(service_node, "Configuration", encoded_server_xml);
                CPLAddXMLAttributeAndValue(scfg, "encoding", "XMLencoded");
                CPLFree(encoded_server_xml);

                char * osXML = CPLSerializeXMLTree(cfg_root);

                m_parent_dataset->SetXML(osXML);
                CPLFree(osXML);
            }

            CPLDestroyXMLNode(cfg_root);
        }
    }
    catch (const CPLString &msg) {
        ret = CE_Failure;
        CPLError(ret, CPLE_AppDefined, "%s", msg.c_str());
    }

    CSLDestroy(keys);
    CSLDestroy(substs);
    if (tileServiceConfig) CPLDestroyXMLNode(tileServiceConfig);

    m_requests = requests;
    return ret;
}

CPLErr WMSMiniDriver_TiledWMS::TiledImageRequest(
    WMSHTTPRequest &request,
    const GDALWMSImageRequestInfo &iri,
    const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    url = m_base_url;
    URLPrepare(url);
    url += CSLGetField(m_requests, -tiri.m_level);
    URLSearchAndReplace(&url, "${GDAL_BBOX}", "%013.8f,%013.8f,%013.8f,%013.8f",
        iri.m_x0, iri.m_y1, iri.m_x1, iri.m_y0);
    return CE_None;
}
