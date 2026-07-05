# 四則演算する

2 つの SIMD 値について、対応するレーン同士で加算・減算・乗算・除算を行います。

## レーンの動作

```text
A:       1   2   3   4   5   6   7   8
B:      10  20  30  40  50  60  70  80

A + B:  11  22  33  44  55  66  77  88
A - B:  -9 -18 -27 -36 -45 -54 -63 -72
A * B:  10  40  90 160 250 360 490 640
A / B: 0.1 0.1 0.1 0.1 0.1 0.1 0.1 0.1
```

## 関連する intrinsic

| 操作 | intrinsic | 公式ドキュメント |
|---|---|---|
| 加算 | `_mm256_add_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_add_ps) |
| 減算 | `_mm256_sub_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_sub_ps) |
| 乗算 | `_mm256_mul_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_mul_ps) |
| 除算 | `_mm256_div_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_div_ps) |

## 使用例

```cpp
const __m256 result = _mm256_add_ps(a, b);
```

これは概念的に次の処理です。

```cpp
for (int lane = 0; lane < 8; ++lane) {
    result[lane] = a[lane] + b[lane];
}
```

## このプロジェクトでの用途

- `Float8` の演算子
- `Vec3f8` のベクトル演算
- レイ上の位置 `origin + t * direction`
- 球との交差判定
- 色と throughput の計算
- サンプルの加算と平均

## 注意点

これは 8 レーン全体を合計する操作ではありません。

```text
レーンごとの加算: a0+b0, a1+b1, ...
全レーンの合計:  a0+a1+...+a7
```

全レーンを 1 つへまとめる処理は [全レーンを集約する](./reduce.md) を参照してください。
