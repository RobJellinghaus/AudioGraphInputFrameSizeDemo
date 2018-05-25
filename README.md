# Audio Graph Input Frame Size Demo

A small Windows C++ app that just plays a single sound in a loop, allowing the input frame node size to be adjusted.

This app is an offshoot of my work on the [NowSound](https://github.com/RobJellinghaus/NowSound)
low-latency Windows sound library.  That library focuses on in-memory audio recording and
looping playback, using the UWP AudioGraph API in Windows 10.

The primary means of in-memory audio playback in AudioGraph is the AudioFrameInputNode.
To pass audio input data from a memory buffer into AudioGraph, one creates an AudioFrame
of a given size (in bytes), and passes it to the AudioFrameInputNode when that node
needs more data.

It turns out that the size of the AudioFrame that you pass to the AudioFrameInputNode