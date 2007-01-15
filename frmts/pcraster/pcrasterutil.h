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



GDALDataType       cellRepresentation2GDALType(CSF_CR cellRepresentation);

CSF_VS             string2ValueScale   (const std::string& string);

std::string        valueScale2String   (CSF_VS valueScale);

std::string        cellRepresentation2String(CSF_CR cellRepresentation);

CSF_VS             GDALType2ValueScale (GDALDataType type);

/*
CSF_CR             string2PCRasterCellRepresentation(
                                        const std::string& string);
                                        */

CSF_CR             GDALType2CellRepresentation(
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

MAP*               mapOpen             (std::string const& filename,
                                        MOPEN_PERM mode);

CSF_VS             fitValueScale       (CSF_VS valueScale,
                                        CSF_CR cellRepresentation);
