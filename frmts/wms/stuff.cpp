#include "stdinc.h"

CPLString MD5String(const char *s) {
    unsigned char hash[16];
    char hhash[33];
    char *tohex = "0123456789abcdef";

    md5(reinterpret_cast<unsigned char *>(const_cast<char *>(s)), strlen(s), hash);
    for (int i = 0; i < 16; ++i) {
        hhash[i * 2] = tohex[(hash[i] >> 4) & 0xf];
        hhash[i * 2 + 1] = tohex[hash[i] & 0xf];
    }
    hhash[32] = '\0';
    return CPLString(hhash);
}

CPLString ProjToWKT(const CPLString &proj) {
    char* wkt;
    OGRSpatialReference sr;
    CPLString srs;

    if (sr.SetFromUserInput(proj.c_str()) != OGRERR_NONE) return srs;
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
    FILE *f = VSIFileFromMemBuffer(file_name.c_str(), buffer, size, false);
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
