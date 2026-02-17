/// \file
/// \brief CLI parser interface for snatch pipeline options.
///
/// This header declares types and contracts used by snatch core and plugin stages. It is part of the extractor-transformer-exporter architecture and is consumed by build-time and runtime plugin integration.
///
/// Copyright (c) 2026 Tomaz Stih
/// SPDX-License-Identifier: GPL-2.0-only

#pragma once
#include "options.h"

class cli_parser {
public:
    // returns 0 on success, non-zero on error. On success, fills out.
    // Note: -h/--help is handled by argparse and will print usage & exit(0).
    int parse(int argc, const char** argv, snatch_options& out) const;
};
