/*
 * $Id: debug.h,v 1.5 2006/01/30 23:07:57 mclark Exp $
 *
 * Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
 * Michael Clark <michael@metaparadigm.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING for details.
 *
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void mc_set_debug(int debug);
extern int mc_get_debug(void);

extern void mc_set_syslog(int syslog);
extern void mc_abort(const char *msg, ...);
extern void mc_debug(const char *msg, ...);
extern void mc_error(const char *msg, ...);
extern void mc_info(const char *msg, ...);

#ifdef MC_MAINTAINER_MODE
#define MC_SET_DEBUG(x) mc_set_debug(x)
#define MC_GET_DEBUG() mc_get_debug()
#define MC_SET_SYSLOG(x) mc_set_syslog(x)
#define MC_ABORT(x, ...) mc_abort(x, ##__VA_ARGS__)
#define MC_DEBUG(x, ...) mc_debug(x, ##__VA_ARGS__)
#define MC_ERROR(x, ...) mc_error(x, ##__VA_ARGS__)
#define MC_INFO(x, ...) mc_info(x, ##__VA_ARGS__)
#else
#define MC_SET_DEBUG(x) if (0) mc_set_debug(x)
#define MC_GET_DEBUG() (0)
#define MC_SET_SYSLOG(x) if (0) mc_set_syslog(x)
#if defined(_MSC_VER) && _MSC_VER <= 1200
/* VC++6 doesn't like macros with ... */
#define MC_ABORT
#define MC_DEBUG
#define MC_ERROR
#define MC_INFO
#else
#define MC_ABORT(x, ...) if (0) mc_abort(x, ##__VA_ARGS__)
#define MC_DEBUG(x, ...) if (0) mc_debug(x, ##__VA_ARGS__)
#define MC_ERROR(x, ...) if (0) mc_error(x, ##__VA_ARGS__)
#define MC_INFO(x, ...) if (0) mc_info(x, ##__VA_ARGS__)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
