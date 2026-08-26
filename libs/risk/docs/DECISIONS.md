# risk decisions

## D47 — `BasicRiskGate`: absolute-Notional drawdown, not a percentage; permanent trip; a flat-array "known instruments" registry
First concrete `RiskGate` (see root `docs/decisions.md` D46 for
`Portfolio::equity()`, the prerequisite this depends on). Three choices
worth recording:

**Absolute drawdown, not `%` of peak equity.** `Portfolio` has no allocated
-starting-capital concept — `cash()`/`equity()` both start at 0 and track
net trading cash flow only, not a funded account balance. A percentage
threshold (`(peak - current) / peak`) divides by a denominator that starts
at or near zero and can go negative from fees alone, well before any real
loss — an early, healthy trade would look like an infinite-percent
drawdown. `max_drawdown` is a plain `Notional` decline from peak instead;
correct given what's actually modeled, revisit if/when `Portfolio` ever
gains a real starting-capital concept.

**Trips once, stays tripped.** `on_tick()` stops re-flattening and
`check()` rejects everything once `tripped_` — no auto-resume. A kill
switch that quietly un-trips on its own isn't one; resuming is an operator
decision, not this class's.

**`known_`: a flat `array<bool, kMaxSymbols * kMaxVenues>`, not a
`set<pair<SymbolId, VenueId>>`.** `on_tick()`'s flatten sweep needs to know
which (symbol, venue) pairs the gate has ever seen (`StateView` has no
enumeration API — point queries only). Since `SymbolId`/`VenueId` are
already dense ids bounded by `Portfolio::kMaxSymbols`/`kMaxVenues` (same
reasoning as D31/D44), a flat bool array indexed the same way `Portfolio`
indexes `positions_` is direct, allocation-free, and avoids introducing a
new container discipline into a codebase that's deliberately avoided
hashing/sets on this exact kind of dense id elsewhere.
