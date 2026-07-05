# 値を選択する

マスクが真のレーンでは一方の値を、偽のレーンではもう一方の値を選びます。

## レーンの動作

```text
mask:    T   F   T   F
A:      10  20  30  40
B:       1   2   3   4
result: 10   2  30   4
```

概念的には、各レーンで次を行います。

```cpp
result[lane] = mask[lane] ? a[lane] : b[lane];
```

## 関連する intrinsic

| 用途 | intrinsic | 公式ドキュメント |
|---|---|---|
| 32 ビット浮動小数点レーンを選ぶ | `_mm256_blendv_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_blendv_ps) |
| 8 ビット単位で整数データを選ぶ | `_mm256_blendv_epi8` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_blendv_epi8) |
| ビット演算で選択を構成する | `_mm256_and_si256`、`_mm256_andnot_si256`、`_mm256_or_si256` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_and_si256) |

## 選択方向を確認する

blend 系の intrinsic は、引数のどちらがマスク真側になるかを名前だけでは判断しにくい場合があります。
必ず公式仕様を確認し、次の 3 ケースでテストしてください。

- 全レーン偽
- 全レーン真
- 真偽が交互

## 構造体を選択する場合

構造体全体を 1 命令で選択できるとは限りません。
その場合は、構造体を構成する各 SIMD フィールドへ**同じマスク**を適用します。

```text
geometry.position
geometry.normal
geometry.t
material.kind
material.albedo
...
```

同じレーンに属するフィールドが、すべて同じ候補から選ばれる必要があります。

## このプロジェクトでの用途

- 近い交点と以前の交点を選ぶ
- 交点に対応する材質を選ぶ
- 表向き・裏向きの法線を選ぶ
- 背景色とヒット色を選ぶ
- 材質ごとの散乱結果を統合する

詳しくは [最近傍ヒットを保持する](../raytracing/nearest-hit.md) を参照してください。
