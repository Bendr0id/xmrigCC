# Algorithms

Algorithm can be defined in 3 ways:

1. By pool, using algorithm negotiation, in this case no need specify algorithm on miner side.
2. Per pool `coin` option, currently only usable value for this option is `monero`, `aqrma`, `dero`, `keva`, `turtle`
3. Per pool `algo` option.

Option `coin` useful for pools without algorithm negotiation support or daemon to allow automatically switch algorithm in next hard fork.

## Algorithm names

| Name | Memory | Version | Notes |
|------|--------|---------|-------|
| `rx/graft` | 2 MB | 2.9.6+ | RandomGRAFT. |
| `rx/yada` | 2 MB | 2.9.3+ | RandomYADA. |
| `cn/superfast` | 2 MB | 2.9.1+ | CryptoNight variant 0 (modified) |
| `cn/cache_hash` | 2 MB | 2.8.4+ | CryptoNight variant 0 (modified) | 
| `argon2/chukwav2` | 1 MB | 2.8.2+ | Argon2id (Chukwa v2). |
| `panthera` | 256 KB | 2.8.0+ | RandomX variant Panthera (Scala [XLA]). |
| `ninja` | 256 KB | 2.7.0+ | Chukwa variant NinjaCoin. |
| `astroBWT` | 20 MB | 2.6.0+ | AstroBWT. |
| `rx/keva` | 1 MB | 2.6.0+ | RandomKEVA. |
| `cn-pico/tlo` | 256 KB | 2.5.0+ | CryptoNight-Pico (Talleo). |
| `rx/sfx` | 2 MB | 2.2.1+ | RandomX (Safex). |
| `rx/arq` | 256 KB | 2.2.0+ | RandomX (Arqma). |
| `rx/0` | 2 MB | 2.1.0+ | RandomX (Monero). |
| `rx/wow` | 1 MB | 2.0.0+ | RandomWOW. |
| `rx/loki` | 2 MB | <2.8.2 | RandomXL. |
| `cn/conceal` | 2 MB | 1.9.5+ | CryptoNight variant 0 (modified). |
| `argon2/chukwa` | 512 KB | 1.9.5+ | Argon2id (Chukwa). |
| `cn-extremelite/upx2` | 128 KB | <1.9.5 | CryptoNight-Extremelite variant UPX2 with reversed shuffle. |
| `cn/fast` | 2 MB | <1.9.5+ | CryptoNight variant 1 with half iterations. |
| `cn/rwz` | 2 MB | <1.9.5+ | CryptoNight variant 2 with 3/4 iterations and reversed shuffle operation. |
| `cn/zls` | 2 MB | <1.9.5+ | CryptoNight variant 2 with 3/4 iterations. |
| `cn/double` | 2 MB | <1.9.5+ | CryptoNight variant 2 with double iterations. |
| `cn/r` | 2 MB | <1.9.5+ | CryptoNightR (Monero's variant 4). |
| `cn-pico` | 256 KB | <1.9.5+ | CryptoNight-Pico. (Turtle) |
| `cn/half` | 2 MB | <1.9.5+ | CryptoNight variant 2 with half iterations. |
| `cn/2` | 2 MB | <1.9.5+ | CryptoNight variant 2. |
| `cn/xao` | 2 MB | <1.9.5+ | CryptoNight variant 0 (modified). |
| `cn/rto` | 2 MB | <1.9.5+ | CryptoNight variant 1 (modified). |
| `cn-heavy/tube` | 4 MB | <1.9.5+ | CryptoNight-Heavy (modified). |
| `cn-heavy/xhv` | 4 MB | <1.9.5+ | CryptoNight-Heavy (modified). |
| `cn-heavy/0` | 4 MB | <1.9.5+ | CryptoNight-Heavy. |
| `cn/1` | 2 MB | <1.9.5+ | CryptoNight variant 1. |
| `cn-lite/1` | 1 MB | <1.9.5+ | CryptoNight-Lite variant 1. |
| `cn-lite/0` | 1 MB | <1.9.5+ | CryptoNight-Lite variant 0. |
| `cn/0` | 2 MB | <1.9.5+ | CryptoNight (original). |

## Migration to v3
Since version 3 mining [algorithm](#algorithm-names) should specified for each pool separately (`algo` option), earlier versions was use one global `algo` option and per pool `variant` option (this option was removed in v3). If your pool support [mining algorithm negotiation](https://github.com/xmrig/xmrig-proxy/issues/168) you may not specify this option at all.
 
#### Example
```json
{
  "pools": [
    {
      "url": "...",
      "algo": "cn/r",
      "coin": null
      ...
    }
 ],
 ...
}
```