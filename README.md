# CereVoice Plugin for GStreamer

This is a very simple plugin for GStreamer that converts text to speech using
the [CereVoice Speech Synthesizer](https://www.cereproc.com/en/products/sdk).

**IMPORTANT:** This project is not affiliated with CereProc in any way. They
cannot provide support for this plugin!

## Requirements

* GNU Make
* pkg-config
* GStreamer 1.8+
* Glib 2.44+
* CereVoice SDK

## Building

By default, the Makefile assumes that the CereVoice SDK is located at
`/opt/cerevoice_sdk`. If this is the case, simply run:

    $ make

If the CereVoice SDK is located somewhere else, specify its location in the
`CEREVOICE_SDK` variable:

    $ make CEREVOICE_SDK=/path/to/sdk

For information about the options and targets supported by the Makefile, run:

    $ make help

## Properties

* `voice-file` - The path to a voice file that will be loaded into the CereVoice
  engine. The voice can be referenced by other `cerevoice` elements through the
  `voice-name` property without reloading it from the disk.

* `license-file` - The path to a license file for the voice specified in the
  `voice-file` property.

* `config-file` - The path to a configuration file for the voice specified in
  the `voice-file` property.

## License

This plugin, like most other GStreamer plugins, is distributed under the Lesser
General Public License (LGPL) v2.1. See the COPYING file for the full text of
the license.

