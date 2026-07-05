# 型を変換・読み替えする

SIMD では、**数値変換**と**ビット列の読み替え**を区別する必要があります。

## 数値変換

整数の値を、同じ数値を表す浮動小数点へ変換します。

```text
整数ビット列: 00000000 00000000 00000000 00000101
整数値:       5
変換後:       5.0f
```

この場合、変換前後のビット列は通常異なります。

## ビット列の読み替え

256 ビットの内容を変更せず、別の SIMD 型として扱います。

```text
変換前の 256 ビット == 読み替え後の 256 ビット
```

マスクを浮動小数点型と整数型の間で受け渡すときに重要です。

## 関連する intrinsic

| 操作 | intrinsic | 公式ドキュメント |
|---|---|---|
| 32 ビット整数を浮動小数点へ数値変換 | `_mm256_cvtepi32_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_cvtepi32_ps) |
| 整数 256 ビットを浮動小数点型として読み替え | `_mm256_castsi256_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_castsi256_ps) |
| 浮動小数点 256 ビットを整数型として読み替え | `_mm256_castps_si256` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_castps_si256) |

## このプロジェクトでの用途

- 乱数の上位 24 ビットを `float` へ数値変換する
- 整数で作った全ビット 1 のマスクを `Bool8` の `__m256` として保持する
- 浮動小数点比較で得たマスクを整数データの選択へ利用する

## よくある間違い

マスクのビット列を保ちたい場面で、数値変換を使わないでください。
true マスクのビット列は、通常の整数値 `1` や浮動小数点値 `1.0f` ではありません。
