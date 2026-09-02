# venue

The concepts a source needs to turn raw venue bytes into `MarketEvent`s: `SymbolTable` (name↔id) and `Parser` (`parse(bytes) -> optional<MarketEvent>`). `ComposedParser` binds a parse policy to a symbol table.

## Diagram

```
raw bytes
   │
   ▼
Parser::parse()      (venue.hpp: the concept)
   │   ComposedParser<Policy, Table> binds a policy to a symbol table
   ▼
optional<MarketEvent>
```

## Variations

- **`binance_historical`** (`binance/binance_historical/`): Binance USD-M historical parser: `BinHistParserPolicy` and `BinHistSymbolTable` composed into `BinHistVenue`.

## Milestones

- [x] ~~`SymbolTable` and `Parser` concepts~~
- [x] ~~`ComposedParser`~~
- [x] ~~`binance_historical` venue~~
