# ビットシフトする

整数 SIMD 値の各レーンを、指定したビット数だけ左右へシフトします。

## 左シフト

```text
00000101 << 1 = 00001010
```

## 論理右シフト

右側へ移動し、左側へ 0 を入れます。

```text
10000000 >> 1 = 01000000
```

符号を維持する算術右シフトとは異なります。

## 関連する intrinsic

| 操作 | intrinsic | 公式ドキュメント |
|---|---|---|
| 32 ビット整数レーンを左シフト | `_mm256_slli_epi32` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_slli_epi32) |
| 32 ビット整数レーンを論理右シフト | `_mm256_srli_epi32` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_srli_epi32) |

## このプロジェクトでの用途

`RngPacket8` の xorshift32 乱数生成で使います。

```text
XOR
左シフト
右シフト
```

を組み合わせ、8 レーンの乱数状態を同時に更新します。

## 注意点

- シフト幅は intrinsic によって即値である必要があります
- 左シフトで失われた上位ビットは戻りません
- 論理右シフトと算術右シフトを区別してください
