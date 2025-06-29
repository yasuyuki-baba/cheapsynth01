#include <JuceHeader.h>

// Main entry point for the test runner
int main(int argc, char* argv[])
{
    // Initialize JUCE
    juce::ScopedJuceInitialiser_GUI juceInit;
    
    // Set up console output
    juce::ConsoleApplication app;
    app.addVersionCommand("--version", "1.0.0");
    app.addHelpCommand("--help|-h", "CheapSynth01 Tests", "Runs unit tests for CheapSynth01");
    
    // Run the tests
    juce::UnitTestRunner testRunner;
    testRunner.setAssertOnFailure(false);
    testRunner.runAllTests();
    
    // Return success if all tests passed
    return testRunner.getNumFailures() == 0 ? 0 : 1;
}
