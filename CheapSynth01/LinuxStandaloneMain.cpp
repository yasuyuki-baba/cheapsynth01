#include "LinuxSettings.h"
#include "config.h"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

extern "C" int __real_main(int argc, char** argv);

// The SDK main stays untouched. GNU/LLD --wrap runs this application-owned
// preparation before the SDK initializes GTK, settings, audio or MIDI.
extern "C" int __wrap_main(int argc, char** argv) {
    bool noIO = false;
    for (int i = 1; i < argc; ++i)
        noIO = noIO || std::string_view(argv[i]) == "--no-io";
    try {
        cs01::prepareLinuxSettings(std::getenv("XDG_CONFIG_HOME"), std::getenv("HOME"), BUNDLE_NAME,
                                   noIO);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return EXIT_FAILURE;
    }
    return __real_main(argc, argv);
}
