/// \file
/// \brief End-to-end plugin pipeline integration tests.
///
/// This test source validates behavior of core parsing, extraction, transformation, and export flows. It helps ensure regressions are caught early for the plugin-driven pipeline.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace {

struct cmd_result {
    int exit_code{1};
    std::string output;
};

/// \brief run_command_capture.
cmd_result run_command_capture(const std::string& cmd) {
    cmd_result res;
    const std::string full = cmd + " 2>&1";
    FILE* pipe = popen(full.c_str(), "r");
    if (!pipe) {
        res.output = "popen failed";
        return res;
    }

    std::array<char, 512> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        res.output += buffer.data();
    }

    const int status = pclose(pipe);
    if (status == -1) {
        res.exit_code = 1;
        return res;
    }
#ifdef _WIN32
    res.exit_code = status;
#else
    if (WIFEXITED(status)) {
        res.exit_code = WEXITSTATUS(status);
    } else {
        res.exit_code = 1;
    }
#endif
    return res;
}

/// \brief q.
std::string q(const std::filesystem::path& p) {
    return std::string("\"") + p.string() + "\"";
}

/// \brief read_file.
std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::in | std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST(pipeline_plugins, ttf_extractor_is_used_end_to_end) {
    const std::filesystem::path out = std::filesystem::temp_directory_path() / "snatch_ttf_pipeline.bin";
    std::filesystem::remove(out);

    const std::string cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor-parameters \"input=" + (std::filesystem::path(TEST_DATA_DIR) / "flappybirdy-regular.ttf").string() + ",first_ascii=65,last_ascii=67,font_size=16\"" +
        " --exporter raw_bin" +
        " --exporter-parameters \"output=" + out.string() + "\"";

    const auto res = run_command_capture(cmd);
    ASSERT_EQ(res.exit_code, 0) << res.output;
    EXPECT_NE(res.output.find("extracted with plugin: ttf_extractor"), std::string::npos) << res.output;
    EXPECT_NE(res.output.find("exported with plugin: raw_bin"), std::string::npos) << res.output;
    ASSERT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

TEST(pipeline_plugins, image_extractor_is_used_end_to_end) {
    const std::filesystem::path out = std::filesystem::temp_directory_path() / "snatch_image_pipeline.bin";
    std::filesystem::remove(out);

    const std::string cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor-parameters \"input=" + (std::filesystem::path(TEST_DATA_DIR) / "12x16.png").string() + ",columns=16,rows=6,first_ascii=32,last_ascii=33\"" +
        " --exporter raw_bin" +
        " --exporter-parameters \"output=" + out.string() + "\"";

    const auto res = run_command_capture(cmd);
    ASSERT_EQ(res.exit_code, 0) << res.output;
    EXPECT_NE(res.output.find("extracted with plugin: image_extractor"), std::string::npos) << res.output;
    EXPECT_NE(res.output.find("exported with plugin: raw_bin"), std::string::npos) << res.output;
    ASSERT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

TEST(pipeline_plugins, transformer_chain_and_raw_c_const_output) {
    const std::filesystem::path out = std::filesystem::temp_directory_path() / "snatch_pipeline_raw.c";
    std::filesystem::remove(out);

    const std::string cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor-parameters \"input=" + (std::filesystem::path(TEST_DATA_DIR) / "flappybirdy-regular.ttf").string() + ",first_ascii=65,last_ascii=67,font_size=16\"" +
        " --transformer partner_bitmap_transform" +
        " --transformer-parameters \"font_mode=proportional,space_width=3,letter_spacing=2\"" +
        " --exporter raw_c" +
        " --exporter-parameters \"output=" + out.string() + ",bytes_per_line=8,symbol=test_font\"";

    const auto res = run_command_capture(cmd);
    ASSERT_EQ(res.exit_code, 0) << res.output;
    EXPECT_NE(res.output.find("extracted with plugin: ttf_extractor"), std::string::npos) << res.output;
    EXPECT_NE(res.output.find("transformed with plugin: partner_bitmap_transform"), std::string::npos) << res.output;
    EXPECT_NE(res.output.find("exported with plugin: raw_c"), std::string::npos) << res.output;

    ASSERT_TRUE(std::filesystem::exists(out));
    const std::string text = read_file(out);
    EXPECT_NE(text.find("const uint8_t test_font[]"), std::string::npos);
}

TEST(pipeline_plugins, image_passthrough_dither_png_concept) {
    const std::filesystem::path out = std::filesystem::temp_directory_path() / "snatch_tutankhamun_dither.png";
    std::filesystem::remove(out);

    const std::string cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor image_passthrough_extractor" +
        " --extractor-parameters \"input=" + (std::filesystem::path(TEST_DATA_DIR) / "tut.png").string() + "\"" +
        " --transformer dither_1bpp_transform" +
        " --transformer-parameters \"threshold=128\"" +
        " --exporter png" +
        " --exporter-parameters \"output=" + out.string() + ",columns=1,rows=1,padding=0,grid_thickness=0\"";

    const auto res = run_command_capture(cmd);
    ASSERT_EQ(res.exit_code, 0) << res.output;
    EXPECT_NE(res.output.find("extracted with plugin: image_passthrough_extractor"), std::string::npos) << res.output;
    EXPECT_NE(res.output.find("transformed with plugin: dither_1bpp_transform"), std::string::npos) << res.output;
    EXPECT_NE(res.output.find("exported with plugin: png"), std::string::npos) << res.output;
    ASSERT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
}

TEST(pipeline_plugins, missing_extractor_input_parameter_is_error) {
    const std::filesystem::path out = std::filesystem::temp_directory_path() / "snatch_missing_input.bin";
    std::filesystem::remove(out);

    const std::string cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor ttf_extractor" +
        " --extractor-parameters \"first_ascii=65,last_ascii=67,font_size=16\"" +
        " --exporter raw_bin" +
        " --exporter-parameters \"output=" + out.string() + "\"";

    const auto res = run_command_capture(cmd);
    EXPECT_NE(res.exit_code, 0);
    EXPECT_NE(res.output.find("extractor input path is required"), std::string::npos) << res.output;
}

TEST(pipeline_plugins, missing_exporter_output_parameter_is_error) {
    const std::string cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor-parameters \"input=" + (std::filesystem::path(TEST_DATA_DIR) / "12x16.png").string() + ",columns=16,rows=6,first_ascii=32,last_ascii=33\"" +
        " --exporter raw_bin";

    const auto res = run_command_capture(cmd);
    EXPECT_NE(res.exit_code, 0);
    EXPECT_NE(res.output.find("exporter output path is required"), std::string::npos) << res.output;
}

TEST(pipeline_plugins, partner_tiny_bin_roundtrip_to_png_grid) {
    const std::filesystem::path tiny_bin = std::filesystem::temp_directory_path() / "snatch_partner_tiny_roundtrip.bin";
    const std::filesystem::path png_out = std::filesystem::temp_directory_path() / "snatch_partner_tiny_roundtrip.png";
    std::filesystem::remove(tiny_bin);
    std::filesystem::remove(png_out);

    const std::string encode_cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor-parameters \"input=" + (std::filesystem::path(TEST_DATA_DIR) / "flappybirdy-regular.ttf").string() + ",first_ascii=65,last_ascii=70,font_size=16\"" +
        " --transformer partner_tiny_transform" +
        " --exporter raw_bin" +
        " --exporter-parameters \"output=" + tiny_bin.string() + ",font_mode=proportional,space_width=3,letter_spacing=1\"";

    const auto encode_res = run_command_capture(encode_cmd);
    ASSERT_EQ(encode_res.exit_code, 0) << encode_res.output;
    ASSERT_TRUE(std::filesystem::exists(tiny_bin));
    ASSERT_GT(std::filesystem::file_size(tiny_bin), 0u);

    const std::string decode_cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor partner_tiny_bin_extractor" +
        " --extractor-parameters \"input=" + tiny_bin.string() + "\"" +
        " --transformer partner_tiny_raster_transform" +
        " --exporter png" +
        " --exporter-parameters \"output=" + png_out.string() + ",columns=3,rows=2,padding=1,grid_thickness=1\"";

    const auto decode_res = run_command_capture(decode_cmd);
    ASSERT_EQ(decode_res.exit_code, 0) << decode_res.output;
    EXPECT_NE(decode_res.output.find("extracted with plugin: partner_tiny_bin_extractor"), std::string::npos) << decode_res.output;
    EXPECT_NE(decode_res.output.find("transformed with plugin: partner_tiny_raster_transform"), std::string::npos) << decode_res.output;
    EXPECT_NE(decode_res.output.find("exported with plugin: png"), std::string::npos) << decode_res.output;
    ASSERT_TRUE(std::filesystem::exists(png_out));
    EXPECT_GT(std::filesystem::file_size(png_out), 0u);
}

TEST(pipeline_plugins, partner_tiny_invalid_spacing_parameters_fail) {
    const std::filesystem::path tiny_bin = std::filesystem::temp_directory_path() / "snatch_partner_tiny_invalid_spacing.bin";
    std::filesystem::remove(tiny_bin);

    const std::string cmd =
        std::string(SNATCH_BIN_PATH) +
        " --plugin-dir " + q(SNATCH_PLUGIN_DIR_PATH) +
        " --extractor-parameters \"input=" + (std::filesystem::path(TEST_DATA_DIR) / "flappybirdy-regular.ttf").string() + ",first_ascii=65,last_ascii=70,font_size=16\"" +
        " --transformer partner_tiny_transform" +
        " --exporter raw_bin" +
        " --exporter-parameters \"output=" + tiny_bin.string() + ",font_mode=proportional,space_width=9,letter_spacing=1\"";

    const auto res = run_command_capture(cmd);
    EXPECT_NE(res.exit_code, 0);
    EXPECT_NE(res.output.find("space_width must be 0..7"), std::string::npos) << res.output;
}
