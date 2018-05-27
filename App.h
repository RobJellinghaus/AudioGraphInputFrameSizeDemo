// AudioGraph input frame size demo app.
// Rob Jellinghaus, 2018. https://github.com/RobJellinghaus/AudioGraphInputFrameSizeDemoApp
// Licensed under the MIT License.

#pragma once

#include "pch.h"

// From https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(novtable) IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};

const int TicksPerSecond = 10000000;

winrt::Windows::Foundation::TimeSpan timeSpanFromSeconds(int seconds);

// release assertion, basically
void Check(bool condition);

// Simple application which generates a sine wave at a fixed frequency, while changing the audio frame size.
// Basically equivalent to the sample code at https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/audio-graphs#audio-frame-input-node
// except that it allows interactively varying the audio input frame size.
// This should not affect the sound.  On some platforms, it evidently does (as of Win 10 version 1803).
// Hopefully this app will help to reproduce the issue on such platforms so it can be fixed.
struct App : winrt::Windows::UI::Xaml::ApplicationT<App>
{
    // The interaction model of this app is:
    // - App displays window with slider.
    // - Slider sets length of audio input frame, from minimum required samples up to 2 seconds.
    // - On each quantum, app creates frame of current size, fills with sine wave anchored at current frequency.
    //   - The phase of the wave is carried from frame to frame, so frame transitions never have audible clicks or pops.
    //   - This is exactly as in the sample code.
    //
    // Therefore, shorter audio frames produce smoother transitions in pitch.
    //
    // I claim the code which populates the audio input frame is free of phase errors or buffer errors that would
    // cause clicking or popping.  I encourage reviewers to validate this claim; it's the entire point of this sample,
    // to root-cause the audible clicking/popping issues with shorter frame sizes.
    //
    // The top and bottom frequencies can be changed to be the same in order to avoid any confusion or bugs
    // relating to frequency change.

public: // UI controls
    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockGraphStatus{ nullptr };
    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockGraphInfo{ nullptr };
    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockTimeInfo{ nullptr };

    winrt::Windows::UI::Xaml::Controls::TextBlock _minimumAudioFrameSize{ nullptr };
    winrt::Windows::UI::Xaml::Controls::Slider _audioFrameSizeSlider{ nullptr };
    winrt::Windows::UI::Xaml::Controls::TextBlock _maximumAudioFrameSize{ nullptr };

    winrt::Windows::UI::Xaml::Controls::StackPanel _stackPanel{ nullptr };

public: // threading
    // The apartment context of the UI thread; must co_await this before updating UI
    // (and must thereafter switch out of UI context ASAP, for liveness).
    winrt::apartment_context _uiThread;

public: // sound generation
    // current phase of sine wave -- ranges from 0 to 2*pi
    double _sineWavePhase;

    // sample rate of audio graph
    int _sampleRateHz;

    // bytes per sample (channel count * sample size in bytes)
    int _bytesPerSample;

    winrt::Windows::Media::Audio::AudioGraph _audioGraph{ nullptr };
    winrt::Windows::Media::Audio::AudioFrameInputNode _audioFrameInputNode{ nullptr };

    winrt::Windows::Foundation::DateTime _lastQuantumTime;

    // The quantum has started; consume input audio for this recording.
    void FrameInputNode_QuantumStarted(
        winrt::Windows::Media::Audio::AudioFrameInputNode sender,
        winrt::Windows::Media::Audio::FrameInputNodeQuantumStartedEventArgs args);

    // It is not clear why FrameInputNode_QuantumStarted is called with zero required bytes.
    // But it evidently is, as this variable indicates.
    int _zeroByteOutgoingFrameCount;

public: // application implementation
    winrt::fire_and_forget LaunchedAsync();

    winrt::fire_and_forget UpdateLoop();

    void OnLaunched(winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);
};

