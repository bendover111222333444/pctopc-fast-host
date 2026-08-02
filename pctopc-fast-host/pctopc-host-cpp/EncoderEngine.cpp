#include "EncoderEngine.h"
#include "Debug.h"
#include <mfapi.h>										// media foundation
#include <mferror.h>									// error macros
#include <mftransform.h>								// defines transformers for media foundation
#include <mfobjects.h>									// mf objects
#include <icodecapi.h>									// interface codec definitons
#include <codecapi.h>									// codec definitions
#include <winrt/base.h>									// com ptrs
#include <winrt/Windows.Foundation.h>
#include <windows.graphics.directx.direct3d11.interop.h> 

#include <mfreadwrite.h>
#pragma comment(lib, "Mfreadwrite.lib")

class StaticMp4Writer {
public:
	static inline winrt::com_ptr<IMFSinkWriter> writer;
	static inline DWORD streamIdx = 0;

	// Pass the encoder here directly
	static void Init(Resolution res, UINT fps, UINT bitrate, IMFTransform* encoder) {
		if (writer || !encoder) return;

		CreateDirectoryW(L"C:\\test", NULL);

		winrt::com_ptr<IMFAttributes> attributes;
		MFCreateAttributes(attributes.put(), 2);
		attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

		HRESULT hr = MFCreateSinkWriterFromURL(
			L"C:\\test\\output.mp4",
			nullptr,
			attributes.get(),
			writer.put()
		);

		if (FAILED(hr)) return;

		// The generic output type for the MP4 container
		winrt::com_ptr<IMFMediaType> mediaTypeOut;
		MFCreateMediaType(mediaTypeOut.put());
		mediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		mediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		mediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
		MFSetAttributeSize(mediaTypeOut.get(), MF_MT_FRAME_SIZE, res.width, res.height);
		MFSetAttributeRatio(mediaTypeOut.get(), MF_MT_FRAME_RATE, fps, 1);
		MFSetAttributeRatio(mediaTypeOut.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

		writer->AddStream(mediaTypeOut.get(), &streamIdx);

		// --- THE FIX IS ALL CONTAINED HERE ---
		// Pull the actual, hardware-generated type directly from the encoder
		winrt::com_ptr<IMFMediaType> actualEncoderType;
		if (SUCCEEDED(encoder->GetOutputAvailableType(0, 0, actualEncoderType.put()))) {
			writer->SetInputMediaType(streamIdx, actualEncoderType.get(), nullptr);
		}
		else {
			// Fallback (will likely result in a broken MP4, but prevents crashes)
			writer->SetInputMediaType(streamIdx, mediaTypeOut.get(), nullptr);
		}

		writer->BeginWriting();
	}

	static void Write(IMFSample* sample) {
		if (writer && sample) {
			writer->WriteSample(streamIdx, sample);
		}
	}

	static void Close() {
		if (writer) {
			writer->Finalize();
			writer = nullptr;
		}
	}

	~StaticMp4Writer() { Close(); }
};

void EncoderEngine::ConvertRGBtoNV12(ID3D11Texture2D* inputTexture, ID3D11Texture2D* outputTexture) {

	winrt::com_ptr<ID3D11VideoProcessorInputView> inputView;

	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {
		.FourCC = DXGI_FORMAT_B8G8R8A8_UNORM,
		.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
	};

	Debug::LogHR("video processor creation", m_videoDevice->CreateVideoProcessorInputView(
		inputTexture,
		m_videoEnum.get(),
		&inputViewDesc,
		inputView.put()
	));

	winrt::com_ptr<ID3D11VideoProcessorOutputView> outputView;

	D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {
	
		.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
	
	};

	m_videoDevice->CreateVideoProcessorOutputView(
		outputTexture, 
		m_videoEnum.get(), 
		&outputViewDesc, 
		outputView.put()
	);

	D3D11_VIDEO_PROCESSOR_STREAM processorStream = {
		.Enable = TRUE,
		.pInputSurface = inputView.get(),
	};

	m_videoContext->VideoProcessorBlt(
		m_videoProcessor.get(), 
		outputView.get(), 
		0, 
		1, 
		&processorStream
	);

}

void EncoderEngine::CleanEncodedFrame() {

	//winrt::com_ptr<IMFMediaEvent> mediaEvent;

	//while (m_eventGenerator->GetEvent(MF_EVENT_FLAG_NO_WAIT, mediaEvent.put()) == S_OK) {

	//	MediaEventType type = MEUnknown;
	//	mediaEvent->GetType(&type);

	//	if (type == METransformHaveOutput) {

	//		MFT_OUTPUT_DATA_BUFFER outputData = {};
	//		outputData.dwStreamID = 0;
	//		outputData.pSample = nullptr;

	//		DWORD status = 0;

	//		HRESULT outputHR = m_encoder->ProcessOutput(
	//			0,										// flags
	//			1,										// number of elements 1 for one tex
	//			&outputData,							// output data receive
	//			&status									// status flags
	//		);

	//		if (outputHR == S_OK && outputData.pSample) {
	//			StaticMp4Writer::Write(outputData.pSample);
	//			outputData.pSample->Release();
	//		}

	//		if (outputData.pEvents) {
	//			outputData.pEvents->Release();
	//		}

	//	}

	//	mediaEvent = nullptr;

	//}

	MFT_OUTPUT_DATA_BUFFER outputData = {}; // output frame sample receive
	DWORD status;							// receive status flags from process output

	while (true) {

		outputData.dwStreamID = 0;			// stream id 0 
		outputData.pSample = nullptr;		// allow driver to allocate

		HRESULT outputHR = m_encoder->ProcessOutput(
			0,										// flags
			1,										// number of elements 1 for one tex
			&outputData,							// output data receive
			&status									// status flags
		);

		if (outputHR == MF_E_TRANSFORM_NEED_MORE_INPUT) {

			if (outputData.pEvents) outputData.pEvents->Release();
			break;

		}

		if (FAILED(outputHR)) {

			printf("[ProcessOutput] Return Code: 0x%08X\n", outputHR);
			if (outputData.pEvents) outputData.pEvents->Release();
			break;

		}

		if (outputData.pSample) {

			winrt::com_ptr<IMFMediaBuffer> mediaBuffer;

			if (SUCCEEDED(outputData.pSample->ConvertToContiguousBuffer(mediaBuffer.put()))) {

				BYTE* rawBytes = nullptr;
				DWORD maxLength = 0;
				DWORD currentLength = 0;

				mediaBuffer->Lock(&rawBytes, &maxLength, &currentLength);
				mediaBuffer->Unlock();
			}

			outputData.pSample->SetSampleTime(m_lastTimestamp);
			outputData.pSample->SetSampleDuration(10'000'000ULL / m_fps);

			StaticMp4Writer::Write(outputData.pSample);
			outputData.pSample->Release();
		
		}

		if (outputData.pEvents) {					// if events there

			outputData.pEvents->Release();

		}

	}

}

void EncoderEngine::EncodeFrame(ID3D11Texture2D* texture, int64_t frameTime) {

	if (!texture || !m_encoder) return;		// prevents stupidity

	if (m_firstFrameTime == -1) {

		m_firstFrameTime = frameTime;

	}

	m_lastTimestamp = frameTime - m_firstFrameTime;

	CleanEncodedFrame();

	winrt::com_ptr<IMFMediaBuffer> buffer;	// buffer
	
	MFCreateDXGISurfaceBuffer(				// 0 copy buffer
		__uuidof(ID3D11Texture2D),			// uuid of texture
		texture,							// texture
		0,									// top slice
		FALSE,								// top down
		buffer.put()						// the buffer
	);

	winrt::com_ptr<IMFSample> sample;		// timin sample object

	MFCreateSample(sample.put());			// inits the sample

	sample->AddBuffer(buffer.get());		// gives raw buffer ptr
	sample->SetSampleTime(m_lastTimestamp);		// used to give sample time
	sample->SetSampleDuration(10'000'000ULL / m_fps);

	HRESULT inputHR = Debug::LogHR("mf startup", m_encoder->ProcessInput(
		0,									// steram index still 0
		sample.get(),						// sample time
		0									// flags 0 for none
	));

	if (inputHR == MF_E_NOTACCEPTING) {

		CleanEncodedFrame();					// get claen wahtecv
		m_encoder->ProcessInput(				// actually process
			0,									// steram index still 0
			sample.get(),						// sample time
			0									// flags 0 for none
		);

	}

	CleanEncodedFrame();						// clean

}

void EncoderEngine::Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext, Resolution res, UINT fps, UINT bitrate) {

	m_fps = fps;

	Debug::LogHR("create video device", device->QueryInterface(				// creates video device
		__uuidof(ID3D11VideoDevice),
		m_videoDevice.put_void()
	));

	Debug::LogHR("create video context", deviceContext->QueryInterface(		// same here creates video context
		__uuidof(ID3D11VideoContext),
		m_videoContext.put_void()
	));

	D3D11_VIDEO_PROCESSOR_CONTENT_DESC videoProcessorDesc = {

		.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,	// progressive
		.InputFrameRate = {fps, 1},									// i dont need to discribe what this means anymore
		.InputWidth = res.width,									// height
		.InputHeight = res.height,									// width
		.OutputFrameRate = {fps, 1},								// fps
		.OutputWidth = res.width,									// width
		.OutputHeight = res.height,									// hiehgt
		.Usage = D3D11_VIDEO_USAGE_OPTIMAL_SPEED,					// speed

	};

	D3D11_TEXTURE2D_DESC nv12TextureDesc = {

		.Width = res.width,						// width
		.Height = res.height,					// height
		.MipLevels = 1,							// 2d
		.ArraySize = 1,							// one texture in one out
		.Format = DXGI_FORMAT_NV12,				// nv12 format
		.SampleDesc = {
			.Count = 1,							// one sample only
			.Quality = 0,						// no defualt quality
		},
		.Usage = D3D11_USAGE_DEFAULT,			// defualt usage
		.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,		// for shader output
		.CPUAccessFlags = 0,					// not cpu is gpu
		.MiscFlags = 0,							// none

	};

	Debug::LogHR("create video enum", m_videoDevice->CreateVideoProcessorEnumerator(
		&videoProcessorDesc,
		m_videoEnum.put()
	));

	Debug::LogHR("create init texture", device->CreateTexture2D(
		&nv12TextureDesc,
		nullptr,
		m_nv12Texture.put()
	));

	Debug::LogHR("mf startup", MFStartup(MF_VERSION));	// inits media foundation

	MFT_REGISTER_TYPE_INFO outputFilters = {
		MFMediaType_Video,								// video output
		MFVideoFormat_H264								// h264 encoding
	};

	IMFActivate** ppActivate = nullptr;					// pointers to pointers for output factory for encoders
	UINT32 encoderCount = 0;							// hardware encoder count
	bool softwareEncoder = false;						// software encoder switch

	Debug::LogHR("mf enum ex", MFTEnumEx(				// search windows for encoders
		MFT_CATEGORY_VIDEO_ENCODER,						// video encoders
		MFT_ENUM_FLAG_HARDWARE,							// harware only
		nullptr,										// input filters (any gpu format supported)
		&outputFilters,									// output filters
		&ppActivate,									// allows windows to give us the driver pointers
		&encoderCount									// allows windows to give encoder count
	));

	if (encoderCount == 0) {

		ppActivate = nullptr;

		Debug::LogHR("mf enum ex software", MFTEnumEx(		// search windows for encoders
			MFT_CATEGORY_VIDEO_ENCODER,						// video encoders
			MFT_ENUM_FLAG_SYNCMFT,							// software backup
			nullptr,										// input filters (any format supported)
			&outputFilters,									// output filters
			&ppActivate,									// allows windows to give us the driver pointers
			&encoderCount									// allows windows to give encoder count
		));

		softwareEncoder = true;

	}

	Debug::LogHR("activate object", ppActivate[0]->ActivateObject(// activates driver interface object
		__uuidof(IMFTransform),							// gets 128 bit uuid
		m_encoder.put_void()							// puts void encoder driver
	));

	winrt::com_ptr<IMFAttributes> attributes;					// attributes imf 

	if (Debug::LogHR("get attributes", m_encoder->GetAttributes(attributes.put())) == S_OK) {	// sets async to true

		Debug::LogHR("set async unlock", attributes->SetUINT32(
			MF_TRANSFORM_ASYNC_UNLOCK,
			TRUE
		));

	}

	Debug::LogHR("query event generator", m_encoder->QueryInterface(
		__uuidof(IMFMediaEventGenerator),
		m_eventGenerator.put_void()
	));

	for (UINT32 i = 0; i < encoderCount; i++) ppActivate[i]->Release();	// releases all encoder pointers
	CoTaskMemFree(ppActivate); // frees Enum ex's allocated memory

	winrt::com_ptr<IMFDXGIDeviceManager> imfDevice;
	UINT token = 0; // IMF security token

	Debug::LogHR("create dxgi device manager", MFCreateDXGIDeviceManager(	// creates the actual device with unique token and everything
		&token,					// reference to token
		imfDevice.put()			// gives device pointer to device manager
	));

	Debug::LogHR("reset device", imfDevice->ResetDevice(device, token));	// gives id3d11 device to imf device so they know eachother

	Debug::LogHR("set d3d manager", m_encoder->ProcessMessage(				// sets the encoder's device manager
		MFT_MESSAGE_SET_D3D_MANAGER,		// enum encoder message setting
		ULONG_PTR(imfDevice.get())			// gives imf device pointer and casts into long ptrs in which it understands
	));

	winrt::com_ptr<IMFMediaType> outputMediaType;

	Debug::LogHR("create output media type", MFCreateMediaType(outputMediaType.put()));		// self explainitory
	Debug::LogHR("set output major type", outputMediaType->SetGUID(							// sets major media classification
		MF_MT_MAJOR_TYPE,																	// major of type
		MFMediaType_Video																	// obv video
	));

	Debug::LogHR("set output subtype", outputMediaType->SetGUID(							// sets encode type
		MF_MT_SUBTYPE,																		// video subtype
		MFVideoFormat_H264																	// h264 encoding
	));

	Debug::LogHR("set output bitrate", outputMediaType->SetUINT32(							// sets bitrate
		MF_MT_AVG_BITRATE,																	// average bitrate attribute
		bitrate																				// the bitrate
	));

	Debug::LogHR("set output frame size", MFSetAttributeSize(									// sets size of output
		outputMediaType.get(),							// gives other media type info
		MF_MT_FRAME_SIZE,								// attribute
		res.width,										// width
		res.height										// height
	));

	Debug::LogHR("set output frame rate", MFSetAttributeRatio(								// sets fps
		outputMediaType.get(),							// gives other media type info
		MF_MT_FRAME_RATE,								// setting (frame rate attribute)
		fps,											// fps
		1												// denominator
	));

	Debug::LogHR("set output type", m_encoder->SetOutputType(							// sets the actual settigns to the encoder
		0,												// stream id (0)
		outputMediaType.get(),							// media type settings
		0												// flags (0) for none
	));

	winrt::com_ptr<IMFMediaType> inputMediaType;		// input media

	Debug::LogHR("create input media type", MFCreateMediaType(inputMediaType.put()));			// same as output

	Debug::LogHR("set input major type", inputMediaType->SetGUID(							// sets major media classification
		MF_MT_MAJOR_TYPE,								// major of type
		MFMediaType_Video								// obv video
	));

	Debug::LogHR("set input subtype", inputMediaType->SetGUID(							// sets input to encode
		MF_MT_SUBTYPE,									// video subtype
		MFVideoFormat_NV12								// r8b8g8a8/argb32 format
	));

	Debug::LogHR("set input frame size", MFSetAttributeSize(									// sets size of output
		inputMediaType.get(),							// gives other media type info
		MF_MT_FRAME_SIZE,								// attribute
		res.width,										// width
		res.height										// height
	));

	Debug::LogHR("set input frame rate", MFSetAttributeRatio(								// sets fps
		inputMediaType.get(),							// gives other media type info
		MF_MT_FRAME_RATE,								// setting (frame rate attribute)
		fps,											// fps
		1												// denominator
	));

	Debug::LogHR("set input type", m_encoder->SetInputType(							// sets the actual settigns to the encoder
		0,												// stream id (0)
		inputMediaType.get(),							// media type settings
		0												// flags (0) for none
	));

	winrt::com_ptr<ICodecAPI> codec;

	if (Debug::LogHR("query codec api", m_encoder->QueryInterface(							// checks if codec api exists
		__uuidof(ICodecAPI),
		codec.put_void()
	)) == S_OK) {

		VARIANT encoderVariant;								// sets temp to unsinted int used to make encoder not produce b frames
		VariantInit(&encoderVariant);						// init variant

		if (codec->IsSupported(&CODECAPI_AVEncMPVDefaultBPictureCount) == S_OK) {

			encoderVariant.vt = VT_UI4;							// unsigned int type
			encoderVariant.ulVal = 0;							// set it to zero

			Debug::LogHR("set b picture count", codec->SetValue(									// binds encoder variant to b picture count
				&CODECAPI_AVEncMPVDefaultBPictureCount,			// b picture count macro
				&encoderVariant									// encoder variant settings
			));
			VariantClear(&encoderVariant);

		}

		if (codec->IsSupported(&CODECAPI_AVEncCommonRealTime) == S_OK) {

			encoderVariant.vt = VT_BOOL;						// changes variant to a bool
			encoderVariant.boolVal = VARIANT_TRUE;				// true macro for bool val

			Debug::LogHR("set real time", codec->SetValue(									// sets to real time
				&CODECAPI_AVEncCommonRealTime,					// real time macro
				&encoderVariant									// boolean variant to true
			));

			VariantClear(&encoderVariant);						// clear it

		}

		if (codec->IsSupported(&CODECAPI_AVEncCommonLowLatency) == S_OK) {

			encoderVariant.vt = VT_BOOL;						// changes variant to a bool
			encoderVariant.boolVal = VARIANT_TRUE;				// true macro for bool val

			Debug::LogHR("set low latency", codec->SetValue(									// sets low latency
				&CODECAPI_AVEncCommonLowLatency,				// low laatency macro
				&encoderVariant									// boolean variant to true
			));
			VariantClear(&encoderVariant);						// clear it

		}

		if (codec->IsSupported(&CODECAPI_AVEncCommonQualityVsSpeed) == S_OK) {

			encoderVariant.vt = VT_UI4;							// back to unsigned int
			encoderVariant.ulVal = 0;							// unsigned int set to 0

			Debug::LogHR("set quality vs speed", codec->SetValue(									// sets quality vs speed
				&CODECAPI_AVEncCommonQualityVsSpeed,			// quality macro
				&encoderVariant									// quality to zero
			));
			VariantClear(&encoderVariant);						// clear it

		}

	};

	Debug::LogHR("begin streaming message", m_encoder->ProcessMessage(							// notifies to begin streaming
		MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,				// streaming enum
		0												// args
	));

	Debug::LogHR("start stream message", m_encoder->ProcessMessage(							// notifies to begin streaming
		MFT_MESSAGE_NOTIFY_START_OF_STREAM,				// streaming start enum
		0												// args
	));

	StaticMp4Writer::Init(res, fps, bitrate, m_encoder.get());

}

void EncoderEngine::Destroy() {

	m_encoder->ProcessMessage(
		MFT_MESSAGE_COMMAND_DRAIN,					// forces GPU to process all queued frames
		0
	);

	CleanEncodedFrame();

	m_encoder->ProcessMessage(
		MFT_MESSAGE_NOTIFY_END_OF_STREAM,			// notify end of stream
		0
	);

	m_encoder->ProcessMessage(
		MFT_MESSAGE_NOTIFY_END_STREAMING,			// notify end streaming
		0
	);
	
	StaticMp4Writer::Close();

	MFShutdown();

}

void EncoderEngine::StartFrame(const winrt_capture::Direct3D11CaptureFrame& frame, ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, HWND hWindow) {

	if (frame != nullptr) {

		winrt_d3d11::IDirect3DSurface surface = frame.Surface();

		auto interopAccess = surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();				// new windows texture
		winrt::com_ptr<ID3D11Texture2D> rawTexture;

		interopAccess->GetInterface(																							// transfers interopacess
			__uuidof(ID3D11Texture2D),																							// interface id
			rawTexture.put_void()																								// output pointer
		);

		if (rawTexture && backBuffer) {

			context->CopyResource(backBuffer, rawTexture.get());
			ConvertRGBtoNV12(backBuffer, m_nv12Texture.get());
			EncodeFrame(m_nv12Texture.get(), frame.SystemRelativeTime().count());
			::PostMessageW(
				hWindow,
				WM_RENDER_UI_MESSAGE,
				0,
				0
			);

		}

		surface.Close();
		frame.Close();

	}

}

void EncoderEngine::CloseFrame(IDXGISwapChain* swapChain) {

	swapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);

}