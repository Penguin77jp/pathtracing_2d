# 全レーンを集約する

レーンごとの演算とは反対に、複数レーンを 1 つの値へまとめる処理をリダクションと呼びます。

## 主な目的

- 8 レーンの合計や平均を求める
- 1 レーンでも真か調べる
- 全レーンが偽か調べる

## 合計と平均

```text
input:  1 2 3 4 5 6 7 8
sum:    36
mean:   4.5
```

このプロジェクトの `Float8::mean()` は、256 ビットを上下の 128 ビットへ分け、加算と水平加算でまとめます。

## any と none

```text
mask: T F F F F F F F
any:  true
none: false
```

```text
mask: F F F F F F F F
any:  false
none: true
```

比較マスクの各レーンから符号ビットを取り出し、スカラー整数として調べる方法があります。

## 関連する intrinsic

| 操作 | intrinsic | 公式ドキュメント |
|---|---|---|
| 8 レーンの符号ビットを整数へ集約 | `_mm256_movemask_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_movemask_ps) |
| 128 ビット同士を加算 | `_mm_add_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_add_ps) |
| 隣接する浮動小数点を水平加算 | `_mm_hadd_ps` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hadd_ps) |
| 最下位レーンを `float` として得る | `_mm_cvtss_f32` | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtss_f32) |

## このプロジェクトでの用途

- 8 サンプルの色を平均する
- 棄却サンプリングで未完了レーンが残っているか調べる
- パストレーサで active なレイが残っているか調べる

## 注意点

水平加算は、256 ビット全体を 1 回で完全に合計するとは限りません。
128 ビット境界や引数ごとの並びを公式仕様で確認してください。
