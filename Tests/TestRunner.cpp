#include <JuceHeader.h>

// Custom test listener to generate XML results
class XmlTestListener : public juce::UnitTestRunner::Listener
{
public:
    XmlTestListener()
    {
        xml = std::make_unique<juce::XmlElement>("testsuites");
    }
    
    void testStarted(const juce::String& currentTestName) override
    {
        currentTest = currentTestName;
        currentTestStartTime = juce::Time::getMillisecondCounter();
    }
    
    void testFinished() override
    {
        // Not used
    }
    
    void testPassed(const juce::String& testName) override
    {
        addTestCase(testName, true, "");
    }
    
    void testFailed(const juce::String& testName, const juce::String& failureMessage) override
    {
        addTestCase(testName, false, failureMessage);
        anyTestsFailed = true;
    }
    
    void allTestsStarted() override
    {
        totalTestsStartTime = juce::Time::getMillisecondCounter();
    }
    
    void allTestsEnded() override
    {
        auto testsRunTime = (juce::Time::getMillisecondCounter() - totalTestsStartTime) / 1000.0;
        xml->setAttribute("time", juce::String(testsRunTime));
        
        // Write the XML to a file
        juce::File outputFile("build_tests/test_results.xml");
        if (!outputFile.getParentDirectory().exists())
            outputFile.getParentDirectory().createDirectory();
        
        xml->writeTo(outputFile);
        juce::String resultSummary = "\nTest results written to: " + outputFile.getFullPathName();
        juce::Logger::writeToLog(resultSummary);
    }
    
    bool didAnyTestsFail() const
    {
        return anyTestsFailed;
    }
    
private:
    void addTestCase(const juce::String& testName, bool success, const juce::String& failureMessage)
    {
        // Get or create the test suite element for this class
        juce::String className = testName.upToFirstOccurrenceOf(":", false, false);
        juce::XmlElement* testSuite = nullptr;
        
        for (auto* suite : xml->getChildIterator())
        {
            if (suite->getStringAttribute("name") == className)
            {
                testSuite = suite;
                break;
            }
        }
        
        if (testSuite == nullptr)
        {
            testSuite = xml->createNewChildElement("testsuite");
            testSuite->setAttribute("name", className);
        }
        
        // Update test suite stats
        int tests = testSuite->getIntAttribute("tests", 0);
        testSuite->setAttribute("tests", tests + 1);
        
        if (!success)
        {
            int failures = testSuite->getIntAttribute("failures", 0);
            testSuite->setAttribute("failures", failures + 1);
        }
        
        // Add the test case
        auto* testCase = testSuite->createNewChildElement("testcase");
        testCase->setAttribute("name", testName.fromFirstOccurrenceOf(":", false, false).trim());
        testCase->setAttribute("classname", className);
        
        auto testTime = (juce::Time::getMillisecondCounter() - currentTestStartTime) / 1000.0;
        testCase->setAttribute("time", juce::String(testTime));
        
        if (!success)
        {
            auto* failure = testCase->createNewChildElement("failure");
            failure->setAttribute("message", failureMessage);
        }
    }
    
    std::unique_ptr<juce::XmlElement> xml;
    juce::String currentTest;
    juce::uint32 currentTestStartTime = 0;
    juce::uint32 totalTestsStartTime = 0;
    bool anyTestsFailed = false;
};

// Main entry point for the test runner
int main(int argc, char* argv[])
{
    // Initialize JUCE
    juce::ScopedJuceInitialiser_GUI juceInit;
    
    // Set up console output
    juce::ConsoleApplication app;
    app.addVersionCommand("--version", "1.0.0");
    app.addHelpCommand("--help|-h", "CheapSynth01 Tests", "Runs unit tests for CheapSynth01");
    
    // Create XML test listener
    XmlTestListener xmlListener;
    
    // Run the tests
    juce::UnitTestRunner testRunner;
    testRunner.setAssertOnFailure(false);
    testRunner.addListener(&xmlListener);
    testRunner.runAllTests();
    
    // Return non-zero exit code if any tests failed
    return xmlListener.didAnyTestsFail() ? 1 : 0;
}
