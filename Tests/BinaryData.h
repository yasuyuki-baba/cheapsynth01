#pragma once

#include <JuceHeader.h>

/**
 * Mock BinaryData class for tests
 * This provides a minimal implementation of the BinaryData class for testing
 */
namespace BinaryData
{
    // Mock data for testing
    extern const char* getNamedResource(const char* resourceNameUTF8, int& dataSizeInBytes);
    extern const char* defaultPreset_xml;
    extern const int defaultPreset_xmlSize;
}
