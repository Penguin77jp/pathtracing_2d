# データを読み書きする

SIMD レジスタとメモリの間で値を移動します。
このプロジェクトでは、乱数生成器の 8 個の初期状態をまとめて読み込む処理があります。

## アラインされたロード

`_mm256_load_si256` は、256 ビットの整数データをメモリから読み込みます。
読み込み元は通常、32 バイト境界に整列している必要があります。

```cpp
alignas(32) std::uint32_t states[8];
```

## 関連する intrinsic

| 操作 | intrinsic | 公式ドキュメント |
|---|---|---|
| アラインされた整数 256 ビットを読み込む | `_mm256_load_si256` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_load_si256) |
| 下位 128 ビットを参照する | `_mm256_castps256_ps128` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_castps256_ps128) |
| 上位 128 ビットを取り出す | `_mm256_extractf128_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_extractf128_ps) |
| 最下位の 1 値をスカラーとして得る | `_mm_cvtss_f32` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtss_f32) |

## このプロジェクトでの用途

- `RngPacket8::seeded()` で 8 レーンの乱数状態を読み込む
- `Float8::mean()` で 256 ビットを 128 ビットずつに分ける
- 集約後の最下位レーンを `float` として取り出す

## 注意点

- `load` と `loadu` はアラインメント条件が異なります
- cast 系は、必ずしも CPU 命令を発行するとは限りません
- SIMD 値をポインタの型変換だけで直接参照するより、対応する load/store intrinsic を使う方が意図を明確にできます
