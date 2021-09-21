/******************************************************************************
 *
 * Name:     cpl_userfaultfd.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Use userfaultfd and VSIL to service page faults
 * Author:   James McClain, <james.mcclain@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Dr. James McClain <james.mcclain@gmail.com>
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

#ifdef ENABLE_UFFD

#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <linux/userfaultfd.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_userfaultfd.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_multiproc.h"

#ifndef UFFD_USER_MODE_ONLY
// The UFFD_USER_MODE_ONLY flag got added in kernel 5.11 which is the one
// used by Ubuntu 20.04, but the linux-libc-dev package corresponds to 5.4
#define UFFD_USER_MODE_ONLY 1
#endif

#define BAD_MMAP (reinterpret_cast<void *>(-1))
#define MAX_MESSAGES (0x100)

static int64_t get_page_limit();
static void cpl_uffd_fault_handler(void * ptr);
static void signal_handler(int signal);
static void uffd_cleanup(void * ptr);

struct cpl_uffd_context {
  bool keep_going = false;

  int uffd = -1;
  struct uffdio_register uffdio_register = {};
  struct uffd_msg uffd_msgs[MAX_MESSAGES];

  std::string filename = std::string("");

  int64_t page_limit = -1;
  int64_t pages_used = 0;

  off_t  file_size = 0;
  off_t  page_size = 0;
  void * page_ptr = nullptr;
  size_t vma_size = 0;
  void * vma_ptr = nullptr;
  CPLJoinableThread* thread = nullptr;
};


static void uffd_cleanup(void * ptr)
{
  struct cpl_uffd_context * ctx = static_cast<struct cpl_uffd_context *>(ptr);

  if (!ctx) return;

  // Signal shutdown
  ctx->keep_going = false;
  if( ctx->thread )
  {
      CPLJoinThread(ctx->thread);
      ctx->thread = nullptr;
  }

  if (ctx->uffd != -1) {
    ioctl(ctx->uffd, UFFDIO_UNREGISTER, &ctx->uffdio_register);
    close(ctx->uffd);
    ctx->uffd = -1;
  }
  if (ctx->page_ptr && ctx->page_size)
    munmap(ctx->page_ptr, ctx->page_size);
  if (ctx->vma_ptr && ctx->vma_size)
    munmap(ctx->vma_ptr, ctx->vma_size);
  ctx->page_ptr = nullptr;
  ctx->vma_ptr = nullptr;
  ctx->page_size = 0;
  ctx->vma_size = 0;
  ctx->pages_used = 0;
  ctx->page_limit = 0;

  delete ctx;

  return;
}

#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic pop
#endif

static int64_t get_page_limit()
{
  int64_t retval;
  const char * variable = CPLGetConfigOption(GDAL_UFFD_LIMIT, nullptr);

  if (variable && sscanf(variable, "%" PRId64, &retval))
    return retval;
  else
    return -1;
}

static void cpl_uffd_fault_handler(void * ptr)
{
  struct cpl_uffd_context * ctx = static_cast<struct cpl_uffd_context *>(ptr);
  struct uffdio_copy uffdio_copy;
  struct pollfd pollfd;

  // Setup pollfd structure
  pollfd.fd = ctx->uffd;
  pollfd.events = POLLIN;

  // Open asset for reading
  VSILFILE * file = VSIFOpenL(ctx->filename.c_str(), "rb");

  if (!file) return;

  // Loop until told to stop
  while(ctx->keep_going) {
    uintptr_t fault_addr;
    uint64_t offset;
    off_t bytes_needed;
    ssize_t bytes_read;

    // Poll for event
    if (poll(&pollfd, 1, 16) == -1) break; // 60Hz when no demand
    if ((pollfd.revents & POLLERR) || (pollfd.revents & POLLNVAL)) break;
    if (!(pollfd.revents & POLLIN)) continue;

    // Read page fault events
    bytes_read = static_cast<ssize_t>(read(ctx->uffd, ctx->uffd_msgs, MAX_MESSAGES*sizeof(uffd_msg)));
    if (bytes_read < 1) {
      if (errno == EWOULDBLOCK) continue;
      else break;
    }

    // If too many pages are in use, evict all pages (evict them from
    // RAM and swap, not just to swap).  It is impossible to control
    // which/when threads access the VMA, so access to the VMA has to
    // forbidden while the activity is in progress.
    //
    // That is done by (1) installing special handlers for SIGSEGV and
    // SIGBUS, (2) mprotecting the VMA so that any threads accessing
    // it receive either SIGSEGV or SIGBUS (which one is apparently a
    // function of the C library, at least on one non-Linux GNU
    // system[1]), (3) unregistering the VMA from userfaultfd,
    // remapping the VMA to evict the pages, registering the VMA
    // again, (4) making the VMA accessible again, and finally (5)
    // restoring the previous signal-handling behavior.
    //
    // [1] https://lists.debian.org/debian-bsd/2011/05/msg00032.html
    if (ctx->page_limit > 0) {
        pthread_mutex_lock(&mutex);
        if (ctx->pages_used > ctx->page_limit) {
            struct sigaction segv;
            struct sigaction old_segv;
            struct sigaction bus;
            struct sigaction old_bus;

            memset(&segv, 0, sizeof(segv));
            memset(&old_segv, 0, sizeof(old_segv));
            memset(&bus, 0, sizeof(bus));
            memset(&old_bus, 0, sizeof(old_bus));

            // Step 1 from the block comment above
            segv.sa_handler = signal_handler;
            bus.sa_handler = signal_handler;
            if (sigaction(SIGSEGV, &segv, &old_segv) == -1) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: sigaction(SIGSEGV) failed");
                pthread_mutex_unlock(&mutex);
                break;
            }
            if (sigaction(SIGBUS, &bus, &old_bus) == -1) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: sigaction(SIGBUS) failed");
                pthread_mutex_unlock(&mutex);
                break;
            }

            // WARNING: LACK OF THREAD-SAFETY.
            //
            // For example, if a user program (or another part of the
            // library) installs a SIGSEGV or SIGBUS handler from another
            // thread after this one has installed its handlers but before
            // this one uninstalls its handlers, the intervening handler
            // will be eliminated.  There are other examples, as well, but
            // there can only be a problems with other threads because the
            // faulting thread is blocked here.
            //
            // This implies that one should not use cpl_virtualmem.h API
            // while other threads are actively generating faults that use
            // this mechanism.
            //
            // Having multiple active threads that use this mechanism but
            // with no changes to signal-handling in other threads is NOT a
            // problem.

            // Step 2
            if (mprotect(ctx->vma_ptr, ctx->vma_size, PROT_NONE) == -1) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: mprotect() failed");
                pthread_mutex_unlock(&mutex);
                break;
            }

            // Step 3
            if (ioctl(ctx->uffd, UFFDIO_UNREGISTER, &ctx->uffdio_register)) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: ioctl(UFFDIO_UNREGISTER) failed");
                pthread_mutex_unlock(&mutex);
                break;
            }
            ctx->vma_ptr = mmap(ctx->vma_ptr, ctx->vma_size, PROT_NONE,
                                MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
            if (ctx->vma_ptr == BAD_MMAP) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: mmap() failed");
                ctx->vma_ptr = nullptr;
                pthread_mutex_unlock(&mutex);
                break;
            }
            ctx->pages_used = 0;
            if (ioctl(ctx->uffd, UFFDIO_REGISTER, &ctx->uffdio_register)) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: ioctl(UFFDIO_REGISTER) failed");
                pthread_mutex_unlock(&mutex);
                break;
            }

            // Step 4.  Problem: A thread might attempt to read here (before
            // the mprotect) and receive a SIGSEGV or SIGBUS.
            if (mprotect(ctx->vma_ptr, ctx->vma_size, PROT_READ) == -1) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: mprotect() failed");
                pthread_mutex_unlock(&mutex);
                break;
            }

            // Step 5.  Solution: Cannot unregister special handlers before
            // any such threads have been handled by them, so sleep for
            // 1/100th of a second.
            // Coverity complains about sleeping under a mutex
            // coverity[sleep]
            usleep(10000);
            if (sigaction(SIGSEGV, &old_segv, nullptr) == -1) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: sigaction(SIGSEGV) failed");
                pthread_mutex_unlock(&mutex);
                break;
            }
            if (sigaction(SIGBUS, &old_bus, nullptr) == -1) {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "cpl_uffd_fault_handler: sigaction(SIGBUS) failed");
                pthread_mutex_unlock(&mutex);
                break;
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    // Handle page fault events
    for (int i = 0; i < static_cast<int>(bytes_read/sizeof(uffd_msg)); ++i) {
      fault_addr = ctx->uffd_msgs[i].arg.pagefault.address & ~(ctx->page_size-1);
      offset = static_cast<uint64_t>(fault_addr) - reinterpret_cast<uint64_t>(ctx->vma_ptr);
      bytes_needed = static_cast<off_t>(ctx->file_size - offset);
      if (bytes_needed > ctx->page_size) bytes_needed = ctx->page_size;

      // Copy data into page
      if (VSIFSeekL(file, offset, SEEK_SET)) break;
      if (VSIFReadL(ctx->page_ptr, bytes_needed, 1, file) != 1) break;
      ctx->pages_used++;

      // Use the page to fulfill the page fault
      uffdio_copy.src = reinterpret_cast<uintptr_t>(ctx->page_ptr);
      uffdio_copy.dst = fault_addr;
      uffdio_copy.len = static_cast<uintptr_t>(ctx->page_size);
      uffdio_copy.mode = 0;
      uffdio_copy.copy = 0;
      if (ioctl(ctx->uffd, UFFDIO_COPY, &uffdio_copy) == -1) break;
    }
  } // end of while loop

  // Return resources
  VSIFCloseL(file);
}

static void signal_handler(int signal)
{
  if (signal == SIGSEGV || signal == SIGBUS)
    sched_yield();
  return;
}

bool CPLIsUserFaultMappingSupported()
{
  // Check the Linux kernel version.  Linux 4.3 or newer is needed for
  // userfaultfd.
  int major = 0, minor = 0;
  struct utsname utsname;

  if (uname(&utsname)) return false;
  sscanf(utsname.release, "%d.%d", &major, &minor);
  if (major < 4) return false;
  if (major == 4 && minor < 3) return false;

  static int nEnableUserFaultFD = -1;
  if( nEnableUserFaultFD < 0 )
  {
      nEnableUserFaultFD =
        CPLTestBool(CPLGetConfigOption("CPL_ENABLE_USERFAULTFD", "YES"));
  }

  return nEnableUserFaultFD != FALSE;
}

/*
 * Returns nullptr on failure, a valid pointer on success.
 */
cpl_uffd_context* CPLCreateUserFaultMapping(const char * pszFilename, void ** ppVma, uint64_t * pnVmaSize)
{
  VSIStatBufL statbuf;
  struct cpl_uffd_context * ctx = nullptr;

  if( !CPLIsUserFaultMappingSupported() )
  {
      CPLError(CE_Failure, CPLE_NotSupported,
               "CPLCreateUserFaultMapping(): Linux kernel 4.3 or newer needed");
      return nullptr;
  }

  // Get the size of the asset
  if (VSIStatL(pszFilename, &statbuf)) return nullptr;

  // Setup the `cpl_uffd_context` struct
  ctx = new cpl_uffd_context();
  ctx->keep_going = true;
  ctx->filename = std::string(pszFilename);
  ctx->page_limit = get_page_limit();
  ctx->pages_used = 0;
  ctx->file_size = static_cast<off_t>(statbuf.st_size);
  ctx->page_size = static_cast<off_t>(sysconf(_SC_PAGESIZE));
  ctx->vma_size = static_cast<size_t>(((statbuf.st_size/ctx->page_size)+1) * ctx->page_size);
  if (ctx->vma_size < static_cast<size_t>(statbuf.st_size)) { // Check for overflow
    uffd_cleanup(ctx);
    CPLError(CE_Failure, CPLE_AppDefined,
             "CPLCreateUserFaultMapping(): File too large for architecture");
    return nullptr;
  }

  // If the mmap failed, free resources and return
  ctx->vma_ptr = mmap(nullptr, ctx->vma_size, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (ctx->vma_ptr == BAD_MMAP) {
    ctx->vma_ptr = nullptr;
    uffd_cleanup(ctx);
    CPLError(CE_Failure, CPLE_AppDefined,
             "CPLCreateUserFaultMapping(): mmap() failed");
    return nullptr;
  }

  // Attempt to acquire a scratch page to use to fulfill requests.
  ctx->page_ptr = mmap(nullptr, static_cast<size_t>(ctx->page_size), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (ctx->page_ptr == BAD_MMAP) {
    ctx->page_ptr = nullptr;
    uffd_cleanup(ctx);
    CPLError(CE_Failure, CPLE_AppDefined,
             "CPLCreateUserFaultMapping(): mmap() failed");
    return nullptr;
  }

  // Get userfaultfd

  // Since kernel 5.2, raw userfaultfd is disabled since if the fault originates
  // from the kernel, that could lead to easier exploitation of kernel bugs.
  // Since kernel 5.11, UFFD_USER_MODE_ONLY can be used to restrict the mechanism
  // to faults occuring only from user space, which is likely to be our use case.
  ctx->uffd = static_cast<int>(syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY));
  if( ctx->uffd == -1 && errno == EINVAL )
      ctx->uffd = static_cast<int>(syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK));
  if( ctx->uffd == -1 )
  {
    const int l_errno = errno;
    ctx->uffd = -1;
    uffd_cleanup(ctx);
    if( l_errno == EPERM )
    {
        // Since kernel 5.2
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CPLCreateUserFaultMapping(): syscall(__NR_userfaultfd) failed: "
                 "insufficient permission. add CAP_SYS_PTRACE capability, or "
                 "set /proc/sys/vm/unprivileged_userfaultfd to 1");
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CPLCreateUserFaultMapping(): syscall(__NR_userfaultfd) failed: "
                 "error = %d", l_errno);
    }
    return nullptr;
  }

  // Query API
  {
    struct uffdio_api uffdio_api = {};

    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;

    if (ioctl(ctx->uffd, UFFDIO_API, &uffdio_api) == -1) {
      uffd_cleanup(ctx);
      CPLError(CE_Failure, CPLE_AppDefined,
               "CPLCreateUserFaultMapping(): ioctl(UFFDIO_API) failed");
      return nullptr;
    }
  }

  // Register memory range
  ctx->uffdio_register.range.start = reinterpret_cast<uintptr_t>(ctx->vma_ptr);
  ctx->uffdio_register.range.len = ctx->vma_size;
  ctx->uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;

  if (ioctl(ctx->uffd, UFFDIO_REGISTER, &ctx->uffdio_register) == -1) {
    uffd_cleanup(ctx);
    CPLError(CE_Failure, CPLE_AppDefined,
             "CPLCreateUserFaultMapping(): ioctl(UFFDIO_REGISTER) failed");
    return nullptr;
  }

  // Start handler thread
  ctx->thread = CPLCreateJoinableThread(cpl_uffd_fault_handler, ctx);
  if( ctx->thread == nullptr )
  {
      CPLError(CE_Failure, CPLE_AppDefined,
               "CPLCreateUserFaultMapping(): CPLCreateJoinableThread() failed");
      uffd_cleanup(ctx);
      return nullptr;
  }

  *ppVma = ctx->vma_ptr;
  *pnVmaSize = ctx->vma_size;
  return ctx;
}

void CPLDeleteUserFaultMapping(cpl_uffd_context * ctx)
{
  if (ctx)
  {
      uffd_cleanup(ctx);
  }
}

#endif // ENABLE_UFFD
