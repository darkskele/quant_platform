import pyarrow as pa

NAME = "mark_kline"

SCHEMA = pa.schema([
    pa.field("ts", pa.timestamp("ns", tz="UTC"), nullable=False),
    pa.field("close_time", pa.timestamp("ns", tz="UTC"), nullable=False),
    pa.field("open", pa.float64(), nullable=False),
    pa.field("high", pa.float64(), nullable=False),
    pa.field("low", pa.float64(), nullable=False),
    pa.field("close", pa.float64(), nullable=False),
])
