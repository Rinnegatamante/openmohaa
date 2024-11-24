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
