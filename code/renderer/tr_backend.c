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

backEndData_t	*backEndData[SMP_FRAMES];
backEndState_t	backEnd;


static float	s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

void GL_SetFogColor(const vec4_t fColor) {
	glState.fFogColor[0] = fColor[0];
	glState.fFogColor[1] = fColor[1];
	glState.fFogColor[2] = fColor[2];
	glState.fFogColor[3] = fColor[3];

	if (!(glState.glStateBits & GLS_FOG_COLOR)) {
		qglFogfv(GL_FOG_COLOR, glState.fFogColor);
	}
}

/*
** GL_Bind
*/
void GL_Bind( image_t *image ) {
	int texnum;

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
		image->UseCount++;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		if (image) {
			image->frameUsed = tr.frameCount;
		}
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
}

/*
** GL_SelectTexture
*/
void GL_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
	{
		return;
	}

	if ( unit == 0 )
	{
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE0_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE0_ARB )\n" );
	}
	else if ( unit == 1 )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE1_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE1_ARB )\n" );
	} else {
		ri.Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );
	}

	glState.currenttmu = unit;
}


/*
** GL_BindMultitexture
*/
void GL_BindMultitexture( image_t *image0, GLuint env0, image_t *image1, GLuint env1 ) {
	int		texnum0, texnum1;

	texnum0 = image0->texnum;
	texnum1 = image1->texnum;

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum0 = texnum1 = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[1] != texnum1 ) {
		GL_SelectTexture( 1 );
		image1->frameUsed = tr.frameCount;
		glState.currenttextures[1] = texnum1;
		qglBindTexture( GL_TEXTURE_2D, texnum1 );
	}
	if ( glState.currenttextures[0] != texnum0 ) {
		GL_SelectTexture( 0 );
		image0->frameUsed = tr.frameCount;
		glState.currenttextures[0] = texnum0;
		qglBindTexture( GL_TEXTURE_2D, texnum0 );
	}
}


/*
** GL_Cull
*/
void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED ) 
	{
		qglDisable( GL_CULL_FACE );
	} 
	else 
	{
		qglEnable( GL_CULL_FACE );

		if ( cullType == CT_BACK_SIDED )
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_FRONT );
			}
			else
			{
				qglCullFace( GL_BACK );
			}
		}
		else
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_BACK );
			}
			else
			{
				qglCullFace( GL_FRONT );
			}
		}
	}
}

/*
** GL_TexEnv
*/
void GL_TexEnv( int env )
{
	if ( env == glState.texEnv[glState.currenttmu] )
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;


	switch ( env )
	{
	case GL_MODULATE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case GL_REPLACE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case GL_DECAL:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
		break;
	case GL_ADD:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
		break;
	case GL_COMBINE4_NV:
	case GL_COMBINE:
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env);
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( unsigned long stateBits )
{
	unsigned long diff = glState.glStateBits ^ (glState.externalSetState | stateBits);

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor, dstFactor;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits\n" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits\n" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	if ((diff & GLS_MULTITEXTURE_ENV) || (diff & GLS_MULTITEXTURE))
	{
		if (stateBits & GLS_MULTITEXTURE_ENV)
		{
			GL_TexEnv(GL_COMBINE4_NV);
		}
		else if (stateBits & GLS_MULTITEXTURE)
		{
			if (glState.cntTexEnvExt != GLS_MULTITEXTURE)
			{
				qglTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
				qglTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
				qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
				qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
				qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
				qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
				qglTexEnvf(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_PRIMARY_COLOR);
				qglTexEnvf(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
				glState.cntTexEnvExt = GLS_MULTITEXTURE;
			}

			GL_TexEnv(GL_COMBINE);
		}
		else
		{
			GL_TexEnv(GL_MODULATE);
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch (stateBits & GLS_ATEST_BITS)
		{
		case 0:
			qglDisable(GL_ALPHA_TEST);
			break;
		case GLS_ATEST_GT_0:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GREATER, 0.05f);
			break;
		case GLS_ATEST_LT_80:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, 0.5f);
			break;
		case GLS_ATEST_GE_80:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.5f);
			break;
		case GLS_ATEST_LT_FOLIAGE1:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, r_alpha_foliage1->value);
			break;
		case GLS_ATEST_GE_FOLIAGE1:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, r_alpha_foliage1->value);
			break;
		case GLS_ATEST_LT_FOLIAGE2:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, r_alpha_foliage2->value);
			break;
		case GLS_ATEST_GE_FOLIAGE2:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, r_alpha_foliage2->value);
			break;
		default:
			assert(0);
			break;
		}
	}

	if (diff & GLS_CLAMP_EDGE)
	{
		float clampValue;

		if (haveClampToEdge)
		{
			if (stateBits & GLS_CLAMP_EDGE) {
				clampValue = GL_CLAMP_TO_EDGE;
			}
			else {
				clampValue = GL_REPEAT;
			}
		}
		else
		{
			if (stateBits & GLS_CLAMP_EDGE) {
				clampValue = GL_CLAMP;
			}
			else {
				clampValue = GL_REPEAT;
			}
		}

		if (qglActiveTextureARB)
		{
			int currenttmu = glState.currenttmu;

			GL_SelectTexture(0);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampValue);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampValue);

			GL_SelectTexture(1);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampValue);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampValue);

			GL_SelectTexture(currenttmu);
		}
		else
		{
			GL_SelectTexture(0);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampValue);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampValue);
		}
	}
	
	if (diff & GLS_FOG)
	{
		if (glState.externalSetState & GLS_FOG)
		{
			qglEnable(GL_FOG);

			if (stateBits & GLS_FOG_BLACK)
			{
				vec4_t fBlackFogColor = { 0, 0, 0, 1 };
				qglFogfv(GL_FOG_COLOR, fBlackFogColor);
			}
			else if (stateBits & GLS_FOG_WHITE)
			{
				vec4_t fWhiteFogColor = { 1, 1, 1, 1 };
				qglFogfv(GL_FOG_COLOR, fWhiteFogColor);
			}
			else
			{
				//
				// Use the global fog
				qglFogfv(GL_FOG_COLOR, glState.fFogColor);
			}
		}
		else
		{
			qglDisable(GL_FOG);
		}
	}
	else if (glState.externalSetState & GLS_FOG)
	{
		if (diff & GLS_FOG_ENABLED)
		{
			if (stateBits & GLS_FOG_ENABLED) {
				qglEnable(GL_FOG);
			}
			else {
				qglDisable(GL_FOG);
			}
		}

		if (diff & GLS_FOG_COLOR)
		{
			if (stateBits & GLS_FOG_BLACK)
			{
				vec4_t fBlackFogColor = { 0, 0, 0, 1 };
				qglFogfv(GL_FOG_COLOR, fBlackFogColor);
			}
			else if (stateBits & GLS_FOG_WHITE)
			{
				vec4_t fWhiteFogColor = { 1, 1, 1, 1 };
				qglFogfv(GL_FOG_COLOR, fWhiteFogColor);
			}
			else
            {
                //
                // Use the global fog
                qglFogfv(GL_FOG_COLOR, glState.fFogColor);
            }
		}
	}

	glState.glStateBits = stateBits | glState.externalSetState;
}



/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	float		c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor( void ) {
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView (void) {
	int clearBits = 0;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish ();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.in2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	R_UploadDlights();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );
	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( r_measureOverdraw->integer || r_shadows->integer == 3 )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}

	if (!( backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
	{
		if ((backEnd.viewParms.farplane_distance && !tr.skyRendered && !tr.portalRendered) || tr.farclip)
		{
			clearBits |= GL_COLOR_BUFFER_BIT;
			qglClearColor(glState.fFogColor[0], glState.fFogColor[1], glState.fFogColor[2], glState.fFogColor[3]);
		}
		else
		{
			if (r_fastsky->integer)
			{
				clearBits |= GL_COLOR_BUFFER_BIT;
				qglClearColor(0.5, 0.5, 1.0, 1.0);
			}
		}
	}
	qglClear( clearBits );

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.ori.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.ori.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.ori.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.ori.origin) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane (GL_CLIP_PLANE0, plane2);
		qglEnable (GL_CLIP_PLANE0);
	} else {
		qglDisable (GL_CLIP_PLANE0);
	}
}

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				entityNum, oldEntityNum;
	int				dlightMap, oldDlightMap;
	qboolean		depthRange, oldDepthRange;
	qboolean		bStaticModel, oldbStaticModel;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;
	float			originalTime;

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldDepthRange = qfalse;
	oldDlightMap = 0;
	oldSort = -1;
	oldbStaticModel = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.numSpheresUsed = 0;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}
		oldSort = drawSurf->sort;
		if (*drawSurf->surface != SF_SPRITE) {
			R_DecomposeSort(drawSurf->sort, &entityNum, &shader, &dlightMap, &bStaticModel);
		} else {
			shader = tr.sortedShaders[((refSprite_t*)drawSurf->surface)->shaderNum];
			entityNum = ENTITYNUM_WORLD;
			dlightMap = 0;
			bStaticModel = qfalse;
		}

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (shader != oldShader || dlightMap != oldDlightMap || (oldShader->flags & 1)
			|| ( entityNum != oldEntityNum && !shader->entityMergable )
			|| ( bStaticModel != oldbStaticModel && !shader->entityMergable )) {
			if (oldShader != NULL) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader );
			oldShader = shader;
			oldDlightMap = dlightMap;
		}

		backEnd.currentSphere = &backEnd.spareSphere;
		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum || bStaticModel != oldbStaticModel ) {
			depthRange = qfalse;

			if (bStaticModel) {
				backEnd.shaderStartTime = 0.0;
				backEnd.currentEntity = 0;
				backEnd.spareSphere.TessFunction = 0;
				backEnd.currentStaticModel = &backEnd.refdef.staticModels[entityNum];
				R_RotateForStaticModel(backEnd.currentStaticModel, &backEnd.viewParms, &backEnd.ori );
			}
			else {
				backEnd.currentStaticModel = NULL;
				if (entityNum != ENTITYNUM_WORLD) {
					backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
					backEnd.shaderStartTime = backEnd.currentEntity->e.shaderTime;

					// set up the transformation matrix
					R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori);

					// set up the dynamic lighting if needed
					if (backEnd.currentEntity->needDlights) {
						R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
					}

					if (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK) {
						// hack the depth range to prevent view model from poking into walls
						depthRange = qtrue;
					}
				}
				else {
					backEnd.currentEntity = &tr.worldEntity;
					backEnd.ori = backEnd.viewParms.world;
					backEnd.shaderStartTime = 0.0;
					R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
				}
			}

			qglLoadMatrixf( backEnd.ori.modelMatrix );

			//
			// change depthrange if needed
			//
			if ( oldDepthRange != depthRange ) {
				if ( depthRange ) {
					qglDepthRange (0, 0.3);
				} else {
					qglDepthRange (0, 1.0);
				}
				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
			oldbStaticModel = bStaticModel;
		}

		tess.dlightMap = dlightMap;
		if (r_fastdlights->integer) {
			tess.dlightMap = 0;
		}

		if (bStaticModel)
		{
			if (r_drawspherelights->integer) {
				RB_Static_BuildDLights();
			}

			if (!backEnd.currentStaticModel->bLightGridCalculated) {
				RB_Grid_SetupStaticModel();
			}
		}
		else if (backEnd.currentEntity->e.tiki)
		{
			if (shader->needsLGrid
				|| shader->needsLSpherical && r_fastentlight->integer
				|| !r_drawspherelights->integer)
			{
				backEnd.currentSphere->TessFunction = RB_CalcLightGridColor;
				RB_Grid_SetupEntity();
			}
			else if (shader->needsLSpherical)
			{
				if (tr.refdef.rdflags & RDF_HUD)
				{
					backEnd.currentSphere = &backEnd.hudSphere;
					backEnd.hudSphere.TessFunction = 0;
					RB_Sphere_SetupEntity();
				}
				else if (backEnd.currentEntity->sphereCalculated)
				{
					backEnd.currentSphere = &backEnd.spheres[backEnd.currentEntity->lightingSphere];
				}
				else
				{
					if (backEnd.numSpheresUsed == MAX_SPHERE_LIGHTS)
					{
						ri.Printf(PRINT_DEVELOPER, "Spherical lighting: Ran out of space in the sphere array!\n");
						backEnd.currentSphere = &backEnd.spareSphere;
					}
					else
					{
						backEnd.currentSphere = &backEnd.spheres[backEnd.numSpheresUsed];
						backEnd.currentEntity->lightingSphere = backEnd.numSpheresUsed++;
						backEnd.currentEntity->sphereCalculated = 1;
					}

					backEnd.currentSphere->TessFunction = NULL;
					RB_Sphere_SetupEntity();
				}
			}
		}

		if (*drawSurf->surface == SF_SPRITE) {
			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	// draw the contents of the last shader batch
	if (oldShader != NULL) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1.0);
	}

	RB_ShadowFinish();
	if (!(backEnd.refdef.rdflags & RDF_HUD)) {
		RB_RenderFlares();
		R_DrawLensFlares();
	}

	if (g_bInfoworldtris)
	{
		g_bInfoworldtris = 0;
		R_PrintInfoWorldtris();
	}

#if 0
	RB_DrawSun();
#endif
	// darken down any stencil shadows
	RB_ShadowFinish();		

	// add light flares on lights that aren't obscured
	RB_RenderFlares();
}

/*
==================
RB_RenderSpriteSurfList
==================
*/
void RB_RenderSpriteSurfList(drawSurf_t* drawSurfs, int numDrawSurfs) {
	shader_t	*shader;
	shader_t	*oldShader;
	qboolean	depthRange;
	qboolean	oldDepthRange;
	int			i;
	drawSurf_t	*drawSurf;

    backEnd.currentEntity = &tr.worldEntity;
    backEnd.currentStaticModel = 0;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.ori = backEnd.viewParms.world;

	oldShader = NULL;
    depthRange = qfalse;
    oldDepthRange = qfalse;

    for (i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++) {
		shader = tr.sortedShaders[((refSprite_t*)drawSurf->surface)->shaderNum];
		depthRange = (((refSprite_t*)drawSurf->surface)->renderfx & RF_DEPTHHACK) != 0;

        if ((shader != oldShader || (oldShader->flags & RF_THIRD_PERSON) != 0) && !shader->entityMergable)
        {
			if (oldShader) {
				RB_EndSurface();
			}

            RB_BeginSurface(shader);
            oldShader = shader;
        }

        qglLoadMatrixf(backEnd.ori.modelMatrix);

        if (oldDepthRange != depthRange)
        {
			if (depthRange) {
				qglDepthRange(0.0, 0.3);
			} else {
                qglDepthRange(0.0, 1.0);
			}

            oldDepthRange = depthRange;
        }

        backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;

        // add the triangles for this surface
        rb_surfaceTable[*drawSurf->surface](drawSurf->surface);
	}

	if (oldShader) {
		RB_EndSurface();
	}

    // go back to the world modelview matrix
    qglLoadMatrixf(backEnd.viewParms.world.modelMatrix);
	// go back to the previous depth range
	if (depthRange) {
		qglDepthRange(0.0, 1.0);
	}
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void	RB_SetGL2D (void) {
	Set2DWindow(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0.0, glConfig.vidWidth, glConfig.vidHeight, 0.0, 0.0, 1.0);
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, int components, const byte* data) {
	int			i, j;
	int			start, end;

	if ( !tr.registered ) {
		return;
	}
	R_SyncRenderThread();

	// we definately want to sync every frame for the cinematics
	qglFinish();

	start = end = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	// make sure rows and cols are powers of 2
	for ( i = 0 ; ( 1 << i ) < cols ; i++ ) {
	}
	for ( j = 0 ; ( 1 << j ) < rows ; j++ ) {
	}
	if ( ( 1 << i ) != cols || ( 1 << j ) != rows) {
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	GL_Bind( tr.scratchImage );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage->width || rows != tr.scratchImage->height ) {
		tr.scratchImage->width = tr.scratchImage->uploadWidth = cols;
		tr.scratchImage->height = tr.scratchImage->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );	
	} else {
		// otherwise, just subimage upload it so that drivers can tell we are going to be changing
		// it and don't try and do a texture compression
		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();

	qglColor3f( tr.identityLight, tr.identityLight, tr.identityLight );
#ifdef __vita__
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	float texcoords[] = {
		0.5f / cols,  0.5f / rows,
		( cols - 0.5f ) / cols ,  0.5f / rows,
		( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows,
		0.5f / cols, ( rows - 0.5f ) / rows
	};
	float vertices[] = {
		x, y, 0.0f,
		x+w, y, 0.0f,
		x+w, y+h, 0.0f,
		x, y+h, 0.0f
	};
	
	vglVertexPointer(3, GL_FLOAT, 0, 4, vertices);
	vglTexCoordPointer(2, GL_FLOAT, 0, 4, texcoords);
	vglDrawObjects(GL_TRIANGLE_FAN, 4, GL_TRUE);
#else
	qglBegin (GL_QUADS);
	qglTexCoord2f ( 0.5f / cols,  0.5f / rows );
	qglVertex2f (x, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols ,  0.5f / rows );
	qglVertex2f (x+w, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( 0.5f / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x, y+h);
	qglEnd ();
#endif
}

/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic ( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes;

	cmd = (const stretchPicCommand_t *)data;

	if ( !backEnd.in2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader );
	}

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	*(int *)tess.vertexColors[ numVerts ] =
		*(int *)tess.vertexColors[ numVerts + 1 ] =
		*(int *)tess.vertexColors[ numVerts + 2 ] =
		*(int *)tess.vertexColors[ numVerts + 3 ] = *(int *)backEnd.color2D;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0][0] = cmd->s1;
	tess.texCoords[ numVerts ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][0][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][0][1] = cmd->t2;

	return (const void *)(cmd + 1);
}

/*
=============
RB_SetupFog
=============
*/
void RB_SetupFog() {
	if (!backEnd.viewParms.farplane_distance) {
		glState.externalSetState &= ~GLS_FOG;
	} else if (r_farplane_nofog->integer) {
		glState.externalSetState &= ~GLS_FOG;
	} else {
		vec4_t vFogColor;

		qglFogf(GL_FOG_START, backEnd.viewParms.farplane_bias);
		qglFogf(GL_FOG_END, backEnd.viewParms.farplane_distance);

        vFogColor[0] = backEnd.viewParms.farplane_color[0] * tr.identityLight;
        vFogColor[1] = backEnd.viewParms.farplane_color[1] * tr.identityLight;
        vFogColor[2] = backEnd.viewParms.farplane_color[2] * tr.identityLight;
        vFogColor[3] = 1.0;
        GL_SetFogColor(vFogColor);

		glState.externalSetState |= GLS_FOG;
	}
}

/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_SetupFog();
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawSurfs

=============
*/
const void* RB_SpriteSurfs(const void* data) {
    const drawSurfsCommand_t* cmd;

    // finish any 2D drawing if needed
    if (tess.numIndexes) {
        RB_EndSurface();
    }

    cmd = (const drawSurfsCommand_t*)data;

    backEnd.refdef = cmd->refdef;
    backEnd.viewParms = cmd->viewParms;
	
	RB_SetupFog();
    RB_RenderSpriteSurfList(cmd->drawSurfs, cmd->numDrawSurfs);

    return (const void*)(cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	qglDrawBuffer( cmd->buffer );

	// clear screen for debugging
	if ( r_clear->integer ) {
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( qboolean quiet ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;

	if ( !backEnd.in2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.Milliseconds();

	for ( i=0 ; i<tr.numImages ; i++ ) {
		image = &tr.images[i];

		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );
#ifdef __vita__
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	
		float texcoord[] = {
			0, 0, 1, 0, 1, 1, 0, 1
		};
		float vertex[] = {
			x, y, 0.0f,
			x+w, y, 0.0f,
			x+w, y+h, 0.0f,
			x, y+h, 0.0f
		};
	
		vglVertexPointer(3, GL_FLOAT, 0, 4, vertex);
		vglTexCoordPointer(2, GL_FLOAT, 0, 4, texcoord);
		vglDrawObjects(GL_TRIANGLE_FAN, 4, GL_TRUE);
#else
		qglBegin (GL_QUADS);
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();
#endif
	}

	qglFinish();

	end = ri.Milliseconds();

    if (!quiet) {
        ri.Printf(PRINT_ALL, "%i msec to draw all images\n", end - start);
    }
}


/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages(qfalse);
	}

	cmd = (const swapBuffersCommand_t *)data;

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if ( r_measureOverdraw->integer ) {
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory( stencilReadback );
	}


	if ( !glState.finishCalled ) {
		qglFinish();
	}

	GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	GLimp_EndFrame();

	backEnd.in2D = qfalse;

	return (const void *)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands

This function will be called synchronously if running without
smp extensions, or asynchronously by another thread.
====================
*/
void RB_ExecuteRenderCommands( const void *data ) {
	int		t1, t2;

	t1 = ri.Milliseconds ();

	if ( !r_smp->integer || data == backEndData[0]->commands.cmds ) {
		backEnd.smpFrame = 0;
	} else {
		backEnd.smpFrame = 1;
	}

	while ( 1 ) {
		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_SPRITE_SURFS:
			data = RB_SpriteSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd( data );
			break;

		case RC_END_OF_LIST:
		default:
			// stop rendering on this thread
			t2 = ri.Milliseconds ();
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}

}


/*
================
RB_RenderThread
================
*/
void RB_RenderThread( void ) {
	const void	*data;

	// wait for either a rendering command or a quit command
	while ( 1 ) {
		// sleep until we have work to do
		data = GLimp_RendererSleep();

		if ( !data ) {
			return;	// all done, renderer is shutting down
		}

		renderThreadActive = qtrue;

		RB_ExecuteRenderCommands( data );

		renderThreadActive = qfalse;
	}
}

