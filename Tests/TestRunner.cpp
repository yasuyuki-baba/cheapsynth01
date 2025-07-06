// Google Test main entry point
#include <gtest/gtest.h>
#include <JuceHeader.h>

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
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Add JUCE environment
    ::testing::AddGlobalTestEnvironment(new JuceEnvironment);
    
    // Enable XML output by default
    ::testing::FLAGS_gtest_output = "xml:test_results.xml";
    
    // Run all tests
    return RUN_ALL_TESTS();
}
