# CereVoice Plugin for GStreamer

This is a very simple plugin for GStreamer that converts text to speech using
the [CereVoice Speech Synthesizer](https://www.cereproc.com/en/products/sdk).

## Building

Make sure you have the CereVoice SDK installed and run `make`, supplying the
path to the SDK in the `CEREVOICE_SDK` environment variable:

    $ CEREVOICE_SDK=/path/to/sdk make

The `check` target will run a basic pipeline that synthesizes speech through an
`autoaudiosink` element. It currently expects a voice and license file in the
current directory called `heather.voice` and `heather.lic`, respectively.

## Properties

* `voice-name` - The name of a voice that was previously loaded by setting the
  `voice-file` and `license-file` properties on a `cerevoice` element.

* `voice-file` - The path to a voice file that will be loaded into the CereVoice
  engine. The voice can be referenced by other `cerevoice` elements through the
  `voice-name` property without reloading it from the disk.

* `license-file` - The path to a license file for the voice specified in the
  `voice-file` property.

* `config-file` - The path to a configuration file for the voice specified in
  the `voice-file` property.

