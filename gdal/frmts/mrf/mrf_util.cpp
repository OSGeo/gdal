/*
* Copyright (c) 2002-2012, California Institute of Technology.
* All rights reserved.  Based on Government Sponsored Research under contracts NAS7-1407 and/or NAS7-03001.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
*   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above copyright notice,
*      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
*   3. Neither the name of the California Institute of Technology (Caltech), its operating division the Jet Propulsion Laboratory (JPL),
*      the National Aeronautics and Space Administration (NASA), nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE CALIFORNIA INSTITUTE OF TECHNOLOGY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Copyright 2014-2015 Esri
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/**
 *
 *  Functions used by the driver, should have prototypes in the header file
 *
 *  Author: Lucian Plesea
 */

#include "marfa.h"
#include <zlib.h>
#include <algorithm>

CPL_CVSID("$Id$");

// LERC is not ready for big endian hosts for now
#if defined(LERC) && defined(WORDS_BIGENDIAN)
#undef LERC
#endif

CPL_C_START
void GDALRegister_mrf(void);
CPL_C_END

NAMESPACE_MRF_START

// These have to be positionally in sync with the enums in marfa.h
static const char * const ILC_N[] = { "PNG", "PPNG", "JPEG", "JPNG", "NONE", "DEFLATE", "TIF",
#if defined(LERC)
        "LERC",
#endif
        "Unknown" };

static const char * const ILC_E[]={ ".ppg", ".ppg", ".pjg", ".pjp", ".til", ".pzp", ".ptf",
#if defined(LERC)
        ".lrc" ,
#endif
        "" };

static const char * const ILO_N[]={ "PIXEL", "BAND", "LINE", "Unknown" };

char const * const * ILComp_Name=ILC_N;
char const * const * ILComp_Ext=ILC_E;
char const * const * ILOrder_Name=ILO_N;

/**
 *  Get the string for a compression type
 */
const char *CompName(ILCompression comp)
{
    if (comp>=IL_ERR_COMP) return ILComp_Name[IL_ERR_COMP];
    return ILComp_Name[comp];
}

/**
 *  Get the string for an order type
 */
const char *OrderName(ILOrder val)
{
    if (val>=IL_ERR_ORD) return ILOrder_Name[IL_ERR_ORD];
    return ILOrder_Name[val];
}

ILCompression CompToken(const char *opt, ILCompression def)
{
    int i;
    if (NULL==opt) return def;
    for (i=0; ILCompression(i) < IL_ERR_COMP; i++)
        if (EQUAL(opt,ILComp_Name[i]))
            break;
    if (IL_ERR_COMP == ILCompression(i))
        return def;
    return ILCompression(i);
}

/**
 *  Find a compression token
 */
ILOrder OrderToken(const char *opt, ILOrder def)
{
    int i;
    if (NULL==opt) return def;
    for (i=0; ILOrder(i)<IL_ERR_ORD; i++)
        if (EQUAL(opt,ILOrder_Name[i]))
            break;
    if (IL_ERR_ORD==ILOrder(i))
        return def;
    return ILOrder(i);
}

//
//  Inserters for ILSize and ILIdx types
//
std::ostream& operator<<(std::ostream &out, const ILSize& sz)
{
    out << "X=" << sz.x << ",Y=" << sz.y << ",Z=" << sz.z
        << ",C=" << sz.c << ",L=" << sz.l;
    return out;
}

std::ostream& operator<<(std::ostream &out, const ILIdx& t) {
    out << "offset=" << t.offset << ",size=" << t.size;
    return out;
}

// Define PPMW to enable this handy debug function

#ifdef PPMW
void ppmWrite(const char *fname, const char *data, const ILSize &sz) {
    FILE *fp=fopen(fname,"wb");
    switch(sz.c) {
    case 4:
        fprintf(fp,"P6 %d %d 255\n",sz.x,sz.y);
        char *d=(char *)data;
        for(int i=sz.x*sz.y;i;i--) {
            fwrite(d,3,1,fp);
            d+=4;
        }
        break;
    case 3:
        fprintf(fp,"P6 %d %d 255\n",sz.x,sz.y);
        fwrite(data,sz.x*sz.y,3,fp);
        break;
    case 1:
        fprintf(fp,"P5 %d %d 255\n",sz.x,sz.y);
        fwrite(data,sz.x,sz.y,fp);
        break;
    default:
        fprintf(stderr,"Can't write ppm file with %d bands\n",sz.c);/*ok*/
        return;
    }
    fclose(fp);
}
#endif

// Returns the size of the index for image and overlays
// If scale is zero, only base image
GIntBig IdxSize(const ILImage &full, const int scale) {
    ILImage img = full;
    img.pagecount = pcount(img.size, img.pagesize);
    GIntBig sz = img.pagecount.l;
    while (scale != 0 && 1 != img.pagecount.x * img.pagecount.y)
    {
        img.size.x = pcount(img.size.x, scale);
        img.size.y = pcount(img.size.y, scale);
        img.pagecount = pcount(img.size, img.pagesize);
        sz += img.pagecount.l;
    }
    return sz*sizeof(ILIdx);
}

ILImage::ILImage() :
    dataoffset(0),
    idxoffset(0),
    quality(85),
    pageSizeBytes(0),
    size(ILSize(1, 1, 1, 1, 0)),
    pagesize(ILSize(384, 384, 1, 1, 0)),
    pagecount(pcount(size, pagesize)),
    comp(IL_PNG),
    order(IL_Interleaved),
    nbo(0),
    hasNoData(FALSE),
    NoDataValue(0.0),
    dt(GDT_Unknown),
    ci(GCI_Undefined)
{}

/**
 *\brief Get a file name by replacing the extension.
 * pass the data file name and the default extension starting with .
 * If name length is not sufficient, it returns the extension
 * If the input name is curl with parameters, the base file extension gets changed and
 * parameters are preserved.
 */

CPLString getFname(const CPLString &in, const char *ext)
{
    if (strlen(in) < strlen(ext))
        return CPLString(ext);

    CPLString ret(in);
    // Is it a web file with parameters?
    size_t extlen = strlen(ext);
    size_t qmark = ret.find_first_of('?');
    if (!(qmark != std::string::npos && 0 == in.find("/vsicurl/http") && qmark >= extlen))
        qmark = ret.size();
    return ret.replace(qmark - extlen, extlen, ext);
}

/**
 *\brief Get a file name, either from the configuration or from the default file name
 * If the token is not defined by CPLGetXMLValue, if the extension of the in name is .xml,
 * it returns the token with the extension changed to defext.
 * Otherwise it returns the token itself
 * It is pretty hard to separate local vs remote due to the gdal file name ornaments
 * Absolute file names start with: ?:/ or /
 *
 */

CPLString getFname(CPLXMLNode *node, const char *token, const CPLString &in, const char *def)
{
    CPLString fn = CPLGetXMLValue(node, token, "");
    if (fn.empty()) // Not provided
        return getFname(in, def);
    size_t slashPos = fn.find_first_of("\\/");

    // Does it look like an absolute path or we wont't find the basename of in
    if (slashPos == 0                               // Starts with slash
        || (slashPos == 2 && fn[1] == ':')          // Starts with disk letter column
        || !(slashPos == fn.find_first_not_of('.')) // Does not start with dots and then slash
        || EQUALN(in,"<MRF_META>",10)               // XML string input
        || in.find_first_of("\\/") == in.npos)      // We can't get a basename from in
        return fn;

    // Relative path, prepand the path from the in file name
    return in.substr(0, in.find_last_of("\\/")+1) + fn;
}

/**
 *\brief Extracts a numerical value from a XML node
 * It works like CPLGetXMLValue except for the default value being
 * a number instead of a string
 */

double getXMLNum(CPLXMLNode *node, const char *pszPath, double def)
{
    const char *textval=CPLGetXMLValue(node,pszPath,NULL);
    if (textval) return atof(textval);
    return def;
}

//
// Calculate offset of index, pos is in pages
//

GIntBig IdxOffset(const ILSize &pos, const ILImage &img)
{
    return img.idxoffset + sizeof(ILIdx) *
        (pos.c + img.pagecount.c * (
         pos.x+img.pagecount.x * (
         pos.y+img.pagecount.y *
         static_cast<GIntBig>(pos.z)
        )));
}

// Is compression type endianness dependent?
bool is_Endianess_Dependent(GDALDataType dt, ILCompression comp) {
    // Add here all endianness dependent compressions
    if (IL_ZLIB == comp || IL_NONE == comp)
        if (GDALGetDataTypeSize( dt ) > 8)
            return true;
    return false;
}

GDALMRFRasterBand *newMRFRasterBand(GDALMRFDataset *pDS, const ILImage &image, int b, int level)

{
    GDALMRFRasterBand *bnd = NULL;
    switch(pDS->current.comp)
    {
    case IL_PPNG: // Uses the PNG code, just has a palette in each PNG
    case IL_PNG:  bnd = new PNG_Band(pDS, image, b, level);  break;
    case IL_JPEG: bnd = new JPEG_Band(pDS, image, b, level); break;
    case IL_JPNG: bnd = new JPNG_Band(pDS, image, b, level); break;
    case IL_NONE: bnd = new Raw_Band(pDS, image, b, level);  break;
    // ZLIB is just raw + deflate
    case IL_ZLIB: bnd = new Raw_Band(pDS, image, b, level);  bnd->SetDeflate(1); break;
    case IL_TIF:  bnd = new TIF_Band(pDS, image, b, level);  break;
#if defined(LERC)
    case IL_LERC: bnd = new LERC_Band(pDS, image, b, level); break;
#endif
    default:
        return NULL;
    }

    // If something was flagged during band creation
    if (CPLGetLastErrorNo() != CE_None) {
        delete bnd;
        return NULL;
    }

    // Copy the RW mode from the dataset
    bnd->SetAccess(pDS->eAccess);
    return bnd;
}

/**
 *\brief log in a given base
 */
double logbase(double val, double base) {
    return log(val) / log(base);
}

/**
 *\brief Is logbase(val, base) an integer?
 *
 */

int IsPower(double value, double base) {
    double v = logbase(value, base);
    return CPLIsEqual(v, int(v + 0.5));
}

/************************************************************************/
/*                           SearchXMLSiblings()                        */
/************************************************************************/

/**
 *\brief Search for a sibling of the root node with a given name.
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

CPLXMLNode *SearchXMLSiblings( CPLXMLNode *psRoot, const char *pszElement )

{
    if( psRoot == NULL || pszElement == NULL )
        return NULL;

    // If the strings starts with '=', skip it and test the root
    // If not, start testing with the next sibling
    if (pszElement[0]=='=') pszElement++;
    else psRoot=psRoot->psNext;

    for (;psRoot!=NULL;psRoot=psRoot->psNext)
        if ((psRoot->eType == CXT_Element ||
             psRoot->eType == CXT_Attribute)
             && EQUAL(pszElement,psRoot->pszValue))
            return psRoot;

    return NULL;
}

//
// Extension to CSL, set an entry if it doesn't already exist
//
char **CSLAddIfMissing(char **papszList,
    const char *pszName, const char *pszValue)
{
    if (CSLFetchNameValue(papszList, pszName))
        return papszList;
    return CSLSetNameValue(papszList, pszName, pszValue);
}

//
// Print a double so it can be read with strod while preserving precision
// Unfortunately this is not quite possible or portable enough at this time
//
CPLString PrintDouble(double d, const char *frmt)
{
    CPLString res;
    res.FormatC(d, NULL);
    double v = CPLStrtod(res.c_str(), NULL);
    if (d == v) return res;

    //  This would be the right code with a C99 compiler that supports %a readback in strod()
    //    return CPLString().Printf("%a",d);

    return CPLString().FormatC(d, frmt);
}

static void XMLSetAttributeVal(CPLXMLNode *parent, const char* pszName,
    const char *val)
{
    CPLCreateXMLNode(parent, CXT_Attribute, pszName);
    CPLSetXMLValue(parent, pszName, val);
}

void XMLSetAttributeVal(CPLXMLNode *parent, const char* pszName,
    const double val, const char *frmt)
{
    XMLSetAttributeVal(parent, pszName, CPLString().FormatC(val, frmt));

    //  Unfortunately the %a doesn't work in VisualStudio scanf or strtod
    //    if (strtod(sVal.c_str(),0) != val)
    //  sVal.Printf("%a",val);
}

CPLXMLNode *XMLSetAttributeVal(CPLXMLNode *parent,
    const char*pszName, const ILSize &sz, const char *frmt)
{
    CPLXMLNode *node = CPLCreateXMLNode(parent, CXT_Element, pszName);
    XMLSetAttributeVal(node, "x", sz.x, frmt);
    XMLSetAttributeVal(node, "y", sz.y, frmt);
    if (sz.z != 1)
        XMLSetAttributeVal(node, "z", sz.z, frmt);
    XMLSetAttributeVal(node, "c", sz.c, frmt);
    return node;
}

//
// Prints a vector of doubles into a string and sets that string as the value of an XML attribute
// If all values are the same, it only prints one
//
void XMLSetAttributeVal(CPLXMLNode *parent,
    const char*pszName, std::vector<double> const &values)
{
    if (values.empty())
        return;

    CPLString value;
    double val = values[0];
    int single_val = true;
    for (int i = 0; i < int(values.size()); i++) {
        if (val != values[i])
            single_val = false;
        value.append(PrintDouble(values[i]) + " ");
        value.resize(value.size() - 1); // Cut the last space
    }
    if (single_val)
        value = PrintDouble(values[0]);
    CPLCreateXMLNode(parent, CXT_Attribute, pszName);
    CPLSetXMLValue(parent, pszName, value);
}

/**
 *\brief Read a ColorEntry XML node, return a GDALColorEntry structure
 *
 */

GDALColorEntry GetXMLColorEntry(CPLXMLNode *p) {
    GDALColorEntry ce;
    ce.c1 = static_cast<short>(getXMLNum(p, "c1", 0));
    ce.c2 = static_cast<short>(getXMLNum(p, "c2", 0));
    ce.c3 = static_cast<short>(getXMLNum(p, "c3", 0));
    ce.c4 = static_cast<short>(getXMLNum(p, "c4", 255));
    return ce;
}

/**
 *\brief Verify or make a file that big
 *
 * @return true if size is OK or if extend succeeded
 */

int CheckFileSize(const char *fname, GIntBig sz, GDALAccess eAccess) {

    VSIStatBufL statb;
    if (VSIStatL(fname, &statb))
        return false;
    if (statb.st_size >= sz)
        return true;

    // Don't change anything unless updating
    if (eAccess != GA_Update)
        return false;

    // There is no ftruncate in VSI, only truncate()
    VSILFILE *ifp = VSIFOpenL(fname, "r+b");
    if( ifp == NULL )
        return false;

// There is no VSIFTruncateL in gdal 1.8 and lower, seek and write something at the end
#if GDAL_VERSION_MAJOR == 1 && GDAL_VERSION_MINOR <= 8
    int zero = 0;
    VSIFSeekL(ifp, sz - sizeof(zero), SEEK_SET);
    int ret = (sizeof(zero) == VSIFWriteL(&zero, sizeof(zero), 1,ifp));
#else
    int ret = VSIFTruncateL(ifp, sz);
#endif
    VSIFCloseL(ifp);
    return !ret;
};

// Similar to compress2() but with flags to control zlib features
// Returns true if it worked
int ZPack(const buf_mgr &src, buf_mgr &dst, int flags) {
    z_stream stream;
    int err;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef*)src.buffer;
    stream.avail_in = (uInt)src.size;
    stream.next_out = (Bytef*)dst.buffer;
    stream.avail_out = (uInt)dst.size;

    int level = std::min(9, flags & ZFLAG_LMASK);
    int wb = MAX_WBITS;
    // if gz flag is set, ignore raw request
    if (flags & ZFLAG_GZ) wb += 16;
    else if (flags & ZFLAG_RAW) wb = -wb;
    int memlevel = 8; // Good compromise
    int strategy = (flags & ZFLAG_SMASK) >> 6;
    if (strategy > 4) strategy = 0;

    err = deflateInit2(&stream, level, Z_DEFLATED, wb, memlevel, strategy);
    if (err != Z_OK) return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return false;
    }
    dst.size = stream.total_out;
    err = deflateEnd(&stream);
    return err == Z_OK;
}

// Similar to uncompress() from zlib, accepts the ZFLAG_RAW
// Return true if it worked
int ZUnPack(const buf_mgr &src, buf_mgr &dst, int flags) {

    z_stream stream;
    int err;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef*)src.buffer;
    stream.avail_in = (uInt)src.size;
    stream.next_out = (Bytef*)dst.buffer;
    stream.avail_out = (uInt)dst.size;

    // 32 means autodetec gzip or zlib header, negative 15 is for raw
    int wb = (ZFLAG_RAW & flags) ? -MAX_WBITS: 32 + MAX_WBITS;
    err = inflateInit2(&stream, wb);
    if (err != Z_OK) return false;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        return false;
    }
    dst.size = stream.total_out;
    err = inflateEnd(&stream);
    return err == Z_OK;
}

NAMESPACE_MRF_END

/************************************************************************/
/*                          GDALRegister_mrf()                          */
/************************************************************************/

USING_NAMESPACE_MRF

void GDALRegister_mrf()

{
    if( GDALGetDriverByName("MRF") != NULL )
        return;

    GDALDriver *driver = new GDALDriver();
    driver->SetDescription("MRF");
    driver->SetMetadataItem(GDAL_DMD_LONGNAME, "Meta Raster Format");
    driver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_marfa.html");
    driver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

#if GDAL_VERSION_MAJOR >= 2
    driver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
#endif

    // These will need to be revisited, do we support complex data types too?
    driver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                            "Byte UInt16 Int16 Int32 UInt32 Float32 Float64");

    driver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='COMPRESS' type='string-select' default='PNG' description='PPNG = Palette PNG; DEFLATE = zlib '>"
        "       <Value>JPEG</Value><Value>PNG</Value><Value>PPNG</Value><Value>JPNG</Value>"
        "       <Value>TIF</Value><Value>DEFLATE</Value><Value>NONE</Value>"
#if defined(LERC)
        "       <Value>LERC</Value>"
#endif
        "   </Option>"
        "   <Option name='INTERLEAVE' type='string-select' default='PIXEL'>"
        "       <Value>PIXEL</Value>"
        "       <Value>BAND</Value>"
        "   </Option>\n"
        "   <Option name='ZSIZE' type='int' description='Third dimension size' default='1'/>"
        "   <Option name='QUALITY' type='int' description='best=99, bad=0, default=85'/>\n"
        "   <Option name='OPTIONS' type='string' description='Freeform dataset parameters'/>\n"
        "   <Option name='BLOCKSIZE' type='int' description='Block size, both x and y, default 512'/>\n"
        "   <Option name='BLOCKXSIZE' type='int' description='Block x size, default=512'/>\n"
        "   <Option name='BLOCKYSIZE' type='int' description='Block y size, default=512'/>\n"
        "   <Option name='NETBYTEORDER' type='boolean' "
                    "description='Force endian for certain compress options, default is host order'/>\n"
        "   <Option name='CACHEDSOURCE' type='string' "
                    "description='The source raster, if this is a cache'/>\n"
        "   <Option name='UNIFORM_SCALE' type='int' description='Scale of overlays in MRF, usually 2'/>\n"
        "   <Option name='NOCOPY' type='boolean' description='Leave created MRF empty, default=no'/>\n"
        "   <Option name='DATANAME' type='string' description='Data file name'/>\n"
        "   <Option name='INDEXNAME' type='string' description='Index file name'/>\n"
        "   <Option name='SPACING' type='int' "
                    "description='Leave this many unused bytes before each tile, default=0'/>\n"
        "   <Option name='PHOTOMETRIC' type='string-select' default='DEFAULT' "
                    "description='Band interpretation, may affect block encoding'>\n"
        "       <Value>MULTISPECTRAL</Value>"
        "       <Value>RGB</Value>"
        "       <Value>YCC</Value>"
        "   </Option>\n"
        "</CreationOptionList>\n");

    driver->SetMetadataItem(
      GDAL_DMD_OPENOPTIONLIST,
      "<OpenOptionList>"
      "    <Option name='NOERRORS' type='boolean' description='Ignore decompression errors' default='FALSE'/>"
      "</OpenOptionList>"
      );

    driver->pfnOpen = GDALMRFDataset::Open;
    driver->pfnIdentify = GDALMRFDataset::Identify;
    driver->pfnCreateCopy = GDALMRFDataset::CreateCopy;
    driver->pfnCreate = GDALMRFDataset::Create;
    driver->pfnDelete = GDALMRFDataset::Delete;
    GetGDALDriverManager()->RegisterDriver(driver);
}
