﻿#include "pch.h"
#include "StyleTransferEffect.h"
#include "StyleTransferEffect.g.cpp"
using namespace std;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::StyleTransferEffectCpp::implementation
{
	StyleTransferEffect::StyleTransferEffect() : outputTransformed(VideoFrame(Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8, 720, 720)),
		Session(nullptr),
		Binding(nullptr)
	{
	}

	IVectorView<VideoEncodingProperties> StyleTransferEffect::SupportedEncodingProperties() {
		VideoEncodingProperties encodingProperties = VideoEncodingProperties();
		encodingProperties.Subtype(L"ARGB32");
		return single_threaded_vector(std::vector<VideoEncodingProperties>{encodingProperties}).GetView();
	}

	bool StyleTransferEffect::TimeIndependent() { return true; }
	MediaMemoryTypes StyleTransferEffect::SupportedMemoryTypes() { return MediaMemoryTypes::GpuAndCpu; }
	bool StyleTransferEffect::IsReadOnly() { return false; }
	void StyleTransferEffect::DiscardQueuedFrames() {}

	void StyleTransferEffect::Close(MediaEffectClosedReason m) {
		OutputDebugString(L"Close Begin | ");
		std::lock_guard<mutex> guard{ Processing };
		OutputDebugString(L"Close\n");
		if (Binding != nullptr) Binding.Clear();
		if (Session != nullptr) Session.Close();
		outputTransformed.Close();
	}


	void StyleTransferEffect::ProcessFrame(ProcessVideoFrameContext context) {
		std::lock_guard<mutex> guard{ Processing };
		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::milliseconds timePassed;
		// If the first time calling ProcessFrame, just start the timer 
		if (firstProcessFrameCall) {
			m_StartTime = now;
			firstProcessFrameCall = false;
		}
		// On the second and any proceding process, 
		else {
			timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_StartTime);
			m_StartTime = now;
			Notifier.SetFrameRate(1000.f / timePassed.count()); // Convert to FPS: milli to seconds, invert 
		}

		OutputDebugString(L"PF Start | ");

		VideoFrame inputFrame = context.InputFrame();
		VideoFrame outputFrame = context.OutputFrame();

		OutputDebugString(L"PF Locked | ");
		Binding.Bind(InputImageDescription, inputFrame);
		Binding.Bind(OutputImageDescription, outputTransformed);

		OutputDebugString(L"PF Eval | ");
		Session.Evaluate(Binding, L"test");
		outputTransformed.CopyToAsync(outputFrame).get();

		OutputDebugString(L"PF End\n ");
	}

	void StyleTransferEffect::SetEncodingProperties(VideoEncodingProperties props, IDirect3DDevice device) {
		encodingProperties = props;
	}

	void StyleTransferEffect::SetProperties(IPropertySet config) {
		this->configuration = config;
		hstring modelName;
		bool useGpu;

		IInspectable val = config.TryLookup(L"ModelName");
		if (val) modelName = unbox_value<hstring>(val);
		else winrt::throw_hresult(E_FAIL);
		val = configuration.TryLookup(L"UseGpu");
		if (val) useGpu = unbox_value<bool>(val);
		else winrt::throw_hresult(E_FAIL);
		val = configuration.TryLookup(L"Notifier");
		if (val) Notifier = val.try_as<StyleTransferEffectNotifier>();
		else winrt::throw_hresult(E_FAIL);

		LearningModel m_model = LearningModel::LoadFromFilePath(modelName);
		LearningModelDeviceKind m_device = useGpu ? LearningModelDeviceKind::DirectX : LearningModelDeviceKind::Cpu;
		Session = LearningModelSession{ m_model, LearningModelDevice(m_device) };
		Binding = LearningModelBinding{ Session };

		InputImageDescription = L"inputImage";
		OutputImageDescription = L"outputImage";
	}
}
