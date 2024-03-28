#include "SolidColorRemake.h"
using namespace ffglex;

static CFFGLPluginInfo PluginInfo(
	PluginFactory< SolidColorRemake >,  // Create method
	"SB02",                        // Plugin unique ID
	"SolidColorRemake",                // Plugin name
	2,                             // API major version number
	1,                             // API minor version number
	1,                             // Plugin major version number
	000,                           // Plugin minor version number
	FF_SOURCE,                     // Plugin type
	"Sample FFGL DMX playback plugin",// Plugin description
	"Resolume FFGL Example"        // About
);

static const char vertexShaderCode[] = R"(#version 410 core
layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;

out vec2 uv;

void main()
{
	gl_Position = vPosition;
	uv = vUV;
}
)";

static const char fragmentShaderCode[] = R"(#version 410 core

in vec2 uv;

out vec4 fragColor;

void main()
{
	fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

SolidColorRemake::SolidColorRemake()
{
	// Input properties
	SetMinInputs( 0 );
	SetMaxInputs( 0 );

	FFGLLog::LogToHost( "Created solid color remake source" );
}

FFResult SolidColorRemake::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, fragmentShaderCode ) )
	{
		DeInitGL();
		return FF_FAIL;
	}
	if( !quad.Initialise() )
	{
		DeInitGL();
		return FF_FAIL;
	}

	//Use base-class init as success result so that it retains the viewport.
	return CFFGLPlugin::InitGL( vp );
}

FFResult SolidColorRemake::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	//FFGL requires us to leave the context in a default state on return, so use this scoped binding to help us do that.
	ScopedShaderBinding shaderBinding( shader.GetGLID() );

	quad.Draw();

	return FF_SUCCESS;
}
FFResult SolidColorRemake::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();

	return FF_SUCCESS;
}
