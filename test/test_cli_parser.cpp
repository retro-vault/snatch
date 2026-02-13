#include <gtest/gtest.h>
#include <string>
#include <vector>

#include <snatch/cli_parser.h>
#include <snatch/options.h>

// helper to build argc/argv arrays
struct argv_builder {
    std::vector<std::string> storage;
    std::vector<const char*> ptrs;

    argv_builder& arg(std::string s) { storage.push_back(std::move(s)); return *this; }
    std::pair<int,const char**> finalize() {
        ptrs.clear();
        for (auto& s : storage) ptrs.push_back(s.c_str());
        return { static_cast<int>(ptrs.size()), ptrs.data() };
    }
};

TEST(cli_parser, basic_ttf_ok) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-s").arg("ttf")
     .arg("-o").arg("bin/out.bin")
     .arg("-e").arg("txt")
     .arg("-m").arg("1,2,3,4")
     .arg("-p").arg("5,6,7,8")
     .arg("-c").arg("16")
     .arg("-r").arg("6")
     .arg("-f").arg("#A0B1C2")
     .arg("-b").arg("001122")
     .arg("-a").arg("32")
     .arg("-z").arg("126")
     .arg("MyFont.ttf");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);

    EXPECT_EQ(opt.src_fmt, source_format::ttf);
    EXPECT_EQ(opt.output_file.string(), "bin/out.bin");
    EXPECT_EQ(opt.exporter, "txt");

    EXPECT_EQ(opt.margins.left, 1);
    EXPECT_EQ(opt.margins.top, 2);
    EXPECT_EQ(opt.margins.right, 3);
    EXPECT_EQ(opt.margins.bottom, 4);

    EXPECT_EQ(opt.padding.left, 5);
    EXPECT_EQ(opt.padding.top, 6);
    EXPECT_EQ(opt.padding.right, 7);
    EXPECT_EQ(opt.padding.bottom, 8);

    EXPECT_EQ(opt.columns, 16);
    EXPECT_EQ(opt.rows, 6);

    EXPECT_EQ(opt.first_ascii, 32);
    EXPECT_EQ(opt.last_ascii, 126);

    EXPECT_EQ(opt.input_file.string(), "MyFont.ttf");

    EXPECT_EQ(opt.fore_color.r, 0xA0);
    EXPECT_EQ(opt.fore_color.g, 0xB1);
    EXPECT_EQ(opt.fore_color.b, 0xC2);
    EXPECT_EQ(opt.back_color.r, 0x00);
    EXPECT_EQ(opt.back_color.g, 0x11);
    EXPECT_EQ(opt.back_color.b, 0x22);
}

TEST(cli_parser, inverse_and_short_flags) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-i")
     .arg("-s").arg("image")
     .arg("-c").arg("8")
     .arg("-r").arg("4")
     .arg("-x").arg("note=hello world") // exporter parameters
     .arg("-e").arg("txt")
     .arg("-o").arg("out.txt")
     .arg("sprite.png");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(opt.inverse);
    EXPECT_EQ(opt.src_fmt, source_format::image);
    EXPECT_EQ(opt.columns, 8);
    EXPECT_EQ(opt.rows, 4);
    EXPECT_EQ(opt.exporter, "txt");
    EXPECT_EQ(opt.exporter_parameters, "note=hello world");
    EXPECT_EQ(opt.output_file.string(), "out.txt");
    EXPECT_EQ(opt.input_file.string(), "sprite.png");
}

TEST(cli_parser, edge4_missing_values_repeat_last) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-m").arg("4,8")
     .arg("-p").arg("1")
     .arg("f.ttf");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);

    EXPECT_EQ(opt.margins.left, 4);
    EXPECT_EQ(opt.margins.top, 8);
    EXPECT_EQ(opt.margins.right, 8);
    EXPECT_EQ(opt.margins.bottom, 8);

    EXPECT_EQ(opt.padding.left, 1);
    EXPECT_EQ(opt.padding.top, 1);
    EXPECT_EQ(opt.padding.right, 1);
    EXPECT_EQ(opt.padding.bottom, 1);
}

TEST(cli_parser, readme_alias_flags_parse) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-sf").arg("image")
     .arg("-output").arg("out/font.bin")
     .arg("sprite.png");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);

    EXPECT_EQ(opt.src_fmt, source_format::image);
    EXPECT_EQ(opt.output_file.string(), "out/font.bin");
    EXPECT_EQ(opt.input_file.string(), "sprite.png");
}

TEST(cli_parser, plugin_dir_override_parses) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("--plugin-dir").arg("/tmp/snatch-plugins")
     .arg("sprite.png");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);

    EXPECT_EQ(opt.plugin_dir.string(), "/tmp/snatch-plugins");
}

TEST(cli_parser, font_size_parses) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("--font-size").arg("16")
     .arg("font.ttf");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(opt.font_size, 16);
}

TEST(cli_parser, transparent_color_sets_flag) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-t").arg("#FF00FF")
     .arg("f.ttf");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(opt.has_transparent);
    EXPECT_EQ(opt.transparent_color.r, 0xFF);
    EXPECT_EQ(opt.transparent_color.g, 0x00);
    EXPECT_EQ(opt.transparent_color.b, 0xFF);
}

TEST(cli_parser, invalid_margins_returns_error) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-m").arg("1,2,XYZ")
     .arg("f.ttf");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    EXPECT_NE(rc, 0);
}

TEST(cli_parser, invalid_colors_returns_error) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-f").arg("#12")  // too short
     .arg("f.ttf");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    EXPECT_NE(rc, 0);
}

TEST(cli_parser, ascii_range_order_invalid) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-a").arg("100")
     .arg("-z").arg("60")
     .arg("f.ttf");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    EXPECT_NE(rc, 0);
}

TEST(cli_parser, missing_input_file_is_error) {
    cli_parser p;
    snatch_options opt;

    argv_builder b;
    b.arg("snatch")
     .arg("-s").arg("ttf");

    auto [argc, argv] = b.finalize();
    int rc = p.parse(argc, argv, opt);
    EXPECT_NE(rc, 0);
}
