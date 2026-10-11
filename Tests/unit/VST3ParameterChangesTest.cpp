#include "VST3ParameterChanges.h"
#include "VST3State.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>

namespace {
using namespace Steinberg;
using namespace Steinberg::Vst;
constexpr auto cutoff = static_cast<unsigned>(cs01::Param::Cutoff);
class Queue final : public IParamValueQueue {
   public:
    ParamID id = cs01::vstHostId(cutoff);
    std::array<ParamValue, 2> values{.2, .8};
    std::array<int32, 2> offsets{3, 47};
    int32 count = 2;
    tresult status = kResultOk;
    tresult PLUGIN_API queryInterface(const TUID, void**) override {
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override {
        return 1;
    }
    uint32 PLUGIN_API release() override {
        return 1;
    }
    ParamID PLUGIN_API getParameterId() override {
        return id;
    }
    int32 PLUGIN_API getPointCount() override {
        return count;
    }
    tresult PLUGIN_API getPoint(int32 index, int32& offset, ParamValue& value) override {
        if (index < 0 || index >= count)
            return kInvalidArgument;
        offset = offsets[index];
        value = values[index];
        return status;
    }
    tresult PLUGIN_API addPoint(int32, ParamValue, int32&) override {
        return kNotImplemented;
    }
};
class Changes final : public IParameterChanges {
   public:
    Queue queue;
    tresult PLUGIN_API queryInterface(const TUID, void**) override {
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override {
        return 1;
    }
    uint32 PLUGIN_API release() override {
        return 1;
    }
    int32 PLUGIN_API getParameterCount() override {
        return 1;
    }
    IParamValueQueue* PLUGIN_API getParameterData(int32 index) override {
        return index == 0 ? &queue : nullptr;
    }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&, int32&) override {
        return nullptr;
    }
};
}  // namespace
TEST(VST3BoundaryTest, LegacyAutomationPreservesHostQueuePointsAndOffsets) {
    Changes host;
    int presets = 0;
    cs01::VST3ParameterChangesView view(host, [&](double) { ++presets; });
    ASSERT_EQ(view.getParameterCount(), 1);
    auto* queue = view.getParameterData(0);
    ASSERT_NE(queue, nullptr);
    EXPECT_EQ(queue->getParameterId(), cutoff);
    ASSERT_EQ(queue->getPointCount(), 2);
    for (int i = 0; i < 2; ++i) {
        int32 offset = -1;
        double value = -1;
        ASSERT_EQ(queue->getPoint(i, offset, value), kResultOk);
        EXPECT_EQ(offset, host.queue.offsets[i]);
        EXPECT_EQ(value, host.queue.values[i]);
    }
    EXPECT_EQ(host.queue.id, cs01::vstHostId(cutoff));
    EXPECT_EQ(presets, 0);
}
TEST(VST3BoundaryTest, ProgramQueueUsesOnlyLastValidPoint) {
    Changes host;
    host.queue.id = cs01::legacyVstProgram;
    double selected = -1;
    int requests = 0;
    cs01::VST3ParameterChangesView view(host, [&](double value) {
        selected = value;
        ++requests;
    });
    EXPECT_EQ(view.getParameterData(0), nullptr);
    EXPECT_EQ(selected, .8);
    EXPECT_EQ(requests, 1);
    for (double invalid : {-1., 1.1, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()}) {
        host.queue.values[1] = invalid;
        EXPECT_EQ(view.getParameterData(0), nullptr);
    }
    host.queue.values[1] = .5;
    host.queue.status = kInvalidArgument;
    EXPECT_EQ(view.getParameterData(0), nullptr);
    host.queue.count = 0;
    host.queue.status = kResultOk;
    EXPECT_EQ(view.getParameterData(0), nullptr);
    EXPECT_EQ(requests, 1);
}
TEST(VST3BoundaryTest, UnknownIdsAndMissingQueuesAreIgnored) {
    Changes host;
    host.queue.id = 0xffffffffu;
    int requests = 0;
    cs01::VST3ParameterChangesView view(host, [&](double) { ++requests; });
    EXPECT_EQ(view.getParameterData(0), nullptr);
    EXPECT_EQ(view.getParameterData(1), nullptr);
    EXPECT_EQ(requests, 0);
}
TEST(VST3BoundaryTest, MidiChannelAndMigrationAliasesAreMappedWithoutChangingHost) {
    Changes host;
    cs01::VST3ParameterChangesView view(host, [](double) {});
    for (unsigned i = 0; i < cs01::vstMidiCount; ++i) {
        host.queue.id = cs01::legacyVstMidiStart + i;
        auto* queue = view.getParameterData(0);
        ASSERT_NE(queue, nullptr);
        EXPECT_EQ(queue->getParameterId(), cs01::internalVstMidiStart + i);
        EXPECT_EQ(host.queue.id, cs01::legacyVstMidiStart + i);
    }
    host.queue.id = cutoff;
    ASSERT_NE(view.getParameterData(0), nullptr);
    EXPECT_EQ(view.getParameterData(0)->getParameterId(), cutoff);
}
TEST(VST3BoundaryTest, BorrowedViewsExposeCorrectReadOnlyComInterfaces) {
    Changes host;
    cs01::VST3ParameterChangesView view(host, [](double) {});
    void* result = nullptr;
    EXPECT_EQ(view.queryInterface(IParameterChanges_iid, &result), kResultOk);
    EXPECT_EQ(result, static_cast<IParameterChanges*>(&view));
    EXPECT_EQ(view.queryInterface(IParameterChanges_iid, nullptr), kInvalidArgument);
    auto* queue = view.getParameterData(0);
    ASSERT_NE(queue, nullptr);
    EXPECT_EQ(queue->queryInterface(IParamValueQueue_iid, &result), kResultOk);
    EXPECT_EQ(result, queue);
    int32 index = -1;
    EXPECT_EQ(queue->addPoint(0, .5, index), kNotImplemented);
    EXPECT_EQ(view.addParameterData(host.queue.id, index), nullptr);
    EXPECT_EQ(host.queue.count, 2);
}

namespace {
class Stream final : public IBStream {
   public:
    std::string bytes;
    size_t position = 0;
    int32 block = 7;
    bool falseOnFinal = true;
    tresult PLUGIN_API queryInterface(const TUID, void**) override {
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override {
        return 1;
    }
    uint32 PLUGIN_API release() override {
        return 1;
    }
    tresult PLUGIN_API read(void* buffer, int32 size, int32* count) override {
        const auto n = std::min(
            {static_cast<size_t>(size), static_cast<size_t>(block), bytes.size() - position});
        std::memcpy(buffer, bytes.data() + position, n);
        position += n;
        *count = static_cast<int32>(n);
        return falseOnFinal && position == bytes.size() ? kResultFalse : kResultOk;
    }
    tresult PLUGIN_API write(void*, int32, int32*) override {
        return kNotImplemented;
    }
    tresult PLUGIN_API seek(int64, int32, int64*) override {
        return kNotImplemented;
    }
    tresult PLUGIN_API tell(int64*) override {
        return kNotImplemented;
    }
};
}  // namespace
TEST(VST3BoundaryTest, StateShortReadsPreserveXmlAndNativeBypassTrailer) {
    for (const bool saved : {false, true}) {
        Stream stream;
        stream.bytes = "<Parameters><PARAM id=\"CUTOFF\" value=\"1200\"/></Parameters>";
        const auto xml = stream.bytes;
        stream.bytes.append(saved ? "\1\0\0\0" : "\0\0\0\0", 4);
        bool bypass = !saved;
        ASSERT_TRUE(cs01::restoreVst3State(
            &stream,
            [&](std::string_view payload) {
                EXPECT_EQ(payload, xml);
                return payload == xml;
            },
            bypass));
        EXPECT_EQ(bypass, saved);
    }
}
TEST(VST3BoundaryTest, FailedOrOversizedStateDoesNotCommitBypass) {
    bool bypass = true;
    EXPECT_FALSE(cs01::restoreVst3State(nullptr, [](auto) { return true; }, bypass));
    Stream stream;
    stream.bytes = "bad";
    EXPECT_FALSE(cs01::restoreVst3State(&stream, [](auto) { return false; }, bypass));
    EXPECT_TRUE(bypass);
    stream.bytes.assign(1024 * 1024 + 1, 'x');
    stream.position = 0;
    stream.block = 4096;
    bool called = false;
    EXPECT_FALSE(cs01::restoreVst3State(
        &stream,
        [&](auto) {
            called = true;
            return true;
        },
        bypass));
    EXPECT_FALSE(called);
    EXPECT_TRUE(bypass);
}
TEST(VST3BoundaryTest, LegacyBinaryXmlAndPrivateTailReachLengthCheckedDecoderUnchanged) {
    Stream stream;
    stream.bytes = "VC2!";
    stream.bytes.append("\3\0\0\0xml", 7);
    stream.bytes.append("\0\0\0\0", 4);
    bool bypass = true;
    EXPECT_TRUE(cs01::restoreVst3State(
        &stream,
        [&](std::string_view payload) {
            EXPECT_EQ(payload, stream.bytes);
            return payload == stream.bytes;
        },
        bypass));
    EXPECT_FALSE(bypass);
}
