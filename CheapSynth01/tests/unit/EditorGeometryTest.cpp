#include "interaction/EditorGeometry.h"
#include <gtest/gtest.h>
#include <limits>

using cs01::ui::fitEditorScale;

TEST(EditorGeometryTest, CompactAndExpandedCanvasesKeepTheirAspectRatio) {
    EXPECT_FLOAT_EQ(*fitEditorScale(1860, 600, 1240, 400, 1.f), 1.5f);
    EXPECT_FLOAT_EQ(*fitEditorScale(1860, 960, 1240, 640, 1.f), 1.5f);
    EXPECT_FLOAT_EQ(*fitEditorScale(2000, 600, 1240, 400, 1.f), 1.5f);
    EXPECT_FLOAT_EQ(*fitEditorScale(1550, 1000, 1240, 640, 1.f), 1.25f);
}

TEST(EditorGeometryTest, PlatformScaleIsRemovedExactlyOnce) {
    EXPECT_FLOAT_EQ(*fitEditorScale(3720, 1200, 1240, 400, 2.f), 1.5f);
    EXPECT_FLOAT_EQ(*fitEditorScale(2325, 750, 1240, 400, 1.25f), 1.5f);
    // Cocoa may report platform scale 1 even on a Retina display. Use the
    // platform coordinate scale, not the screen backing scale a second time.
    EXPECT_FLOAT_EQ(*fitEditorScale(1550, 500, 1240, 400, 1.f), 1.25f);
}

TEST(EditorGeometryTest, ScaleLimitsMatchTheEditorMenuAndCornerResizer) {
    EXPECT_FLOAT_EQ(*fitEditorScale(620, 200, 1240, 400, 1.f), .75f);
    EXPECT_FLOAT_EQ(*fitEditorScale(3720, 1200, 1240, 400, 1.f), 2.f);
    EXPECT_FLOAT_EQ(*fitEditorScale(930, 480, 1240, 640, 1.f), .75f);
}

TEST(EditorGeometryTest, EmptyAndInvalidHostNotificationsAreIgnored) {
    EXPECT_FALSE(fitEditorScale(0, 400, 1240, 400, 1.f));
    EXPECT_FALSE(fitEditorScale(1240, -1, 1240, 400, 1.f));
    EXPECT_FALSE(fitEditorScale(1240, 400, 0, 400, 1.f));
    EXPECT_FALSE(fitEditorScale(1240, 400, 1240, 0, 1.f));
    for (float scale : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()})
        EXPECT_FALSE(fitEditorScale(1240, 400, 1240, 400, scale));
}
