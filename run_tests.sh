#!/bin/bash

# Script to build and run tests
cd "$(dirname "$0")" || exit 1

# Optional functional groups map to Google Test suite names. Keep the group
# filter first so a caller-supplied --gtest_filter can override it.
GROUP_FILTER=""
TEST_ARGS=()
while [ "$#" -gt 0 ]; do
    case "$1" in
        --group)
            if [ "$#" -lt 2 ]; then
                echo "Missing name after --group" >&2
                exit 2
            fi
            case "$2" in
                audio|audio-core)
                    GROUP_FILTER='DspStructureRegressionTest.*:IG02610BehavioralModel*.*:IG05630BehavioralModelTest.*:VCOProcessorTest.*:ToneGeneratorTest.*:ToneGeneratorRealTest.*:ManualPitchTest.*:VcoSpectrumTest.*:OriginalVCFProcessorTest.*:ModernVCFProcessorTest.*:CS01IIVCFCircuitTest.*:IG05630Test.*:IG02610ControlTest.*:IG02610OversamplingTest.*:IG02610SpectrumTest.*:IG02610NonlinearSafetyTest.*:CS01VCFCircuitTest.*:VCAProcessorTest.*:ResponseMeasurementTest.*:EGProcessorTest.*:EGTimingTest.*:RCEnvelopeModelTest.*:LFOProcessorTest.*:NoiseGeneratorTest.*'
                    ;;
                midi|control)
                    GROUP_FILTER='MidiProcessorTest.*:MidiResetGraphTest.*:MidiPanicGraphTest.*:MidiRealtimeGraphTest.*:BendInputTest.*'
                    ;;
                ui)
                    GROUP_FILTER='EditorVisibilityTest.*:ProgramPanelTest.*:AudioDisplayFifoTest.*:ParameterFormattingTest.*'
                    ;;
                presets|state)
                    GROUP_FILTER='ProgramManagerTest.*:PresetSelectionTest.*:ParameterVersionTest.*:ProductionStateTest.*:SessionGraphTest.*'
                    ;;
                integration)
                    GROUP_FILTER='AudioGraphTest.*:MidiResetGraphTest.*:MidiPanicGraphTest.*:MidiRealtimeGraphTest.*:SessionGraphTest.*:EnvelopeRangeTest.*:BendInputTest.*:ModulationRangeTest.*:WholeGraphObservationTest.*:OutputConversionTest.*'
                    ;;
                *)
                    echo "Unknown test group '$2'. Use audio, midi, ui, presets, or integration." >&2
                    exit 2
                    ;;
            esac
            shift 2
            ;;
        *)
            TEST_ARGS+=("$1")
            shift
            ;;
    esac
done

# Color definitions
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${YELLOW}CheapSynth01 Test Runner${NC}"
echo "========================================"

# Create build directory
echo -e "${YELLOW}Creating build directory...${NC}"
mkdir -p build_tests
cd build_tests

# Run CMake
echo -e "${YELLOW}Running CMake...${NC}"
cmake .. -DSTANDALONE_ONLY=ON
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

# Build
echo -e "${YELLOW}Building tests...${NC}"
make CheapSynth01Tests
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# Run tests and generate JUnit XML report
echo -e "${YELLOW}Running tests...${NC}"
echo "========================================"
if [ -n "$GROUP_FILTER" ]; then
    ./Tests/CheapSynth01Tests_artefacts/Debug/CheapSynth01Tests "--gtest_filter=$GROUP_FILTER" "${TEST_ARGS[@]}"
else
    ./Tests/CheapSynth01Tests_artefacts/Debug/CheapSynth01Tests "${TEST_ARGS[@]}"
fi
TEST_RESULT=$?

# Display test summary
if [ $TEST_RESULT -eq 0 ]; then
    echo "========================================"
    echo -e "${GREEN}All tests passed!${NC}"
else
    echo "========================================"
    echo -e "${RED}Tests failed${NC}"
fi

echo -e "${BLUE}Test results saved to:${NC} $(pwd)/test_results.xml"
echo "========================================"

# Return the original test result
exit $TEST_RESULT
