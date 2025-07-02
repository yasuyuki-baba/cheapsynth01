#include <JuceHeader.h>
#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <map>

/**
 * Generate JUnit XML format test report using UnitTestRunner result data
 */
void generateJUnitXmlReport(const juce::UnitTestRunner& testRunner, const juce::String& outputPath)
{
    // Set up XML structure
    juce::XmlElement rootElement("testsuites");
    rootElement.setAttribute("name", "CheapSynth01Tests");
    
    int totalTests = 0;
    int totalFailures = 0;
    
    // Process test results
    std::map<juce::String, juce::XmlElement*> testSuites;  // test suite name -> XML element
    std::map<juce::String, int> suiteTestCounts;           // test suite name -> test count
    std::map<juce::String, int> suiteFailureCounts;        // test suite name -> failure count
    std::map<juce::String, std::set<juce::String>> suiteTestNames; // test suite -> set of test names
    
    // Process each test result
    for (int i = 0; i < testRunner.getNumResults(); ++i)
    {
        const auto* result = testRunner.getResult(i);
        juce::String suiteName = result->unitTestName;
        
        // Extract test case name from the message
        juce::String message = result->messages[0];
        juce::String testName;
        
        // Look for test names in format "Test Name / Method Name"
        if (message.contains("Starting tests in:") && message.contains("/"))
        {
            juce::String afterPrefix = message.fromFirstOccurrenceOf("Starting tests in:", false, false).trim();
            testName = afterPrefix.fromLastOccurrenceOf("/", false, false).trim();
            
            // Remove trailing "..." if present
            if (testName.contains("..."))
                testName = testName.upToFirstOccurrenceOf("...", false, false).trim();
        }
        else
        {
            // If we can't extract a method name, use a summary of the message
            testName = message.substring(0, 30).trim();
            if (testName.isEmpty())
                testName = "Test";
        }
        
        // Check if this is a failure message
        bool isFailure = false;
        juce::String failureMessage;
        
        for (const auto& msg : result->messages)
        {
            if (msg.contains("FAILED"))
            {
                isFailure = true;
                failureMessage = msg.contains(" - ") ? 
                                 msg.fromLastOccurrenceOf(" - ", false, false).trim() : 
                                 "Test failed";
                break;
            }
        }
        
        // Make sure test name is unique within its suite
        if (!testName.isEmpty())
        {
            if (suiteTestNames[suiteName].count(testName) > 0)
            {
                // Add index to make unique if needed
                int index = 1;
                juce::String baseName = testName;
                while (suiteTestNames[suiteName].count(testName) > 0)
                {
                    testName = baseName + " (" + juce::String(index++) + ")";
                }
            }
            
            // Remember this test name is used
            suiteTestNames[suiteName].insert(testName);
        }
        
        // Get or create test suite element
        if (testSuites.find(suiteName) == testSuites.end())
        {
            juce::XmlElement* suiteElement = new juce::XmlElement("testsuite");
            suiteElement->setAttribute("name", suiteName);
            testSuites[suiteName] = suiteElement;
            suiteTestCounts[suiteName] = 0;
            suiteFailureCounts[suiteName] = 0;
        }
        
        // Create test case element if we have a valid test name
        if (!testName.isEmpty())
        {
            // Create testcase element
            juce::XmlElement* testCaseElement = new juce::XmlElement("testcase");
            testCaseElement->setAttribute("name", testName);
            testCaseElement->setAttribute("classname", suiteName);
            testCaseElement->setAttribute("time", "0");
            
            // Add failure element if test failed
            if (isFailure)
            {
                juce::XmlElement* failureElement = new juce::XmlElement("failure");
                failureElement->setAttribute("message", failureMessage);
                testCaseElement->addChildElement(failureElement);
                suiteFailureCounts[suiteName]++;
                totalFailures++;
            }
            
            // Add test case to suite
            testSuites[suiteName]->addChildElement(testCaseElement);
            suiteTestCounts[suiteName]++;
            totalTests++;
        }
    }
    
    // Add test suites to root element with counts
    for (const auto& [name, suiteElement] : testSuites)
    {
        suiteElement->setAttribute("tests", suiteTestCounts[name]);
        suiteElement->setAttribute("failures", suiteFailureCounts[name]);
        suiteElement->setAttribute("errors", "0");
        rootElement.addChildElement(suiteElement);
    }
    
    // Set overall counts
    rootElement.setAttribute("tests", totalTests);
    rootElement.setAttribute("failures", totalFailures);
    
    // Write XML to file - use absolute path to avoid JUCE assertion
    juce::File outputFile = juce::File::getCurrentWorkingDirectory().getChildFile(outputPath);

    // Ensure parent directory exists
    if (outputFile.getParentDirectory().createDirectory())
    {
        // Write XML
        juce::FileOutputStream stream(outputFile);
        
        if (stream.openedOk())
        {
            rootElement.writeToStream(stream, {});
            std::cout << "JUnit XML report created at " << outputPath << std::endl;
            std::cout << "Total tests: " << totalTests << ", Failures: " << totalFailures << std::endl;
        }
        else
        {
            std::cerr << "Failed to create test report: could not open output file." << std::endl;
        }
    }
    else
    {
        std::cerr << "Failed to create test report: could not create directory." << std::endl;
    }
}

int main(int argc, char* argv[])
{
    // Get XML output path from arguments
    juce::String xmlOutputPath = "test_results.xml";
    if (argc > 1)
        xmlOutputPath = argv[1];
    
    // Initialize JUCE
    juce::ScopedJuceInitialiser_GUI juceInit;
    
    // Run all tests
    std::cout << "Running all tests..." << std::endl;
    
    // Create and configure test runner
    juce::UnitTestRunner testRunner;
    testRunner.setAssertOnFailure(false);
    
    // Run the tests
    testRunner.runAllTests();
    
    std::cout << "Tests completed." << std::endl;
    
    // Generate JUnit XML report using the test runner's results
    generateJUnitXmlReport(testRunner, xmlOutputPath);
    
    // Return success code (XML generation handles reporting failures)
    return 0;
}
