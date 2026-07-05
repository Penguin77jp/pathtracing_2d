# 公式リファレンスの使い分け

## Intel Intrinsics Guide

- URL: <https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html>
- intrinsic 名による検索
- シグネチャ
- 対応する命令セット
- 擬似コード
- 対応命令
- レイテンシ・スループット情報

このガイドの各リンクは、可能な範囲で `#text=intrinsic名` を付け、検索対象を指定しています。
ブラウザや Intel サイトの更新によってフィルターが反映されない場合は、ページ内の検索欄へ intrinsic 名を入力してください。

## Clang ヘッダーリファレンス

- AVX: <https://clang.llvm.org/doxygen/avxintrin_8h.html>
- AVX2: <https://clang.llvm.org/doxygen/avx2intrin_8h.html>
- FMA: <https://clang.llvm.org/doxygen/fmaintrin_8h.html>

Clang が提供する intrinsic の宣言と説明を、実際のヘッダーファイルに近い形で確認できます。
ページ内検索で intrinsic 名を探してください。

## Microsoft x86 intrinsics list

- URL: <https://learn.microsoft.com/en-us/cpp/intrinsics/x86-intrinsics-list>

MSVC が対応する intrinsic、必要な命令セット、シグネチャを一覧で確認できます。

## Intel Software Developer's Manual

- URL: <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html>

intrinsic ではなく、CPU 命令そのものの厳密な仕様を確認する資料です。
命令の例外、ビット単位の処理、動作モードなど、より低いレベルを調べるときに使います。

## 推奨する確認順

1. このプロジェクトの「やりたいこと」ページで候補を知る
2. Intel Intrinsics Guide で intrinsic の仕様を読む
3. Clang または使用コンパイラのヘッダーで宣言を確認する
4. 必要なら Intel SDM で対応命令を確認する
5. 小さい入力で全真・全偽・境界値をテストする
