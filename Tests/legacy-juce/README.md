# Historical JUCE assertion reference

Full Tests snapshot of origin/main at `32b91c8344a0581232afd7e0a79c5d25da22f42a`: 272 Google Test definitions.
These files/CMake metadata are inactive reference inputs. Active replacements
live in Tests/CoreTest.cpp and Tests/unit. Individual assertion dispositions are
recorded in docs/iPlug2-test-inventory.csv. JUCE internals and old session/automation
compatibility are outside the current scope. Current release contracts live in
Tests/packaging; archived release tools describe JUCE-era distribution.

`cmake/JuceGraphRealtimePatch.cmake` and `ThirdParty/AGPL-3.0.txt` in this
archive preserve the old JUCE build patch and license. They are not used by
the current build. Archived CMake files, scripts and documentation retain
historical assumptions and are not a runnable build of the current project.

The current conditional EG equation checks are maintained only in
`Tests/research/test_eg_decay_equations.py` (path relative to the repository
root). Run `python3 -m unittest discover -s Tests/research -v`; paths to
`Tests/eg_decay_equation_check.py` in historical documents describe the old
layout.
