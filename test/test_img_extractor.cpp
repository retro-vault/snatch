/// \file
/// \brief Unit tests for image glyph extraction.
///
/// This test source validates behavior of core parsing, extraction, transformation, and export flows. It helps ensure regressions are caught early for the plugin-driven pipeline.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include <gtest/gtest.h>

#include "snatch/img_extractor.h"

TEST(img_extractor, extracts_single_cell_from_image) {
    image_extract_options opt;
    opt.input_file = std::string(TEST_DATA_DIR) + "/12x16.png";
    opt.columns = 1;
    opt.rows = 1;
    opt.first_ascii = 65;
    opt.last_ascii = 65;

    extracted_font out;
    std::string err;
    img_extractor ex;
    const bool ok = ex.extract(opt, out, err);

    ASSERT_TRUE(ok) << err;
    EXPECT_EQ(out.first_codepoint, 65);
    EXPECT_EQ(out.last_codepoint, 65);
    ASSERT_EQ(out.bitmap_view.glyph_count, 1);
    ASSERT_EQ(out.glyphs.size(), 1u);
    EXPECT_GE(out.glyphs[0].view.height, 1);
}
