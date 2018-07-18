# Audio Graph Input Frame Size Demo

A small Windows C++ app that just plays a single sound in a loop, allowing the input frame node size to be adjusted.

This app is an offshoot of my work on the [NowSound](https://github.com/RobJellinghaus/NowSound)
low-latency Windows sound library.  That library focuses on in-memory audio recording and
looping playback, using the UWP AudioGraph API in Windows 10.

The primary means of in-memory audio playback in AudioGraph is the AudioFrameInputNode.
To pass audio input data from a memory buffer into AudioGraph, one creates an AudioFrame
of a given size (in bytes), and passes it to the AudioFrameInputNode when that node
needs more data.

It turns out that the size of the AudioFrame that you pass to the AudioFrameInputNode can
significantly affect the sound, on some combinations of hardware and audio drivers.  So
this app is useful for providing a quick quality check of the low-latency audio handling of
a particular system.

The slider affects the input frame size.  Checking the "Change frequency?" checkbox will
make the sine wave change in pitch with every new input frame.  Shorter frames will hence
cause smoother changes in pitch.

The sine wave code takes care to never alter the phase of the sine wave when altering the
pitch.  So from an algorithmic perspective, all sound produced by this application is
a continuous sine wave (perhaps of changing frequency, but with constant amplitude).
There should _never_ be any clicks, pops, pauses, or other interruptions in the tone.
Any such noises are due to bugs in the audio driver or other issues in the hardware or
operating system.

I would be interested to hear of your experience with this test application.  Thanks very
much!