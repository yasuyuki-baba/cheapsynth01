#include "BinaryData.h"

namespace BinaryData
{
    // Simple XML for default preset
    const char* defaultPreset_xml = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<PRESET name=\"Default\">"
        "  <PARAMETERS>"
        "    <PARAM id=\"vco_waveform\" value=\"0\"/>"
        "    <PARAM id=\"vco_octave\" value=\"0\"/>"
        "    <PARAM id=\"vcf_cutoff\" value=\"0.7\"/>"
        "    <PARAM id=\"vcf_resonance\" value=\"0.3\"/>"
        "    <PARAM id=\"vcf_eg_intensity\" value=\"0.5\"/>"
        "    <PARAM id=\"eg_attack\" value=\"0.1\"/>"
        "    <PARAM id=\"eg_decay\" value=\"0.3\"/>"
        "    <PARAM id=\"eg_sustain\" value=\"0.7\"/>"
        "    <PARAM id=\"eg_release\" value=\"0.2\"/>"
        "    <PARAM id=\"lfo_waveform\" value=\"0\"/>"
        "    <PARAM id=\"lfo_rate\" value=\"0.4\"/>"
        "    <PARAM id=\"lfo_intensity\" value=\"0.2\"/>"
        "    <PARAM id=\"vca_level\" value=\"0.8\"/>"
        "    <PARAM id=\"breath_control\" value=\"0.0\"/>"
        "  </PARAMETERS>"
        "</PRESET>";
    
    const int defaultPreset_xmlSize = static_cast<int>(std::strlen(defaultPreset_xml));
    
    const char* getNamedResource(const char* resourceNameUTF8, int& dataSizeInBytes)
    {
        if (std::strcmp(resourceNameUTF8, "Default.xml") == 0)
        {
            dataSizeInBytes = defaultPreset_xmlSize;
            return defaultPreset_xml;
        }
        
        dataSizeInBytes = 0;
        return nullptr;
    }
}
