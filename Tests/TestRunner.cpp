// Google Test main entry point
#include <JuceHeader.h>

#include <gtest/gtest.h>

/**
 * Custom environment for JUCE initialization
 */
class JuceEnvironment : public ::testing::Environment {
   public:
    void SetUp() override {
        // Initialize JUCE
        juceInitialiser = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
    }

    void TearDown() override {
        // Clean up JUCE
        juceInitialiser.reset();
    }

   private:
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juceInitialiser;
};

// Custom main function to initialize JUCE before running tests
int main(int argc, char** argv) {
    bool includeObservations = false;
    bool observationsOnly = false;
    int remaining = 1;
    for (int i = 1; i < argc; ++i) {
        const juce::String argument(argv[i]);
        if (argument == "--all")
            includeObservations = true;
        else if (argument == "--observations")
            observationsOnly = true;
        else
            argv[remaining++] = argv[i];
    }
    argc = remaining;
    argv[argc] = nullptr;

    // Set defaults before parsing so explicit Google Test flags remain authoritative.
    ::testing::FLAGS_gtest_output = "xml:test_results.xml";
    ::testing::FLAGS_gtest_filter = observationsOnly      ? "*.Observation_*"
                                    : includeObservations ? "*"
                                                          : "*-*.Observation_*";
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Add JUCE environment
    ::testing::AddGlobalTestEnvironment(new JuceEnvironment);

    // Run all tests
    return RUN_ALL_TESTS();
}
