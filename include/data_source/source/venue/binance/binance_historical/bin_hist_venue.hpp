#pragma once
#include "bin_hist_parser_policy.hpp"
#include "bin_hist_symbol_table.hpp"
#include "venue.hpp"

namespace qp::data_source::source::venue::binance::binance_historical {

/// The one type a Source needs to talk to binance_historical: a
/// venue::Parser-satisfying type binding this venue's field-extraction
/// logic (BinHistParserPolicy) to its symbol universe (BinHistSymbolTable).
/// This is the only thing outside this directory should ever name — the
/// two pieces it binds are implementation, not the public seam.
using BinHistVenue = ComposedParser<BinHistParserPolicy, BinHistSymbolTable>;

static_assert(qp::data_source::source::venue::Parser<BinHistVenue>);

}  // namespace qp::data_source::source::venue::binance::binance_historical
