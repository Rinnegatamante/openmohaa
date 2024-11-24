#include <new>

extern sysfuncs_t g_engsysfuncs;

void * operator new( std::size_t n )
{
	return g_engsysfuncs.pfnSysMalloc( n );
}

void operator delete( void *p )
{
	g_engsysfuncs.pfnSysFree( p );
}
