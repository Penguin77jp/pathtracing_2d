# Ray Tracing in One Weekend SIMD 演習版

## 実装済み

- AVX2 + FMA のビルド設定
- 数学ユーティリティ（`min`、`abs`、`clamp`、色の成分積、反射・屈折ヘルパー）
- 材質のスカラー／8レーンデータ構造
- 最近傍ヒットの材質をレーン単位で選択
- 材質非依存の反復型パストレーサ制御フロー
- `scatter_packet()` による材質ディスパッチの共通インターフェース
- Lambertian の散乱方向、near-zeroフォールバック、albedo減衰
- Metal の鏡面反射、fuzz、表面内側へ向かうレイの吸収
- Dielectric の反射・屈折、全反射、Schlick近似
- 単位球内サンプリングと単位ベクトル生成
- `t_min = 1.0e-4f` によるself-intersection対策
- ガンマ補正
- 垂直FOV、`vup`、焦点距離、defocus blur対応カメラ
- 材質確認シーンと最終ランダムシーン
- 法線プレビュー、クイック設定、PPM出力（stbがあればPNGも可）

## 材質インターフェース

パストレーサ本体は材質の種類を知りません。

```text
ray_color_path_packet()
    -> scatter_packet()
        -> scatter_lambertian_packet()
        -> scatter_metal_packet()
        -> scatter_dielectric_packet()
```

各材質は共通の `ScatterRecord8` を返します。

```text
次のレイ
attenuation
scatteredマスク
```

材質固有の計算を変更しても、`ray_color_path_packet()` の計算フローは変更しません。

## 残っている SIMD_EXERCISE

コード内で `SIMD_EXERCISE` を検索してください。

1. `Bool8::any()` / `none()`
2. 単位円盤内の棄却サンプリング

単位円盤サンプリングはdefocus blurで使います。現在は固定の原点を返すため、材質レンダリングは動作しますが、被写界深度のランダムなレンズサンプリングはまだ有効になっていません。

## ビルド

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 実行

法線プレビュー:

```bash
./build/ray_tracing_in_one_weekend_simd --quick --normal --output=normal.ppm
```

材質確認:

```bash
./build/ray_tracing_in_one_weekend_simd --quick --materials --path --output=materials.ppm
```

最終シーン:

```bash
./build/ray_tracing_in_one_weekend_simd --quick --final --path --output=final.ppm
```
