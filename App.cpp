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
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
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
    
    // NOTE that if this logic is inlined into the create_task lambda in InitializeAsync,
    // this assignment blows up saying that it is assigning to a value of 0xFFFFFFFFFFFF.
    // Probable compiler bug?  TODO: replicate the bug in test app.
    AudioGraph audioGraph = result.Graph();
    int latencyInSamples = audioGraph.LatencyInSamples();
    int samplesPerQuantum = audioGraph.SamplesPerQuantum();

    co_await _uiThread;
    std::wstringstream wstr;
    wstr << L"Latency in samples: " << latencyInSamples << " | Samples per quantum: " << samplesPerQuantum;
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
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<App>(); });
}
