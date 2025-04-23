/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_openfilegdb.h"

#include <random>
#include <sstream>

/************************************************************************/
/*                        CPLGettimeofday()                             */
/************************************************************************/

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <sys/timeb.h>

namespace
{
struct CPLTimeVal
{
    time_t tv_sec; /* seconds */
    long tv_usec;  /* and microseconds */
};
}  // namespace

static int CPLGettimeofday(struct CPLTimeVal *tp, void * /* timezonep*/)
{
    struct _timeb theTime;

    _ftime(&theTime);
    tp->tv_sec = static_cast<time_t>(theTime.time);
    tp->tv_usec = theTime.millitm * 1000;
    return 0;
}
#else
#include <sys/time.h> /* for gettimeofday() */
#define CPLTimeVal timeval
#define CPLGettimeofday(t, u) gettimeofday(t, u)
#endif

/***********************************************************************/
/*                      OFGDBGenerateUUID()                            */
/***********************************************************************/

// Probably not the best UUID generator ever. One issue is that mt19937
// uses only a 32-bit seed.
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
std::string OFGDBGenerateUUID(bool bInit)
{
    struct CPLTimeVal tv;
    memset(&tv, 0, sizeof(tv));
    static uint32_t nCounter = 0;
    const bool bReproducibleUUID =
        CPLTestBool(CPLGetConfigOption("OPENFILEGDB_REPRODUCIBLE_UUID", "NO"));

    if (bInit)
    {
        if (bReproducibleUUID)
            nCounter = 0;
        return std::string();
    }

    uint32_t nCounterLocal = nCounter;
    // From POSIX.1-2001 as an example of an implementation of rand()
    // for reproducible output.
    // We have to use that rather than relying on std::mt19937 +
    // std::uniform_int_distribution since they don't given the same output
    // from the same seed on all platforms.
    const auto reproducibleRand = [&nCounterLocal]()
    {
        nCounterLocal = static_cast<uint32_t>(
            (static_cast<uint64_t>(nCounterLocal) * 1103515245U + 12345U) &
            UINT32_MAX);
        return (nCounterLocal / 65536U) % 32768U;
    };

    std::stringstream ss;

    {
        if (!bReproducibleUUID)
        {
            CPLGettimeofday(&tv, nullptr);
            ++nCounter;
        }
        std::mt19937 gen(nCounter +
                         static_cast<unsigned>(tv.tv_sec ^ tv.tv_usec));
        std::uniform_int_distribution<> dis(0, 15);

        ss << "{";
        ss << std::hex;
        for (int i = 0; i < 8; i++)
        {
            ss << (bReproducibleUUID ? (reproducibleRand() % 16) : dis(gen));
        }
        ss << "-";
        for (int i = 0; i < 4; i++)
        {
            ss << (bReproducibleUUID ? (reproducibleRand() % 16) : dis(gen));
        }
        ss << "-4";
        for (int i = 0; i < 3; i++)
        {
            ss << (bReproducibleUUID ? (reproducibleRand() % 16) : dis(gen));
        }
    }

    {
        if (!bReproducibleUUID)
        {
            CPLGettimeofday(&tv, nullptr);
            ++nCounter;
        }
        std::mt19937 gen(nCounter +
                         static_cast<unsigned>(tv.tv_sec ^ tv.tv_usec));
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);

        ss << "-";
        ss << (bReproducibleUUID ? 8 : dis2(gen));
        for (int i = 0; i < 3; i++)
        {
            ss << (bReproducibleUUID ? (reproducibleRand() % 16) : dis(gen));
        }
        ss << "-";
        for (int i = 0; i < 12; i++)
        {
            ss << (bReproducibleUUID ? (reproducibleRand() % 16) : dis(gen));
        };
        ss << "}";
    }

    if (bReproducibleUUID)
        nCounter = nCounterLocal;

    return ss.str();
}
