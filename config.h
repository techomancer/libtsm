/* SPDX-License-Identifier: MIT */
/* config.h for IRIX build */

#ifndef CONFIG_H
#define CONFIG_H

/* Disable extra debugging by default */
/* #undef BUILD_ENABLE_DEBUG */

/* MIPSpro C99 compatibility */
/* Always define __attribute__ as empty for IRIX/MIPSpro builds */
/* MIPSpro doesn't support GCC attributes at all */
#ifndef __attribute__
#define __attribute__(x)
#endif

#ifdef __sgi
/* Additional SGI/IRIX compatibility defines */

#ifndef COLD
#define COLD
#endif

/* MIPSpro C99 mode uses __inline instead of inline */
#ifndef inline
#define inline __inline
#endif

/* MIPSpro doesn't support ##__VA_ARGS__ properly in C99 mode */
/* We'll rely on the fact that all llog macros in libtsm always have at least format */
/* So we don't need the ## comma-removal trick */
#endif

/* __func__ support for MIPSpro C99 */
#ifndef __func__
#define __func__ __FUNCTION__
#endif

#endif /* CONFIG_H */
