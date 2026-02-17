/// \file
/// \brief Unit tests for CLI parsing behavior.
///
/// This test source validates behavior of core parsing, extraction, transformation, and export flows. It helps ensure regressions are caught early for the plugin-driven pipeline.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <snatch/cli_parser.h>
#include <snatch/options.h>

struct argv_builder {
    std::vector<std::string> storage;
    std::vector<const char*> ptrs;

    argv_builder& arg(std::string s) { storage.push_back(std::move(s)); return *this; }
/// \brief finalize.
    std::pair<int, const char**> finalize() {
        ptrs.clear();
        for (auto& s : storage) ptrs.push_back(s.c_str());
        return {static_cast<int>(ptrs.size()), ptrs.data()};
    }
};

TEST(cli_parser, minimal_pipeline_args_parse) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("--extractor").arg("ttf_extractor")
     .arg("--extractor-parameters").arg("input=font.ttf,first_ascii=32,last_ascii=127,font_size=16")
     .arg("--transformer").arg("partner_bitmap_transform")
     .arg("--transformer-parameters").arg("font_mode=proportional")
     .arg("--exporter").arg("raw_c")
     .arg("--exporter-parameters").arg("output=out/font.c,symbol=my_font")
     .arg("--plugin-dir").arg("/tmp/snatch-plugins");

    auto [argc, argv] = b.finalize();
    const int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);

    EXPECT_EQ(opt.extractor, "ttf_extractor");
    EXPECT_EQ(opt.extractor_parameters, "input=font.ttf,first_ascii=32,last_ascii=127,font_size=16");
    EXPECT_EQ(opt.transformer, "partner_bitmap_transform");
    EXPECT_EQ(opt.transformer_parameters, "font_mode=proportional");
    EXPECT_EQ(opt.exporter, "raw_c");
    EXPECT_EQ(opt.exporter_parameters, "output=out/font.c,symbol=my_font");
    EXPECT_EQ(opt.plugin_dir.string(), "/tmp/snatch-plugins");
}

TEST(cli_parser, extractor_override_and_params_parse) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("--extractor").arg("image_passthrough_extractor")
     .arg("--extractor-parameters").arg("input=sprite.png,mode=passthrough");

    auto [argc, argv] = b.finalize();
    const int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);

    EXPECT_EQ(opt.extractor, "image_passthrough_extractor");
    EXPECT_EQ(opt.extractor_parameters, "input=sprite.png,mode=passthrough");
}

TEST(cli_parser, stage_specific_values_belong_to_plugin_parameters) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("--extractor").arg("image_extractor")
     .arg("--extractor-parameters").arg("input=sheet.png,columns=16,rows=6,margins=0,0,0,0,padding=1,1,1,1");

    auto [argc, argv] = b.finalize();
    const int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(opt.extractor, "image_extractor");
    EXPECT_EQ(opt.extractor_parameters, "input=sheet.png,columns=16,rows=6,margins=0,0,0,0,padding=1,1,1,1");
}

TEST(cli_parser, positional_input_is_rejected) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch").arg("font.ttf");

    auto [argc, argv] = b.finalize();
    const int rc = p.parse(argc, argv, opt);
    EXPECT_NE(rc, 0);
}
