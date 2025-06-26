#pragma once

#include <JuceHeader.h>

class PresetPanel : public juce::Component,
                    private juce::ComboBox::Listener,
                    private juce::Timer
{
public:
    PresetPanel(juce::AudioProcessor& p);
    ~PresetPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void populatePresetMenu();
    void loadPreset(int presetIndex);

    juce::AudioProcessor& audioProcessor;

    juce::ComboBox presetMenu;
    juce::TextButton prevButton{ "<" };
    juce::TextButton nextButton{ ">" };
};
