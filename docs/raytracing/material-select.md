# 材質をレーンごとに保持する

`MaterialPacket8` は、8 レーン分の材質情報を SoA 形式で保持します。

```text
kind[8]
albedo.x[8]
albedo.y[8]
albedo.z[8]
fuzz[8]
refraction_index[8]
```

## レーン内の整合性

1 つのレーンについて、すべてのフィールドが同じ物体から選ばれる必要があります。

```text
lane 3:
    kind               = Metal
    albedo             = 同じ Metal の色
    fuzz               = 同じ Metal の fuzz
    refraction_index   = 同じ候補側の値
```

一部だけを選択すると、材質種別とパラメータが食い違います。

## 浮動小数点と整数の違い

`albedo`、`fuzz`、`refraction_index` は浮動小数点 SIMD 型ですが、`kind` は `__m256i` です。

そのため、同じ「マスク選択」でも、データ型に対応する intrinsic またはビット演算が必要です。
マスクの意味は共通です。

```text
mask が真  -> candidate の同じレーン
mask が偽  -> current の同じレーン
```

## 課題で確認すること

- `Bool8` の true/false のビット表現
- 浮動小数点マスクと整数データの型の違い
- 各フィールドに同じマスクを使っているか
- 全真・全偽・交互マスクで期待どおりか

## 関連する操作

- [値を選択する](../operations/select.md)
- [型を変換・読み替えする](../operations/convert-and-cast.md)
- [最近傍ヒットを保持する](./nearest-hit.md)
