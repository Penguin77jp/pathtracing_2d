# intrinsic 索引

現在のプロジェクトで使用している intrinsic を、目的別にまとめています。
関数名をすでに知っている場合の索引です。

すべての intrinsic は `<immintrin.h>` 経由で利用しています。

## 初期化

| intrinsic | 簡単な説明 | ISA | 公式 |
|---|---|---|---|
| `_mm256_setzero_ps` | 8 レーンを 0.0 で初期化 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_setzero_ps) |
| `_mm256_set1_ps` | 同じ浮動小数点を 8 レーンへ複製 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_set1_ps) |
| `_mm256_set1_epi32` | 同じ 32 ビット整数を 8 レーンへ複製 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_set1_epi32) |

## 四則演算

| intrinsic | 簡単な説明 | ISA | 公式 |
|---|---|---|---|
| `_mm256_add_ps` | 対応する浮動小数点レーンを加算 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_add_ps) |
| `_mm256_sub_ps` | 対応する浮動小数点レーンを減算 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_sub_ps) |
| `_mm256_mul_ps` | 対応する浮動小数点レーンを乗算 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_mul_ps) |
| `_mm256_div_ps` | 対応する浮動小数点レーンを除算 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_div_ps) |

## 数学関数

| intrinsic | 簡単な説明 | ISA | 公式 |
|---|---|---|---|
| `_mm256_min_ps` | 対応するレーンの小さい値を返す | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_min_ps) |
| `_mm256_max_ps` | 対応するレーンの大きい値を返す | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_max_ps) |
| `_mm256_sqrt_ps` | 各レーンの平方根を求める | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_sqrt_ps) |
| `_mm256_fmadd_ps` | 各レーンで `a*b+c` を計算 | FMA | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_fmadd_ps) |

## 比較と選択

| intrinsic | 簡単な説明 | ISA | 公式 |
|---|---|---|---|
| `_mm256_cmp_ps` | 浮動小数点レーンを指定条件で比較 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_cmp_ps) |
| `_mm256_cmpeq_epi32` | 32 ビット整数レーンを等値比較 | AVX2 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_cmpeq_epi32) |
| `_mm256_blendv_ps` | マスクに応じて浮動小数点レーンを選択 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_blendv_ps) |
| `_mm256_movemask_ps` | 各浮動小数点レーンの符号ビットを整数へ集約 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_movemask_ps) |

## 浮動小数点ビット論理演算

| intrinsic | 簡単な説明 | ISA | 公式 |
|---|---|---|---|
| `_mm256_and_ps` | 256 ビットの AND | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_and_ps) |
| `_mm256_or_ps` | 256 ビットの OR | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_or_ps) |
| `_mm256_xor_ps` | 256 ビットの XOR | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_xor_ps) |
| `_mm256_andnot_ps` | `(~a) & b` を計算 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_andnot_ps) |

## 整数演算と乱数生成

| intrinsic | 簡単な説明 | ISA | 公式 |
|---|---|---|---|
| `_mm256_xor_si256` | 256 ビット整数の XOR | AVX2 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_xor_si256) |
| `_mm256_slli_epi32` | 各 32 ビット整数を左シフト | AVX2 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_slli_epi32) |
| `_mm256_srli_epi32` | 各 32 ビット整数を論理右シフト | AVX2 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_srli_epi32) |
| `_mm256_load_si256` | 32 バイト境界から整数 256 ビットをロード | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_load_si256) |

## キャストと数値変換

| intrinsic | 簡単な説明 | ISA | 公式 |
|---|---|---|---|
| `_mm256_castsi256_ps` | ビット列を変えず整数型から浮動小数点型へ読み替え | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_castsi256_ps) |
| `_mm256_cvtepi32_ps` | 32 ビット整数を浮動小数点へ数値変換 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_cvtepi32_ps) |

## 256 ビットから 128 ビットへの集約

| intrinsic | 簡単な説明 | ISA | 公式 |
|---|---|---|---|
| `_mm256_castps256_ps128` | 下位 128 ビットを `__m128` として参照 | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_castps256_ps128) |
| `_mm256_extractf128_ps` | 指定した 128 ビット半分を取り出す | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_extractf128_ps) |
| `_mm_add_ps` | 4 レーンの浮動小数点を対応レーンごとに加算 | SSE | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_add_ps) |
| `_mm_hadd_ps` | 128 ビット内で隣接レーンを水平加算 | SSE3 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_hadd_ps) |
| `_mm_cvtss_f32` | 最下位の浮動小数点をスカラーとして返す | SSE | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_cvtss_f32) |

## 課題で追加候補となる intrinsic

次は現時点のコードには未使用ですが、整数材質種別のマスク選択を調べる際の候補です。
ここでは課題の完成コードは示しません。

| intrinsic | 調べる観点 | ISA | 公式 |
|---|---|---|---|
| `_mm256_castps_si256` | 浮動小数点マスクのビット列を整数型として扱う | AVX | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_castps_si256) |
| `_mm256_blendv_epi8` | 実行時マスクによる整数データの選択 | AVX2 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_blendv_epi8) |
| `_mm256_and_si256` | 整数ビット列の AND | AVX2 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_and_si256) |
| `_mm256_andnot_si256` | 整数ビット列で `(~a) & b` | AVX2 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_andnot_si256) |
| `_mm256_or_si256` | 整数ビット列の OR | AVX2 | [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_or_si256) |
