#include "JuceHeader.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;
    juce::UnitTestRunner runner;
    runner.runAllTests();

    for (int i = 0; i < runner.getNumResults(); ++i)
        if (runner.getResult(i)->failures > 0)
            return 1;

    return 0;
}
