#pragma once

#include "pch.h"

const int TicksPerSecond = 10000000;

winrt::Windows::Foundation::TimeSpan timeSpanFromSeconds(int seconds);

// release assertion, basically
void Check(bool condition);

// Simple application which exercises NowSoundLib, allowing test of basic looping.
struct App : winrt::Windows::UI::Xaml::ApplicationT<App>
{
    // The interaction model of this app is:
    // - App displays window with slider.
    // - Slider sets length of audio input frame.
    // - App fills each frame with current frame length of sine wave anchored at current frequency.
    //   - The phase of the wave is carried from frame to frame, so frame transitions never have audible clicks or pops.
    // - App cycles current frequency linearly from 200Hz to 1000Hz and back on an eight-second cycle.
    //
    // Therefore, shorter audio frames produce smoother transitions in pitch.
    //
    // I claim the code which populates the audio input frame is free of phase errors or buffer errors that would
    // cause clicking or popping.  I encourage reviewers to validate this claim; it's the entire point of this sample,
    // to root-cause the audible clicking/popping issues with shorter frame sizes.
    //
    // The top and bottom frequencies can be changed to be the same in order to avoid any confusion or bugs
    // relating to frequency change.

    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockGraphStatus{ nullptr };
    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockGraphInfo{ nullptr };
    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockTimeInfo{ nullptr };

    winrt::Windows::UI::Xaml::Controls::TextBlock _minimumAudioFrameSize{ nullptr };
    winrt::Windows::UI::Xaml::Controls::Slider _audioFrameSizeSlider{ nullptr };
    winrt::Windows::UI::Xaml::Controls::TextBlock _maximumAudioFrameSize{ nullptr };

    winrt::Windows::UI::Xaml::Controls::StackPanel _stackPanel{ nullptr };

    // The apartment context of the UI thread; must co_await this before updating UI
    // (and must thereafter switch out of UI context ASAP, for liveness).
    winrt::apartment_context _uiThread;

    const int BytesPerSample = sizeof(float) * 2; // stereo float samples

    // minimum frequency of the pitch ramp
    const int MinimumFrequencyHz = 200;
    // maximum frequency of the pitch ramp
    const int MaximumFrequencyHz = 2000;
    // duration of the pitch ramp (minimum -> maximum -> back to minimum, e.g. one complete cycle)
    const int FrequencyCycleDurationSecs = 8;

    // current frequency of sine wave
    int _currentFrequency;
    // is ramp headed up or headed down?
    bool _frequencyIsRampingUp;

    // current phase of sine wave -- ranges from 0 to 2*pi
    double _sineWavePhase;

    std::unique_ptr<byte> _buffer;

    winrt::fire_and_forget LaunchedAsync();

    winrt::fire_and_forget UpdateLoop();

    void OnLaunched(winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);
};

