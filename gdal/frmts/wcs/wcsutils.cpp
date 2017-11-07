#include "ogr_spatialref.h"
#include "wcsutils.h"

CPLString String(const char *str)
{
    CPLString retval = str;
    return retval;
}

CPLString URLEncode(CPLString str)
{
    char *pszEncoded = CPLEscapeString(str, -1, CPLES_URL );
    CPLString str2 = pszEncoded;
    CPLFree( pszEncoded );
    return str2;
}

CPLString URLRemoveKey(const char *url, CPLString key)
{
    CPLString retval = url;
    key += "=";
    while (true) {
        size_t pos = retval.ifind(key);
        if (pos != std::string::npos) {
            if (pos > 0) {
                size_t end = retval.find("&", pos);
                retval.erase(pos-1, end - pos + 1);
            }
        } else {
            break;
        }
    }
    return retval;
}

std::vector<CPLString> Split(const char *value, const char *delim, bool swap_the_first_two)
{
    std::vector<CPLString> array;
    char **tokens = CSLTokenizeString2(
        value, delim, CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES | CSLT_HONOURSTRINGS);
    int n = CSLCount(tokens);
    for (int i = 0; i < n; ++i) {
        array.push_back(tokens[i]);
    }
    CSLDestroy(tokens);
    if (swap_the_first_two && array.size() >= 2) {
        CPLString tmp = array[0];
        array[0] = array[1];
        array[1] = tmp;
    }
    return array;
}

CPLString Join(std::vector<CPLString> array, const char *delim, bool swap_the_first_two)
{
    CPLString str;
    for (unsigned int i = 0; i < array.size(); ++i) {
        if (i > 0) {
            str += delim;
        }
        if (swap_the_first_two) {
            if (i == 0 && array.size() > 0) {
                str += array[1];
            } else if (i == 1) {
                str += array[0];
            }
        } else {
            str += array[i];
        }
    }
    return str;
}

std::vector<double> Flist(std::vector<CPLString> array,
                          unsigned int from,
                          unsigned int count)
{
    std::vector<double> retval;
    for (unsigned int i = from; i < array.size() && i < from + count; ++i) {
        retval.push_back(CPLAtof(array[i]));
    }
    return retval;
}

int IndexOf(CPLString str, std::vector<CPLString> array)
{
    int index = -1;
    for (unsigned int i = 0; i < array.size(); ++i) {
        if (array[i] == str) {
            index = i;
            break;
        }
    }
    return index;
}

int IndexOf(int i, std::vector<int> array)
{
    int index = -1;
    for (unsigned int j = 0; j < array.size(); ++j) {
        if (array[j] == i) {
            index = j;
            break;
        }
    }
    return index;
}

std::vector<int> IndexOf(std::vector<CPLString> strs, std::vector<CPLString> array)
{
    std::vector<int> retval;
    for (unsigned int i = 0; i < strs.size(); ++i) {
        retval.push_back(IndexOf(strs[i], array));
    }
    return retval;
}

bool Contains(std::vector<int> array, int value)
{
    for (unsigned int i = 0; i < array.size(); ++i) {
        if (array[i] == value) {
            return true;
        }
    }
    return false;
}

CPLString FromParenthesis(CPLString s)
{
    size_t beg = s.find_first_of("(");
    size_t end = s.find_last_of(")");
    if (beg == std::string::npos || end == std::string::npos) {
        return "";
    }
    return s.substr(beg + 1, end - beg - 1);
}

std::vector<CPLString> ParseSubset(std::vector<CPLString> subset_array, CPLString dim)
{
    // array is SUBSET defs, a SUBSET def is dim[,crs](low[,high])
    std::vector<CPLString> retval;
    unsigned int i;
    CPLString params;
    for (i = 0; i < subset_array.size(); ++i) {
        params = subset_array[i];
        size_t pos = params.find(dim + "(");
        if (pos != std::string::npos) {
            retval.push_back(""); // crs
            break;
        }
        pos = params.find(dim + ",");
        if (pos != std::string::npos) {
            params.erase(0, pos + 1);
            pos = params.find("(");
            retval.push_back(params.substr(0, pos - 1));
            break;
        }
    }
    if (retval.size() > 0) {
        std::vector<CPLString> params_array = Split(FromParenthesis(params), ",");
        retval.push_back(params_array[0]);
        if (params_array.size() > 1) {
            retval.push_back(params_array[1]);
        } else {
            retval.push_back("");
        }
    }
    return retval;
}

/* -------------------------------------------------------------------- */
/*      FileIsReadable                                                  */
/* -------------------------------------------------------------------- */

bool FileIsReadable(CPLString filename)
{
    VSILFILE *file = VSIFOpenL(filename, "r");
    if (file) {
        VSIFCloseL(file);
        return true;
    }
    return false;
}

/* -------------------------------------------------------------------- */
/*      MakeDir                                                         */
/* -------------------------------------------------------------------- */

bool MakeDir(CPLString dirname)
{
    VSIStatBufL stat;
    if (VSIStatL(dirname, &stat) != 0) {
        CPLString parent = CPLGetDirname(dirname);
        if (!parent.empty() && parent != ".") {
            if (!MakeDir(parent)) {
                return false;
            }
        }
        return VSIMkdir(dirname, 0755) == 0;
    }
    return true;
}

/* -------------------------------------------------------------------- */
/*      SetupCache                                                      */
/* -------------------------------------------------------------------- */

bool SetupCache(CPLString &cache_dir, bool clear)
{
    if (cache_dir == "") {
#ifdef WIN32
        const char* home = CPLGetConfigOption("USERPROFILE", NULL);
#else
        const char* home = CPLGetConfigOption("HOME", NULL);
#endif
        if (home) {
            cache_dir = CPLFormFilename(home, ".gdal", NULL);
        } else {
            const char *dir = CPLGetConfigOption("CPL_TMPDIR", NULL);
            if (!dir) dir = CPLGetConfigOption( "TMPDIR", NULL );
            if (!dir) dir = CPLGetConfigOption( "TEMP", NULL );
            const char* username = CPLGetConfigOption("USERNAME", NULL);
            if (!username) username = CPLGetConfigOption("USER", NULL);
            if (dir && username) {
                CPLString subdir = ".gdal_";
                subdir += username;
                cache_dir = CPLFormFilename(dir, subdir, NULL);
            }
        }
    }
    cache_dir = CPLFormFilename(cache_dir, "wcs_cache", NULL);
    if (!MakeDir(cache_dir)) {
        return false;
    }
    if (clear) {
        char **folder = VSIReadDir(cache_dir);
        int size = folder ? CSLCount(folder) : 0;
        for (int i = 0; i < size; i++) {
            if (*folder[i] == '.') {
                continue;
            }
            CPLString filepath = CPLFormFilename(cache_dir, folder[i], NULL);
            remove(filepath);
        }
        CSLDestroy(folder);
    }
    return true;      
}

/* -------------------------------------------------------------------- */
/*      DeleteEntryFromCache                                            */
/* -------------------------------------------------------------------- */

bool DeleteEntryFromCache(CPLString cache_dir, CPLString  key, CPLString value)
{
    // depending on which one of key & value is not "" delete the relevant entry
    CPLString db_name = CPLFormFilename(cache_dir, "db", NULL);
    char **data = CSLLoad(db_name); // returns NULL in error and for empty files
    char **data2 = CSLAddNameValue(NULL, "foo", "bar");
    CPLString filename = "";
    if (data) {
        for (int i = 0; data[i]; ++i) {
            char *val = strchr(data[i], '=');
            if (*val == '=') {
                *val = '\0';
                val = val + 1;
                if ((key != "" && key == data[i])
                    || (value != "" && value == val)
                    || (strcmp(data[i], "foo") == 0))
                {
                    if (key != "") {
                        filename = data[i];
                    } else if (value != "") {
                        filename = data[i];
                    }
                    continue;
                }
                data2 = CSLAddNameValue(data2, data[i], val);
            }
        }
        CSLDestroy(data);
    }
    CSLSave(data2, db_name); // returns 0 in error and for empty arrays
    CSLDestroy(data2);
    if (filename != "") {
        char **folder = VSIReadDir(cache_dir);
        int size = folder ? CSLCount(folder) : 0;
        for (int i = 0; i < size; i++) {
            if (*folder[i] == '.') {
                continue;
            }
            CPLString name = folder[i];
            if (name.find(filename) != std::string::npos) {
                CPLString filepath = CPLFormFilename(cache_dir, name, NULL);
                remove(filepath);
            }
        }
        CSLDestroy(folder);
    }
    return true;
}

/* -------------------------------------------------------------------- */
/*      FromCache                                                       */
/* -------------------------------------------------------------------- */

int FromCache(CPLString cache_dir, CPLString &filename, CPLString url)
{
    int retval = 0;
    CPLString db_name = CPLFormFilename(cache_dir, "db", NULL);
    VSILFILE *db = VSIFOpenL(db_name, "r");
    if (db) {
        while (const char *line = CPLReadLineL(db)) {
            char *value = strchr((char *)line, '=');
            if (*value == '=') {
                *value = '\0';
            } else {
                continue;
            }
            if (strcmp(url, value + 1) == 0) {
                filename = line;
                retval = 1;
                break;
            }
        }
        VSIFCloseL(db);
    }
    if (retval == 0) {
        db = VSIFOpenL(db_name, "a");
        if (!db) {
            CPLError(CE_Failure, CPLE_FileIO, "Can't open file '%s': %i\n", db_name.c_str(), errno);
            return -1;
        }
        // using tempnam is not a good solution
        char *path = tempnam(cache_dir, "");
        filename = CPLGetFilename(path);
        free(path);
        CPLString entry = filename + "=" + url + "\n"; // '=' for compatibility with CSL
        VSIFWriteL(entry.c_str(), sizeof(char), entry.size(), db);
        VSIFCloseL(db);
    }
    filename = CPLFormFilename(cache_dir, filename, NULL);
    return retval;
}

CPLXMLNode *AddSimpleMetaData(char ***metadata,
                              CPLXMLNode *node,
                              CPLString &path,
                              CPLString from,
                              std::vector<CPLString> keys)
{
    CPLXMLNode *node2 = CPLGetXMLNode(node, from);
    if (node2) {
        path = path + from + ".";
        for (unsigned int i = 0; i < keys.size(); i++) {
            if (CPLXMLNode *node3 = CPLGetXMLNode(node2, keys[i])) {
                CPLString name = path + keys[i];
                CPLString value = CPLGetXMLValue(node3, NULL, "");
                *metadata = CSLSetNameValue(*metadata, name, value);
            }
        }
    }
    return node2;
}

CPLString GetKeywords(CPLXMLNode *root,
                      CPLString path,
                      CPLString kw)
{
    CPLString words = "";
    CPLXMLNode *keywords = (path != "") ? CPLGetXMLNode(root, path) : root;
    if (keywords) {
        for (CPLXMLNode *node = keywords->psChild; node != NULL; node = node->psNext) {
            if (node->eType != CXT_Element) {
                continue;
            }
            if (kw == node->pszValue) {
                if (words != "") {
                    words += ",";
                }
                words += CPLGetXMLValue(node, NULL, "");
            }
        }
    }
    return words;
}

CPLString ParseCRS(CPLXMLNode *node)
{
    // test for attrs crs (OWS) and srsName (GML), and text contents of subnode (GridBaseCRS)
    CPLString crs = CPLGetXMLValue(node, "crs", "");
    if (crs == "") {
        crs = CPLGetXMLValue(node, "srsName", "");
        if (crs == "") {
            crs = CPLGetXMLValue(node, "GridBaseCRS", "");
        }
    }
    if (crs == "") {
        return crs;
    }
    // split compound names
    // see for example
    // http://www.eurogeographics.org/sites/default/files/2016-01-18_INSPIRE-KEN-CovFaq.pdf
    size_t pos = crs.find("?");
    if (pos != std::string::npos) {
        if (crs.find("crs-compound?")) { // 1=uri&2=uri...
            // assuming the first is for X,Y
            crs = crs.substr(pos+1);
            pos = crs.find("&");
            if (pos != std::string::npos) {
                pos = pos - 2;
            }
            crs = crs.substr(2, pos);
        }
    }
    return crs;
}

// if appropriate, try to create WKT description from CRS name
// return false if failure
// appropriate means, that the name is a real CRS
bool CRS2Projection(CPLString crs, OGRSpatialReference &oSRS, char **projection)
{
    if (*projection != NULL) {
        CPLFree(projection);
    }
    *projection = NULL;
    if (crs.empty()) {
        return true;
    }
    if (crs.find(":imageCRS") != std::string::npos) {
        // raw image.
        return true;
    }
    // rasdaman uses urls, which return gml:ProjectedCRS XML, which is not recognized by GDAL currently
    if (crs.find("EPSG")) { // ...EPSG...(\d+)
        const char *digits = "0123456789";
        size_t pos1 = crs.find_last_of(digits);
        if (pos1 != std::string::npos) {
            size_t pos2 = pos1 - 1;
            char c = crs.at(pos2);
            while (strchr(digits, c)) {
                pos2 = pos2 - 1;
                c = crs.at(pos2);
            }
            crs = "EPSGA:" + crs.substr(pos2 + 1, pos1 - pos2);
        }
    }
    if (oSRS.SetFromUserInput(crs) == OGRERR_NONE) {
        oSRS.exportToWkt(projection);
        return true;
    }
    return false;
}

bool CRSImpliesAxisOrderSwap(CPLString crs, bool &swap, char **projection)
{
    OGRSpatialReference oSRS;
    char *tmp = NULL;
    if (projection != NULL) {
        tmp = *projection;
    }
    if (!CRS2Projection(crs, oSRS, &tmp)) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to interpret coverage CRS '%s'.", crs.c_str() );
        return false;
    }
    if (tmp) {
        swap = oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting();
    } else {
        swap = false;
    }
    if (projection == NULL) {
        CPLFree(tmp);
    }
    return true;
}

std::vector<std::vector<int>> ParseGridEnvelope(CPLXMLNode *node,
                                                bool swap_the_first_two)
{
    std::vector<std::vector<int>> envelope;
    std::vector<CPLString> array = Split(CPLGetXMLValue(node, "low", ""), " ", swap_the_first_two);
    std::vector<int> lows;
    for (unsigned int i = 0; i < array.size(); ++i) {
        lows.push_back(atoi(array[i]));
    }
    envelope.push_back(lows);
    array = Split(CPLGetXMLValue(node, "high", ""), " ", swap_the_first_two);
    std::vector<int> highs;
    for (unsigned int i = 0; i < array.size(); ++i) {
        highs.push_back(atoi(array[i]));
    }
    envelope.push_back(highs);
    return envelope;
}

std::vector<CPLString> ParseBoundingBox(CPLXMLNode *node)
{
    std::vector<CPLString> bbox;
    CPLString lc = CPLGetXMLValue(node, "lowerCorner", ""), uc;
    if (lc == "") {
        lc = CPLGetXMLValue(node, "LowerCorner", "");
    }
    if (lc == "") {
        for (CPLXMLNode *n = node->psChild; n != NULL; n = n->psNext) {
            if (n->eType != CXT_Element || !EQUAL(n->pszValue, "pos")) {
                continue;
            }
            if (lc == "") {
                lc = CPLGetXMLValue(node, NULL, "");
            } else {
                uc = CPLGetXMLValue(node, NULL, "");
            }
        }
    } else {
        uc = CPLGetXMLValue(node, "upperCorner", "");
        if (uc == "") {
            uc = CPLGetXMLValue(node, "UpperCorner", "");
        }
    }
    if (lc != "" && uc != "") {
        bbox.push_back(lc);
        bbox.push_back(uc);
    }
    return bbox;
}
