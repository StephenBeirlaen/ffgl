#pragma once
#include <FFGLSDK.h>
#include <unordered_map>

class Frame
{
	public:
		std::unordered_map< std::uint16_t, std::uint8_t > dmxChannelValues;
};

class RecordedSequence
{
	public:
		std::uint8_t sequenceNumber;
		FFUInt32 recordingParameterId;
		char* recordingParameterValue;

		std::vector< Frame > frames;
};

class Layer
{
	public:
		std::uint8_t layerNumber;

		std::vector< RecordedSequence > recordedSequences;

		FFUInt32 activeClipParameterId;
		float activeClipParameterValue;
		std::uint8_t activeClipIndex;

		FFUInt32 framePositionParameterId;
		float framePositionParameterValue;

		FFUInt32 opacityParameterId;
		float opacityParameterValue;
};

class DmxPlayback : public CFFGLPlugin
{
public:
	DmxPlayback();

	//CFFGLPlugin
	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;

	FFResult SetFloatParameter( unsigned int index, float value ) override;
	FFResult SetTextParameter( unsigned int index, const char* value ) override;

	float GetFloatParameter( unsigned int index ) override;
	char* GetTextParameter( unsigned int index ) override;

private:
	std::vector< Layer > layers; // An inmemory map of all the source's layers and their parameters

	const std::uint8_t numLayers = 16;
	const std::uint8_t numSequencesPerLayer = 10;

	const std::uint16_t dataFrameLength = 32 * 16 * 2;
	unsigned char dmxPixelDataFrame[ 32 * 16 * 2 ];

	ffglex::FFGLShader shader;  //!< Utility to help us compile and link some shaders into a program.
	ffglex::FFGLScreenQuad quad;//!< Utility to help us render a full screen quad.
	
	GLuint dmxDataTextureId;
};
