# Ray Tracing SIMD Guide

このガイドは、intrinsic の関数名ではなく、**やりたいこと**から SIMD 操作を探すための入口です。

初めて読む場合は、先に [レーンとマスク](./basics/lane-and-mask.md) を確認してください。

## やりたいことから探す

| やりたいこと | 簡単な説明 |
|---|---|
| [初期値を作る](./operations/initialize.md) | 全レーンを 0 にする、または同じ値を全レーンへ設定する |
| [四則演算する](./operations/arithmetic.md) | 対応するレーン同士で加算・減算・乗算・除算する |
| [比較する](./operations/compare.md) | 各レーンを比較し、真偽を表すマスクを作る |
| [値を選択する](./operations/select.md) | マスクに応じて 2 つの値からレーンごとに選ぶ |
| [論理演算する](./operations/logical.md) | マスクやビット列に AND・OR・XOR・AND NOT を適用する |
| [数学関数を使う](./operations/math-functions.md) | 最小値・最大値・平方根・積和演算を行う |
| [データを読み書きする](./operations/memory.md) | メモリから SIMD 値を読み込む、または一部を取り出す |
| [型を変換・読み替えする](./operations/convert-and-cast.md) | 数値変換と、ビット列を変えない型の読み替えを区別する |
| [ビットシフトする](./operations/shift.md) | 整数レーンのビット列を左右に移動する |
| [全レーンを集約する](./operations/reduce.md) | 合計・平均・any・none のように複数レーンを 1 つへまとめる |

## レイトレーサの処理から探す

| 処理 | 関連する SIMD の考え方 |
|---|---|
| [最近傍ヒットを保持する](./raytracing/nearest-hit.md) | 比較、マスク、レーンごとの選択 |
| [材質をレーンごとに保持する](./raytracing/material-select.md) | 複数フィールドへ同じマスクを適用する |
| [有効なレイを管理する](./raytracing/active-lanes.md) | active mask、any、none、条件付き選択 |
| [棄却サンプリングをパケット化する](./raytracing/rejection-sampling.md) | 完了レーンを保持し、未完了レーンだけ再試行する |

## 関数名から探す

現在のプロジェクトで使用している intrinsic は、[intrinsic 索引](./reference/intrinsics.md) にまとめています。

## 公式資料

公式資料の役割と見方は、[公式リファレンスの使い分け](./reference/official-resources.md) を参照してください。
