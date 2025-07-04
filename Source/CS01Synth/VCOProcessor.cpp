#include "VCOProcessor.h"
#include "SynthConstants.h"

VCOProcessor::VCOProcessor(juce::AudioProcessorValueTreeState& vts, bool isNoiseMode)
    : AudioProcessor(BusesProperties()
                        .withInput("LFOInput", juce::AudioChannelSet::mono(), true)
                        .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(vts),
      toneGenerator(std::make_unique<ToneGenerator>(apvts)),
      noiseGenerator(std::make_unique<NoiseGenerator>(apvts)),
      currentGenerator(nullptr) // Will be set in parameterChanged
{
    // Register as listener for feet parameter
    apvts.addParameterListener(ParameterIds::feet, this);
    
    // Set the initial generator type based on current parameter value
    auto* feetParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
    if (feetParam != nullptr)
    {
        // Trigger the parameter changed handler to setup the current generator
        float paramValue = feetParam->getIndex(); // Get current index as float
        parameterChanged(ParameterIds::feet, paramValue);
    }
    else
    {
        // Fallback to constructor parameter if parameter not found
        if (isNoiseMode)
            currentGenerator = static_cast<ISoundGenerator*>(noiseGenerator.get());
        else
            currentGenerator = static_cast<ISoundGenerator*>(toneGenerator.get());
    }
}

VCOProcessor::~VCOProcessor()
{
    // Remove parameter listener
    apvts.removeParameterListener(ParameterIds::feet, this);
}

void VCOProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParameterIds::feet)
    {
        auto* feetParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
        if (feetParam != nullptr)
        {
            bool isNoiseMode = (feetParam->getIndex() == static_cast<int>(Feet::WhiteNoise));
            ISoundGenerator* oldGenerator = currentGenerator;
            ISoundGenerator* newGenerator = nullptr;
            
            if (isNoiseMode)
                newGenerator = static_cast<ISoundGenerator*>(noiseGenerator.get());
            else
                newGenerator = static_cast<ISoundGenerator*>(toneGenerator.get());
            
            if (oldGenerator == newGenerator)
                return;
                
            // Save current state before switching
            bool wasActive = false;
            int currentNote = 0;
            float velocity = 1.0f;
            int pitchWheel = 8192; // center position
            
            if (oldGenerator && oldGenerator->isActive())
            {
                wasActive = true;
                currentNote = oldGenerator->getCurrentlyPlayingNote();
            }
            
            // Switch generator
            currentGenerator = newGenerator;
            
            // Initialize if already prepared
            if (isPrepared && currentGenerator)
                currentGenerator->prepare(lastSpec);
            
            // Transfer state to new generator if needed
            if (wasActive && currentGenerator != nullptr)
                currentGenerator->startNote(currentNote, velocity, pitchWheel);
            
            if (onGeneratorTypeChanged)
                onGeneratorTypeChanged();
        }
    }
}

void VCOProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lastSpec = { sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };
    
    // Prepare both generators
    if (toneGenerator)
        toneGenerator->prepare(lastSpec);
        
    if (noiseGenerator)
        noiseGenerator->prepare(lastSpec);
        
    isPrepared = true;
}

bool VCOProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getChannelSet(false, 0);
    
    // Output must always be mono
    if (mainOut != juce::AudioChannelSet::mono())
        return false;
    
    // For Tone type, check LFO input
    const auto& mainIn = layouts.getChannelSet(true, 0);
    if (mainIn != juce::AudioChannelSet::mono())
        return false;
    
    return true;
}

void VCOProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (!currentGenerator)
    {
        return;
    }

    // Process audio block

    // Process LFO input for Tone generator
    if (currentGenerator == toneGenerator.get())
    {
        auto lfoInput = getBusBuffer(buffer, true, 0);
        lfoValue = lfoInput.getNumSamples() > 0 ? lfoInput.getSample(0, 0) : 0.0f;

        auto modDepth = apvts.getRawParameterValue(ParameterIds::modDepth)->load();
        const float lfoModRangeSemitones = 1.0f;
        float finalLfoValue = lfoValue * modDepth * lfoModRangeSemitones;

        // Set LFO value directly to the ToneGenerator
        toneGenerator->setLfoValue(finalLfoValue);
    }

    // Clear audio buffer
    buffer.clear();

    // Sound generation using the current generator
    if (currentGenerator->isActive())
    {
            
        currentGenerator->renderNextBlock(buffer, 0, buffer.getNumSamples());
    }
    // If not active, buffer remains cleared

    // Pass MIDI buffer through (processing is done in MidiProcessor)
}
