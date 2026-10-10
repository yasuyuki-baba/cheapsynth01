#pragma once
#define PLUG_NAME "CheapSynth01"
#define PLUG_MFR "Yasuyuki Baba"
#define PLUG_VERSION_HEX 0x00020000
#define PLUG_VERSION_STR "2.0.0"
// Retain the identifiers used by the JUCE release.
#define PLUG_UNIQUE_ID 'CS01'
#define PLUG_MFR_ID 'BABA'
// JUCE_VST3_CAN_REPLACE_VST2 was disabled in the previous build.
#define VST3_PROCESSOR_UID 0xABCDEF01, 0x9182FAEB, PLUG_MFR_ID, PLUG_UNIQUE_ID
#define VST3_CONTROLLER_UID 0xABCDEF01, 0x1234ABCD, PLUG_MFR_ID, PLUG_UNIQUE_ID
#define PLUG_URL_STR "https://github.com/yasuyuki-baba/cheapsynth01"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright Yasuyuki Baba"
#define PLUG_CLASS_NAME CS01AudioProcessor
#define BUNDLE_NAME "cheapsynth01"
#define BUNDLE_MFR "yasuyukibaba"
#define BUNDLE_DOMAIN "com"
#define PLUG_CHANNEL_IO "0-1 0-2"
#define SHARED_RESOURCES_SUBPATH "CheapSynth01"
#define PLUG_LATENCY 16
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1240
#define PLUG_HEIGHT 400
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 1
#define PLUG_MIN_WIDTH 1240
#define PLUG_MAX_WIDTH 1240
#define PLUG_MIN_HEIGHT 400
#define PLUG_MAX_HEIGHT 640
#define AUV2_ENTRY CheapSynth01_Entry
#define AUV2_ENTRY_STR "CheapSynth01_Entry"
#define AUV2_FACTORY CheapSynth01_Factory
#define AUV2_VIEW_CLASS CheapSynth01_View
#define AUV2_VIEW_CLASS_STR "CheapSynth01_View"
#define VST3_SUBCATEGORY "Instrument|Synth"
#define CLAP_MANUAL_URL "https://github.com/yasuyuki-baba/cheapsynth01"
#define CLAP_SUPPORT_URL "https://github.com/yasuyuki-baba/cheapsynth01/issues"
#define CLAP_DESCRIPTION "CS01-inspired monophonic synthesizer"
#define CLAP_FEATURES "instrument", "synthesizer", "mono"
#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64
#define ROBOTO_FN "Roboto-Regular.ttf"
