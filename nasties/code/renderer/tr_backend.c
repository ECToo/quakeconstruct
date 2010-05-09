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

maskDef_t		maskState;
qboolean		cmd2D = qfalse;

GLenum			base_glsl_shader;

int currentGLSLShader = 0;

#define	MAX_GLSL_SHADERS	32

GLenum glslShaders[MAX_GLSL_SHADERS];
GLenum glslShaderFragment[MAX_GLSL_SHADERS];
GLenum glslShaderVertex[MAX_GLSL_SHADERS];
char* glslShaderFiles[MAX_GLSL_SHADERS];


qboolean loadShaderSource(char *file, char **source) {
	int		length;

	length = ri.FS_ReadFile( file, (void **)source);
	if (!*source) {
		Com_Printf("^1Unable to find \"%s\".\n", file);
		return qfalse;
	}
	//Com_Printf("^2Shader Component %s loaded.\n", file);
	return qtrue;
}

void printInfoLog(GLenum obj)
{
	int infologLength = 0;
	int charsWritten  = 0;
	char *infoLog;
	qglGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
	if (infologLength > 1)
	{
	    infoLog = (char *)malloc(infologLength);
	    qglGetInfoLogARB(obj, infologLength, &charsWritten, infoLog);
		Com_Printf("\n^1%s\n",infoLog);
	    free(infoLog);
	}else{
		Com_Printf("^2OK\n");
	}
}

char *formatStr(const char *fmt, ...) {
	char		msg[MAXPRINTMSG];
	va_list		argptr;

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	return msg;
}

GLenum getShaderProgram(char *file) {
	int i;
	char* ffile;
	for(i=0; i<sizeof(glslShaderFiles)/sizeof(char*); i++) {
		ffile = glslShaderFiles[i];
		if(ffile) {
			if(!Q_stricmp(ffile,file)) {
				return glslShaders[i];
			}
		}
	}
	return -1;
}

void useShaderProgram(char *file) {
	GLenum sh = getShaderProgram(file);
	if(sh != -1) {
		qglUseProgramObjectARB(sh);
	} else {
		Com_Printf("^1Couldn't use: %i [%s]\n",sh,file);
	}
}

void revertShaderProgram( void ) {
	qglUseProgramObjectARB(base_glsl_shader);
}

qboolean getShaderForFile(char *file, GLenum *prog, GLenum *fragment, GLenum *vertex) {
	int i;
	char* ffile;
	for(i=0; i<sizeof(glslShaderFiles)/sizeof(char*); i++) {
		ffile = glslShaderFiles[i];
		if(ffile) {
			if(!Q_stricmp(ffile,file)) {
				*prog = glslShaders[i];
				*fragment = glslShaderFragment[i];
				*vertex = glslShaderVertex[i];
				return qtrue;
			}
		}
	}
	return qfalse;
}

void reloadShader(char *file, GLenum program, GLenum fragment, GLenum vertex) {
	char *vertSource;
	char *fragSource;

	if(!loadShaderSource(formatStr("%s.%s",file,"frag"), &fragSource)) return;
	if(!loadShaderSource(formatStr("%s.%s",file,"vert"), &vertSource)) return;

	qglShaderSourceARB(vertex, 1, &vertSource, NULL);
	qglShaderSourceARB(fragment, 1, &fragSource, NULL);

	qglCompileShaderARB(vertex);
	qglCompileShaderARB(fragment);

	qglAttachObjectARB(program, vertex);
	qglAttachObjectARB(program, fragment);

	qglLinkProgramARB(program);

	//qglUseProgramObjectARB(program);

	if(qglGetInfoLogARB && qglGetShaderiv) {
		Com_Printf("-%s.vert: ", file);
		printInfoLog(vertex);
		Com_Printf("-%s.frag: ", file);
		printInfoLog(fragment);
	}
	Com_Printf("^2Reloaded Shader: %s\n", file);
}

GLenum loadShader(char *file) {
	GLenum program;
	GLenum my_vertex_shader;
	GLenum my_fragment_shader;
	char *vertSource;
	char *fragSource;
	char filename[MAX_QPATH];
	int fsize;

	if(getShaderForFile(file,&program,&my_fragment_shader,&my_vertex_shader)) {
		reloadShader(file,program,my_fragment_shader,my_vertex_shader);
		return program;
	}

	if(currentGLSLShader >= MAX_GLSL_SHADERS) {
		Com_Printf("MAX_GLSL_SHADERS Exceeded > %i\n",MAX_GLSL_SHADERS);
	}

	if(!loadShaderSource(formatStr("%s.%s",file,"frag"), &fragSource)) return -1;
	if(!loadShaderSource(formatStr("%s.%s",file,"vert"), &vertSource)) return -1;

	// Create Shader And Program Objects
	program = qglCreateProgramObjectARB();
	my_vertex_shader = qglCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	my_fragment_shader = qglCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

	// Load Shader Sources
	qglShaderSourceARB(my_vertex_shader, 1, &vertSource, NULL);
	qglShaderSourceARB(my_fragment_shader, 1, &fragSource, NULL);

	// Compile The Shaders
	qglCompileShaderARB(my_vertex_shader);
	qglCompileShaderARB(my_fragment_shader);

	// Attach The Shader Objects To The Program Object
	qglAttachObjectARB(program, my_vertex_shader);
	qglAttachObjectARB(program, my_fragment_shader);

	// Link The Program Object
	qglLinkProgramARB(program);

	// Use The Program Object Instead Of Fixed Function OpenGL
	// qglUseProgramObjectARB(program);

	if(qglGetInfoLogARB && qglGetShaderiv) {
		Com_Printf("-%s.vert: ",file);
		printInfoLog(my_vertex_shader);
		Com_Printf("-%s.frag: ",file);
		printInfoLog(my_fragment_shader);
	}

	glslShaders[currentGLSLShader] = program;
	glslShaderFragment[currentGLSLShader] = my_fragment_shader;
	glslShaderVertex[currentGLSLShader] = my_vertex_shader;

	Q_strncpyz(filename, file, sizeof(filename));

	fsize = sizeof(filename);
	glslShaderFiles[currentGLSLShader] = Z_Malloc(fsize);
	Com_Memset(glslShaderFiles[currentGLSLShader], 0, fsize);
	Q_strncpyz(glslShaderFiles[currentGLSLShader], file, fsize);

	currentGLSLShader++;
	
	return program;
}

void loadAllGLSLShaders(void) {
	char **shaderFiles;
	int numShaders;
	int i;

	shaderFiles = ri.FS_ListFiles( "GLSL", ".frag", &numShaders );

	if ( !shaderFiles || !numShaders )
	{
		ri.Printf( PRINT_WARNING, "WARNING: no GLSL shader files found\n" );
		return;
	}

	if ( numShaders > MAX_GLSL_SHADERS ) {
		numShaders = MAX_GLSL_SHADERS;
	}

	Com_Printf("^3LOADING GLSL SHADERS[%i]\n",numShaders);

	for ( i = 0; i < numShaders; i++ )
	{
		char filename[MAX_QPATH];

		Com_sprintf( filename, sizeof( filename ), "GLSL/%s", shaderFiles[i] );
		Q_strncpyz(filename, filename, strlen(filename)-4);
		if(Q_stricmp(filename,"GLSL/base")) {
			ri.Printf( PRINT_ALL, "...loading '%s'\n", filename );
			loadShader(filename);
		}
	}
}

void beginGLSL() {
	if(!qglCreateProgramObjectARB) {Com_Printf("ARB_ERROR: No qglCreateProgramObjectARB"); return;}
	if(!qglCreateShaderObjectARB) {Com_Printf("ARB_ERROR: No qglCreateShaderObjectARB"); return;}
	if(!qglCompileShaderARB) {Com_Printf("ARB_ERROR: No qglCompileShaderARB"); return;}
	if(!qglLinkProgramARB) {Com_Printf("ARB_ERROR: No qglLinkProgramARB"); return;}
	if(!qglShaderSourceARB) {Com_Printf("ARB_ERROR: No qglShaderSourceARB"); return;}
	if(!qglAttachObjectARB) {Com_Printf("ARB_ERROR: No qglCompileShaderARB"); return;}
	if(!qglUseProgramObjectARB) {Com_Printf("ARB_ERROR: No qglUseProgramObjectARB"); return;}

	//base_glsl_shader = loadShader("GLSL/base");
	//qglUseProgramObjectARB(base_glsl_shader);

	loadAllGLSLShaders();

	ri.Cmd_AddCommand( "reloadShaders", loadAllGLSLShaders );
}



static float	s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


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
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		image->frameUsed = tr.frameCount;
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
}

void setupRT( int index, int width, int height ) {
	unsigned int* data;						// Stored Data

	if(index < 0 || index > MAX_RENDER_TARGETS) {
		ri.Printf( PRINT_WARNING, "WARNING: bad render target index %i.\nMust be a value between 0-%i\n",index,MAX_RENDER_TARGETS);
		return;
	}

	// Create Storage Space For Texture Data (128x128x4)
	//data = (unsigned int*)new GLuint[((128 * 128)* 4 * sizeof(unsigned int))];
	data = malloc(((width * height)* 4 * sizeof(unsigned int)));

	qglGenTextures(1,&renderTargets[index].texture);
	qglBindTexture(GL_TEXTURE_2D, renderTargets[index].texture);
	qglTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, data);			// Build Texture Using Information In data
	qglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

	//Com_Memset(data,0,0);

	renderTargets[index].width = width;
	renderTargets[index].height = height;
	renderTargets[index].valid = qtrue;
	
	free(data);
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
	unsigned long diff = stateBits ^ glState.glStateBits;

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

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	glState.glStateBits = stateBits;
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
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );
	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( r_measureOverdraw->integer || r_shadows->integer == 2 )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	if ( r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
		qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );	// FIXME: get color of sky
#else
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
#endif
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

		plane2[0] = DotProduct (backEnd.viewParms.or.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.or.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.or.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.or.origin) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane (GL_CLIP_PLANE0, plane2);
		qglEnable (GL_CLIP_PLANE0);
	} else {
		qglDisable (GL_CLIP_PLANE0);
	}
}


#define	MAC_EVENT_PUMP_MSEC		5

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs, qboolean override ) {
	shader_t		*shader, *oldShader;
	rendertarget_t	rt;
	int				fogNum, oldFogNum;
	int				entityNum, oldEntityNum;
	int				dlighted, oldDlighted;
	qboolean		depthRange, oldDepthRange;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;
	float			originalTime;
	int				loc;
#ifdef __MACOS__
	int				macEventTime;

	Sys_PumpEvents();		// crutch up the mac's limited buffer queue size

	// we don't want to pump the event loop too often and waste time, so
	// we are going to check every shader change
	macEventTime = ri.Milliseconds() + MAC_EVENT_PUMP_MSEC;
#endif

	if(backEnd.refdef.glsl_override != NULL) {
		if(strlen(backEnd.refdef.glsl_override) > 0) {
			//Com_Printf("GLSL OVERRIDE: %s\n",backEnd.refdef.glsl_override);
			base_glsl_shader = getShaderProgram((char*)backEnd.refdef.glsl_override);
			qglUseProgramObjectARB(base_glsl_shader);
		}
	}

	if(base_glsl_shader) {
		if(qglUniform1fARB) {
			loc = qglGetUniformLocationARB(base_glsl_shader, "cgtime");
			qglUniform1fARB(loc, backEnd.refdef.floatTime);
		}

		if(qglUniform3fvARB) {
			loc = qglGetUniformLocationARB(base_glsl_shader, "viewPos");
			qglUniform3fARB(loc, 
				backEnd.refdef.vieworg[0],
				backEnd.refdef.vieworg[1],
				backEnd.refdef.vieworg[2]);
		}

		if(qglUniform3fvARB) {
			loc = qglGetUniformLocationARB(base_glsl_shader, "viewNormal");
			qglUniform3fARB(loc, 
				backEnd.refdef.viewaxis[0][0],
				backEnd.refdef.viewaxis[0][1],
				backEnd.refdef.viewaxis[0][2]);
		}
	}

	rt.valid = qfalse;

	if(backEnd.viewParms.isRenderTarget) {
		rt = renderTargets[backEnd.viewParms.rt_index];
		if(rt.valid) {
			//GL_State( GLS_DEPTHTEST_DISABLE );
			backEnd.viewParms.viewportX = 0;
			backEnd.viewParms.viewportY = 0;
			backEnd.viewParms.viewportWidth = rt.width;
			backEnd.viewParms.viewportHeight = rt.height;

			SetViewportAndScissor();

			if(!override) {
				qglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
				qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); //
				//GL_State( GLS_DEFAULT );
			}
		}
	}

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	if(!override) RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	oldDlighted = qfalse;
	oldSort = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}
		oldSort = drawSurf->sort;
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted 
			|| ( entityNum != oldEntityNum && !shader->entityMergable ) ) {
			if (oldShader != NULL) {
#ifdef __MACOS__	// crutch up the mac's limited buffer queue size
				int		t;

				t = ri.Milliseconds();
				if ( t > macEventTime ) {
					macEventTime = t + MAC_EVENT_PUMP_MSEC;
					Sys_PumpEvents();
				}
#endif
				RB_EndSurface(override);
			}
			//shader->stages[0]->stateBits &= ~GLS_DEPTHTEST_DISABLE;
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = qfalse;

			if ( entityNum != ENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );

				// set up the dynamic lighting if needed
				if ( backEnd.currentEntity->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
				}

				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
			}

			qglLoadMatrixf( backEnd.or.modelMatrix );

			//
			// change depthrange if needed
			//
			if ( oldDepthRange != depthRange ) {
				if ( depthRange ) {
					qglDepthRange (0, 0.3);
				} else {
					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if (oldShader != NULL) {
		RB_EndSurface(override);
	}

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}

#if 0
	RB_DrawSun();
#endif
	// darken down any stencil shadows
	RB_ShadowFinish();		

	// add light flares on lights that aren't obscured
	RB_RenderFlares();

	if(rt.valid) {
		qglBindTexture(GL_TEXTURE_2D, rt.texture);
		
		qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, rt.width, rt.height, 0);

		/*if(override) {
			qglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); // | GL_COLOR_BUFFER_BIT
		}*/
	}

#ifdef __MACOS__
	Sys_PumpEvents();		// crutch up the mac's limited buffer queue size
#endif
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
	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	GL_State( GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	qglDisable( GL_CULL_FACE );
	qglDisable( GL_CLIP_PLANE0 );

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {
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

	GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );	
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();

	qglColor3f( tr.identityLight, tr.identityLight, tr.identityLight );

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
}

void RE_UploadCinematic (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {

	GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );	
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}
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
RB_QuadPic
=============
*/
const void *RB_QuadPic ( const void *data ) {
	const quadPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes, vn;
	float	r = 0;
	float	x,y;

	cmd = (const quadPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface(qfalse);
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
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

	tess.xyz[ numVerts ][0] = cmd->x0;
	tess.xyz[ numVerts ][1] = cmd->y0;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0][0] = cmd->s1;
	tess.texCoords[ numVerts ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x1;
	tess.xyz[ numVerts + 1 ][1] = cmd->y1;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x2;
	tess.xyz[ numVerts + 2 ][1] = cmd->y2;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][0][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x3;
	tess.xyz[ numVerts + 3 ][1] = cmd->y3;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][0][1] = cmd->t2;

	return (const void *)(cmd + 1);
}


int matrixPos(int r, int c) {
	return ((r*4)+1*c+1)-1;		
}

float mrowcol(int p, float *m1, float *m2) {
	int c = p % 4;
	int r = p / 4;
	float mv = 0;
	int i=0;
	for(i=0; i<4; i++) {
		int p1 = matrixPos(r,i);
		int p2 = matrixPos(i,c);
		mv += m1[p1] * m2[p2];
	}
	return mv;
}

void quickMultiply1x4by4x4(float *mat1, float *mat2, float *out) {
	int i,j;
	for(i=0; i<4; i++) {
		for(j=0; j<4; j++) {
			int rc = (j*4) + i;
			out[i] += mat1[j] * mat2[rc];
		}
	}	
}

void quickMultiply4x4(float *mat1, float *mat2, float *out) {
	int i;
	for(i=0; i<16; i++) {
		out[i] = mrowcol(i,mat1,mat2);
	}
}


/*
=============
RB_VectorToScreen
=============
*/
const void *RB_VectorToScreen ( const void *data ) {
	const vectorToScreenCommand_t *cmd;
	float modelview[16];
	float projection[16];
	float temp[16];
	float vout[4];
	int view[4];

	//SetViewportAndScissor();

	cmd = (const vectorToScreenCommand_t *) data;

	vout[0] = *cmd->x;
	vout[1] = *cmd->y;
	vout[2] = *cmd->z;
	vout[3] = 1;

	//qglGetFloatv(GL_MODELVIEW_MATRIX, modelview);
	//qglGetFloatv(GL_PROJECTION_MATRIX, projection);
	//qglGetIntegerv(GL_VIEWPORT, view);

	R_RotateForViewer();
	R_SetupProjection();

	quickMultiply4x4(backEnd.viewParms.or.modelMatrix,backEnd.viewParms.projectionMatrix,temp);
	quickMultiply1x4by4x4(vout,temp,vout);

	view[0] = backEnd.viewParms.viewportX;
	view[1] = backEnd.viewParms.viewportY;
	view[2] = backEnd.viewParms.viewportWidth;
	view[3] = backEnd.viewParms.viewportHeight;

	vout[0] /= vout[3];
	vout[1] /= vout[3];
	vout[2] /= vout[3];
	
	vout[0] = view[0] + (view[2] * (vout[0] + 1)) / 2;
	vout[1] = view[1] + (view[3] * (vout[1] + 1)) / 2;
	vout[2] = (vout[2] + 1) / 2;
	
	//Com_Printf("MODEL: [%f,%f,%f,%f]\n",modelview[0],modelview[1],modelview[2],modelview[3]);
	//Com_Printf("PROJECT: [%f,%f,%f,%f]\n",projection[0],projection[1],projection[2],projection[3]);
	Com_Printf("VIEW: [%i,%i,%i,%i]\n",view[0],view[1],view[2],view[3]);
	Com_Printf("VIN: %f,%f,%f\n",*cmd->x,*cmd->y,*cmd->z);
	Com_Printf("VOUT: %f,%f,%f\n",vout[0],vout[1],vout[2]);

	*cmd->x = vout[0];
	*cmd->y = vout[1];
	*cmd->z = vout[2];

	return (const void *)(cmd + 1);
}

/*
=============
RB_TransformPic
=============
*/
const void *RB_TransformPic ( const void *data ) {
	const transformPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes, vn;
	float	r = 0;
	float	x,y;

	cmd = (const transformPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface(qfalse);
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
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

	tess.xyz[ numVerts ][0] = -1;
	tess.xyz[ numVerts ][1] = -1;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0][0] = cmd->s1;
	tess.texCoords[ numVerts ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = 1;
	tess.xyz[ numVerts + 1 ][1] = -1;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = 1;
	tess.xyz[ numVerts + 2 ][1] = 1;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][0][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = -1;
	tess.xyz[ numVerts + 3 ][1] = 1;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][0][1] = cmd->t2;

	r = cmd->r;
	if(r != 0) {
		r = r / 57.3;
	}

	for(vn = numVerts;vn<=numVerts+3;vn++) {
		//Scale, Rotate, Translate
		tess.xyz[ vn ][0] *= cmd->w/2;
		tess.xyz[ vn ][1] *= cmd->h/2;

		x = tess.xyz[ vn ][0];
		y = tess.xyz[ vn ][1];

		tess.xyz[ vn ][0] = (cos(r)*x) - (sin(r)*y);
		tess.xyz[ vn ][1] = (sin(r)*x) + (cos(r)*y);

		tess.xyz[ vn ][0] += cmd->x;
		tess.xyz[ vn ][1] += cmd->y;
	}

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

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface(qfalse);
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
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
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface(qfalse);
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs, cmd->override );

	return (const void *)(cmd + 1);
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
void RB_ShowImages( void ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.Milliseconds();

	for ( i=0 ; i<tr.numImages ; i++ ) {
		image = tr.images[i];

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
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );

}

void doMask() {
	float x,y,w,h;

	if(maskState.enabled) {
		x = maskState.x;
		y = maskState.y;
		w = maskState.w;
		h = maskState.h;

		RB_SetGL2D();
		qglClear( GL_COLOR_BUFFER_BIT );

		qglFinish();

		GL_Bind( tr.whiteImage );
		qglColor3f( 1, 1, 1 );

		qglBegin(GL_QUADS);
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();

		qglFinish();
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
		RB_EndSurface(qfalse);
	}

	//doMask();

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
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

	backEnd.projection2D = qfalse;

	return (const void *)(cmd + 1);
}

const void *RB_EndMask( const void *data ) {
	const voidCommand_t *cmd;

	cmd = (const voidCommand_t *)data;

	if ( tess.numIndexes ) {
		RB_EndSurface(qfalse);
	}
	backEnd.currentEntity = &backEnd.entity2D;
	RB_BeginSurface( tr.maskEndShader, 0 );
	RB_EndSurface(qfalse);

	return (const void *)(cmd + 1);
}

const void *RB_GetPixel( const void *data ) {
	const getPixelCommand_t *cmd;

	cmd = (const getPixelCommand_t *)data;

	R_GetPixel(cmd->x, cmd->y, cmd->r, cmd->g, cmd->b);

	return (const void *)(cmd + 1);
}

const void *RB_Advance( const void *data ) {
	const voidCommand_t *cmd;
	cmd = (const voidCommand_t *)data;

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
		if ( backEnd.projection2D == qfalse && cmd2D ) {
			RB_SetGL2D();
		}
		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_TRANSFORM_PIC:
			data = RB_TransformPic( data );
			break;
		case RC_QUAD_PIC:
			data = RB_QuadPic( data );
			break;
		case RC_VECTOR_TOSCREEN:
			data = RB_VectorToScreen( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
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
		case RC_ENDMASK:
			data = RB_EndMask(data);
			break;
		case RC_BEGIN2D:
			//if ( !backEnd.projection2D ) {
				//cmd2D = qtrue;
				//RB_SetGL2D();
				//GL_State( GLS_DEPTHFUNC_EQUAL );
			//}
			data = RB_Advance(data);
			break;
		case RC_END2D:
			//if ( cmd2D ) {
				//cmd2D = qfalse;
			//}
			data = RB_Advance(data);
			break;
		case RC_GETPIXEL:
			data = RB_GetPixel(data);
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

