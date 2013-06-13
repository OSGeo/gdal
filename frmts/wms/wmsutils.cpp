/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Supporting utility functions for GDAL WMS driver.
 * Author:   Adam Nowacki, nowak@xpam.de
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

#include "wmsdriver.h"

CPLString MD5String(const char *s) {
    unsigned char hash[16];
    char hhash[33];
    const char *tohex = "0123456789abcdef";
    struct cvs_MD5Context context;
    cvs_MD5Init(&context);
    cvs_MD5Update(&context, reinterpret_cast<unsigned char const *>(s), strlen(s));
    cvs_MD5Final(hash, &context);
    for (int i = 0; i < 16; ++i) {
        hhash[i * 2] = tohex[(hash[i] >> 4) & 0xf];
        hhash[i * 2 + 1] = tohex[hash[i] & 0xf];
    }
    hhash[32] = '\0';
    return CPLString(hhash);
}

CPLString ProjToWKT(const CPLString &proj) {
    char* wkt = NULL;
    OGRSpatialReference sr;
    CPLString srs;

    /* We could of course recognize OSGEO:41001 to SetFromUserInput(), but this hackish SRS */
    /* is almost only used in the context of WMS */
    if (strcmp(proj.c_str(),"OSGEO:41001") == 0)
    {
        if (sr.SetFromUserInput("EPSG:3857") != OGRERR_NONE) return srs;
    }
    else if (EQUAL(proj.c_str(),"EPSG:NONE"))
    {
        return srs;
    }
    else
    {
        if (sr.SetFromUserInput(proj.c_str()) != OGRERR_NONE) return srs;
    }
    sr.exportToWkt(&wkt);
    srs = wkt;
    OGRFree(wkt);
    return srs;
}

void URLAppend(CPLString *url, const char *s) {
    if ((s == NULL) || (s[0] == '\0')) return;
    if (s[0] == '&') {
        if (url->find('?') == std::string::npos) url->append(1, '?');
        if (((*url)[url->size() - 1] == '?') || ((*url)[url->size() - 1] == '&')) url->append(s + 1);
        else url->append(s);
    } else url->append(s);
}

void URLAppendF(CPLString *url, const char *s, ...) {
    CPLString tmp;
    va_list args;

    va_start(args, s);
    tmp.vPrintf(s, args);
    va_end(args);

    URLAppend(url, tmp.c_str());
}

void URLAppend(CPLString *url, const CPLString &s) {
    URLAppend(url, s.c_str());
}

CPLString BufferToVSIFile(GByte *buffer, size_t size) {
    CPLString file_name;

    file_name.Printf("/vsimem/wms/%p/wmsresult.dat", buffer);
    VSILFILE *f = VSIFileFromMemBuffer(file_name.c_str(), buffer, size, false);
    if (f == NULL) return CPLString();
    VSIFCloseL(f);
    return file_name;
}

CPLErr MakeDirs(const char *path) {
    char *p = CPLStrdup(CPLGetDirname(path));
    if (strlen(p) >= 2) {
        MakeDirs(p);
    }
    VSIMkdir(p, 0744);
    CPLFree(p);
    return CE_None;
}

int VersionStringToInt(const char *version) {
    if (version == NULL) return -1;
    const char *p = version;
    int v = 0;
    for (int i = 3; i >= 0; --i) {
        v += (1 << (i * 8)) * atoi(p);
        for (; (*p != '\0') && (*p != '.'); ++p);
        if (*p != '\0') ++p;
    }
    return v;
}

int StrToBool(const char *p) {
    if (p == NULL) return -1;
    if (EQUAL(p, "1") || EQUAL(p, "true") || EQUAL(p, "yes") || EQUAL(p, "enable") || EQUAL(p, "enabled") || EQUAL(p, "on")) return 1;
    if (EQUAL(p, "0") || EQUAL(p, "false") || EQUAL(p, "no") || EQUAL(p, "disable") || EQUAL(p, "disabled") || EQUAL(p, "off")) return 0;
    return -1;
}

int URLSearchAndReplace (CPLString *base, const char *search, const char *fmt, ...) {
    CPLString tmp;
    va_list args;

    size_t start = base->find(search);
    if (start == std::string::npos) {
        return -1;
    }

    va_start(args, fmt);
    tmp.vPrintf(fmt, args);
    va_end(args);

    base->replace(start, strlen(search), tmp);
    return start;
}
