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

class App;

// A given Tone represents a single sine wave input to the audio graph, along with the
// UI controls that manipulate the Tone.
class Tone
{
public: // constants
        // Minimum frequency (and starting frequency as it happens).
    const uint32_t MinimumFrequencyHz = 300;
    // Maximum frequency.
    const uint32_t MaximumFrequencyHz = 900;
    // Time to cycle from min to max. Also time to cycle from max back to min.
    const uint32_t FrequencyCycleTimeSecs = 4;

private:
    winrt::Windows::UI::Xaml::Controls::StackPanel _stackPanel{ nullptr };

    winrt::Windows::UI::Xaml::Controls::TextBlock _preTextBlock{ nullptr };

    winrt::Windows::UI::Xaml::Controls::TextBlock _minimumAudioFrameSize{ nullptr };
    winrt::Windows::UI::Xaml::Controls::Slider _audioFrameSizeSlider{ nullptr };
    winrt::Windows::UI::Xaml::Controls::TextBlock _maximumAudioFrameSize{ nullptr };

    winrt::Windows::UI::Xaml::Controls::TextBlock _checkBoxLabel{ nullptr };
    winrt::Windows::UI::Xaml::Controls::CheckBox _checkBox{ nullptr };

    winrt::Windows::UI::Xaml::Controls::TextBlock _postTextBlock{ nullptr };

    // The App that instantiated this.
    const App* _app;

private: // sound/frequency state
    // Current frequency of sine wave -- updated by audio graph quantum.
    double _sineWaveFrequency;

    // Current phase of sine wave; ranges from 0 to 2*pi.
    double _sineWavePhase;

    // Is sine wave frequency descending?  (If not, it's ascending.)
    bool _isSineWaveFrequencyDescending;

    // Total sample count measured by required samples for audio frame input node.
    uint32_t _audioInputFrameSampleCount;

    // The audio input frame length (in samples), set from the slider.
    uint32_t _audioInputFrameLengthInSamples;

    // It is not clear why FrameInputNode_QuantumStarted is called with zero required bytes.
    // But it evidently is, as this variable indicates.
    int _zeroByteOutgoingFrameCount;

    winrt::Windows::Media::Audio::AudioFrameInputNode _audioFrameInputNode{ nullptr };

    // user can set whether frequency is changing or not
    bool _isSineWaveFrequencyChanging;

public: // properties
    winrt::Windows::UI::Xaml::Controls::StackPanel Panel() const { return _stackPanel; }

public:
    // Tones must be constructed from the UI context.
    Tone(const App* app);

    // Must be called from the ui context.
    void UpdateUI();

    // The quantum has started; consume input audio for this recording.
    void FrameInputNode_QuantumStarted(
        winrt::Windows::Media::Audio::AudioFrameInputNode sender,
        winrt::Windows::Media::Audio::FrameInputNodeQuantumStartedEventArgs args);
};

// Simple application which generates a sine wave at a fixed frequency, while changing the audio frame size.
// Basically equivalent to the sample code at https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/audio-graphs#audio-frame-input-node
// except that it allows interactively varying the audio input frame size.
// This should not affect the sound.  On some platforms, it evidently does (as of Win 10 version 1803).
// Hopefully this app will help to reproduce the issue on such platforms so it can be fixed.
class App : public winrt::Windows::UI::Xaml::ApplicationT<App>
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

private: // UI controls
    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockGraphStatus{ nullptr };
    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockGraphInfo{ nullptr };

    winrt::Windows::UI::Xaml::Controls::StackPanel _stackPanel{ nullptr };

    std::vector<std::unique_ptr<Tone>> _tones{};

private: // threading
    // The apartment context of the UI thread; must co_await this before updating UI
    // (and must thereafter switch out of UI context ASAP, for liveness).
    winrt::apartment_context _uiThread;

private: // sound generation
    // sample rate of audio graph
    uint32_t _sampleRateHz;
    // latency in samples reported by audio graph
    uint32_t _latencyInSamples;
    // channel count of audio graph
    uint32_t _channelCount;
    // samples per quantum of audio graph
    uint32_t _samplesPerQuantum;

    // Total sample count measured by audio graph quantumstarted events * audio graph required samples.
    uint32_t _audioGraphQuantumSampleCount;

    // The number of QuantumStarted events we've received from the audio graph.
    uint32_t _audioGraphQuantumCount;

    // bytes per sample (channel count * sample size in bytes)
    uint32_t _bytesPerSample;

    winrt::Windows::Media::Audio::AudioGraph _audioGraph{ nullptr };
    winrt::Windows::Media::Audio::AudioDeviceOutputNode _audioDeviceOutputNode{ nullptr };

public: // properties
    winrt::Windows::Media::Audio::AudioGraph Graph() const { return _audioGraph; }
    winrt::Windows::Media::Audio::AudioDeviceOutputNode OutputNode() const { return _audioDeviceOutputNode; }

    uint32_t SampleRateHz() const { return _sampleRateHz; }
    uint32_t ChannelCount() const { return _channelCount; }
    uint32_t SamplesPerQuantum() const { return _samplesPerQuantum; }
    uint32_t BytesPerSample() const { return _bytesPerSample; }

public: // application implementation
    winrt::fire_and_forget LaunchedAsync();

    winrt::fire_and_forget UpdateLoop();

    void OnLaunched(winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);
};

