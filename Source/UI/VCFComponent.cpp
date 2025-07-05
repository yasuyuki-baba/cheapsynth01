#include "VCFComponent.h"
#include "../Parameters.h"
#include "../CS01Synth/IFilter.h"

VCFComponent::VCFComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
{
    cutoffSlider.setSliderStyle(juce::Slider::LinearVertical);
    cutoffSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(cutoffSlider);
    cutoffLabel.setText("CUTOFF", juce::dontSendNotification);
    addAndMakeVisible(cutoffLabel);
    
    // Toggle button for CS01 filter resonance
    resonanceButton.setButtonText("RES");
    addAndMakeVisible(resonanceButton);
    resonanceLabel.setText("ON/OFF", juce::dontSendNotification);
    addAndMakeVisible(resonanceLabel);
    
    // Slider for Modern filter resonance
    resonanceSlider.setSliderStyle(juce::Slider::LinearVertical);
    resonanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addChildComponent(resonanceSlider); // Hidden initially
    resonanceSliderLabel.setText("RESONANCE", juce::dontSendNotification);
    addChildComponent(resonanceSliderLabel); // Hidden initially
    
    vcfEgDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
    vcfEgDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(vcfEgDepthSlider);
    vcfEgDepthLabel.setText("EG DEPTH", juce::dontSendNotification);
    addAndMakeVisible(vcfEgDepthLabel);

    cutoffAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::cutoff), cutoffSlider);
    vcfEgDepthAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::vcfEgDepth), vcfEgDepthSlider);

    // Set up filter type monitoring
    auto* filterTypeParam = valueTreeState.getParameter(ParameterIds::filterType);
    filterTypeAttachment = std::make_unique<juce::ParameterAttachment>(
        *filterTypeParam,
        [this](float value) { updateResonanceControl(value); },
        valueTreeState.undoManager);
    
    // Set initial state
    updateResonanceControl(filterTypeParam->getValue());
}

VCFComponent::~VCFComponent() {}

void VCFComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("VCF", getLocalBounds(), juce::Justification::centredTop, 1);
}

void VCFComponent::updateResonanceControl(float filterType)
{
    // filterType 0 = CS01, 1 = Modern
    if (static_cast<int>(filterType) == 0)
    {
        // CS01 mode: Show toggle button
        resonanceButton.setVisible(true);
        resonanceLabel.setVisible(true);
        resonanceSlider.setVisible(false);
        resonanceSliderLabel.setVisible(false);
        
        // Set up attachment
        if (!resonanceAttachment)
        {
            auto* param = valueTreeState.getParameter(ParameterIds::resonance);
            if (param != nullptr)
            {
                // Use standard ButtonParameterAttachment
                // Button ON = 1.0f, OFF = 0.0f mapping
                resonanceAttachment = std::make_unique<juce::ButtonParameterAttachment>(
                    *param, resonanceButton);
                
                // Set button's initial state (ON if 0.5 or higher)
                resonanceButton.setToggleState(param->getValue() >= 0.5f, juce::dontSendNotification);
            }
        }
        if (resonanceSliderAttachment)
        {
            resonanceSliderAttachment.reset();
        }
    }
    else
    {
        // Modern mode: Show slider
        resonanceButton.setVisible(false);
        resonanceLabel.setVisible(false);
        resonanceSlider.setVisible(true);
        resonanceSliderLabel.setVisible(true);
        
        // Set up attachment
        if (!resonanceSliderAttachment)
        {
            resonanceSliderAttachment = std::make_unique<juce::SliderParameterAttachment>(
                *valueTreeState.getParameter(ParameterIds::resonance), resonanceSlider);
        }
        if (resonanceAttachment)
        {
            resonanceAttachment.reset();
        }
    }
}

// Update UI when filter processor changes
void VCFComponent::updateFilterControl(IFilter* filterProcessor)
{
    if (filterProcessor != nullptr)
    {
        // Get resonance control type from IFilter
        auto controlType = filterProcessor->getResonanceMode();
        
        // Update UI based on control type
        if (controlType == IFilter::ResonanceMode::Toggle)
        {
            // CS01 filter: Show toggle button
            updateResonanceControl(0.0f); // Set to CS01 mode (0)
        }
        else // Continuous
        {
            // Modern filter: Show slider
            updateResonanceControl(1.0f); // Set to Modern mode (1)
        }
    }
}

void VCFComponent::resized()
{
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    grid.templateRows    = { Track(Fr(5)), Track(Fr(1)) }; // Make slider area taller
    grid.templateColumns = { Track(Fr(1)), Track(Fr(1)), Track(Fr(1)) };

    // Use standard grid item layout and adjust resonance controls later
    grid.items = {
        juce::GridItem(cutoffSlider),
        // Placeholder for resonance control position (custom placement later)
        juce::GridItem(),
        juce::GridItem(vcfEgDepthSlider),
        
        juce::GridItem(cutoffLabel),
        // Placeholder for resonance label position
        juce::GridItem(),
        juce::GridItem(vcfEgDepthLabel)
    };
    
    grid.performLayout(getLocalBounds().reduced(10).withTrimmedTop(20));
    
    // Manually place resonance-related UI after applying grid layout
    auto bounds = getLocalBounds().reduced(10).withTrimmedTop(20);
    
    // Calculate column sizes
    int columnWidth = bounds.getWidth() / 3;
    int rowHeight = bounds.getHeight() * 5 / 6; // First row is 5/6 of the total height
    int labelHeight = bounds.getHeight() / 6;   // Label row is 1/6 of the total height
    
    // Get the area for the second column (resonance column)
    juce::Rectangle<int> resonanceArea(
        bounds.getX() + columnWidth,
        bounds.getY(),
        columnWidth,
        rowHeight
    );
    
    // Position resonance controls
    resonanceButton.setBounds(resonanceArea);
    resonanceSlider.setBounds(resonanceArea);
    
    // Position resonance labels
    juce::Rectangle<int> resonanceLabelArea(
        bounds.getX() + columnWidth,
        bounds.getY() + rowHeight,
        columnWidth,
        labelHeight
    );
    
    resonanceLabel.setBounds(resonanceLabelArea);
    resonanceSliderLabel.setBounds(resonanceLabelArea);
}
