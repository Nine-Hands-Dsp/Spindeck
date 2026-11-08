cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), macOS 10.13 minimum (avant project())
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

project(Spindeck VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.15
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(SpinAssets SOURCES nine_hands_logo_blanc.png nine_hands_logo_noir.png)

if(APPLE)
  set(SPIN_FORMATS VST3 AU)
else()
  set(SPIN_FORMATS VST3)
endif()

# instrument : la piste Reaper doit etre armee avec le monitoring, entree MIDI = ION Discover DJ
juce_add_plugin(Spindeck
  COMPANY_NAME "NINE HANDS | DSP"
  PLUGIN_MANUFACTURER_CODE Wiga
  PLUGIN_CODE WgSd
  FORMATS ${SPIN_FORMATS}
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  VST3_CATEGORIES Instrument Sampler
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  PRODUCT_NAME "Spindeck"
  BUNDLE_ID com.ninehandsdsp.spindeck
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Spindeck PRIVATE
  PluginProcessor.cpp
  PluginEditor.cpp
  NativeWindow.cpp
  Track.cpp)

target_compile_definitions(Spindeck PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_USE_MP3AUDIOFORMAT=1
  JUCE_USE_FLAC=1
  JUCE_USE_OGGVORBIS=1
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Spindeck
  PRIVATE
    SpinAssets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
