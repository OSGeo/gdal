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

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
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
#include "cpl_vsi.h"


#define BAD_MMAP (reinterpret_cast<void *>(-1))

static void uffd_cleanup(void * ptr);
static int64_t get_page_limit();
static void * fault_handler(void * ptr);

struct cpl_uffd_context {
  bool keep_going = true;
  int uffd = -1;
  struct uffdio_register uffdio_register = {};
  std::string filename = std::string("");
  off_t  file_size = 0;
  off_t  page_size = 0;
  void * page_ptr = nullptr;
  size_t vma_size = 0;
  void * vma_ptr = nullptr;
};


static void uffd_cleanup(void * ptr)
{
  struct cpl_uffd_context * ctx = static_cast<struct cpl_uffd_context *>(ptr);

  if (!ctx) return;

  if (ctx->uffd != -1) {
    ioctl(ctx->uffd, UFFDIO_UNREGISTER, &ctx->uffdio_register);
    close(ctx->uffd);
  }
  if (ctx->page_ptr && ctx->page_size)
    munmap(ctx->page_ptr, ctx->page_size);
  if (ctx->vma_ptr && ctx->vma_size)
    munmap(ctx->vma_ptr, ctx->vma_size);

  delete ctx;

  return;
}

static int64_t get_page_limit()
{
  int64_t retval;
  const char * variable = CPLGetConfigOption(GDAL_UFFD_LIMIT, nullptr);

  if (variable && sscanf(variable, "%" PRId64, &retval))
    return retval;
  else
    return -1;
}

static void * fault_handler(void * ptr)
{
  int64_t page_limit;
  int64_t pages_used;
  struct cpl_uffd_context * ctx = static_cast<struct cpl_uffd_context *>(ptr);
  struct uffd_msg msg;
  struct uffdio_copy uffdio_copy;
  struct pollfd pollfd;

  // Machinery to limit the number of physical + swap pages consumed
  page_limit = get_page_limit();
  pages_used = 0;

  // Register cleanup handler
  pthread_cleanup_push(uffd_cleanup, ctx);

  pollfd.fd = ctx->uffd;
  pollfd.events = POLLIN;

  // Open asset for reading
  VSILFILE * file = VSIFOpenL(ctx->filename.c_str(), "rb");

  if (!file) return nullptr;

  // Loop until told to stop
  while(ctx->keep_going) {
    uintptr_t fault_addr;
    uint64_t offset;
    off_t bytes_needed;

    // Poll for event
    if (poll(&pollfd, 1, 16) == -1) break; // 60Hz when no demand
    if ((pollfd.revents & POLLERR) || (pollfd.revents & POLLNVAL)) break;
    if (!(pollfd.revents & POLLIN)) continue;

    // Read page fault event
    if (read(ctx->uffd, &msg, sizeof(msg)) < 1) {
      if (errno == EWOULDBLOCK) continue;
      else break;
    }

    fault_addr = msg.arg.pagefault.address & ~(ctx->page_size-1);
    offset = static_cast<uint64_t>(fault_addr) - reinterpret_cast<uint64_t>(ctx->vma_ptr);
    bytes_needed = static_cast<off_t>(ctx->file_size - offset);
    if (bytes_needed > ctx->page_size) bytes_needed = ctx->page_size;

    // Limit number of pages
    if ((page_limit > 0) && (pages_used >= page_limit)) {
      pages_used = 0;
      ctx->page_ptr = mmap(ctx->page_ptr, ctx->page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
      if (ctx->page_ptr == BAD_MMAP) {
        ctx->page_ptr = nullptr;
        break;
      }
    }

    // Copy data into page
    if (VSIFSeekL(file, offset, SEEK_SET)) break;
    if (VSIFReadL(ctx->page_ptr, bytes_needed, 1, file) != 1) break;
    pages_used++;

    uffdio_copy.src = reinterpret_cast<uintptr_t>(ctx->page_ptr);
    uffdio_copy.dst = fault_addr;
    uffdio_copy.len = static_cast<uintptr_t>(ctx->page_size);
    uffdio_copy.mode = 0;
    uffdio_copy.copy = 0;
    if (ioctl(ctx->uffd, UFFDIO_COPY, &uffdio_copy) == -1) break;
  }

  // Return resources
  VSIFCloseL(file);

  pthread_cleanup_pop(1);

  return nullptr;
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
    return true;
}

/*
 * Returns nullptr on failure, a valid pointer on success.
 */
void * CPLCreateUserFaultMapping(const char * pszFilename, void ** ppVma, uint64_t * pnVmaSize)
{
  VSIStatBufL statbuf;
  pthread_t thread;
  struct cpl_uffd_context * ctx = nullptr;
  struct uffdio_api uffdio_api;

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
  ctx->filename = std::string(pszFilename);
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

  // Attempt to aquire a scratch page to use to fulfill requests.
  ctx->page_ptr = mmap(nullptr, ctx->page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (ctx->page_ptr == BAD_MMAP) {
    ctx->page_ptr = nullptr;
    uffd_cleanup(ctx);
    CPLError(CE_Failure, CPLE_AppDefined,
             "CPLCreateUserFaultMapping(): mmap() failed");
    return nullptr;
  }

  // Get non-blocking file descriptor
  if ((ctx->uffd = static_cast<int>(syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK))) == -1) {
    ctx->uffd = -1;
    uffd_cleanup(ctx);
    CPLError(CE_Failure, CPLE_AppDefined,
             "CPLCreateUserFaultMapping(): syscall(__NR_userfaultfd) failed");
    return nullptr;
  }

  // Query API
  uffdio_api.api = UFFD_API;
  uffdio_api.features = 0;
  if (ioctl(ctx->uffd, UFFDIO_API, &uffdio_api) == -1) {
    uffd_cleanup(ctx);
    CPLError(CE_Failure, CPLE_AppDefined,
             "CPLCreateUserFaultMapping(): ioctl(UFFDIO_API) failed");
    return nullptr;
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

  // Start handler
  if (pthread_create(&thread, nullptr, fault_handler, ctx) || pthread_detach(thread)) {
    uffd_cleanup(ctx);
    CPLError(CE_Failure, CPLE_AppDefined,
             "CPLCreateUserFaultMapping(): pthread_create() failed");
    return nullptr;
  }

  *ppVma = ctx->vma_ptr;
  *pnVmaSize = ctx->vma_size;
  return ctx;
}

void CPLDeleteUserFaultMapping(void * p)
{
  struct cpl_uffd_context * ctx = static_cast<struct cpl_uffd_context *>(p);
  if (ctx) ctx->keep_going = false;
}

#endif // ENABLE_UFFD
