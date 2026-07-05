# 論理演算する

マスクやビット列に対して、AND・OR・XOR・AND NOT をレーンごとに適用します。

## 基本操作

| 操作 | 意味 | 代表的な用途 |
|---|---|---|
| AND | 両方が真のレーンだけ真 | `active` かつ `hit` |
| OR | どちらかが真なら真 | 複数の終了条件をまとめる |
| XOR | 一方だけ真なら真 | マスクの反転など |
| AND NOT | 第 1 入力を反転して第 2 入力と AND | マスク除去、符号ビット除去 |

## マスクの例

```text
A:       T  T  F  F
B:       T  F  T  F
A AND B: T  F  F  F
A OR B:  T  T  T  F
A XOR B: F  T  T  F
```

## 関連する intrinsic

| 操作 | 浮動小数点ビット列 | 整数ビット列 | 公式ドキュメント |
|---|---|---|---|
| AND | `_mm256_and_ps` | `_mm256_and_si256` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_and_ps) |
| OR | `_mm256_or_ps` | `_mm256_or_si256` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_or_ps) |
| XOR | `_mm256_xor_ps` | `_mm256_xor_si256` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_xor_ps) |
| AND NOT | `_mm256_andnot_ps` | `_mm256_andnot_si256` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_andnot_ps) |

## AND NOT の注意

`andnot(a, b)` は一般に、次の順序です。

```text
(~a) & b
```

`a & (~b)` ではありません。引数順序を公式仕様で確認してください。

## このプロジェクトでの用途

- `Bool8` の `&`、`|`、`^`、`~`
- 浮動小数点の符号ビットを除去して絶対値を作る
- active mask と材質マスクを組み合わせる
- 完了したレーンを処理対象から外す
