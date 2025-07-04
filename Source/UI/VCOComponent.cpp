#include "VCOComponent.h"
#include "../Parameters.h"

VCOComponent::VCOComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
{
    // Glissando, Pitch, PWM Speed Sliders (same as before)
    glissandoSlider.setSliderStyle(juce::Slider::LinearVertical);
    glissandoSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(glissandoSlider);
    glissandoLabel.setText("GLISS.", juce::dontSendNotification);
    addAndMakeVisible(glissandoLabel);
    glissandoAttachment = std::make_unique<juce::SliderParameterAttachment>(*valueTreeState.getParameter(ParameterIds::glissando), glissandoSlider);

    pitchSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(pitchSlider);
    pitchLabel.setText("PITCH", juce::dontSendNotification);
    addAndMakeVisible(pitchLabel);
    pitchAttachment = std::make_unique<juce::SliderParameterAttachment>(*valueTreeState.getParameter(ParameterIds::pitch), pitchSlider);

    pwmSpeedSlider.setSliderStyle(juce::Slider::LinearVertical);
    pwmSpeedSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(pwmSpeedSlider);
    pwmSpeedLabel.setText("PWM SPEED", juce::dontSendNotification);
    addAndMakeVisible(pwmSpeedLabel);
    pwmSpeedAttachment = std::make_unique<juce::SliderParameterAttachment>(*valueTreeState.getParameter(ParameterIds::pwmSpeed), pwmSpeedSlider);

    // --- Waveform Buttons ---
    waveTypeParam = valueTreeState.getParameter(ParameterIds::waveType);
    jassert(waveTypeParam != nullptr);
    addAndMakeVisible(waveTypeLabel);
    waveTypeLabel.setText("WAVEFORM", juce::dontSendNotification);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(waveTypeParam))
    {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i)
        {
            auto* button = waveTypeButtons.add(new juce::ToggleButton(choices[i]));
            addAndMakeVisible(button);
            button->setRadioGroupId(1); // Group ID for waveform
            button->setClickingTogglesState(true);
            button->onClick = [choiceParam, i] { *choiceParam = i; };
        }
    }
    waveTypeParam->addListener(this);


    // --- Feet Buttons ---
    feetParam = valueTreeState.getParameter(ParameterIds::feet);
    jassert(feetParam != nullptr);
    addAndMakeVisible(feetLabel);
    feetLabel.setText("FEET", juce::dontSendNotification);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(feetParam))
    {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i)
        {
            auto* button = feetButtons.add(new juce::ToggleButton(choices[i]));
            addAndMakeVisible(button);
            button->setRadioGroupId(2); // Group ID for feet
            button->setClickingTogglesState(true);
            button->onClick = [choiceParam, i] { *choiceParam = i; };
        }
    }
    feetParam->addListener(this);
    
    // Initial update
    parameterValueChanged(waveTypeParam->getParameterIndex(), waveTypeParam->getValue());
    parameterValueChanged(feetParam->getParameterIndex(), feetParam->getValue());
}

VCOComponent::~VCOComponent()
{
    if (waveTypeParam) waveTypeParam->removeListener(this);
    if (feetParam) feetParam->removeListener(this);
}

void VCOComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("VCO", getLocalBounds(), juce::Justification::centredTop, 1);
}

void VCOComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10).withTrimmedTop(20);
    
    // Simple grid layout similar to other components
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    // Row settings: top for controls, bottom for labels (5:1 ratio)
    grid.templateRows = { Track(Fr(5)), Track(Fr(1)) };
    
    // Column settings: 5 columns (3 sliders, waveform selection buttons, feet selection buttons)
    // Express ratio in integer values (total 6.5 converted to 13)
    grid.templateColumns = { 
        Track(Fr(2)),    // glissando (2/13)
        Track(Fr(2)),    // pitch (2/13)
        Track(Fr(2)),    // pwm speed (2/13)
        Track(Fr(4)),    // waveform (4/13)
        Track(Fr(3))     // feet (3/13) - wider width
    };

    // Add sliders to grid
    grid.items.add(juce::GridItem(glissandoSlider).withArea(1, 1));
    grid.items.add(juce::GridItem(pitchSlider).withArea(1, 2));
    grid.items.add(juce::GridItem(pwmSpeedSlider).withArea(1, 3));
    
    // Add labels to grid
    grid.items.add(juce::GridItem(glissandoLabel).withArea(2, 1));
    grid.items.add(juce::GridItem(pitchLabel).withArea(2, 2));
    grid.items.add(juce::GridItem(pwmSpeedLabel).withArea(2, 3));
    grid.items.add(juce::GridItem(waveTypeLabel).withArea(2, 4));
    grid.items.add(juce::GridItem(feetLabel).withArea(2, 5));
    
    // Apply grid layout
    grid.performLayout(bounds);
    
    // Calculate area for placing waveform selection buttons and feet selection buttons
    // Calculate width considering column ratios (ratio of each column out of total 13Fr)
    float totalFr = 2.0f + 2.0f + 2.0f + 4.0f + 3.0f; // Total 13Fr
    float totalWidth = bounds.getWidth();
    
    float glissWidth = totalWidth * (2.0f / totalFr);
    float pitchWidth = totalWidth * (2.0f / totalFr);
    float pwmWidth = totalWidth * (2.0f / totalFr);
    float waveWidth = totalWidth * (4.0f / totalFr); // 4Fr so width is 4/13
    float feetWidth = totalWidth * (3.0f / totalFr); // 3Fr so width is 3/13
    
    int rowHeight = bounds.getHeight() * 5 / 6; // Height of top area (5:1 ratio)
    
    auto waveButtonArea = juce::Rectangle<int>(
        bounds.getX() + glissWidth + pitchWidth + pwmWidth, // Right edge of 3rd column
        bounds.getY(),                                      // Start position of top area
        waveWidth,                                          // Width of waveform selection buttons (4Fr)
        rowHeight                                           // Height of top area
    );
    
    auto feetButtonArea = juce::Rectangle<int>(
        waveButtonArea.getRight(),                          // Right edge of waveform button area
        bounds.getY(),                                      // Start position of top area
        feetWidth,                                          // Width of feet selection buttons (3Fr)
        rowHeight                                           // Height of top area
    );
    
    // Place waveform selection buttons
    int numWaveButtons = waveTypeButtons.size();
    float waveButtonHeight = waveButtonArea.getHeight() / numWaveButtons;
    
    for (int i = 0; i < numWaveButtons; ++i) {
        auto* button = waveTypeButtons[i];
        auto buttonBounds = waveButtonArea.withHeight(waveButtonHeight).withY(waveButtonArea.getY() + i * waveButtonHeight);
        button->setBounds(buttonBounds.reduced(2));
    }
    
    // Place feet selection buttons
    int numFeetButtons = feetButtons.size();
    float feetButtonHeight = feetButtonArea.getHeight() / numFeetButtons;
    
    for (int i = 0; i < numFeetButtons; ++i) {
        auto* button = feetButtons[i];
        auto buttonBounds = feetButtonArea.withHeight(feetButtonHeight).withY(feetButtonArea.getY() + i * feetButtonHeight);
        button->setBounds(buttonBounds.reduced(2));
    }
    
    // Set label text alignment to center
    waveTypeLabel.setJustificationType(juce::Justification::centred);
    feetLabel.setJustificationType(juce::Justification::centred);
}

void VCOComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    if (parameterIndex == waveTypeParam->getParameterIndex())
    {
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(waveTypeParam))
        {
            waveTypeButtons[choiceParam->getIndex()]->setToggleState(true, juce::dontSendNotification);
        }
    }
    else if (parameterIndex == feetParam->getParameterIndex())
    {
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(feetParam))
        {
            feetButtons[choiceParam->getIndex()]->setToggleState(true, juce::dontSendNotification);
        }
    }
}

void VCOComponent::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
    // Not needed for this component
}
