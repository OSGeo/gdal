/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Return number of current threads in current process
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_multiproc.h"

/************************************************************************/
/*                      CPLGetCurrentThreadCount()                      */
/************************************************************************/

/**
 * \fn CPLGetCurrentThreadCount()
 *
 * Return the current number of threads of the current process.
 *
 * Implemented for Linux, Windows, FreeBSD, netBSD and MACOSX.
 *
 * Return 0 on other platforms or in case of error.
 *
 * @since 3.12
 */

#ifdef __linux

#include "cpl_string.h"

#include <cstdio>
#include <string>

int CPLGetCurrentThreadCount()
{
    int nRet = 0;
    FILE *fp = fopen("/proc/self/stat", "rb");
    if (fp)
    {
        std::string osBuffer;
        osBuffer.resize(4096);
        const size_t nRead = fread(osBuffer.data(), 1, osBuffer.size(), fp);
        if (nRead > 0 && nRead < osBuffer.size())
        {
            osBuffer.resize(nRead);
            const auto nPos = osBuffer.find(')');
            if (nPos != std::string::npos)
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString2(osBuffer.c_str() + nPos + 1, " ", 0));
                if (aosTokens.size() >= 18)
                {
                    nRet = atoi(aosTokens[17]);
                }
            }
        }
        fclose(fp);
    }
    return nRet;
}

#elif defined(_WIN32)

#include <windows.h>
#include <tlhelp32.h>

int CPLGetCurrentThreadCount()
{
    int nRet = 0;

    const DWORD pid = GetCurrentProcessId();
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 te32;
        te32.dwSize = static_cast<int>(sizeof(THREADENTRY32));

        if (Thread32First(hThreadSnap, &te32))
        {
            do
            {
                if (te32.th32OwnerProcessID == pid)
                {
                    nRet++;
                }
            } while (Thread32Next(hThreadSnap, &te32));
        }

        CloseHandle(hThreadSnap);
    }
    return nRet;
}

#elif defined(__FreeBSD__)

#include <sys/types.h>
#include <sys/user.h>  // must be after sys/types.h
#include <sys/sysctl.h>
#include <unistd.h>

int CPLGetCurrentThreadCount()
{
    const pid_t pid = getpid();
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};

    struct kinfo_proc kp;
    size_t len = sizeof(kp);

    if (sysctl(mib, 4, &kp, &len, nullptr, 0) == -1)
    {
        return 0;
    }

    return kp.ki_numthreads;
}

#elif defined(__NetBSD__)

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/lwp.h>
#include <unistd.h>

int CPLGetCurrentThreadCount()
{
    const pid_t pid = getpid();
    int mib[5] = {CTL_KERN, KERN_PROC, static_cast<int>(pid),
                  static_cast<int>(sizeof(struct kinfo_lwp)), 0};

    size_t len = 0;
    if (sysctl(mib, 5, nullptr, &len, nullptr, 0) == -1)
    {
        return 0;
    }

    return static_cast<int>(len / sizeof(struct kinfo_lwp));
}

#elif defined(__APPLE__) && defined(__MACH__)

#include <mach/mach.h>

int CPLGetCurrentThreadCount()
{
    const mach_port_t task = mach_task_self();

    thread_act_array_t thread_list;
    mach_msg_type_number_t thread_count = 0;

    kern_return_t kr = task_threads(task, &thread_list, &thread_count);
    if (kr == KERN_SUCCESS)
    {
        for (mach_msg_type_number_t i = 0; i < thread_count; i++)
        {
            mach_port_deallocate(task, thread_list[i]);
        }

        vm_deallocate(task, reinterpret_cast<vm_address_t>(thread_list),
                      thread_count * sizeof(thread_t));
    }

    return static_cast<int>(thread_count);
}

#else

#include "cpl_error.h"

int CPLGetCurrentThreadCount()
{
    CPLDebugOnce(
        "CPL",
        "CPLGetCurrentThreadCount() unimplemented on this operating system");
    return 0;
}

#endif
