import pyarrow as pa

NAME = "funding"

SCHEMA = pa.schema([
    pa.field("ts", pa.timestamp("ns", tz="UTC"), nullable=False),
    pa.field("funding_rate", pa.float64(), nullable=False),
])
