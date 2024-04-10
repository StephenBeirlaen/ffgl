#include "DmxPlayback.h"
#include "CsvReader.h"

using namespace ffglex;

static CFFGLPluginInfo PluginInfo(
	PluginFactory< DmxPlayback >,  // Create method
	"SB01",                        // Plugin unique ID
	"DMX Playback",                // Plugin name
	2,                             // API major version number
	1,                             // API minor version number
	1,                             // Plugin major version number
	000,                           // Plugin minor version number
	FF_SOURCE,                     // Plugin type
	"FFGL DMX recording playback plugin for Resolume 7",// Plugin description
	"Stephen B"        // About
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
uniform sampler2D DmxDataTexture;

in vec2 uv;

out vec4 fragColor;

void main()
{
	vec2 uvWithYInverted = vec2(uv.x, 1.0 - uv.y);
	fragColor = texture( DmxDataTexture, uvWithYInverted ).rrra;
}
)";

DmxPlayback::DmxPlayback() :
	dmxDataTextureId( -1 )
{
	// Input properties (0 means that it is a source, if it has inputs, it is an effect)
	SetMinInputs( 0 );
	SetMaxInputs( 0 );

	// Build an in-memory map of all the source's parameters. All parameters have an ID which must be unique and consecutive
	FFUInt32 nextParameterId = 0; // The first parameter's ID must start at 0

	for( std::uint8_t layerIndex = 0; layerIndex < numLayers; ++layerIndex )
	{
		std::vector< RecordedSequence > layerRecordedSequences;
		for( std::uint8_t sequenceIndex = 0; sequenceIndex < numSequencesPerLayer; ++sequenceIndex )
		{
			RecordedSequence recordedSequence;

			recordedSequence.sequenceNumber          = sequenceIndex + 1;
			recordedSequence.recordingParameterId = nextParameterId++;
			recordedSequence.recordingParameterValue = "";

			layerRecordedSequences.push_back( recordedSequence );
		}

		Layer layer;

		layer.recordedSequences         = layerRecordedSequences;
		layer.activeClipParameterId     = nextParameterId++;
		layer.activeClipParameterValue  = 1.0;
		layer.framePositionParameterId  = nextParameterId++;
		layer.framePositionParameterValue = 1.0;
		layer.opacityParameterId        = nextParameterId++;
		layer.opacityParameterValue     = 1.0;
		layer.layerNumber               = layerIndex + 1;

		layers.push_back( layer );
	}

	// Loop over all the parameters and make them known in the UI
	for( auto& layer : layers )
	{
		// Define the group name where parameters of this layer should be grouped into
		std::stringstream group_name_ss;
		group_name_ss << "Layer " << std::to_string(layer.layerNumber) << " recordings";	

		// Configure recording file parameters
		for( auto& recordedSequence : layer.recordedSequences )
		{
			// Configure a recording file parameter
			std::stringstream recording_param_ss;
			recording_param_ss << "Recording " << std::to_string(layer.layerNumber) << "." << std::to_string(recordedSequence.sequenceNumber);
			SetFileParamInfo( recordedSequence.recordingParameterId, recording_param_ss.str().c_str(), { "csv" }, "" );

			// Assign the parameter to a group
			SetParamGroup( recordedSequence.recordingParameterId, group_name_ss.str() );
		}

		// Configure an active clip parameter
		std::stringstream active_clip_ss;
		active_clip_ss << "L" << std::to_string(layer.layerNumber) << " active clip";
		SetOptionParamInfo( layer.activeClipParameterId, active_clip_ss.str().c_str(), numSequencesPerLayer, layer.activeClipParameterValue );
		/*SetParamElementInfo( layer.activeClipParameterId, 0, "1", 1.0f );
		SetParamElementInfo( layer.activeClipParameterId, 1, "2", 2.0f );
		SetParamElementInfo( layer.activeClipParameterId, 2, "3", 3.0f );
		SetParamElementInfo( layer.activeClipParameterId, 3, "4", 4.0f );
		SetParamElementInfo( layer.activeClipParameterId, 4, "5", 5.0f );*/
		for( std::uint8_t clipCounter = 0; clipCounter < numSequencesPerLayer; ++clipCounter )
		{
			SetParamElementInfo( layer.activeClipParameterId, clipCounter, std::to_string( clipCounter + 1 ).c_str(), (float)(clipCounter + 1));
		}

		// Configure a frame position parameter
		std::stringstream frame_position_ss;
		frame_position_ss << "L" << std::to_string( layer.layerNumber ) << " frame";
		SetParamInfof( layer.framePositionParameterId, frame_position_ss.str().c_str(), FF_TYPE_STANDARD );
		SetParamRange( layer.framePositionParameterId, 0, 1 );
		
		// Configure an opacity parameter
		std::stringstream opacity_ss;
		opacity_ss << "L" << std::to_string(layer.layerNumber) << " opacity";
		SetParamInfof( layer.opacityParameterId, opacity_ss.str().c_str(), FF_TYPE_ALPHA );
		SetParamRange( layer.opacityParameterId, 0, 1 );
	}

	FFGLLog::LogToHost( "Created DMX Playback source" );
}
FFResult DmxPlayback::InitGL( const FFGLViewportStruct* vp )
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

	dmxDataTextureId = shader.FindUniform( "DmxDataTexture" );

	//Use base-class init as success result so that it retains the viewport.
	return CFFGLPlugin::InitGL( vp );
}

FFResult DmxPlayback::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	//FFGL requires us to leave the context in a default state on return, so use this scoped binding to help us do that.
	ScopedShaderBinding shaderBinding( shader.GetGLID() );

	// To represent 512 DMX channels, construct an array for a 32x16 px texture consisting of 2 color channels
	// Every nth element (starting from 0) contains the RED color channel
	// Every n+1th element (starting from 0) contains the ALPHA color channel
	std::fill_n( dmxPixelDataFrame, dataFrameLength, 0 );

	for( auto& layer : layers )
	{
		RecordedSequence &activeSequenceForLayer = layer.recordedSequences.at( layer.activeClipIndex );

		if( activeSequenceForLayer.frames.empty() )
		{
			continue;
		}

		size_t numberOfFramesInSequence     = activeSequenceForLayer.frames.size();
		size_t currentFrameNumberInSequence = size_t( floor( layer.framePositionParameterValue * numberOfFramesInSequence ) );
		if( currentFrameNumberInSequence == numberOfFramesInSequence )
		{
			currentFrameNumberInSequence--;
		}

		Frame &currentFrameForLayer = activeSequenceForLayer.frames.at( currentFrameNumberInSequence );

		for( auto& dmxChannelValue : currentFrameForLayer.dmxChannelValues )
		{
			// Calculate the indexes for the color and alpha channels of the pixel that is associated with the DMX channel
			uint16_t colorChannelIndexInPixelDataArray = ( dmxChannelValue.first - 1 ) * 2;
			uint16_t alphaChannelIndexInPixelDataArray = colorChannelIndexInPixelDataArray + 1;

			// The new dmxValue must be multiplied by the layer's opacity
			unsigned char newDmxValue = (unsigned char)(dmxChannelValue.second * layer.opacityParameterValue);

			// Set the color channel and alpha channel
			// The different layers take priority in a Highest Takes Precedence (HTP) fashion. Only if the value is higher, overwrite the previous layer.
			if( newDmxValue <= dmxPixelDataFrame[ colorChannelIndexInPixelDataArray ] )
			{
				continue;
			}

			dmxPixelDataFrame[ colorChannelIndexInPixelDataArray ] = newDmxValue;

			// The alpha channel for recorded channels must always be set to 255.
			// Channels that are not in the recording, must stay transparent (0), so that multiple recordings can be layered on top of each other.
			dmxPixelDataFrame[ alphaChannelIndexInPixelDataArray ] = 255;
		}
	}

	glGenTextures( 1, &dmxDataTextureId );
	if( dmxDataTextureId == 0 )
		return false;

	//Use the scoped binding so that the context state is restored to it's default as required by ffgl.
	Scoped2DTextureBinding textureBinding( dmxDataTextureId );
	
	// Avoid color interpolation when sampling from the texture, with GL_NEAREST
	// GL_NEAREST = Returns the value of the texture element that is nearest (in Manhattan distance) to the specified texture coordinates.
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	
	// Generate a texture from the input data. A pixel data format with only RED and ALPHA is not available, but as an alternative, GL_RG also has two color channels RED and GREEN.
	// We can store the RED channel in the RED channel and the and ALPHA channel in the GREEN channel.
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RG8, 32, 16, 0, GL_RG, GL_UNSIGNED_BYTE, dmxPixelDataFrame );

	// Use a Swizzle mask to rearrange the color components of the texture again. This will only be used in the shader when retrieving colors.
	// Since we have a texture with RED and GREEN color channels, we can remap the RED onto RED and GREEN onto ALPHA.
	// Then, when sampling colors inside the shader we can use .rrra instead of the slightly more confusing .rrrg.
	GLint swizzleMask[] = { GL_RED, GL_ZERO, GL_ZERO, GL_GREEN };
	glTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask );

	quad.Draw();

	return FF_SUCCESS;
}

FFResult DmxPlayback::DeInitGL()
{
	glDeleteTextures( 1, &dmxDataTextureId );
	dmxDataTextureId = -1;

	shader.FreeGLResources();
	quad.Release();

	return FF_SUCCESS;
}

FFResult DmxPlayback::SetFloatParameter( unsigned int index, float value )
{
	for( auto& layer : layers )
	{
		if( layer.activeClipParameterId == index )
		{
			layer.activeClipParameterValue = value;
			layer.activeClipIndex          = (std::uint8_t)value - 1;

			return FF_SUCCESS;
		}

		if( layer.framePositionParameterId == index )
		{
			layer.framePositionParameterValue = value;

			return FF_SUCCESS;
		}

		if( layer.opacityParameterId == index )
		{
			layer.opacityParameterValue = value;

			return FF_SUCCESS;
		}
	}

	return FF_FAIL;
}

// Also used for FF_TYPE_FILE
FFResult DmxPlayback::SetTextParameter( unsigned int index, const char* value )
{
	for( auto& layer : layers )
	{
		for( auto& recordedSequence : layer.recordedSequences )
		{
			if( recordedSequence.recordingParameterId == index )
			{
				char* recordingFile = (char*)value;

				if( strlen( recordingFile ) == 0 )
				{
					recordedSequence.recordingParameterValue = "";
					recordedSequence.frames.clear();

					return FF_SUCCESS;
				}

				// Create a vector of <uint16_t, uint8_t vector> pairs to store the time-based DMX recording data
				// Each pair represents <DMX channel, series of DMX values for this channel over time>
				std::vector< std::pair< std::uint16_t, std::vector< uint8_t > > > dmxRecordingData;
				try
				{
					dmxRecordingData = read_csv_recording( recordingFile );
				}
				catch( std::runtime_error exception )
				{
					recordedSequence.recordingParameterValue = "";
					recordedSequence.frames.clear();

					return FF_SUCCESS;
				}

				if( dmxRecordingData.empty() )
				{
					recordedSequence.recordingParameterValue = "";
					recordedSequence.frames.clear();

					return FF_SUCCESS;
				}

				size_t numFrames = dmxRecordingData.at( 0 ).second.size();

				std::vector< Frame > recordedFrames( 0 );
				
				for( size_t frameIndex = 0; frameIndex < numFrames; ++frameIndex )
				{
					Frame frame;

					std::unordered_map< std::uint16_t, std::uint8_t > dmxChannelValues;

					for( auto& dmxChannelData : dmxRecordingData )
					{
						if( dmxChannelData.first < 1 || dmxChannelData.first > 512)
						{
							continue;
						}

						uint16_t dmxChannel = dmxChannelData.first;
						uint8_t dmxValue    = dmxChannelData.second.at( frameIndex );

						dmxChannelValues[ dmxChannel ] = dmxValue;
					}

					frame.dmxChannelValues = dmxChannelValues;

					recordedFrames.push_back( frame );
				}

				recordedSequence.recordingParameterValue = recordingFile;
				recordedSequence.frames = recordedFrames;

				return FF_SUCCESS;
			}
		}
	}

	return FF_FAIL;
}

float DmxPlayback::GetFloatParameter( unsigned int index )
{
	for( auto& layer : layers )
	{
		if( layer.activeClipParameterId == index )
		{
			return layer.activeClipParameterValue;
		}

		if( layer.framePositionParameterId == index )
		{
			return layer.framePositionParameterValue;
		}

		if( layer.opacityParameterId == index )
		{
			return layer.opacityParameterValue;
		}
	}

	return 0.0f;
}

// Also used for FF_TYPE_FILE
char* DmxPlayback::GetTextParameter( unsigned int index )
{
	for( auto& layer : layers )
	{
		for( auto& recordedSequence : layer.recordedSequences )
		{
			if( recordedSequence.recordingParameterId == index )
			{
				return recordedSequence.recordingParameterValue;
			}
		}
	}

	return (char*)FF_FAIL;
}
