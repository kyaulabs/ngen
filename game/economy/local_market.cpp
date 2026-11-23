#include "economy/local_market.hpp"
#include <algorithm>
#include <limits>
#include <span>

namespace ngen::economy {
namespace {
Outcome settle(MarketSnapshot state, Trade trade) {
    const auto units = trade.quantity.count;
    const auto price = trade.unit_price.subunits;
    if (!units || !price || (trade.buyer != Account::player && trade.buyer != Account::station))
        return Rejection::invalid;
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    if (units > max / price)
        return Rejection::overflow;
    const auto total = units * price;
    auto &buyer = trade.buyer == Account::player ? state.player : state.station;
    auto &seller = trade.buyer == Account::player ? state.station : state.player;
    if (buyer.credits.subunits < total)
        return Rejection::insufficient_credits;
    if (seller.stock.count < units)
        return Rejection::insufficient_stock;
    if (seller.credits.subunits > max - total || buyer.stock.count > max - units)
        return Rejection::overflow;

    buyer.credits.subunits -= total;
    seller.credits.subunits += total;
    buyer.stock.count += units;
    seller.stock.count -= units;
    return Receipt{trade.id, GameCredit{total}, state};
}
} // namespace

Outcome LocalMarket::apply(Trade trade) {
    if (!trade.id.value)
        return Rejection::invalid;
    const auto entries = std::span{journal_}.first(used_);
    const auto existing =
        std::ranges::find(entries, trade.id, [](const Entry &entry) { return entry.request.id; });
    if (existing != entries.end()) {
        if (existing->request != trade)
            return Rejection::conflict;
        return existing->outcome;
    }
    if (used_ == journal_.size())
        return Rejection::journal_full;

    const auto result = settle(state_, trade);
    journal_[used_++] = {trade, result};
    if (const auto *receipt = std::get_if<Receipt>(&result))
        state_ = receipt->after;
    return result;
}
} // namespace ngen::economy
