/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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
std::string OFGDBGenerateUUID()
{
    struct CPLTimeVal tv;
    memset(&tv, 0, sizeof(tv));
    static uint32_t nCounter = 0;
    const bool bReproducibleUUID =
        CPLTestBool(CPLGetConfigOption("OPENFILEGDB_REPRODUCIBLE_UUID", "NO"));

    std::stringstream ss;

    {
        if (!bReproducibleUUID)
            CPLGettimeofday(&tv, nullptr);
        std::mt19937 gen(++nCounter +
                         (bReproducibleUUID
                              ? 0
                              : static_cast<unsigned>(tv.tv_sec ^ tv.tv_usec)));
        std::uniform_int_distribution<> dis(0, 15);

        ss << "{";
        ss << std::hex;
        for (int i = 0; i < 8; i++)
        {
            ss << dis(gen);
        }
        ss << "-";
        for (int i = 0; i < 4; i++)
        {
            ss << dis(gen);
        }
        ss << "-4";
        for (int i = 0; i < 3; i++)
        {
            ss << dis(gen);
        }
    }

    {
        if (!bReproducibleUUID)
            CPLGettimeofday(&tv, nullptr);
        std::mt19937 gen(++nCounter +
                         (bReproducibleUUID
                              ? 0
                              : static_cast<unsigned>(tv.tv_sec ^ tv.tv_usec)));
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);

        ss << "-";
        ss << dis2(gen);
        for (int i = 0; i < 3; i++)
        {
            ss << dis(gen);
        }
        ss << "-";
        for (int i = 0; i < 12; i++)
        {
            ss << dis(gen);
        };
        ss << "}";
        return ss.str();
    }
}
