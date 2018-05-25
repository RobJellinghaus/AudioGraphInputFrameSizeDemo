// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

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

const int TicksPerSecond = 10000000;

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

// Simple application which exercises NowSoundLib, allowing test of basic looping.
struct App : ApplicationT<App>
{
    // The interaction model of this app is:
    // - App opens dialog to load sound file.
    // - App displays window with slider.
    // - Slider sets length of audio input frame used for looping.
    // - App plays sound file in a continuous loop.

    // Label string.
    const std::wstring AudioGraphStateString = L"Audio graph state: ";

    TextBlock _textBlockGraphStatus{ nullptr };
    TextBlock _textBlockGraphInfo{ nullptr };
    TextBlock _textBlockTimeInfo{ nullptr };

    TextBlock _minimumAudioFrameSize{ nullptr };
    Slider _audioFrameSizeSlider{ nullptr };
    TextBlock _maximumAudioFrameSize{ nullptr };

    StackPanel _stackPanel{ nullptr };

    static int _nextTrackNumber;

    // The apartment context of the UI thread; must co_await this before updating UI
    // (and must thereafter switch out of UI context ASAP, for liveness).
    apartment_context _uiThread;

    const int BytesPerSample = sizeof(float) * 2; // stereo float samples
    const int SampleRate = 48000; // only one supported sample rate to simplify testing

    std::unique_ptr<byte> _buffer;

    fire_and_forget LaunchedAsync();

    void OnLaunched(LaunchActivatedEventArgs const&)
    {
        _textBlockGraphStatus = TextBlock();
        _textBlockGraphStatus.Text(AudioGraphStateString);
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
        sliderPanel.Orientation = Orientation::Horizontal;
        TextBlock sliderLabel{};
        sliderLabel.Text = "Audio frame size: ";
        sliderPanel.Children().Append(sliderLabel);
        _minimumAudioFrameSize = TextBlock();
        _minimumAudioFrameSize.Text = "_";
        sliderPanel.Children().Append(_minimumAudioFrameSize);
        _audioFrameSizeSlider = Slider();
        sliderPanel.Children().Append(_audioFrameSizeSlider);
        _maximumAudioFrameSize = TextBlock();
        _maximumAudioFrameSize.Text = "_";
        sliderPanel.Children().Append(_maximumAudioFrameSize);

        _stackPanel.Children().Append(sliderPanel);

        xamlWindow.Content(_stackPanel);
        xamlWindow.Activate();

        LaunchedAsync();
    }
};

int App::_nextTrackNumber{ 1 };

fire_and_forget App::LaunchedAsync()
{
    apartment_context ui_thread{};
    _uiThread = ui_thread;

    AudioGraphSettings settings(AudioRenderCategory::Media);
    settings.QuantumSizeSelectionMode(Windows::Media::Audio::QuantumSizeSelectionMode::LowestLatency);
    settings.DesiredRenderDeviceAudioProcessing(Windows::Media::AudioProcessing::Raw);
    // leaving PrimaryRenderDevice uninitialized will use default output device
    CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

    Check(result.Status() != AudioGraphCreationStatus::Success);
    
    // NOTE that if this logic is inlined into the create_task lambda in InitializeAsync,
    // this assignment blows up saying that it is assigning to a value of 0xFFFFFFFFFFFF.
    // Probable compiler bug?  TODO: replicate the bug in test app.
    AudioGraph audioGraph = result.Graph();
    int latencyInSamples = audioGraph.LatencyInSamples;
    int samplesPerQuantum = audioGraph.SamplesPerQuantum;

    co_await _uiThread;
    std::wstringstream wstr;
    wstr << L"Latency in samples: " << latencyInSamples << " | Samples per quantum: " << samplesPerQuantum;
    _textBlockGraphInfo.Text(wstr.str());
    co_await resume_background();

    // Create a device output node
    CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await audioGraph.CreateDeviceOutputNodeAsync();

    if (deviceOutputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
    {
        // Cannot create device output node
        Check(false);
        return;
    }

    AudioDeviceOutputNode deviceOutputNode = deviceOutputNodeResult.DeviceOutputNode();

    audioGraph.Start();

    // This must be called on the UI thread.
    co_await _uiThread;
    FileOpenPicker picker;
    picker.SuggestedStartLocation(PickerLocationId::MusicLibrary);
    picker.FileTypeFilter().Append(L".wav");
    StorageFile file = co_await picker.PickSingleFileAsync();

    if (!file)
    {
        Check(false);
        return;
    }

    CreateAudioFileInputNodeResult fileInputResult = co_await audioGraph.CreateFileInputNodeAsync(file);
    Check(AudioFileNodeCreationStatus::Success != fileInputResult.Status());
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<App>(); });
}
