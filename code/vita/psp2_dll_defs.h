#pragma once

#ifndef _VITA_DEFS_H
#define _VITA_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vitasdk.h>

#include "psp2_dll_imports.h"

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#define _snprintf snprintf
#define _sprintf sprintf
#define _vsnprintf vsnprintf

typedef struct dllexport_s
{
	const char *name;
	void *func;
} dllexport_t;

extern dllexport_t psp2_exports[];

#ifdef __cplusplus
}
#endif

#endif
