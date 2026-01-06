/*
 * SHL - Library Log/Debug Interface (IRIX-compatible version)
 *
 * Copyright (c) 2010-2013 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 *
 * IRIX Compatibility Note:
 * This version removes ##__VA_ARGS__ which MIPSpro doesn't support.
 * Since all macros have a format parameter, the comma is always needed.
 */

#ifndef SHL_LLOG_H
#define SHL_LLOG_H

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

enum llog_severity {
	LLOG_FATAL = 0,
	LLOG_ALERT = 1,
	LLOG_CRITICAL = 2,
	LLOG_ERROR = 3,
	LLOG_WARNING = 4,
	LLOG_NOTICE = 5,
	LLOG_INFO = 6,
	LLOG_DEBUG = 7,
	LLOG_SEV_NUM,
};

typedef void (*llog_submit_t) (void *data,
			       const char *file,
			       int line,
			       const char *func,
			       const char *subs,
			       unsigned int sev,
			       const char *format,
			       va_list args);

/* Note: __attribute__ is defined as empty in config.h for IRIX */
static inline void llog_format(llog_submit_t llog,
		 void *data,
		 const char *file,
		 int line,
		 const char *func,
		 const char *subs,
		 unsigned int sev,
		 const char *format,
		 ...)
{
	int saved_errno = errno;
	va_list list;

	if (llog) {
		va_start(list, format);
		errno = saved_errno;
		llog(data, file, line, func, subs, sev, format, list);
		va_end(list);
	}
}

#ifndef LLOG_SUBSYSTEM
static const char *LLOG_SUBSYSTEM;
#endif

#define LLOG_DEFAULT __FILE__, __LINE__, __func__, LLOG_SUBSYSTEM

/* IRIX version: MIPSpro doesn't support ##__VA_ARGS__ */
/* Solution: Move format into __VA_ARGS__ so it's never empty */
#define llog_printf(obj, sev, ...) \
	llog_format((obj)->llog, \
		    (obj)->llog_data, \
		    LLOG_DEFAULT, \
		    (sev), \
		    __VA_ARGS__)
#define llog_dprintf(obj, data, sev, ...) \
	llog_format((obj), \
		    (data), \
		    LLOG_DEFAULT, \
		    (sev), \
		    __VA_ARGS__)

static inline void llog_dummyf(llog_submit_t llog, void *data, unsigned int sev,
		 const char *format, ...)
{
}

/*
 * Helpers
 * They pick up all the default values and submit the message to the
 * llog-subsystem. The llog_debug() function will discard the message unless
 * BUILD_ENABLE_DEBUG is defined.
 */

#ifdef BUILD_ENABLE_DEBUG
	#define llog_ddebug(obj, data, ...) \
		llog_dprintf((obj), (data), LLOG_DEBUG, __VA_ARGS__)
	#define llog_debug(obj, ...) \
		llog_ddebug((obj)->llog, (obj)->llog_data, __VA_ARGS__)
#else
	#define llog_ddebug(obj, data, ...) \
		llog_dummyf((obj), (data), LLOG_DEBUG, __VA_ARGS__)
	#define llog_debug(obj, ...) \
		llog_ddebug((obj)->llog, (obj)->llog_data, __VA_ARGS__)
#endif

#define llog_info(obj, ...) \
	llog_printf((obj), LLOG_INFO, __VA_ARGS__)
#define llog_dinfo(obj, data, ...) \
	llog_dprintf((obj), (data), LLOG_INFO, __VA_ARGS__)
#define llog_notice(obj, ...) \
	llog_printf((obj), LLOG_NOTICE, __VA_ARGS__)
#define llog_dnotice(obj, data, ...) \
	llog_dprintf((obj), (data), LLOG_NOTICE, __VA_ARGS__)
#define llog_warning(obj, ...) \
	llog_printf((obj), LLOG_WARNING, __VA_ARGS__)
#define llog_dwarning(obj, data, ...) \
	llog_dprintf((obj), (data), LLOG_WARNING, __VA_ARGS__)
#define llog_error(obj, ...) \
	llog_printf((obj), LLOG_ERROR, __VA_ARGS__)
#define llog_derror(obj, data, ...) \
	llog_dprintf((obj), (data), LLOG_ERROR, __VA_ARGS__)
#define llog_critical(obj, ...) \
	llog_printf((obj), LLOG_CRITICAL, __VA_ARGS__)
#define llog_dcritical(obj, data, ...) \
	llog_dprintf((obj), (data), LLOG_CRITICAL, __VA_ARGS__)
#define llog_alert(obj, ...) \
	llog_printf((obj), LLOG_ALERT, __VA_ARGS__)
#define llog_dalert(obj, data, ...) \
	llog_dprintf((obj), (data), LLOG_ALERT, __VA_ARGS__)
#define llog_fatal(obj, ...) \
	llog_printf((obj), LLOG_FATAL, __VA_ARGS__)
#define llog_dfatal(obj, data, ...) \
	llog_dprintf((obj), (data), LLOG_FATAL, __VA_ARGS__)

/*
 * Default log messages
 * These macros can be used to produce default log messages. You can use them
 * directly in an "return" statement. The "v" variants automatically cast the
 * result to void so it can be used in return statements inside of void
 * functions. The "d" variants use the logging object directly as the parent
 * might not exist, yet.
 *
 * Most of the messages work only if debugging is enabled. This is, because they
 * are used in debug paths and would slow down normal applications.
 */

#define llog_dEINVAL(obj, data) \
	(llog_derror((obj), (data), "invalid arguments"), -EINVAL)
#define llog_EINVAL(obj) \
	(llog_dEINVAL((obj)->llog, (obj)->llog_data))
#define llog_vEINVAL(obj) \
	((void)llog_EINVAL(obj))
#define llog_vdEINVAL(obj, data) \
	((void)llog_dEINVAL((obj), (data)))

#define llog_dEFAULT(obj, data) \
	(llog_derror((obj), (data), "internal operation failed"), -EFAULT)
#define llog_EFAULT(obj) \
	(llog_dEFAULT((obj)->llog, (obj)->llog_data))
#define llog_vEFAULT(obj) \
	((void)llog_EFAULT(obj))
#define llog_vdEFAULT(obj, data) \
	((void)llog_dEFAULT((obj), (data)))

#define llog_dENOMEM(obj, data) \
	(llog_derror((obj), (data), "out of memory"), -ENOMEM)
#define llog_ENOMEM(obj) \
	(llog_dENOMEM((obj)->llog, (obj)->llog_data))
#define llog_vENOMEM(obj) \
	((void)llog_ENOMEM(obj))
#define llog_vdENOMEM(obj, data) \
	((void)llog_dENOMEM((obj), (data)))

#define llog_dEPIPE(obj, data) \
	(llog_derror((obj), (data), "fd closed unexpectedly"), -EPIPE)
#define llog_EPIPE(obj) \
	(llog_dEPIPE((obj)->llog, (obj)->llog_data))
#define llog_vEPIPE(obj) \
	((void)llog_EPIPE(obj))
#define llog_vdEPIPE(obj, data) \
	((void)llog_dEPIPE((obj), (data)))

#define llog_dERRNO(obj, data) \
	(llog_derror((obj), (data), "syscall failed (%d): %m", errno), -errno)
#define llog_ERRNO(obj) \
	(llog_dERRNO((obj)->llog, (obj)->llog_data))
#define llog_vERRNO(obj) \
	((void)llog_ERRNO(obj))
#define llog_vdERRNO(obj, data) \
	((void)llog_dERRNO((obj), (data)))

#define llog_dERR(obj, data, _r) \
	(errno = -(_r), llog_derror((obj), (data), "syscall failed (%d): %m", (_r)), (_r))
#define llog_ERR(obj, _r) \
	(llog_dERR((obj)->llog, (obj)->llog_data, (_r)))
#define llog_vERR(obj, _r) \
	((void)llog_ERR((obj), (_r)))
#define llog_vdERR(obj, data, _r) \
	((void)llog_dERR((obj), (data), (_r)))

#endif /* SHL_LLOG_H */
