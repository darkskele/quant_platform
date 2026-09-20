import pyarrow as pa

CATALOG_SCHEMA = pa.schema([
    pa.field("venue", pa.string(), nullable=False),
    pa.field("market", pa.string(), nullable=False),
    pa.field("dataset", pa.string(), nullable=False),
    pa.field("symbol", pa.string(), nullable=False),
    pa.field("year", pa.int16(), nullable=False),
    pa.field("month", pa.int8(), nullable=True),
    pa.field("day", pa.int8(), nullable=True),
    pa.field("first_ts", pa.timestamp("ns", tz="UTC"), nullable=False),
    pa.field("last_ts", pa.timestamp("ns", tz="UTC"), nullable=False),
    pa.field("rows", pa.int64(), nullable=False),
    pa.field("bytes", pa.int64(), nullable=False),
    pa.field("path", pa.string(), nullable=False),
    pa.field("sha256", pa.string(), nullable=False),
    pa.field("schema_version", pa.int16(), nullable=False),
    pa.field("parser", pa.string(), nullable=False),
    pa.field("ingested_at", pa.timestamp("ns", tz="UTC"), nullable=False),
])
