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
    _audioFrameSizeSlider.MinWidth(300);
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
    _samplesPerQuantum = audioGraph.SamplesPerQuantum();
    _sampleRateHz = audioGraph.EncodingProperties().SampleRate();
    _channelCount = audioGraph.EncodingProperties().ChannelCount();

    _audioGraphQuantumCount = 0;
    _audioGraphQuantumSampleCount = 0;

    // make sure we're float encoding
    Check(audioGraph.EncodingProperties().BitsPerSample() == sizeof(float) * 8);
    // make sure we're stereo output (only mode supported for this tiny demo)
    Check(_channelCount == 2);

    _audioGraph = audioGraph;
    _audioGraphQuantumCount = 0;

    _bytesPerSample = sizeof(float) * _channelCount;

    // switch to UI thread to update UI, now that we know the audio graph's state
    co_await _uiThread;
    std::wstringstream wstr;
    wstr << L"Sample rate: " << _sampleRateHz
        << L"| Latency in samples: " << latencyInSamples
        << " | Samples per quantum: " << _samplesPerQuantum;
    _textBlockGraphInfo.Text(wstr.str());

    wstr = std::wstringstream{};
    wstr << _samplesPerQuantum;
    _minimumAudioFrameSize.Text(wstr.str());
    _audioFrameSizeSlider.Minimum(_samplesPerQuantum);
    _audioInputFrameLengthInSamples = _samplesPerQuantum;
    _audioFrameSizeSlider.Maximum(_sampleRateHz * 2);
    _audioFrameSizeSlider.Value(_samplesPerQuantum);
    wstr = std::wstringstream{};
    wstr << _audioFrameSizeSlider.Maximum();
    _maximumAudioFrameSize.Text(wstr.str());
    co_await resume_background();

    // Create a device output node
    CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await audioGraph.CreateDeviceOutputNodeAsync();

    Check(deviceOutputNodeResult.Status() == AudioDeviceNodeCreationStatus::Success);

    AudioDeviceOutputNode deviceOutputNode = deviceOutputNodeResult.DeviceOutputNode();

    _audioGraph.QuantumStarted([&](AudioGraph, IInspectable)
    {
        _audioGraphQuantumSampleCount += _samplesPerQuantum;
        _audioGraphQuantumCount++;

        // kenneth, what is the frequency
        const double sineWaveFrequencyChange = ((float)_samplesPerQuantum / _sampleRateHz) / FrequencyCycleTimeSecs * (MaximumFrequencyHz - MinimumFrequencyHz);
        if (_isSineWaveFrequencyDescending)
        {
            _sineWaveFrequency -= sineWaveFrequencyChange;
            if (_sineWaveFrequency < MinimumFrequencyHz)
            {
                _isSineWaveFrequencyDescending = false;
            }
        }
        else
        {
            _sineWaveFrequency += sineWaveFrequencyChange;
            if (_sineWaveFrequency > MaximumFrequencyHz)
            {
                _isSineWaveFrequencyDescending = true;
            }
        }
    });

    audioGraph.Start();

    // This must be called on the UI thread.
    co_await _uiThread;
    _textBlockGraphStatus.Text(L"Graph started");
    co_await resume_background();

    // Create the frame input node AFTER the graph has started.
    // (This is the scenario used by the app I'm writing, which has to work free of crackling/static issues in playback.)
    _audioFrameInputNode = audioGraph.CreateFrameInputNode();
    _audioFrameInputNode.QuantumStarted([&](AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
    {
        FrameInputNode_QuantumStarted(sender, args);
    });
    _audioFrameInputNode.AddOutgoingConnection(deviceOutputNode);

    Check(_audioFrameInputNode.EncodingProperties().SampleRate() == _sampleRateHz);
    Check(_audioFrameInputNode.EncodingProperties().ChannelCount() == _channelCount);

    UpdateLoop();
}

fire_and_forget App::UpdateLoop()
{
    // infinite UI update loop
    while (true)
    {
        // top of loop is always in background

        // wait in intervals of 1/100 sec
        co_await resume_after(TimeSpan((int)(TicksPerSecond * 0.01)));
        
        co_await _uiThread;

        // Update audio input frame length from slider.  (Value() method evidently is not agile.)
        _audioInputFrameLengthInSamples = (uint32_t)_audioFrameSizeSlider.Value();

        std::wstringstream wstr{};
        wstr << "Input frame length: " << _audioInputFrameLengthInSamples
            << " | Audio graph quanta: " << _audioGraphQuantumCount
            << " | Audio graph samples: " << _audioGraphQuantumSampleCount
            << " | Input frame samples: " << _audioInputFrameSampleCount
            << " | Zero frame count: " << _zeroByteOutgoingFrameCount
            << " | Sine wave frequency: " << _sineWaveFrequency;
        _textBlockTimeInfo.Text(wstr.str());

        co_await resume_background();
    }
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

    DateTime dateTimeNow = DateTime::clock::now();
    TimeSpan sinceLast = dateTimeNow - _lastQuantumTime;
    _lastQuantumTime = dateTimeNow;

    // we are looping; let's play!
    // float samplesSinceLastQuantum = (float)sinceLast.count() * _sampleRateHz / TicksPerSecond;
    // _requiredSamplesHistogram.Add(requiredSamples);
    // _sinceLastSampleTimingHistogram.Add(samplesSinceLastQuantum);

    uint32_t frameSizeInSamples = _audioInputFrameLengthInSamples;
    _audioInputFrameSampleCount += frameSizeInSamples;

    // Stereo float samples outbound in this audio frame.
    Windows::Media::AudioFrame audioFrame(frameSizeInSamples * sizeof(float) * _channelCount);

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
        Check(capacityInBytes / _bytesPerSample == frameSizeInSamples);

        float* data = (float*)dataInBytes;
        const double amplitude = 0.8;
        const double pi = std::acos(-1);
        // The current frequency only affects the change in phase; it does not actually alter the phase
        // at any point. So there should never be any audible clicks or pops as a result of a frequency
        // change here.
        const double phaseIncrement = _sineWaveFrequency * 2 * pi / _sampleRateHz;
        for (uint32_t i = 0; i < frameSizeInSamples; i++)
        {
            // set both stereo channels to same sine value
            data[i * 2] = data[i * 2 + 1] = (float)sin(_sineWavePhase);
            _sineWavePhase += phaseIncrement;
            if (_sineWavePhase > 2 * pi)
            {
                _sineWavePhase -= 2 * pi;
            }
        }
    }

    sender.AddFrame(audioFrame);
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<App>(); });
}
