# 初期値を作る

SIMD 値を使い始めるときは、全レーンを 0 にするか、同じ値を全レーンへ複製します。

## 全レーンを 0 にする

```text
result: 0  0  0  0  0  0  0  0
```

このプロジェクトでは、`Bool8` や `Float8` の初期値に使われます。

## 同じ値を全レーンへ設定する

```text
input:  3.0
result: 3  3  3  3  3  3  3  3
```

スカラーの定数を、8 本のレイすべてへ適用したいときに使います。

## 関連する intrinsic

| 操作 | intrinsic | 公式ドキュメント |
|---|---|---|
| 浮動小数点を 0 で初期化 | `_mm256_setzero_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_setzero_ps) |
| 同じ浮動小数点を複製 | `_mm256_set1_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_set1_ps) |
| 同じ 32 ビット整数を複製 | `_mm256_set1_epi32` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_set1_epi32) |

## このプロジェクトでの用途

- `Float8` のデフォルト値
- `Bool8::constant()` の真偽マスク
- レイの最小・最大距離
- 色、屈折率、反射係数などの定数

## 注意点

`set1` は、8 個の異なる値を設定する操作ではありません。
1 個の値を全レーンへ複製します。
