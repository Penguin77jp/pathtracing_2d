# 数学関数を使う

四則演算以外に、このプロジェクトでは最小値・最大値・平方根・積和演算を使います。

## 最小値と最大値

対応するレーン同士を比較し、小さい方または大きい方を返します。

```text
A:   1  8  3  9
B:   2  4  3 10
min: 1  4  3  9
max: 2  8  3 10
```

## 平方根

各レーンの平方根を求めます。
球との交差方程式やベクトルの長さで使います。

## 積和演算

各レーンで次をまとめて計算します。

```text
a * b + c
```

FMA は丸めが 1 回になるため、`a * b` と `+ c` を別々に行う場合と末尾ビットが異なることがあります。

## 関連する intrinsic

| 操作 | intrinsic | 必要な ISA | 公式ドキュメント |
|---|---|---|---|
| 最小値 | `_mm256_min_ps` | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_min_ps) |
| 最大値 | `_mm256_max_ps` | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_max_ps) |
| 平方根 | `_mm256_sqrt_ps` | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_sqrt_ps) |
| 積和 | `_mm256_fmadd_ps` | FMA | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_fmadd_ps) |

## このプロジェクトでの用途

- `Float8::min()`、`max()`、`clamp()`
- ベクトルの正規化
- 球との交差方程式
- `Vec3f8::dot()` の内積

## ビルド設定

このプロジェクトは `_mm256_fmadd_ps` を使うため、GCC/Clang では AVX2 に加えて FMA を有効化しています。

```text
-mavx2 -mfma
```
