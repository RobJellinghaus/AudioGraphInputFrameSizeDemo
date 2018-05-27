// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "app.h"

#include <string>
#include <sstream>

using namespace std::chrono;
using namespace winrt;

using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Media;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::System;

TimeSpan timeSpanFromSeconds(int seconds)
{
    // TimeSpan is in 100ns units
    return TimeSpan(seconds * TicksPerSecond);
}

void Check(bool condition)
{
    if (!condition)
    {
        DebugBreak();
        std::abort();
    }
}

void App::OnLaunched(LaunchActivatedEventArgs const&)
{
    _textBlockGraphStatus = TextBlock();
    _textBlockGraphStatus.Text(L"");
    _textBlockGraphInfo = TextBlock();
    _textBlockGraphInfo.Text(L"");
    _textBlockTimeInfo = TextBlock();
    _textBlockTimeInfo.Text(L"");

    Window xamlWindow = Window::Current();

    _stackPanel = StackPanel();
    _stackPanel.Children().Append(_textBlockGraphStatus);
    _stackPanel.Children().Append(_textBlockGraphInfo);
    _stackPanel.Children().Append(_textBlockTimeInfo);

    StackPanel sliderPanel{};
    sliderPanel.Orientation(Orientation::Horizontal);
    TextBlock sliderLabel{};
    sliderLabel.Text(L"Audio frame size: ");
    sliderPanel.Children().Append(sliderLabel);
    _minimumAudioFrameSize = TextBlock();
    _minimumAudioFrameSize.Text(L"_");
    sliderPanel.Children().Append(_minimumAudioFrameSize);
    _audioFrameSizeSlider = Slider();
    sliderPanel.Children().Append(_audioFrameSizeSlider);
    _maximumAudioFrameSize = TextBlock();
    _maximumAudioFrameSize.Text(L"_");
    sliderPanel.Children().Append(_maximumAudioFrameSize);

    _stackPanel.Children().Append(sliderPanel);

    xamlWindow.Content(_stackPanel);
    xamlWindow.Activate();

    LaunchedAsync();
}

fire_and_forget App::LaunchedAsync()
{
    apartment_context ui_thread{};
    _uiThread = ui_thread;

    AudioGraphSettings settings(AudioRenderCategory::Media);
    settings.QuantumSizeSelectionMode(Windows::Media::Audio::QuantumSizeSelectionMode::LowestLatency);
    settings.DesiredRenderDeviceAudioProcessing(Windows::Media::AudioProcessing::Raw);
    // leaving PrimaryRenderDevice uninitialized will use default output device
    CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

    Check(result.Status() == AudioGraphCreationStatus::Success);

    // Make local variable for audio graph to ensure preserved across coroutine boundaries.
    // (If just instance variable, it would become inaccessible.)
    AudioGraph audioGraph = result.Graph();
    int latencyInSamples = audioGraph.LatencyInSamples();
    int samplesPerQuantum = audioGraph.SamplesPerQuantum();
    _sampleRateHz = audioGraph.EncodingProperties().SampleRate();
    // make sure we're float encoding
    Check(audioGraph.EncodingProperties().BitsPerSample() == sizeof(float) * 8);
    // make sure we're stereo output (only mode supported for this tiny demo)
    Check(audioGraph.EncodingProperties().ChannelCount() == 2);

    _audioGraph = audioGraph;

    _bytesPerSample = sizeof(float) * 2;

    co_await _uiThread;
    std::wstringstream wstr;
    wstr << L"Sample rate: " << _sampleRateHz
        << L"| Latency in samples: " << latencyInSamples
        << " | Samples per quantum: " << samplesPerQuantum;
    _textBlockGraphInfo.Text(wstr.str());
    co_await resume_background();

    // Create a device output node
    CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await audioGraph.CreateDeviceOutputNodeAsync();

    Check(deviceOutputNodeResult.Status() == AudioDeviceNodeCreationStatus::Success);

    AudioDeviceOutputNode deviceOutputNode = deviceOutputNodeResult.DeviceOutputNode();

    audioGraph.Start();

    // This must be called on the UI thread.
    co_await _uiThread;

    _textBlockGraphStatus.Text(L"Graph started");

    co_await resume_background();

    _audioFrameInputNode = audioGraph.CreateFrameInputNode();
    _audioFrameInputNode.QuantumStarted([&](AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
    {
        FrameInputNode_QuantumStarted(sender, args);
    });
    _audioFrameInputNode.AddOutgoingConnection(deviceOutputNode);

}

void App::FrameInputNode_QuantumStarted(AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
{
    Check(sender == _audioFrameInputNode);

    Check(args.RequiredSamples() >= 0);
    uint32_t requiredSamples = (uint32_t)args.RequiredSamples();

    if (requiredSamples == 0)
    {
        _zeroByteOutgoingFrameCount++;
        return;
    }

    // sender.DiscardQueuedFrames();

    DateTime dateTimeNow = DateTime::clock::now();
    TimeSpan sinceLast = dateTimeNow - _lastQuantumTime;
    _lastQuantumTime = dateTimeNow;

    // we are looping; let's play!
    float samplesSinceLastQuantum = (float)sinceLast.count() * _sampleRateHz / TicksPerSecond;

    //_requiredSamplesHistogram.Add(requiredSamples);
    //_sinceLastSampleTimingHistogram.Add(samplesSinceLastQuantum);

    uint32_t frameSizeInSamples = requiredSamples;

    Windows::Media::AudioFrame audioFrame(frameSizeInSamples * _bytesPerSample);

    {
        // This nested scope sets the extent of the LockBuffer call below, which must close before the AddFrame call.
        // Otherwise the AddFrame will throw E_ACCESSDENIED when it tries to take a read lock on the frame.
        uint8_t* dataInBytes{};
        uint32_t capacityInBytes{};

        // OMG KENNY KERR WINS AGAIN:
        // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
        Windows::Media::AudioBuffer buffer(audioFrame.LockBuffer(Windows::Media::AudioBufferAccessMode::Write));
        IMemoryBufferReference reference(buffer.CreateReference());
        winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
        check_hresult(interop->GetBuffer(&dataInBytes, &capacityInBytes));

        Check((capacityInBytes % _bytesPerSample) == 0);
        Check(capacityInBytes / _bytesPerSample == requiredSamples);

        uint32_t samplesRemaining = requiredSamples;

        float* data = (float*)dataInBytes;
        const float amplitude = 0.8;
        const int frequencyHz = 300;
        const double pi = std::acos(-1);
        const float phaseIncrement = frequencyHz * 2 * pi / _sampleRateHz;
        for (uint32_t i = 0; i < samplesRemaining; i++)
        {
            data[i] = sin(_sineWavePhase);
            _sineWavePhase += phaseIncrement;
        }
    }

    sender.AddFrame(audioFrame);
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<App>(); });
}
