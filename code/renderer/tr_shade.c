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
// tr_shade.c

#include "tr_local.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

/*
================
R_ArrayElementDiscrete

This is just for OpenGL conformance testing, it should never be the fastest
================
*/
static void APIENTRY R_ArrayElementDiscrete( GLint index ) {
	qglColor4ubv( tess.svars.colors[ index ] );
	if ( glState.currenttmu ) {
		qglMultiTexCoord2fARB( 0, tess.svars.texcoords[ 0 ][ index ][0], tess.svars.texcoords[ 0 ][ index ][1] );
		qglMultiTexCoord2fARB( 1, tess.svars.texcoords[ 1 ][ index ][0], tess.svars.texcoords[ 1 ][ index ][1] );
	} else {
		qglTexCoord2fv( tess.svars.texcoords[ 0 ][ index ] );
	}
	qglVertex3fv( tess.xyz[ index ] );
}

/*
===================
R_DrawStripElements

===================
*/
static int		c_vertexes;		// for seeing how long our average strips are
static int		c_begins;
static void R_DrawStripElements( int numIndexes, const glIndex_t *indexes, void ( APIENTRY *element )(GLint) ) {
	int i;
	int last[3] = { -1, -1, -1 };
	qboolean even;

	c_begins++;

	if ( numIndexes <= 0 ) {
		return;
	}

	qglBegin( GL_TRIANGLE_STRIP );

	// prime the strip
	element( indexes[0] );
	element( indexes[1] );
	element( indexes[2] );
	c_vertexes += 3;

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	even = qfalse;

	for ( i = 3; i < numIndexes; i += 3 )
	{
		// odd numbered triangle in potential strip
		if ( !even )
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( indexes[i+0] == last[2] ) && ( indexes[i+1] == last[1] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;
				assert( indexes[i+2] < tess.numVertexes );
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );

				c_vertexes += 3;

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( last[2] == indexes[i+1] ) && ( last[0] == indexes[i+0] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;

				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );
				c_vertexes += 3;

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i+0];
		last[1] = indexes[i+1];
		last[2] = indexes[i+2];
	}

	qglEnd();
}



/*
==================
R_DrawElements

Optionally performs our own glDrawElements that looks for strip conditions
instead of using the single glDrawElements call that may be inefficient
without compiled vertex arrays.
==================
*/
#ifdef __vita__
#define R_DrawElements(numIndexes, indexes) glDrawElements(GL_TRIANGLES, numIndexes, GL_INDEX_TYPE, indexes)
#else
static void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	int		primitives;

	primitives = r_primitives->integer;

	// default is to use triangles if compiled vertex arrays are present
	if ( primitives == 0 ) {
		if ( qglLockArraysEXT ) {
			primitives = 2;
		} else {
			primitives = 1;
		}
	}


	if ( primitives == 2 ) {
		qglDrawElements( GL_TRIANGLES, 
						numIndexes,
						GL_INDEX_TYPE,
						indexes );
		return;
	}

	if ( primitives == 1 ) {
		R_DrawStripElements( numIndexes,  indexes, qglArrayElement );
		return;
	}
	
	if ( primitives == 3 ) {
		R_DrawStripElements( numIndexes,  indexes, R_ArrayElementDiscrete );
		return;
	}

	// anything else will cause no drawing
}
#endif

/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;
static qboolean	setArraysOnce;

/*
=================
R_BindAnimatedImage

=================
*/
static void R_BindAnimatedImage( textureBundle_t *bundle ) {
	int index;

	if (bundle->numImageAnimations <= 1) {
		GL_Bind(bundle->image[0]);
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	index = myftol((tess.shaderTime + bundle->imageAnimationPhase) * bundle->imageAnimationSpeed * FUNCTABLE_SIZE);
	index >>= FUNCTABLE_SIZE2;

	if (index < 0) {
		index = 0;	// may happen with shader time offsets
	}
	if (!(bundle->flags & BUNDLE_ANIMATE_ONCE)) {
		index %= bundle->numImageAnimations;
	} else if (index >= bundle->numImageAnimations) {
		index = bundle->numImageAnimations - 1;
	}

	GL_Bind(bundle->image[index]);
}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris (shaderCommands_t *input, int flags) {
	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);

	if (flags == 1) {
		GL_State(GLS_POLYMODE_LINE);
	} else {
        GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);
        qglDepthRange(0, 0);
	}

	if (flags < 3) {
		qglDisableClientState(GL_COLOR_ARRAY);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

		qglVertexPointer(3, GL_FLOAT, 16, input->xyz);	// padded for SIMD

		if (qglLockArraysEXT) {
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment("glLockArraysEXT\n");
		}

		R_DrawElements(input->numIndexes, input->indexes);

		if (qglUnlockArraysEXT) {
			qglUnlockArraysEXT();
			GLimp_LogComment("glUnlockArraysEXT\n");
		}
		qglDepthRange(0, 1);
	}

	if (flags > 1) {
		vec3_t start, end;

        start[1] = start[0] = 0.0;
		end[1] = end[0] = 0.0;

        GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);
        qglDepthRange(0, 0);

        qglDisable(GL_TEXTURE_2D);
        qglBegin(GL_LINES);

        qglColor4f(1.0, 0.0, 0.0, 1.0);

        start[2] = 0.0 - 5.0;
        end[2] = 0.0 + 5.0;
        qglVertex3fv(start);
        qglVertex3fv(end);

        qglColor4f(0.0, 1.0, 0.0, 1.0);

        start[2] = start[2] + 5.0;
        end[2] = end[2] - 5.0;
        start[1] = start[1] - 5.0;
        end[1] = end[1] + 5.0;
        qglVertex3fv(start);
        qglVertex3fv(end);

        qglColor4f(0.0, 0.0, 1.0, 1.0);

        start[1] = start[1] + 5.0;
        start[0] = start[0] - 5.0;
        end[0] = end[0] + 5.0;
        qglVertex3fv(start);
        qglVertex3fv(end);

        start[0] = start[0] + 5.0;
        end[0] = end[0] - 5.0;

        qglEnd();
        qglEnable(GL_TEXTURE_2D);
        qglDepthRange(0.0, 1.0);

	}
}

/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals (shaderCommands_t *input) {
	int		i;
	vec3_t	temp;

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);
	qglDepthRange( 0, 0 );	// never occluded
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglBegin (GL_LINES);
	for (i = 0 ; i < input->numVertexes ; i++) {
		qglVertex3fv (input->xyz[i]);
		VectorMA (input->xyz[i], 2, input->normal[i], temp);
		qglVertex3fv (temp);
	}
	qglEnd ();

	qglDepthRange( 0, 1 );
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader ) {
	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = shader;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.vertexColorValid = r_vertexLight->integer != 0;
	tess.xstages = shader->unfoggedStages;
	tess.numPasses = shader->numUnfoggedPasses;
	tess.currentStageIteratorFunc = shader->optimalStageIteratorFunc;

}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void DrawMultitextured( shaderCommands_t *input, int stage ) {
	shaderStage_t	*pStage;

	pStage = tess.xstages[stage];

	GL_State( pStage->stateBits );

	// this is an ugly hack to work around a GeForce driver
	// bug with multitexture and clip planes
	if ( backEnd.viewParms.isPortal ) {
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	//
	// base
	//
	GL_SelectTexture( 0 );
	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
	R_BindAnimatedImage( &pStage->bundle[0] );

/*
	// was removed since 2.0
	if (pStage->stateBits & GLS_MULTITEXTURE_ENV)
	{
		glState.cntnvblendmode = pStage->multitextureEnv;
		glState.cntTexEnvExt = GLS_MULTITEXTURE_ENV;

		if (pStage->multitextureEnv == GL_ADD)
		{
			qglTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
			qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB, 0);
			qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_ONE_MINUS_SRC_COLOR);
			qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_TEXTURE1);
			qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
			qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE3_RGB, 0);
			qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND3_RGB, GL_ONE_MINUS_SRC_COLOR);
		}
		else if (pStage->multitextureEnv == GL_MODULATE)
		{
			qglTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE1);
			qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
		}
		else
		{
			ri.Printf(3, "Unknown MT mode for: %s\n", input->shader);
		}
	}
*/

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	if ( r_lightmap->integer && pStage->bundle[1].isLightmap ) {
		GL_TexEnv( GL_REPLACE );
/*
	// was removed since 2.0
	} else if (pStage->stateBits & GLS_MULTITEXTURE_ENV) {
		glState.cntTexEnvExt = GLS_MULTITEXTURE_ENV;

		GL_TexEnv(GL_COMBINE);
		qglTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
		qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
		qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
		qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
		qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_SRC_ALPHA);
		qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
*/
	} else {
		GL_TexEnv( pStage->multitextureEnv );
	}

	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[1] );

	if (!input->dlightMap || !pStage->bundle[1].isLightmap) {
		R_BindAnimatedImage(&pStage->bundle[1]);
	} else {
		GL_Bind(tr.dlightImages[input->dlightMap - 1]);
	}

	R_DrawElements( input->numIndexes, input->indexes );

	qglDisable(GL_TEXTURE_2D);
	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture( 0 );
}



/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
static void ProjectDlightTexture( void ) {
	int		i, l;
#if idppc_altivec
	vec_t	origin0, origin1, origin2;
	float   texCoords0, texCoords1;
	vector float floatColorVec0, floatColorVec1;
	vector float modulateVec, colorVec, zero;
	vector short colorShort;
	vector signed int colorInt;
	vector unsigned char floatColorVecPerm, modulatePerm, colorChar;
	vector unsigned char vSel = (vector unsigned char)(0x00, 0x00, 0x00, 0xff,
							   0x00, 0x00, 0x00, 0xff,
							   0x00, 0x00, 0x00, 0xff,
							   0x00, 0x00, 0x00, 0xff);
#else
	vec3_t	origin;
	vec3_t	normal;
#endif
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	MAC_STATIC float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	unsigned	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	int		planetype;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	float	modulate;

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

#if idppc_altivec
	// There has to be a better way to do this so that floatColor 
	// and/or modulate are already 16-byte aligned.
	floatColorVecPerm = vec_lvsl(0,(float *)floatColor);
	modulatePerm = vec_lvsl(0,(float *)&modulate);
	modulatePerm = (vector unsigned char)vec_splat((vector unsigned int)modulatePerm,0);
	zero = (vector float)vec_splat_s8(0);
#endif

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
#if idppc_altivec
		origin0 = dl->transformed[0];
		origin1 = dl->transformed[1];
		origin2 = dl->transformed[2];
#else
		VectorCopy( dl->transformed, origin );
#endif
		radius = dl->radius * 1.75f;
		scale = 1.75f / radius;

		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;
#if idppc_altivec
		floatColorVec0 = vec_ld(0, floatColor);
		floatColorVec1 = vec_ld(11, floatColor);
		floatColorVec0 = vec_perm(floatColorVec0,floatColorVec0,floatColorVecPerm);
#endif
		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
#if idppc_altivec
			vec_t dist0, dist1, dist2;
#else
			vec3_t	dist;
#endif
			float	vertdist;
			int		clip;

			backEnd.pc.c_dlightVertexes++;

#if idppc_altivec
			//VectorSubtract( origin, tess.xyz[i], dist );
			dist0 = origin0 - tess.xyz[i][0];
			dist1 = origin1 - tess.xyz[i][1];
			dist2 = origin2 - tess.xyz[i][2];
			texCoords0 = 0.5f + dist0 * scale;
			texCoords1 = 0.5f + dist1 * scale;

			clip = 0;
			if ( texCoords0 < 0.0f ) {
				clip |= 1;
			} else if ( texCoords0 > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords1 < 0.0f ) {
				clip |= 4;
			} else if ( texCoords1 > 1.0f ) {
				clip |= 8;
			}
			texCoords[0] = texCoords0;
			texCoords[1] = texCoords1;
			
			// modulate the strength based on the height and color
			if ( dist2 > radius ) {
				clip |= 16;
				modulate = 0.0f;
			} else if ( dist2 < -radius ) {
				clip |= 32;
				modulate = 0.0f;
			} else {
				dist2 = Q_fabs(dist2);
				if ( dist2 < radius * 0.5f ) {
					modulate = 1.0f;
				} else {
					modulate = 2.0f * (radius - dist2) * scale;
				}
			}
			clipBits[i] = clip;

			modulateVec = vec_ld(0,(float *)&modulate);
			modulateVec = vec_perm(modulateVec,modulateVec,modulatePerm);
			colorVec = vec_madd(floatColorVec0,modulateVec,zero);
			colorInt = vec_cts(colorVec,0);	// RGBx
			colorShort = vec_pack(colorInt,colorInt);		// RGBxRGBx
			colorChar = vec_packsu(colorShort,colorShort);	// RGBxRGBxRGBxRGBx
			colorChar = vec_sel(colorChar,vSel,vSel);		// RGBARGBARGBARGBA replace alpha with 255
			vec_ste((vector unsigned int)colorChar,0,(unsigned int *)colors);	// store color
#else
			VectorCopy(tess.normal[i], normal);

			if (fabs(normal[0]) > fabs(normal[1])) {
				if (fabs(normal[2]) < fabs(normal[0])) {
					planetype = 0;
				} else {
					planetype = 1;
				}
			} else {
				if (fabs(normal[2]) < fabs(normal[1])) {
					planetype = 1;
				}
				else {
					planetype = 2;
				}
			}

			VectorSubtract( origin, tess.xyz[i], dist );
			if (planetype == 0)
			{
				texCoords[0] = 0.5f + dist[1] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}
			else if (planetype == 1)
			{
				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}
			else
			{
				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[1] * scale;
			}

			vertdist = DotProduct(dist, normal);
			clip = 0;
			if ( texCoords[0] < 0.0f ) {
				clip |= 1;
			} else if ( texCoords[0] > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0.0f ) {
				clip |= 4;
			} else if ( texCoords[1] > 1.0f ) {
				clip |= 8;
			}
			// modulate the strength based on the height and color
			if (vertdist > radius) {
				clip |= 16;
				modulate = 0.0f;
			}
			else if (vertdist < -radius) {
				clip |= 32;
				modulate = 0.0f;
			}
			else {
				dist[2] = Q_fabs(dist[2]);
				if (dist[2] < radius * 0.5f) {
					modulate = 1.0f;
				}
				else {
					modulate = 2.0f * (radius - dist[2]) * scale;
				}
			}
			clipBits[i] = clip;

			colors[0] = myftol(floatColor[0] * modulate);
			colors[1] = myftol(floatColor[1] * modulate);
			colors[2] = myftol(floatColor[2] * modulate);
			colors[3] = 255;
#endif
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		qglEnableClientState(GL_COLOR_ARRAY);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, colorArray);
		qglTexCoordPointer(2, GL_FLOAT, 0, texCoordsArray[0]);

		GL_Bind( tr.dlightImage );
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->type & additive) {
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		else {
			GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		R_DrawElements( numIndexes, hitIndexes );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
}

/*
===============
ComputeColors
===============
*/
static void ComputeColors( shaderStage_t *pStage )
{
	int		i;

	//
	// rgbGen
	//
	switch ( pStage->rgbGen )
	{
		case CGEN_IDENTITY:
			Com_Memset( tess.svars.colors, 0xff, tess.numVertexes * 4 );
			break;
		default:
		case CGEN_IDENTITY_LIGHTING:
			Com_Memset( tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4 );
			break;
		case CGEN_ENTITY:
			RB_CalcColorFromEntity((unsigned char*)tess.svars.colors);
			break;
		case CGEN_ONE_MINUS_ENTITY:
			RB_CalcColorFromOneMinusEntity((unsigned char*)tess.svars.colors);
			break;
		case CGEN_EXACT_VERTEX:
			if (!tess.vertexColorValid) {
				static qboolean bWarned = qfalse;

				if (!bWarned) {
					// mohta and mohtt shows the warning once
                    bWarned = qtrue;

                    ri.Printf(
                        PRINT_WARNING,
                        "Vertex color specified for shader '%s', but vertex colors are not valid for this model\n",
                        tess.shader->name
					);
				}
				break;
			}
			Com_Memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			break;
		case CGEN_VERTEX:
            if (!tess.vertexColorValid) {
                static qboolean bWarned = qfalse;

				if (!bWarned) {
					bWarned = qtrue;

					ri.Printf(
						PRINT_WARNING,
						"Vertex color specified for shader '%s', but vertex colors are not valid for this model\n",
						tess.shader->name);
				}
				break;
			}
			if (tr.identityLight == 1)
			{
				Com_Memcpy(tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof(tess.vertexColors[0]));
			}
			else
			{
				for (i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
					tess.svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
					tess.svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
					tess.svars.colors[i][3] = tess.vertexColors[i][3];
				}
			}
			break;
		case CGEN_ONE_MINUS_VERTEX:
			if ( tr.identityLight == 1 )
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = 255 - tess.vertexColors[i][0];
					tess.svars.colors[i][1] = 255 - tess.vertexColors[i][1];
					tess.svars.colors[i][2] = 255 - tess.vertexColors[i][2];
				}
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = ( 255 - tess.vertexColors[i][0] ) * tr.identityLight;
					tess.svars.colors[i][1] = ( 255 - tess.vertexColors[i][1] ) * tr.identityLight;
					tess.svars.colors[i][2] = ( 255 - tess.vertexColors[i][2] ) * tr.identityLight;
				}
			}
			break;
		case CGEN_WAVEFORM:
			RB_CalcWaveColor(&pStage->rgbWave, (unsigned char*)tess.svars.colors, NULL);
			break;
		case CGEN_MULTIPLY_BY_WAVEFORM:
			RB_CalcWaveColor(&pStage->rgbWave, (unsigned char*)tess.svars.colors, pStage->colorConst);
			break;
		case CGEN_LIGHTING_GRID:
			RB_CalcLightGridColor((unsigned char*)tess.svars.colors);
			break;
		case CGEN_LIGHTING_SPHERICAL:
		case CGEN_STATIC:
			if (!r_drawspherelights->integer) {
				break;
			}

			if (!backEnd.currentStaticModel) {
				if (backEnd.currentSphere->TessFunction) {
					backEnd.currentSphere->TessFunction((unsigned char*)tess.svars.colors);
				}
				break;
			}

			if (!tess.vertexColorValid) {
				ri.Printf(PRINT_WARNING, "Vertex color specified for shader '%s', but vertex colors are not valid for this model\n", tess.shader->name);
			}

			if (backEnd.currentStaticModel->useSpecialLighting)
			{
				for (i = 0; i < tess.numVertexes; i++)
				{
					int j;
					vec3_t colorout;
					int r, g, b;

					colorout[0] = tess.vertexColors[i][0];
					colorout[1] = tess.vertexColors[i][1];
					colorout[2] = tess.vertexColors[i][2];

					for (j = 0; j < backEnd.currentStaticModel->numdlights; j++)
					{
						float ooLightDistSquared;
						float dot;
						vec3_t diff;
						dlight_t* dl;

						dl = &backEnd.refdef.dlights[backEnd.currentStaticModel->dlights[j].index];
						VectorSubtract(backEnd.currentStaticModel->dlights[j].transformed, tess.xyz[i], diff);

						dot = DotProduct(diff, tess.normal[i]);
						if (dot >= 0)
						{
							float ooLen;

							ooLen = 1.0 / VectorLengthSquared(diff);
							ooLightDistSquared = dot * (7500.0 * dl->radius * ooLen * sqrt(ooLen));
							colorout[0] = dl->color[0] * ooLightDistSquared + colorout[0];
							colorout[1] = dl->color[1] * ooLightDistSquared + colorout[1];
							colorout[2] = dl->color[2] * ooLightDistSquared + colorout[2];
						}
					}

					r = colorout[0];
					g = colorout[1];
					b = colorout[2];

					if (tr.overbrightShift)
					{
						r = (int)((float)r * tr.overbrightMult);
						g = (int)((float)g * tr.overbrightMult);
						b = (int)((float)b * tr.overbrightMult);
					}

					if (r > 0xFF || g > 0xFF || b > 0xFF)
					{
						float t;

						t = 255.0 / (float)Q_max(r, Q_max(g, b));

						r = (int)((float)r * t);
						g = (int)((float)g * t);
						b = (int)((float)b * t);
					}

					tess.svars.colors[i][0] = r;
					tess.svars.colors[i][1] = g;
					tess.svars.colors[i][2] = b;
					// Fixed in OPM
					//  This sets the alpha channel which will prevent vertices from hiding
					tess.svars.colors[i][3] = tess.vertexColors[i][3];
				}
			}
			else
			{
				if (tr.identityLight == 1.0)
				{
					Com_Memcpy(tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof(tess.vertexColors[0]));
				}
				else
				{
					for (i = 0; i < tess.numVertexes; i++) {
						tess.svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
						tess.svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
						tess.svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
					}
				}
			}
			break;
		case CGEN_CONSTANT:
			RB_CalcColorFromConstant((unsigned char*)tess.svars.colors, pStage->colorConst);
			break;
		case CGEN_GLOBAL_COLOR:
			RB_CalcColorFromConstant((unsigned char*)tess.svars.colors, backEnd.color2D);
			break;
		case CGEN_SCOORD:
			RB_CalcRGBFromTexCoords(
				(unsigned char*)tess.svars.colors,
				pStage->alphaMin,
				pStage->alphaMax,
				pStage->alphaConstMin,
				pStage->alphaConst,
				1.0,
				0.0,
				tess.svars.texcoords[0][0]
			);
			break;
		case CGEN_TCOORD:
			RB_CalcRGBFromTexCoords(
				(unsigned char*)tess.svars.colors,
				pStage->alphaMin,
				pStage->alphaMax,
				pStage->alphaConstMin,
				pStage->alphaConst,
				0.0,
				1.0,
				tess.svars.texcoords[0][0]
			);
			break;
		case CGEN_DOT:
			RB_CalcRGBFromDot((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
			break;
		case CGEN_ONE_MINUS_DOT:
			RB_CalcRGBFromOneMinusDot((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
			break;
	}

	//
	// alphaGen
	//
	switch ( pStage->alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( pStage->rgbGen != CGEN_IDENTITY
			&& pStage->rgbGen != CGEN_LIGHTING_GRID
			&& pStage->rgbGen != CGEN_LIGHTING_SPHERICAL)
		{
			if ( pStage->rgbGen != CGEN_VERTEX || tr.identityLight != 1 ) {
				for ( i = 0; i < tess.numVertexes; i++ ) {
					tess.svars.colors[i][3] = 0xff;
				}
			}
		}
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity((unsigned char*)tess.svars.colors);
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity((unsigned char*)tess.svars.colors);
		break;
	case AGEN_VERTEX:
		if (pStage->rgbGen != CGEN_VERTEX) {
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for (i = 0; i < tess.numVertexes; i++)
		{
			tess.svars.colors[i][3] = 255 - tess.vertexColors[i][3];
		}
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha((unsigned char*)tess.svars.colors, pStage->alphaMax, pStage->specOrigin);
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha(&pStage->alphaWave, (unsigned char*)tess.svars.colors);
		break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;

			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.ori.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	case AGEN_DOT:
		RB_CalcAlphaFromDot((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
		return;
	case AGEN_ONE_MINUS_DOT:
		RB_CalcAlphaFromOneMinusDot((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
		return;
	case AGEN_CONSTANT:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, pStage->colorConst[3]);
		break;
	case AGEN_GLOBAL_ALPHA:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, backEnd.color2D[3]);
		break;
	case AGEN_SKYALPHA:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, tr.refdef.sky_alpha * 255.0);
		break;
	case AGEN_ONE_MINUS_SKYALPHA:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, (1.0 - tr.refdef.sky_alpha) * 255.0);
		break;
	case AGEN_SCOORD:
		RB_CalcAlphaFromTexCoords(
			(unsigned char*)tess.svars.colors,
			pStage->alphaMin,
			pStage->alphaMax,
			pStage->alphaConstMin,
			pStage->alphaConst,
			1.0,
			0.0,
			tess.svars.texcoords[0][0]
		);
		break;
	case AGEN_TCOORD:
		RB_CalcAlphaFromTexCoords(
			(unsigned char*)tess.svars.colors,
			pStage->alphaMin,
			pStage->alphaMax,
			pStage->alphaConstMin,
			pStage->alphaConst,
			0.0,
			1.0,
			tess.svars.texcoords[0][0]
		);
		break;
	case AGEN_DIST_FADE:
		if (backEnd.currentStaticModel)
		{
			unsigned char alpha;

			for (i = 0; i < tess.numVertexes; i++) {
				float len;
				vec3_t org, v;

				VectorSubtract(backEnd.viewParms.ori.origin, backEnd.currentStaticModel->origin, v);
				org[0] = tess.xyz[i][0] - DotProduct(v, backEnd.currentStaticModel->axis[0]);
				org[1] = tess.xyz[i][1] - DotProduct(v, backEnd.currentStaticModel->axis[1]);
				org[2] = tess.xyz[i][2] - DotProduct(v, backEnd.currentStaticModel->axis[2]);
			
				len = (VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange;
				if (len < 0) {
					alpha = 0xff;
				} else if (len > 1) {
					alpha = 0;
				} else {
					alpha = ((1.0 - len) * 255.0);
                }
                tess.svars.colors[i][3] = alpha;
			}
		}
		else
        {
            unsigned char alpha;

            for (i = 0; i < tess.numVertexes; i++) {
                float len;
                vec3_t org, v;

				if (backEnd.currentEntity) {
					VectorSubtract(backEnd.viewParms.ori.origin, backEnd.currentEntity->e.origin, v);
					org[0] = tess.xyz[i][0] - DotProduct(v, backEnd.currentEntity->e.axis[0]);
					org[1] = tess.xyz[i][1] - DotProduct(v, backEnd.currentEntity->e.axis[1]);
					org[2] = tess.xyz[i][2] - DotProduct(v, backEnd.currentEntity->e.axis[2]);
				} else {
					VectorCopy(backEnd.viewParms.ori.origin, org);
				}

				
				len = (VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange;
				if (len < 0) {
					alpha = 0xff;
				} else if (len > 1) {
					alpha = 0;
				} else {
					alpha = ((1.0 - len) * 255.0);
                }
                tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	case AGEN_ONE_MINUS_DIST_FADE:
		if (backEnd.currentStaticModel)
		{
			unsigned char alpha;

			for (i = 0; i < tess.numVertexes; i++) {
				float len;
				vec3_t org, v;

                VectorSubtract(backEnd.viewParms.ori.origin, backEnd.currentStaticModel->origin, v);
                org[0] = tess.xyz[i][0] - DotProduct(v, backEnd.currentStaticModel->axis[0]);
                org[1] = tess.xyz[i][1] - DotProduct(v, backEnd.currentStaticModel->axis[1]);
                org[2] = tess.xyz[i][2] - DotProduct(v, backEnd.currentStaticModel->axis[2]);
			
				len = (VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange;
				if (len < 0) {
					alpha = 0;
				} else if (len > 1) {
					alpha = 0xff;
				} else {
					alpha = (len * 255.0);
                }
                tess.svars.colors[i][3] = alpha;
			}
		}
		else
        {
            unsigned char alpha;

            for (i = 0; i < tess.numVertexes; i++) {
                float len;
                vec3_t org, v;

				if (backEnd.currentEntity) {
                    VectorSubtract(backEnd.viewParms.ori.origin, backEnd.currentEntity->e.origin, v);
                    org[0] = tess.xyz[i][0] - DotProduct(v, backEnd.currentEntity->e.axis[0]);
                    org[1] = tess.xyz[i][1] - DotProduct(v, backEnd.currentEntity->e.axis[1]);
                    org[2] = tess.xyz[i][2] - DotProduct(v, backEnd.currentEntity->e.axis[2]);
				} else {
					VectorCopy(backEnd.viewParms.ori.origin, org);
				}

				
				len = (VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange;
				if (len < 0) {
					alpha = 0;
				} else if (len > 1) {
					alpha = 0xff;
				} else {
					alpha = (len * 255.0);
                }
                tess.svars.colors[i][3] = alpha;
			}
		}
		break;
    case AGEN_TIKI_DIST_FADE:
    case AGEN_ONE_MINUS_TIKI_DIST_FADE:
		{
			unsigned char alpha;

			if (backEnd.currentStaticModel)
			{
				float lenSqr;
				float fDistNearSqr;
				float fDistFarSqr;
				vec3_t org;

				VectorSubtract(backEnd.currentStaticModel->origin, backEnd.viewParms.ori.origin, org);

				lenSqr = VectorLengthSquared(org);
                fDistNearSqr = tess.shader->fDistNear * tess.shader->fDistNear;
                fDistFarSqr = (tess.shader->fDistNear + tess.shader->fDistRange) * (tess.shader->fDistNear + tess.shader->fDistRange);
				if (lenSqr <= fDistNearSqr) {
					alpha = 0;
				} else if (lenSqr >= fDistFarSqr) {
					alpha = 0xff;
				} else {
					float len;

					len = VectorLength(org);
					alpha = (((VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange) * 255.0);
				}

				if (pStage->alphaGen == AGEN_TIKI_DIST_FADE) {
					alpha = 0xff - alpha;
				}
			}
			else if (backEnd.currentEntity)
			{
				float lenSqr;
				float fDistNearSqr;
				float fDistFarSqr;
				vec3_t org;

				VectorSubtract(backEnd.currentEntity->e.origin, backEnd.viewParms.ori.origin, org);

				lenSqr = VectorLengthSquared(org);
				fDistNearSqr = tess.shader->fDistNear * tess.shader->fDistNear;
				fDistFarSqr = (tess.shader->fDistNear + tess.shader->fDistRange) * (tess.shader->fDistNear + tess.shader->fDistRange);
				if (lenSqr <= fDistNearSqr) {
					alpha = 0;
				} else if (lenSqr >= fDistFarSqr) {
					alpha = 0xff;
				} else {
					float len;

                    len = VectorLength(org);
                    alpha = (((VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange) * 255.0);
				}

				if (pStage->alphaGen == AGEN_TIKI_DIST_FADE) {
					alpha = 0xff - alpha;
				}
			}
			else
			{
				const char* type;
				if (pStage->alphaGen == AGEN_TIKI_DIST_FADE) {
					type = "tikiDistFade";
				} else {
					type = "oneMinusTikiDistFade";
				}

				ri.Error(ERR_DROP, "ERROR: '%s' shader command used on a non-tiki\n", type);
				alpha = 0xff;
			}
			RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, alpha);
		}
		break;
	case AGEN_DOT_VIEW:
		RB_CalcAlphaFromDotView((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
		break;
	case AGEN_ONE_MINUS_DOT_VIEW:
		RB_CalcAlphaFromOneMinusDotView((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
        break;
    case AGEN_HEIGHT_FADE:
        RB_CalcAlphaFromHeightFade((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
        break;
	default:
		break;
	}
}

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords( shaderStage_t *pStage ) {
	int		i;
	int		b;

	for ( b = 0; b < NUM_TEXTURE_BUNDLES && pStage->bundle[b].image[0]; b++ ) {
		int tm;

		//
		// generate the texture coordinates
		//
		switch ( pStage->bundle[b].tcGen )
		{
		case TCGEN_IDENTITY:
			Com_Memset( tess.svars.texcoords[b], 0, sizeof( float ) * 2 * tess.numVertexes );
			break;
		case TCGEN_TEXTURE:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][0][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][0][1];
			}
			break;
		case TCGEN_LIGHTMAP:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][1][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][1][1];
			}
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			RB_CalcEnvironmentTexCoords( ( float * ) tess.svars.texcoords[b] );
            break;
		case TCGEN_VECTOR:
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.texcoords[b][i][0] = DotProduct(tess.xyz[i], pStage->bundle[b].tcGenVectors[0]);
				tess.svars.texcoords[b][i][1] = DotProduct(tess.xyz[i], pStage->bundle[b].tcGenVectors[1]);
			}
			break;
        case TCGEN_ENVIRONMENT_MAPPED2:
            RB_CalcEnvironmentTexCoords2((float*)tess.svars.texcoords[b]);
            break;
		case TCGEN_SUN_REFLECTION:
			RB_CalcSunReflectionTexCoords(tess.svars.texcoords[b][0]);
			break;
        default:
            ri.Printf(
				PRINT_DEVELOPER,
                "WARNING: invalid tcGen '%d' specified for shader '%s'\n",
                pStage->bundle[b].tcGen,
                tess.shader->name
			);
			break;
		}

		//
		// alter texture coordinates
		//
		for ( tm = 0; tm < pStage->bundle[b].numTexMods ; tm++ ) {
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		// break out of for loop
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoords( &pStage->bundle[b].texMods[tm].wave, 
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord,
									 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( pStage->bundle[b].texMods[tm].scroll,
										 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( pStage->bundle[b].texMods[tm].scale,
									 ( float * ) tess.svars.texcoords[b] );
				break;
			
			case TMOD_STRETCH:
				RB_CalcStretchTexCoords( &pStage->bundle[b].texMods[tm].wave, 
						               ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords( &pStage->bundle[b].texMods[tm],
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( pStage->bundle[b].texMods[tm].rotateSpeed,
										pStage->bundle[b].texMods[tm].rotateCoef,
										( float * ) tess.svars.texcoords[b],
										pStage->bundle[b].texMods[tm].rotateStart );
				break;

			case TMOD_OFFSET:
				RB_CalcOffsetTexCoords(pStage->bundle[b].texMods[tm].scroll, tess.svars.texcoords[b][0]);
				break;

            case TMOD_PARALLAX:
				RB_CalcParallaxTexCoords(pStage->bundle[b].texMods[tm].rate, tess.svars.texcoords[b][0]);
                break;

            case TMOD_MACRO:
				RB_CalcMacroTexCoords(pStage->bundle[b].texMods[tm].scale, tess.svars.texcoords[b][0]);
                break;

            case TMOD_WAVETRANS:
				RB_CalcTransWaveTexCoords(&pStage->bundle[b].texMods[tm].wave, tess.svars.texcoords[b][0]);
                break;

            case TMOD_WAVETRANT:
				RB_CalcTransWaveTexCoordsT(&pStage->bundle[b].texMods[tm].wave, tess.svars.texcoords[b][0]);
                break;

            case TMOD_BULGETRANS:
				RB_CalcBulgeTexCoords(&pStage->bundle[b].texMods[tm].wave, tess.svars.texcoords[b][0]);
                break;

			default:
				ri.Printf(PRINT_WARNING, "WARNING: invalid tcMod '%d' specified for shader '%s'\n", pStage->bundle[b].texMods[tm].type, tess.shader->name );
				break;
			}
		}
	}
}

/*
** RB_IterateStagesGeneric
*/
static void RB_IterateStagesGeneric( shaderCommands_t *input )
{
	int stage;

	for (stage = 0; stage < MAX_SHADER_STAGES; stage++)
	{
		shaderStage_t* pStage = tess.xstages[stage];

		if (!pStage)
		{
			break;
		}

		if (pStage->alphaGen == AGEN_SKYALPHA && tr.refdef.sky_alpha < 0.01) {
			continue;
		}
		else if (pStage->alphaGen == AGEN_ONE_MINUS_SKYALPHA && tr.refdef.sky_alpha > 0.99) {
			continue;
		}

		ComputeTexCoords(pStage);
		ComputeColors(pStage);

		if (!setArraysOnce)
		{
			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, input->svars.colors);
		}

		//
		// do multitexture
		//
		if (pStage->bundle[1].image[0] != 0)
		{
			DrawMultitextured(input, stage);
		}
		else
		{
			if (backEnd.in2D) {
				GL_State(pStage->stateBits | GLS_DEPTHTEST_DISABLE);
			}
			else {
				GL_State(pStage->stateBits);
			}

			if (!setArraysOnce)
			{
				qglTexCoordPointer(2, GL_FLOAT, 0, input->svars.texcoords[0]);
			}

			//
			// set state
			//
			if (pStage->bundle[0].vertexLightmap && r_vertexLight->integer && r_lightmap->integer)
			{
				GL_Bind(tr.whiteImage);
			}
			else {
				if (input->dlightMap && tess.xstages[stage]->bundle[0].isLightmap) {
					GL_Bind(tr.dlightImages[input->dlightMap - 1]);
				}
				else {
					R_BindAnimatedImage(&pStage->bundle[0]);
				}
			}

			//
			// draw
			//
			R_DrawElements(input->numIndexes, input->indexes);
		}
		// allow skipping out to show just lightmaps during development
		if (r_lightmap->integer && (pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap))
		{
			break;
		}
	}
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;

	input = &tess;

	input->shaderTime = backEnd.refdef.floatTime;
	if ((input->shader->flags & 1) != 0) {
		input->shaderTime = backEnd.refdef.floatTime - backEnd.shaderStartTime;
	}

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if (tess.numPasses > 1 || (tess.xstages[0] && tess.xstages[0]->multitextureEnv))
	{
		setArraysOnce = qfalse;
		qglDisableClientState(GL_COLOR_ARRAY);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		setArraysOnce = qtrue;

		qglEnableClientState(GL_COLOR_ARRAY);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, tess.svars.colors);

		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texcoords[0]);
	}

	//
	// lock XYZ
	//
	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD
	if (qglLockArraysEXT)
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	//
	// enable color and texcoord arrays after the lock if necessary
	//
	if ( !setArraysOnce )
	{
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglEnableClientState( GL_COLOR_ARRAY );
	}

	//
	// call shader function
	//
	RB_IterateStagesGeneric( input );

	// 
	// now do any dynamic lighting needed
	//
	if (tess.dlightBits && !tess.dlightMap && tess.shader->sort <= SS_OPAQUE) {
		ProjectDlightTexture();
	}

	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}

	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}


/*
** RB_StageIteratorVertexLitTexture
*/
void RB_StageIteratorVertexLitTextureUnfogged( void )
{
	shaderCommands_t *input;
	shader_t		*shader;

	input = &tess;

	shader = input->shader;

	if (backEnd.currentSphere->TessFunction && r_drawspherelights->integer)
	{
		//
		// compute colors using the sphere function
		//
		backEnd.currentSphere->TessFunction((unsigned char*)tess.svars.colors);
	}
	else if (backEnd.currentStaticModel)
	{
		//
		// compute colors
		//
		ComputeColors(input->xstages[0]);
	}
	else
	{
		//
		// calculate using previously calculated lighting grid
		// for the entity
		//
		RB_CalcLightGridColor((unsigned char*)tess.svars.colors);
	}

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorVertexLitTexturedUnfogged( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	//
	// set arrays and lock
	//
	qglEnableClientState( GL_COLOR_ARRAY);
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY);

	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );
	qglTexCoordPointer( 2, GL_FLOAT, 16, tess.texCoords[0][0] );
	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);

	if ( qglLockArraysEXT )
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	//
	// call special shade routine
	//
	R_BindAnimatedImage( &tess.xstages[0]->bundle[0] );
	GL_State( tess.xstages[0]->stateBits );
	R_DrawElements( input->numIndexes, input->indexes );

	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && !tess.dlightMap && tess.shader->sort <= SS_OPAQUE ) {
		ProjectDlightTexture();
	}

	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
}

//define	REPLACE_MODE

void RB_StageIteratorLightmappedMultitextureUnfogged( void ) {
	shaderCommands_t *input;

	input = &tess;

	//
	// log this call
	//
	if ( r_logFile->integer ) {
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorLightmappedMultitexture( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	//
	// set color, pointers, and lock
	//
	GL_State( GLS_DEFAULT | (tess.xstages[0]->stateBits & (GLS_ATEST_BITS | GLS_FOG_BITS)));
	qglVertexPointer( 3, GL_FLOAT, 16, input->xyz );

#ifdef REPLACE_MODE
	qglDisableClientState( GL_COLOR_ARRAY );
	qglColor3f( 1, 1, 1 );
	qglShadeModel( GL_FLAT );
#else
	qglEnableClientState( GL_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.constantColor255 );
#endif

	//
	// select base stage
	//
	GL_SelectTexture( 0 );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	R_BindAnimatedImage( &tess.xstages[0]->bundle[0] );
	qglTexCoordPointer( 2, GL_FLOAT, 16, tess.texCoords[0][0] );

	//
	// configure second stage
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( GL_MODULATE );
	}
	if (tess.dlightMap && tess.xstages[0]->bundle[1].isLightmap) {
		GL_Bind(tr.dlightImages[tess.dlightMap - 1]);
	} else {
		R_BindAnimatedImage(&tess.xstages[0]->bundle[1]);
	}
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 16, tess.texCoords[0][1] );

	//
	// lock arrays
	//
	if ( qglLockArraysEXT ) {
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	qglDisable( GL_TEXTURE_2D );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	GL_SelectTexture( 0 );
#ifdef REPLACE_MODE
	GL_TexEnv( GL_MODULATE );
	qglShadeModel( GL_SMOOTH );
#endif

	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && !tess.dlightMap && tess.shader->sort <= SS_OPAQUE ) {
		ProjectDlightTexture();
	}

	//
	// unlock arrays
	//
	if ( qglUnlockArraysEXT ) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
}

/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0) {
		return;
	}
	if (input->indexes[SHADER_MAX_INDEXES-1] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");
	}	
	if (input->xyz[SHADER_MAX_VERTEXES-1][0] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");
	}

	if (tess.xyz[SHADER_MAX_VERTEXES - 1][0] && r_shadows->integer != 3) {
		tess.xyz[SHADER_MAX_VERTEXES - 1][0] = 0;
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ComputeShadowVolume();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	if (!(tr.refdef.rdflags & RDF_NOWORLDMODEL) && !backEnd.in2D)
	{
		//
		// draw debugging stuff
		//
		if (r_showtris->integer && developer->integer) {
			DrawTris(input, r_showtris->integer);
		}
		if (r_shownormals->integer && developer->integer) {
			DrawNormals(input);
		}
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;

	GLimp_LogComment( "----------\n" );
}

