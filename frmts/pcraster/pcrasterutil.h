// Library headers.
#ifndef INCLUDED_STRING
#include <string>
#define INCLUDED_STRING
#endif

// PCRaster library headers.
#ifndef INCLUDED_CSF
#include "csf.h"
#define INCLUDED_CSF
#endif

// Module headers.
#ifndef INCLUDED_GDAL_PRIV
#include "gdal_priv.h"
#define INCLUDED_GDAL_PRIV
#endif



GDALDataType       PCRasterType2GDALType    (CSF_CR PCRType);

CSF_VS             string2PCRasterValueScale(const std::string& string);

std::string        PCRasterValueScale2String(CSF_VS valueScale);

CSF_VS             GDALType2PCRasterValueScale(
                                        GDALDataType type);

/*
CSF_CR             string2PCRasterCellRepresentation(
                                        const std::string& string);
                                        */

CSF_CR             GDALType2PCRasterCellRepresentation(
                                        GDALDataType type,
                                        bool exact);

void*              createBuffer        (size_t size,
                                        CSF_CR type);

void               deleteBuffer        (void* buffer,
                                        CSF_CR type);

bool               isContinuous        (CSF_VS valueScale);

double             missingValue        (CSF_CR type);

void               alterFromStdMV      (void* buffer,
                                        size_t size,
                                        CSF_CR cellRepresentation,
                                        double missingValue);

void               alterToStdMV        (void* buffer,
                                        size_t size,
                                        CSF_CR cellRepresentation,
                                        double missingValue);

CSF_CR             updateCellRepresentation(
                                        CSF_VS valueScale,
                                        CSF_CR cellRepresentation);

MAP*               open                (std::string const& filename,
                                        MOPEN_PERM mode);

CSF_VS             fitValueScale       (CSF_VS valueScale,
                                        CSF_CR cellRepresentation);
