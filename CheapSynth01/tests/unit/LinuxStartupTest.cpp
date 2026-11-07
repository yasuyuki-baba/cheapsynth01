#include <gtest/gtest.h>
#include <X11/Xlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>

namespace {
namespace fs = std::filesystem;
using namespace std::chrono_literals;

std::string readFile(const fs::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

class LinuxStartupTest : public ::testing::Test {
   protected:
    void SetUp() override {
        std::string pattern = "/tmp/cheapsynth01-startup-XXXXXX";
        ASSERT_NE(mkdtemp(pattern.data()), nullptr);
        root = pattern;
        home = root / "home";
        fs::create_directory(home);
        display = XOpenDisplay(nullptr);
        ASSERT_NE(display, nullptr);
    }

    void TearDown() override {
        if (child > 0) {
            kill(child, SIGKILL);
            waitpid(child, nullptr, 0);
        }
        if (display)
            XCloseDisplay(display);
        if (!root.empty()) {
            if (fs::is_directory(root / "locked"))
                fs::permissions(root / "locked", fs::perms::owner_all);
            fs::remove_all(root);
        }
    }

    void start(std::optional<std::string> xdg, bool noIO = true,
               std::optional<std::string> homeOverride = std::nullopt) {
        ASSERT_LT(child, 0);
        const int log = open((root / "stderr.log").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        ASSERT_GE(log, 0);
        child = fork();
        if (child == 0) {
            if (dup2(log, STDERR_FILENO) < 0)
                _exit(126);
            close(log);
            if (chdir(root.c_str()) != 0)
                _exit(126);
            umask(0);
            const std::string selectedHome = homeOverride.value_or(home.string());
            if (setenv("LC_ALL", "C", 1) != 0 || setenv("HOME", selectedHome.c_str(), 1) != 0 ||
                setenv("XDG_DATA_HOME", (root / "data").c_str(), 1) != 0)
                _exit(126);
            if (xdg) {
                if (setenv("XDG_CONFIG_HOME", xdg->c_str(), 1) != 0)
                    _exit(126);
            } else if (unsetenv("XDG_CONFIG_HOME") != 0)
                _exit(126);
            if (noIO)
                execl(CS01_APP_PATH, CS01_APP_PATH, "--no-io", nullptr);
            else
                execl(CS01_APP_PATH, CS01_APP_PATH, nullptr);
            _exit(127);
        }
        close(log);
        ASSERT_GT(child, 0);
    }

    bool collectExit() {
        if (child <= 0)
            return true;
        const pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
            child = -1;
            return true;
        }
        return false;
    }

    Window editorWindow() {
        Window parent, rootWindow, *children = nullptr;
        unsigned count = 0;
        if (!XQueryTree(display, DefaultRootWindow(display), &rootWindow, &parent, &children,
                        &count))
            return None;
        Window found = None;
        for (unsigned i = 0; i < count; ++i) {
            XWindowAttributes attributes{};
            char* name = nullptr;
            if (XGetWindowAttributes(display, children[i], &attributes) &&
                XFetchName(display, children[i], &name) && name &&
                std::string(name) == "CheapSynth01" && attributes.map_state == IsViewable &&
                attributes.width == 1240)
                found = children[i];
            if (name)
                XFree(name);
        }
        if (children)
            XFree(children);
        return found;
    }

    void expectStartupAndClose() {
        Window editor = None;
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        while (std::chrono::steady_clock::now() < deadline && !collectExit()) {
            editor = editorWindow();
            if (editor != None)
                break;
            std::this_thread::sleep_for(20ms);
        }
        ASSERT_NE(editor, None) << readFile(root / "stderr.log");
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.window = editor;
        event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False);
        event.xclient.data.l[1] = CurrentTime;
        ASSERT_NE(XSendEvent(display, editor, False, NoEventMask, &event), 0);
        XFlush(display);
        const auto closeDeadline = std::chrono::steady_clock::now() + 5s;
        while (!collectExit() && std::chrono::steady_clock::now() < closeDeadline)
            std::this_thread::sleep_for(20ms);
        ASSERT_LT(child, 0) << "Standalone did not exit";
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0) << readFile(root / "stderr.log");
    }

    void expectFailure(const std::string& diagnostic) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!collectExit() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(20ms);
        ASSERT_LT(child, 0) << "Initialization failure did not terminate the application";
        ASSERT_TRUE(WIFEXITED(status)) << "Application terminated by a signal";
        EXPECT_EQ(WEXITSTATUS(status), 1);
        const auto log = readFile(root / "stderr.log");
        EXPECT_NE(log.find(diagnostic), std::string::npos) << log;
        EXPECT_NE(log.find("Standalone initialization failed"), std::string::npos) << log;
        EXPECT_EQ(editorWindow(), None);
    }

    fs::path settings(const fs::path& base) {
        return base / "cheapsynth01" / "settings.ini";
    }

    fs::path root;
    fs::path home;
    Display* display = nullptr;
    pid_t child = -1;
    int status = 0;
};

TEST_F(LinuxStartupTest, CreatesMissingXdgAncestorsWithPrivatePermissions) {
    const auto base = root / "missing" / "nested" / "config";
    start(base.string());
    ASSERT_NO_FATAL_FAILURE(expectStartupAndClose());
    EXPECT_TRUE(fs::is_regular_file(settings(base)));
    for (auto directory = base / "cheapsynth01"; directory != root;
         directory = directory.parent_path())
        EXPECT_EQ(fs::status(directory).permissions() & fs::perms::all, fs::perms::owner_all);
}

TEST_F(LinuxStartupTest, UnsetXdgCreatesMissingHomeConfig) {
    start(std::nullopt);
    ASSERT_NO_FATAL_FAILURE(expectStartupAndClose());
    EXPECT_TRUE(fs::is_regular_file(settings(home / ".config")));
}

TEST_F(LinuxStartupTest, EmptyXdgUsesHomeConfig) {
    start("");
    ASSERT_NO_FATAL_FAILURE(expectStartupAndClose());
    EXPECT_TRUE(fs::is_regular_file(settings(home / ".config")));
}

TEST_F(LinuxStartupTest, RelativeXdgUsesHomeInsteadOfWorkingDirectory) {
    start("relative-config");
    ASSERT_NO_FATAL_FAILURE(expectStartupAndClose());
    EXPECT_TRUE(fs::is_regular_file(settings(home / ".config")));
    EXPECT_FALSE(fs::exists(root / "relative-config"));
}

TEST_F(LinuxStartupTest, AbsoluteXdgDoesNotRequireHome) {
    const auto base = root / "config";
    start(base.string(), true, "");
    ASSERT_NO_FATAL_FAILURE(expectStartupAndClose());
    EXPECT_TRUE(fs::is_regular_file(settings(base)));
}

TEST_F(LinuxStartupTest, MissingAbsoluteBaseFailsExplicitly) {
    start(std::nullopt, false, "");
    expectFailure("absolute XDG_CONFIG_HOME or HOME is required");
}

TEST_F(LinuxStartupTest, RelativeHomeAndXdgFailExplicitly) {
    start("config", true, "relative-home");
    expectFailure("absolute XDG_CONFIG_HOME or HOME is required");
    EXPECT_FALSE(fs::exists(root / "relative-home"));
}

TEST_F(LinuxStartupTest, ExistingDirectoryPermissionsArePreserved) {
    const auto base = root / "config";
    fs::create_directory(base);
    const auto permissions = fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec;
    fs::permissions(base, permissions);
    start(base.string());
    ASSERT_NO_FATAL_FAILURE(expectStartupAndClose());
    EXPECT_EQ(fs::status(base).permissions() & fs::perms::all, permissions);
}

TEST_F(LinuxStartupTest, ReopenRestoresSettingsWithoutOverwritingValues) {
    const auto base = root / "config";
    start(base.string());
    ASSERT_NO_FATAL_FAILURE(expectStartupAndClose());
    auto content = readFile(settings(base));
    const auto buffer = content.find("buffer=512");
    ASSERT_NE(buffer, std::string::npos);
    content.replace(buffer, std::string("buffer=512").size(), "buffer=256");
    std::ofstream(settings(base)) << content;
    start(base.string());
    ASSERT_NO_FATAL_FAILURE(expectStartupAndClose());
    EXPECT_EQ(readFile(settings(base)), content);
}

TEST_F(LinuxStartupTest, FileInDirectoryPathFailsBeforeAudioStartup) {
    std::ofstream(root / "blocked") << "keep";
    start((root / "blocked" / "config").string(), false);
    expectFailure("Cannot create settings directory");
    EXPECT_EQ(readFile(root / "blocked"), "keep");
}

TEST_F(LinuxStartupTest, UnwritableParentFailsWithoutCrashing) {
    if (geteuid() == 0)
        GTEST_SKIP() << "Permission denial requires an unprivileged user";
    fs::create_directory(root / "locked");
    fs::permissions(root / "locked", fs::perms::owner_read | fs::perms::owner_exec);
    start((root / "locked" / "config").string(), false);
    expectFailure("Permission denied");
}

TEST_F(LinuxStartupTest, UnwritableSettingsDirectoryReportsWriteError) {
    if (geteuid() == 0)
        GTEST_SKIP() << "Permission denial requires an unprivileged user";
    const auto base = root / "locked";
    fs::create_directories(base / "cheapsynth01");
    fs::permissions(base, fs::perms::owner_read | fs::perms::owner_exec);
    fs::permissions(base / "cheapsynth01", fs::perms::owner_read | fs::perms::owner_exec);
    start(base.string(), false);
    expectFailure("Cannot save settings");
    fs::permissions(base / "cheapsynth01", fs::perms::owner_all);
}

TEST_F(LinuxStartupTest, SettingsDirectoryInPlaceOfFileReportsRenameFailure) {
    const auto base = root / "config";
    fs::create_directories(settings(base));
    start(base.string(), false);
    expectFailure("Cannot save settings");
    EXPECT_TRUE(fs::is_directory(settings(base)));
}

TEST_F(LinuxStartupTest, TemporaryFileWriteFailureIsNotReportedAsSuccess) {
    const auto base = root / "config";
    fs::create_directories(base / "cheapsynth01" / ".settings.ini.new");
    start(base.string(), false);
    expectFailure("Cannot save settings");
    EXPECT_FALSE(fs::exists(settings(base)));
}

TEST_F(LinuxStartupTest, BufferedWriteFailurePreservesExistingSettings) {
    const auto base = root / "config";
    fs::create_directories(base / "cheapsynth01");
    const std::string original = "[audio]\ndriver=0\n";
    std::ofstream(settings(base)) << original;
    fs::create_symlink("/dev/full", base / "cheapsynth01" / ".settings.ini.new");
    start(base.string(), false);
    expectFailure("No space left on device");
    EXPECT_EQ(readFile(settings(base)), original);
}

TEST_F(LinuxStartupTest, InvalidAudioDriverFailsBeforeDeviceLookup) {
    const auto base = root / "config";
    fs::create_directories(base / "cheapsynth01");
    std::ofstream(settings(base)) << "[audio]\ndriver=999\n";
    start(base.string(), false);
    expectFailure("Could not initialize audio driver 999");
}
}  // namespace
