# venue decisions

1. `Parser` and `SymbolTable` are concepts; `ComposedParser<Policy, Table>` binds a field-extraction policy to a symbol universe.
2. `SymbolTable` is compile-time (`FixedSymbolTable` over a fixed symbol list), so name↔id interning is allocation-free.
3. `binance_historical` composes `BinHistParserPolicy` with `BinHistSymbolTable` into `BinHistVenue`.
