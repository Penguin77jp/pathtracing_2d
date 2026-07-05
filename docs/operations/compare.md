# 比較する

SIMD の比較は、各レーンを個別に比較し、結果としてマスクを作ります。

## レーンの動作

```text
A:       1   5   3   9
B:       2   4   3  10
A < B:   T   F   F   T
A == B:  F   F   T   F
```

結果は通常の数値ではなく、[マスク](../basics/lane-and-mask.md#マスク) です。

## 関連する intrinsic

| 操作 | intrinsic | 公式ドキュメント |
|---|---|---|
| 浮動小数点を比較する | `_mm256_cmp_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_cmp_ps) |
| 32 ビット整数の等値比較 | `_mm256_cmpeq_epi32` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_cmpeq_epi32) |

## 比較条件

`_mm256_cmp_ps` は、第 3 引数で比較方法を指定します。
このプロジェクトでは次の条件が使われています。

| 条件 | 意味 |
|---|---|
| `_CMP_LT_OQ` | より小さい |
| `_CMP_LE_OQ` | 以下 |
| `_CMP_GT_OQ` | より大きい |
| `_CMP_GE_OQ` | 以上 |
| `_CMP_EQ_OQ` | 等しい |
| `_CMP_NEQ_OQ` | 等しくない |

`OQ` などの接尾辞は NaN の扱いにも関係します。厳密な意味は公式ドキュメントで確認してください。

## このプロジェクトでの用途

- 球との交差距離が範囲内か判定する
- より近い解を選ぶ
- 法線がレイの内向きか判定する
- 材質種別が一致するレーンを抽出する
- サンプルが単位球や単位円盤の内側か判定する

## 次に読む

比較で作ったマスクは、通常 [値を選択する](./select.md) または [論理演算する](./logical.md) ために使います。
