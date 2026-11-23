#include "LinuxSettings.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

char* g_swell_defini = nullptr;
namespace fs = std::filesystem;
namespace {
std::string read(const fs::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
class LinuxSettingsTest : public ::testing::Test {
   protected:
    fs::path root;
    void SetUp() override {
        char pattern[] = "/tmp/cs01-settings-XXXXXX";
        ASSERT_NE(mkdtemp(pattern), nullptr);
        root = pattern;
    }
    void TearDown() override {
        fs::remove_all(root);
    }
    fs::path settings() const {
        return root / "cheapsynth01/settings.ini";
    }
    void existing(std::string_view text) {
        fs::create_directory(root / "cheapsynth01");
        std::ofstream(settings()) << text;
    }
    fs::path prepare(bool noIO = false) {
        return cs01::prepareLinuxSettings(root.c_str(), nullptr, "cheapsynth01", noIO);
    }
};

TEST_F(LinuxSettingsTest, CompletesPartialSettingsAndPreservesUnrelatedPreferences) {
    const std::string original = "; user comment\n[.swell]\nuiscale=1.5\n[audio]\nbuffer=256\n";
    existing(original);
    prepare();
    const auto text = read(settings());
    EXPECT_EQ(text.substr(0, original.size()), original);
    EXPECT_NE(text.find("buffer=256"), std::string::npos);
    EXPECT_NE(text.find("indev=default"), std::string::npos);
    prepare();
    EXPECT_EQ(read(settings()), text);
    EXPECT_EQ(fs::status(settings()).permissions() & fs::perms::all,
              fs::perms::owner_read | fs::perms::owner_write);
}

TEST_F(LinuxSettingsTest, NormalizesNumbersBeforeTheSDKWritesDefaults) {
    existing("[audio]\ndriver=00\nbuffer=0256\n");
    prepare();
    const auto text = read(settings());
    EXPECT_NE(text.find("driver=0\n"), std::string::npos);
    EXPECT_NE(text.find("buffer=256\n"), std::string::npos);
    prepare();
    EXPECT_EQ(read(settings()), text);
}

TEST_F(LinuxSettingsTest, RejectsDeviceNamesThatTheSDKWouldTruncate) {
    const auto original = "[audio]\nindev=" + std::string(200, 'x') + "\n";
    existing(original);
    EXPECT_THROW(prepare(), std::runtime_error);
    EXPECT_EQ(read(settings()), original);
}

TEST_F(LinuxSettingsTest, InvalidDriverStopsBeforeChangingExistingSettings) {
    const std::string original = "[audio]\ndriver=999\n";
    existing(original);
    EXPECT_THROW(prepare(), std::runtime_error);
    EXPECT_EQ(read(settings()), original);
    EXPECT_FALSE(fs::exists(root / "cheapsynth01/.settings.ini.new"));
}

TEST_F(LinuxSettingsTest, NoIOModeDoesNotRequireAValidAudioDriver) {
    existing("[audio]\ndriver=999\n");
    EXPECT_NO_THROW(prepare(true));
}

TEST_F(LinuxSettingsTest, RefusesTemporarySymlinksAndPreservesOriginalForRetry) {
    const std::string original = "[audio]\ndriver=0\n";
    existing(original);
    const auto temporary = root / "cheapsynth01/.settings.ini.new";
    fs::create_symlink("/dev/full", temporary);
    EXPECT_THROW(prepare(), std::system_error);
    EXPECT_EQ(read(settings()), original);
    EXPECT_TRUE(fs::is_symlink(temporary));
    fs::remove(temporary);
    EXPECT_NO_THROW(prepare());
    EXPECT_NE(read(settings()).find("buffer=512"), std::string::npos);
}

TEST_F(LinuxSettingsTest, BufferedWriteFailureLeavesOriginalIntact) {
    const std::string original = "[audio]\ndriver=0\n";
    existing(original);
    const pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        rlimit limit{};
        if (getrlimit(RLIMIT_FSIZE, &limit) != 0)
            _exit(2);
        limit.rlim_cur = 0;
        if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
            _exit(2);
        signal(SIGXFSZ, SIG_IGN);
        try {
            prepare();
        } catch (const std::system_error& error) {
            _exit(error.code().value() == EFBIG ? 0 : 3);
        }
        _exit(4);
    }
    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
    EXPECT_EQ(read(settings()), original);
    EXPECT_FALSE(fs::exists(root / "cheapsynth01/.settings.ini.new"));
    EXPECT_NO_THROW(prepare());
}
}  // namespace
