from . import funding, kline, mark_kline, premium_kline

BY_KIND = {
    kline.NAME: kline.SCHEMA,
    mark_kline.NAME: mark_kline.SCHEMA,
    premium_kline.NAME: premium_kline.SCHEMA,
    funding.NAME: funding.SCHEMA,
}
