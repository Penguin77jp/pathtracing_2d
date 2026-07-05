# Ray Tracing in One Weekend SIMD 演習版

## 実装済み

- AVX2 + FMA のビルド設定
- 数学ユーティリティ（`min`、`abs`、`clamp`、色の成分積、反射・屈折ヘルパー）
- 材質のスカラー／8レーンデータ構造
- Metal の散乱処理
- `t_min = 0.001` による self-intersection 対策
- ガンマ補正
- 垂直FOV、`vup`、焦点距離、defocus blur 対応カメラ
- 材質確認シーンと最終ランダムシーン
- 法線プレビュー、クイック設定、PPM出力（stbがあればPNGも可）

## SIMD_EXERCISE

コード内で `SIMD_EXERCISE` を検索してください。

1. `Bool8::any()` / `none()`
2. 最近傍ヒットの材質をレーン単位で選択
3. 単位球・単位ベクトル・単位円盤の棄却サンプリング
4. Lambertian 散乱
5. Dielectric の反射／屈折選択
6. active mask を持つ反復型パストレーサ

## ビルド

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 実行

実装前でも法線プレビューは動作します。

```bash
./build/ray_tracing_in_one_weekend_simd --quick --normal
```

`--path` は演習未実装の間はマゼンタを返す確認用スタブです。

演習実装後の材質確認:

```bash
./build/ray_tracing_in_one_weekend_simd --quick --materials --path
```

最終シーン:

```bash
./build/ray_tracing_in_one_weekend_simd --final --path
```

出力先指定:

```bash
./build/ray_tracing_in_one_weekend_simd --quick --output=preview.ppm
```
