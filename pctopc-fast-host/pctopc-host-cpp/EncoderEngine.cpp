#include "EncoderEngine.h"
#include "Debug.h"
#include <winrt/base.h>									// com ptrs
#include <winrt/Windows.Foundation.h>
#include <windows.graphics.directx.direct3d11.interop.h> 

EncoderEngine::~EncoderEngine() { Destroy(); }

class StaticMp4Writer {
public:

	static std::mutex m_writerMutex;

	static inline FILE* m_file = nullptr;

	static void Init() {
		
		_wfopen_s(&m_file, L"C:\\test\\output.h264", L"wb");
	
	}

	static void Write(const uint8_t* data, size_t size) {

		std::lock_guard<std::mutex> lock(m_writerMutex);
		if (m_file && data && size > 0) {
		
			fwrite(data, 1, size, m_file);
		
		}

	}

	static void Close() {
		
		if (m_file) {
		
			fclose(m_file);
			m_file = nullptr;
		
		}

	}

};

std::mutex StaticMp4Writer::m_writerMutex;

void EncoderEngine::ConvertRGBtoNV12(ID3D11Texture2D* inputTexture, ID3D11Texture2D* outputTexture) {

	winrt::com_ptr<ID3D11VideoProcessorInputView> inputView;

	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {};
	inputViewDesc.FourCC = DXGI_FORMAT_B8G8R8A8_UNORM;
	inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;

	Debug::LogHR("video processor creation", m_videoDevice->CreateVideoProcessorInputView(
		inputTexture,
		m_videoEnum.get(),
		&inputViewDesc,
		inputView.put()
	));

	winrt::com_ptr<ID3D11VideoProcessorOutputView> outputView;

	D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {};
	outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;

	Debug::LogHR("video processor output view", m_videoDevice->CreateVideoProcessorOutputView(
		outputTexture,
		m_videoEnum.get(),
		&outputViewDesc,
		outputView.put()
	));

	D3D11_VIDEO_PROCESSOR_STREAM processorStream = {};
	processorStream.Enable = TRUE;
	processorStream.pInputSurface = inputView.get();

	Debug::LogHR("video processor blt", m_videoContext->VideoProcessorBlt(
		m_videoProcessor.get(),
		outputView.get(),
		0,
		1,
		&processorStream
	));

}

void EncoderEngine::EncodeFrame(ID3D11Texture2D* texture, int64_t frameTime) {

	if (!m_encoderSession || !texture) return;

	if (m_firstFrameTime == -1) {
		m_firstFrameTime = frameTime;
	}
	m_lastTimestamp = frameTime - m_firstFrameTime;
	
	NV_ENC_REGISTER_RESOURCE registerTextureParams = {};
	registerTextureParams.version = NV_ENC_REGISTER_RESOURCE_VER;
	registerTextureParams.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
	registerTextureParams.resourceToRegister = texture;
	registerTextureParams.width = m_res.width;
	registerTextureParams.height = m_res.height;
	registerTextureParams.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;

	NVENCSTATUS currentStatus = m_nvenc.nvEncRegisterResource(m_encoderSession, &registerTextureParams);
	Debug::LogNV("register resource", currentStatus);
	if (currentStatus != NV_ENC_SUCCESS) return;					// return if not suceed

	NV_ENC_MAP_INPUT_RESOURCE mapParams = {};
	mapParams.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
	mapParams.registeredResource = registerTextureParams.registeredResource;

	currentStatus = m_nvenc.nvEncMapInputResource(m_encoderSession, &mapParams);
	Debug::LogNV("map resource", currentStatus);
	if(currentStatus != NV_ENC_SUCCESS) return;					// return if not suceed

	NV_ENC_PIC_PARAMS picParams = {};
	picParams.version = NV_ENC_PIC_PARAMS_VER;
	picParams.inputBuffer = mapParams.mappedResource;
	picParams.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12;
	picParams.inputWidth = m_res.width;
	picParams.inputHeight = m_res.height;
	picParams.outputBitstream = m_bitstreamBuffers[m_currentBufferIndex];
	picParams.inputTimeStamp = static_cast<uint64_t>(m_lastTimestamp);
	picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

	currentStatus = m_nvenc.nvEncEncodePicture(m_encoderSession, &picParams);
	Debug::LogNV("encode picture", currentStatus);
	if (currentStatus != NV_ENC_SUCCESS && currentStatus != NV_ENC_ERR_NEED_MORE_INPUT) return;	// return if not suceed but allows more input

	NV_ENC_LOCK_BITSTREAM bitstreamLockParams = {};
	bitstreamLockParams.version = NV_ENC_LOCK_BITSTREAM_VER;
	bitstreamLockParams.outputBitstream = m_bitstreamBuffers[m_currentBufferIndex];

	if (currentStatus == NV_ENC_SUCCESS) {

		NV_ENC_LOCK_BITSTREAM bitstreamLockParams = {};
		bitstreamLockParams.version = NV_ENC_LOCK_BITSTREAM_VER;
		bitstreamLockParams.outputBitstream = m_bitstreamBuffers[m_currentBufferIndex];

		NVENCSTATUS lockStatus = m_nvenc.nvEncLockBitstream(m_encoderSession, &bitstreamLockParams);
		Debug::LogNV("lock resource", lockStatus);

		if (lockStatus == NV_ENC_SUCCESS) {
			
			StaticMp4Writer::Write((const uint8_t*)bitstreamLockParams.bitstreamBufferPtr, bitstreamLockParams.bitstreamSizeInBytes);
			lockStatus = m_nvenc.nvEncUnlockBitstream(m_encoderSession, m_bitstreamBuffers[m_currentBufferIndex]);
			Debug::LogNV("unlock resource", lockStatus);
		
		}

	}

}

void EncoderEngine::Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext, Resolution res, UINT fps, UINT bitrate) {

	m_encoderSession = nullptr;
	m_currentBufferIndex = 0;
	m_firstFrameTime = -1;
	m_res = res;
	m_fps = fps;

	Debug::LogHR("create video device", device->QueryInterface(				// creates video device
		__uuidof(ID3D11VideoDevice),
		m_videoDevice.put_void()
	));

	Debug::LogHR("create video context", deviceContext->QueryInterface(		// same here creates video context
		__uuidof(ID3D11VideoContext),
		m_videoContext.put_void()
	));

	D3D11_VIDEO_PROCESSOR_CONTENT_DESC videoProcessorDesc = {};
	videoProcessorDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;	// progressive
	videoProcessorDesc.InputFrameRate = { fps, 1 };								// i dont need to discribe what this means anymore
	videoProcessorDesc.InputWidth = m_res.width;								// height
	videoProcessorDesc.InputHeight = m_res.height;								// width
	videoProcessorDesc.OutputFrameRate = { fps, 1 };							// fps
	videoProcessorDesc.OutputWidth = m_res.width;								// width
	videoProcessorDesc.OutputHeight = m_res.height;								// hiehgt
	videoProcessorDesc.Usage = D3D11_VIDEO_USAGE_OPTIMAL_SPEED;					// speed

	D3D11_TEXTURE2D_DESC nv12TextureDesc = {};
	nv12TextureDesc.Width = m_res.width;					// width
	nv12TextureDesc.Height = m_res.height;					// height
	nv12TextureDesc.MipLevels = 1;							// 2d
	nv12TextureDesc.ArraySize = 1;							// one texture in one out
	nv12TextureDesc.Format = DXGI_FORMAT_NV12;				// nv12 format
	nv12TextureDesc.SampleDesc.Count = 1;					// one sample only
	nv12TextureDesc.SampleDesc.Quality = 0;					// no defualt quality
	nv12TextureDesc.Usage = D3D11_USAGE_DEFAULT;			// defualt usage
	nv12TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;		// for shader output
	nv12TextureDesc.CPUAccessFlags = 0;						// not cpu is gpu
	nv12TextureDesc.MiscFlags = 0;							// none

	Debug::LogHR("create video enum", m_videoDevice->CreateVideoProcessorEnumerator(
		&videoProcessorDesc,
		m_videoEnum.put()
	));

	Debug::LogHR("create video processor", m_videoDevice->CreateVideoProcessor(
		m_videoEnum.get(),
		0,
		m_videoProcessor.put()
	));

	Debug::LogHR("create init texture", device->CreateTexture2D(
		&nv12TextureDesc,
		nullptr,
		m_nv12Texture.put()
	));

	m_nvencModule = LoadLibraryA("nvEncodeAPI64.dll");
	if (!m_nvencModule) { printf("[NVENC ERROR] failed to load nvEncodeAPI64.dll\n"); return; }

	typedef NVENCSTATUS(NVENCAPI* NvEncodeAPIGetMaxSupportedVersion_t)(uint32_t*);
	NvEncodeAPIGetMaxSupportedVersion_t getMaxVersionFunc = (NvEncodeAPIGetMaxSupportedVersion_t)GetProcAddress(m_nvencModule, "NvEncodeAPIGetMaxSupportedVersion");
	if (!getMaxVersionFunc) { printf("[NVENC ERROR] failed to find NvEncodeAPIGetMaxSupportedVersion\n"); return; }

	uint32_t maxSupportedVersion = 0;
	getMaxVersionFunc(&maxSupportedVersion);

	uint32_t headerVersion = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
	if (headerVersion > maxSupportedVersion) {
		printf("[NVENC ERROR] driver max supported version: %u, header version: %u\n", maxSupportedVersion, headerVersion);
		return;
	}

	m_nvenc.version = NV_ENCODE_API_FUNCTION_LIST_VER;

	typedef NVENCSTATUS(NVENCAPI* NvEncodeAPICreateInstance_t)(NV_ENCODE_API_FUNCTION_LIST*);
	NvEncodeAPICreateInstance_t createInstanceFunc = (NvEncodeAPICreateInstance_t)GetProcAddress(m_nvencModule, "NvEncodeAPICreateInstance");
	if (!createInstanceFunc) return;

	NVENCSTATUS currentStatus = createInstanceFunc(&m_nvenc);
	Debug::LogNV("create instance", currentStatus);

	if (currentStatus == NV_ENC_SUCCESS) {

		NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sessionParams = {};
		sessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;		// matching version
		sessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;					// directx device
		sessionParams.device = device;											// actual device
		sessionParams.apiVersion = NVENCAPI_VERSION;							// api version

		currentStatus = m_nvenc.nvEncOpenEncodeSessionEx(&sessionParams, &m_encoderSession);
		Debug::LogNV("create session", currentStatus);
		if (currentStatus != NV_ENC_SUCCESS) return;							// return if not suceed

		NV_ENC_PRESET_CONFIG presetConfig = {};
		presetConfig.version = NV_ENC_PRESET_CONFIG_VER;						// default config
		presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;						// defualt config

		currentStatus = m_nvenc.nvEncGetEncodePresetConfigEx(
			m_encoderSession,											// session
			NV_ENC_CODEC_H264_GUID,										// h264 encoding
			NV_ENC_PRESET_P1_GUID,										// low latency
			NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY,						// low latrency
			&presetConfig												// preset config
		);

		Debug::LogNV("encoder preset ex", currentStatus);
		if (currentStatus != NV_ENC_SUCCESS) return;					// return if not suceed

		presetConfig.presetCfg.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;				// constant bitrate
		presetConfig.presetCfg.rcParams.averageBitRate = bitrate;							// actual bitrate
		presetConfig.presetCfg.rcParams.maxBitRate = bitrate;								// bitrate max
		presetConfig.presetCfg.rcParams.enableLookahead = 0;								// no lookahead
		presetConfig.presetCfg.encodeCodecConfig.h264Config.repeatSPSPPS = 1;				// allows midstream broadcast

		NV_ENC_INITIALIZE_PARAMS initParams = {};
		initParams.version = NV_ENC_INITIALIZE_PARAMS_VER;					// defualt vers
		initParams.encodeGUID = NV_ENC_CODEC_H264_GUID;						// global identifier
		initParams.presetGUID = NV_ENC_PRESET_P1_GUID;						// low latency preset
		initParams.encodeWidth = m_res.width;								// encode width
		initParams.encodeHeight = m_res.height;								// encode height
		initParams.darWidth = m_res.width;									// i think this is output width
		initParams.darHeight = m_res.height;								// same too
		initParams.frameRateNum = m_fps;									// fps numerator
		initParams.frameRateDen = 1;										// div 1 denom
		initParams.enablePTD = 1;											// enables picture decision
		initParams.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;		// ULTRA low LATENCY!

		currentStatus = m_nvenc.nvEncInitializeEncoder(
			m_encoderSession,											// session
			&initParams													// params
		);

		Debug::LogNV("init encoder", currentStatus);
		if (currentStatus != NV_ENC_SUCCESS) return;					// return if not suceed

		for (int i = 0; i < m_bufferCount; i++) {

			NV_ENC_CREATE_BITSTREAM_BUFFER creationBuffer = {};
			creationBuffer.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;			// defualt buffer macro

			currentStatus = m_nvenc.nvEncCreateBitstreamBuffer(m_encoderSession, &creationBuffer);	// create actual buffer in m_nvenc func strut
			Debug::LogNV("init bitstream " + static_cast<char>(i), currentStatus);
			if (currentStatus != NV_ENC_SUCCESS) return;				// return if not suceed

			m_bitstreamBuffers[i] = creationBuffer.bitstreamBuffer;

		}

		StaticMp4Writer::Init();

	}

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

void EncoderEngine::Destroy() {

	if (!m_encoderSession) return;

	std::lock_guard<std::mutex> lock(StaticMp4Writer::m_writerMutex);

	NV_ENC_PIC_PARAMS eosParams = {};
	eosParams.version = NV_ENC_PIC_PARAMS_VER;
	eosParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;

	m_nvenc.nvEncEncodePicture(m_encoderSession, &eosParams);

	for (int i = 0; i < m_bufferCount; i++) {

		m_nvenc.nvEncDestroyBitstreamBuffer(m_encoderSession, m_bitstreamBuffers[i]);
	
	}

	m_nvenc.nvEncDestroyEncoder(m_encoderSession);
	m_encoderSession = nullptr;

	StaticMp4Writer::Close();

}