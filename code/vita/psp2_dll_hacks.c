#include <stdint.h>
#include <vitasdk.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef CGAME_DLL
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../cgame/cg_public.h"
#else
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#define VITA_HACK
#include "../fgame/g_public.h"
#endif

void *__wrap_malloc(size_t size) {
	return SYS_MALLOC(size);
}

void *__wrap_realloc(void *ptr, size_t size) {
	return SYS_REALLOC(ptr, size);
}

void *__wrap_calloc(size_t number, size_t size) {
	return SYS_CALLOC(number, size);
}

void __wrap_free(void *ptr) {
	return SYS_FREE(ptr);
}

FILE *__wrap_fopen(char *fname, char *mode) {
	return SYS_FOPEN(fname, mode);
}

int __wrap_fclose(FILE *stream) {
	return SYS_FCLOSE(stream);
}

int __wrap_fseek(FILE *stream, long int offset, int origin) {
	return SYS_FSEEK(stream, offset, origin);
}

long int __wrap_ftell(FILE *stream) {
	return SYS_FTELL(stream);
}

int __wrap_fprintf(FILE *stream, const char *fmt, ...) {
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);
	
	return SYS_FPRINTF(stream, "%s", string);
}

size_t __wrap_fread(void *ptr, size_t size, size_t count, FILE *stream) {
	return SYS_FREAD(ptr, size, count, stream);
}

size_t __wrap_fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
	return SYS_FWRITE(ptr, size, count, stream);
}

int __wrap_sprintf(char *str, const char *fmt, ...) {
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);
	
	return SYS_SPRINTF(str, "%s", string);
}

int __wrap_snprintf(char *str, size_t n, const char *fmt, ...) {
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);
	
	return SYS_SNPRINTF(str, n, "%s", string);
}

int __wrap_vsnprintf(char *s, size_t n, const char *fmt, va_list arg) {
	return SYS_VSNPRINTF(s, n, fmt, arg);
}

int __wrap_mkdir(const char *pathname, mode_t mode) {
	return sceIoMkdir(pathname, mode);
}

int __wrap_printf(const char *fmt, ...) {
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);
	
	return sceClibPrintf("%s", string);
}

void* __dso_handle = (void*) &__dso_handle;

extern void _init_vita_reent( void );
extern void _free_vita_reent( void );
extern void _init_vita_heap( void );
extern void _free_vita_heap( void );

extern void __libc_init_array( void );
extern void __libc_fini_array( void );

void _init_vita_newlib( void )
{
	_init_vita_heap( );
	_init_vita_reent( );
}

void _free_vita_newlib( void )
{
	_free_vita_reent( );
	_free_vita_heap( );
}

void _fini( void ) { }
void _init( void ) { }

// small heap for internal libc use
unsigned int _newlib_heap_size_user = 2 * 1024 * 1024;

//////////////////////////////////////

typedef struct modarg_s
{
	sysfuncs_t imports;
	dllexport_t *exports;
} modarg_t;

#ifdef CGAME_DLL
extern clientGameExport_t *GetCGameAPI(void);
#else
extern game_export_t *GetGameAPI(game_import_t *import);
#endif

dllexport_t psp2_exports[] =
{
#ifdef CGAME_DLL
	{ "GetCGameAPI", (void*)GetCGameAPI},
#else
	{ "GetGameAPI", (void*)GetGameAPI},
#endif
	{ NULL, NULL },
};

int module_stop( SceSize argc, const void *args )
{
	__libc_fini_array( );
	_free_vita_newlib( );
	return SCE_KERNEL_STOP_SUCCESS;
}

int module_exit( )
{
	__libc_fini_array( );
	_free_vita_newlib( );
	return SCE_KERNEL_STOP_SUCCESS;
}

sysfuncs_t g_engsysfuncs;

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start( SceSize argc, void *args )
{
	modarg_t *arg = *(modarg_t **)args;
	arg->exports = psp2_exports;
	g_engsysfuncs = arg->imports;
	
	_init_vita_newlib( );
	__libc_init_array( );

	return SCE_KERNEL_START_SUCCESS;
}
