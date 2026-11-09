# Spindeck (NINE HANDS | DSP)

Two-deck DJ software inside a VST3 instrument for Reaper, made for the **ION Discover DJ** controller.
Windows and macOS (VST3, plus AU on macOS).

<p align="center">
  <a href="https://github.com/Nine-Hands-Dsp/Spindeck/releases/latest/download/Spindeck-VST3-Windows.zip"><img alt="Télécharger pour Windows" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-Windows-0078D6?style=for-the-badge"></a>
  <a href="https://github.com/Nine-Hands-Dsp/Spindeck/releases/latest/download/Spindeck-VST3-macOS.zip"><img alt="Télécharger pour macOS" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-macOS-111111?style=for-the-badge&logo=apple&logoColor=white"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/Spindeck/releases/latest"><img alt="Dernière version" src="https://img.shields.io/github/v/release/Nine-Hands-Dsp/Spindeck?label=derni%C3%A8re%20version&color=919496"></a>
</p>

## What it does

- Two decks with scrolling waveforms (10 s, beat grid, cue marker), track overview (click to seek), spinning platters.
- Play, CDJ-style cue, sync (tempo + phase), reverse, pitch -/+ (0.1 % steps, auto-repeat), pitch range 4 / 8 / 16 / 50 %.
- Scratch: SCRATCH button on the controller, then touch the jog wheel. Also with the mouse on the on-screen platter.
- Jog wheel without scratch mode: pitch bend while playing, audible search when stopped.
- Mixer: 3-band DJ EQ (kill on the left), filter (low-pass left / high-pass right), volume, crossfader (Mix or Scratch curve), master with soft limiter.
- BPM and beat grid detected automatically when a track is loaded.
- Library: music folder, search, BPM column, streaming preview (PREVIEW button or play icon of a row, seek bar, volume; follows the selection; MENU > Preview output: master or outputs 3-4); BROWSE knob + LOAD A / LOAD B on the controller, drag and drop from the list or from Explorer / Finder.
- Controller LEDs (play, cue, sync beat, reverse, scratch) driven back over MIDI.
- Every knob / fader is automatable and can be MIDI-learned (right-click > MIDI Learn).
- Formats: WAV, AIFF, FLAC, OGG, MP3 (plus M4A through the OS decoders). Tracks up to 60 min.

## Reaper setup

1. Insert Spindeck on a track (it is an instrument).
2. Record-arm the track and turn record monitoring on.
3. Track input: MIDI > ION Discover DJ > All channels.
4. LEDs: track Routing > MIDI Hardware Output > ION Discover DJ.
5. Scratching: ASIO buffer of 128 or 256 samples.

## Controller mapping (ION Discover DJ)

Taken from the open-source Mixxx mapping of the controller. The Discover DJ sends its crossfader backwards, so it is
inverted by default (MENU > Invert crossfader to change it).
Scratch sensitivity of the jog wheels: MENU > Scratch sensitivity (25 / 50 / 75 / 100 %, default 50 %).
The mouse on the on-screen platter always follows the record 1:1.

Window: 100 % (1600 x 900, default and minimum), 125 %, 150 % (right-click or MENU > Plugin size).
FULL button (or MENU > Full screen): the window fills the screen it is on (taskbar excluded), follows it when moved to another screen, and can go below 100 % on a small screen.
MENU > Controller > None turns the preset off (MIDI Learn only).
MENU > MIDI Learn > Save as default: the current MIDI Learn assignments are recalled every time Spindeck is opened in a new project (saved projects keep their own).

## Build

JUCE 8.0.15 + CMake. GitHub Actions builds Windows (windows-2022) and macOS (universal binary).
Anti-freeze v2 applied (software renderer on Windows, repaint watchdog, no heavy work in paint, no host call from the audio thread).

`tools/render/build.sh <JUCE dir> <out dir>` builds a Linux test tool that checks the BPM analysis, the audio engine
(play, sync, scratch, cue, crossfader) and exports screenshots of the interface.
