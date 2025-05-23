/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"


/*

  for a projection shadow:

  point[x] += light vector * ( z - shadow plane )
  point[y] +=
  point[z] = shadow plane

  1 0 light[x] / light[z]

*/

typedef struct {
	int		i2;
	int		facing;
} edgeDef_t;

#define	MAX_EDGE_DEFS	32

static	edgeDef_t	edgeDefs[SHADER_MAX_VERTEXES][MAX_EDGE_DEFS];
static	int			numEdgeDefs[SHADER_MAX_VERTEXES];
static	int			facing[SHADER_MAX_INDEXES/3];

void R_RenderShadowEdges( void ) {
	int		i;

#if 0
	int		numTris;

	// dumb way -- render every triangle's edges
	numTris = tess.numIndexes / 3;

	for ( i = 0 ; i < numTris ; i++ ) {
		int		i1, i2, i3;

		if ( !facing[i] ) {
			continue;
		}

		i1 = tess.indexes[ i*3 + 0 ];
		i2 = tess.indexes[ i*3 + 1 ];
		i3 = tess.indexes[ i*3 + 2 ];

		qglBegin( GL_TRIANGLE_STRIP );
		qglVertex3fv( tess.xyz[ i1 ] );
		qglVertex3fv( tess.xyz[ i1 + tess.numVertexes ] );
		qglVertex3fv( tess.xyz[ i2 ] );
		qglVertex3fv( tess.xyz[ i2 + tess.numVertexes ] );
		qglVertex3fv( tess.xyz[ i3 ] );
		qglVertex3fv( tess.xyz[ i3 + tess.numVertexes ] );
		qglVertex3fv( tess.xyz[ i1 ] );
		qglVertex3fv( tess.xyz[ i1 + tess.numVertexes ] );
		qglEnd();
	}
#else
	int		c, c2;
	int		j, k;
	int		i2;
	int		c_edges, c_rejected;
	int		hit[2];

	// an edge is NOT a silhouette edge if its face doesn't face the light,
	// or if it has a reverse paired edge that also faces the light.
	// A well behaved polyhedron would have exactly two faces for each edge,
	// but lots of models have dangling edges or overfanned edges
	c_edges = 0;
	c_rejected = 0;

	for ( i = 0 ; i < tess.numVertexes ; i++ ) {
		c = numEdgeDefs[ i ];
		for ( j = 0 ; j < c ; j++ ) {
			if ( !edgeDefs[ i ][ j ].facing ) {
				continue;
			}

			hit[0] = 0;
			hit[1] = 0;

			i2 = edgeDefs[ i ][ j ].i2;
			c2 = numEdgeDefs[ i2 ];
			for ( k = 0 ; k < c2 ; k++ ) {
				if ( edgeDefs[ i2 ][ k ].i2 == i ) {
					hit[ edgeDefs[ i2 ][ k ].facing ]++;
				}
			}

			// if it doesn't share the edge with another front facing
			// triangle, it is a sil edge
			if ( hit[ 1 ] == 0 ) {
#ifdef __vita__
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				glDisableClientState(GL_COLOR_ARRAY);
				float *vertices = gVertexBuffer;
				sceClibMemcpy(gVertexBuffer, tess.xyz[ i ], sizeof(vec3_t));
				gVertexBuffer += 3;
				sceClibMemcpy(gVertexBuffer, tess.xyz[ i + tess.numVertexes ], sizeof(vec3_t));
				gVertexBuffer += 3;
				sceClibMemcpy(gVertexBuffer, tess.xyz[ i2 ], sizeof(vec3_t));
				gVertexBuffer += 3;
				sceClibMemcpy(gVertexBuffer, tess.xyz[ i2 + tess.numVertexes ], sizeof(vec3_t));
				gVertexBuffer += 3;
#if 1
				if (gVertexBuffer > ((uint8_t *)gVertexBufferPtr + 0x100000)) {
					printf("R_RenderShadowEdges: vertexBufferOffset: 0x%x\n", gVertexBuffer - gVertexBufferPtr);
				}
#endif
				vglVertexPointerMapped(3, vertices);
				vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
#else
				qglBegin( GL_TRIANGLE_STRIP );
				qglVertex3fv( tess.xyz[ i ] );
				qglVertex3fv( tess.xyz[ i + tess.numVertexes ] );
				qglVertex3fv( tess.xyz[ i2 ] );
				qglVertex3fv( tess.xyz[ i2 + tess.numVertexes ] );
				qglEnd();
#endif
				c_edges++;
			} else {
				c_rejected++;
			}
		}
	}
#endif
}

void R_AddEdgeDef(int i1, int i2, int facing) {
	int		c;

	c = numEdgeDefs[i1];
	if (c == MAX_EDGE_DEFS) {
		return;		// overflow
	}
	edgeDefs[i1][c].i2 = i2;
	edgeDefs[i1][c].facing = facing;

	numEdgeDefs[i1]++;
}

/*
=================
RB_ShadowTessEnd

triangleFromEdge[ v1 ][ v2 ]


  set triangle from edge( v1, v2, tri )
  if ( facing[ triangleFromEdge[ v1 ][ v2 ] ] && !facing[ triangleFromEdge[ v2 ][ v1 ] ) {
  }
=================
*/
void RB_ShadowTessEnd( void ) {
	int		i;
	int		numTris;
	vec3_t	lightDir;

	// we can only do this if we have enough space in the vertex buffers
	if ( tess.numVertexes >= SHADER_MAX_VERTEXES / 2 ) {
		return;
	}

	if ( glConfig.stencilBits < 4 ) {
		return;
	}

	VectorCopy( backEnd.currentEntity->lightDir, lightDir );

	// project vertexes away from light direction
	for ( i = 0 ; i < tess.numVertexes ; i++ ) {
		VectorMA( tess.xyz[i], -512, lightDir, tess.xyz[i+tess.numVertexes] );
	}

	// decide which triangles face the light
	Com_Memset( numEdgeDefs, 0, 4 * tess.numVertexes );

	numTris = tess.numIndexes / 3;
	for ( i = 0 ; i < numTris ; i++ ) {
		int		i1, i2, i3;
		vec3_t	d1, d2, normal;
		float	*v1, *v2, *v3;
		float	d;

		i1 = tess.indexes[ i*3 + 0 ];
		i2 = tess.indexes[ i*3 + 1 ];
		i3 = tess.indexes[ i*3 + 2 ];

		v1 = tess.xyz[ i1 ];
		v2 = tess.xyz[ i2 ];
		v3 = tess.xyz[ i3 ];

		VectorSubtract( v2, v1, d1 );
		VectorSubtract( v3, v1, d2 );
		CrossProduct( d1, d2, normal );

		d = DotProduct( normal, lightDir );
		if ( d > 0 ) {
			facing[ i ] = 1;
		} else {
			facing[ i ] = 0;
		}

		// create the edges
		R_AddEdgeDef( i1, i2, facing[ i ] );
		R_AddEdgeDef( i2, i3, facing[ i ] );
		R_AddEdgeDef( i3, i1, facing[ i ] );
	}

	// draw the silhouette edges

	GL_Bind( tr.whiteImage );
	qglEnable( GL_CULL_FACE );
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	qglColor3f( 0.2f, 0.2f, 0.2f );

	// don't write to the color buffer
	qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

	qglEnable( GL_STENCIL_TEST );
	qglStencilFunc( GL_ALWAYS, 1, 255 );

	// mirrors have the culling order reversed
	if ( backEnd.viewParms.isMirror ) {
		qglCullFace( GL_FRONT );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );

		R_RenderShadowEdges();

		qglCullFace( GL_BACK );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_DECR );

		R_RenderShadowEdges();
	} else {
		qglCullFace( GL_BACK );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );

		R_RenderShadowEdges();

		qglCullFace( GL_FRONT );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_DECR );

		R_RenderShadowEdges();
	}


	// reenable writing to the color buffer
	qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
}

/*
=================
RB_ComputeShadowVolume

=================
*/
void RB_ComputeShadowVolume() {
    int i;
    int numTris;
    vec3_t lightDir;

    if (r_shadows->integer != 3 || glConfig.stencilBits < 8) {
        return;
    }

    VectorSet(lightDir, -2.0, 1.5, 1.0);

    // project vertexes away from light direction
    for (i = 0; i < tess.numVertexes; i++) {
        VectorMA(tess.xyz[i], -512, lightDir, tess.xyz[i + tess.numVertexes]);
    }

    // decide which triangles face the light
    Com_Memset(numEdgeDefs, 0, 4 * tess.numVertexes);

    numTris = tess.numIndexes / 3;
    for (i = 0; i < numTris; i++) {
        int		i1, i2, i3;
        vec3_t	d1, d2, normal;
        float* v1, * v2, * v3;
        float	d;

        i1 = tess.indexes[i * 3 + 0];
        i2 = tess.indexes[i * 3 + 1];
        i3 = tess.indexes[i * 3 + 2];

        v1 = tess.xyz[i1];
        v2 = tess.xyz[i2];
        v3 = tess.xyz[i3];

        VectorSubtract(v2, v1, d1);
        VectorSubtract(v3, v1, d2);
        CrossProduct(d1, d2, normal);

        d = DotProduct(normal, lightDir);
        if (d > 0) {
            facing[i] = 1;
        } else {
            facing[i] = 0;
        }

        // create the edges
        R_AddEdgeDef(i1, i2, facing[i]);
        R_AddEdgeDef(i2, i3, facing[i]);
        R_AddEdgeDef(i3, i1, facing[i]);
    }

	// draw the silhouette edges

	GL_Bind( tr.whiteImage );
	qglEnable( GL_CULL_FACE );
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	qglColor3f( 0.2f, 0.2f, 0.2f );

	// don't write to the color buffer
	qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

	qglEnable( GL_STENCIL_TEST );
	qglStencilFunc( GL_ALWAYS, 1, 255 );

	// mirrors have the culling order reversed
	if ( backEnd.viewParms.isMirror ) {
		qglCullFace( GL_FRONT );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );

		R_RenderShadowEdges();

		qglCullFace( GL_BACK );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_DECR );

		R_RenderShadowEdges();
	} else {
		qglCullFace( GL_BACK );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );

		R_RenderShadowEdges();

		qglCullFace( GL_FRONT );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_DECR );

		R_RenderShadowEdges();
	}


	// reenable writing to the color buffer
	qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
}

/*
=================
RB_ShadowFinish

Darken everything that is is a shadow volume.
We have to delay this until everything has been shadowed,
because otherwise shadows from different body parts would
overlap and double darken.
=================
*/
void RB_ShadowFinish( void ) {
	if ( r_shadows->integer != 2 ) {
		return;
	}
	if ( glConfig.stencilBits < 4 ) {
		return;
	}
	qglEnable( GL_STENCIL_TEST );
	qglStencilFunc( GL_NOTEQUAL, 0, 255 );

	qglDisable (GL_CLIP_PLANE0);
	qglDisable (GL_CULL_FACE);

	GL_Bind( tr.whiteImage );

    qglLoadIdentity ();

	qglColor3f( 0.6f, 0.6f, 0.6f );
	GL_State( GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO );

//	qglColor3f( 1, 0, 0 );
//	GL_State( GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
#ifdef __vita__
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	float vertex[] = {
		-100, 100, -10,
		100, 100, -10,
		100, -100, -10,
		-100, -100, -10
	};
	vglVertexPointer(3, GL_FLOAT, 0, 4, vertex);
	vglDrawObjects(GL_TRIANGLE_FAN, 4, GL_TRUE);
#else
	qglBegin( GL_QUADS );
	qglVertex3f( -100, 100, -10 );
	qglVertex3f( 100, 100, -10 );
	qglVertex3f( 100, -100, -10 );
	qglVertex3f( -100, -100, -10 );
	qglEnd ();
#endif
	qglColor4f(1,1,1,1);
	qglDisable( GL_STENCIL_TEST );
}
