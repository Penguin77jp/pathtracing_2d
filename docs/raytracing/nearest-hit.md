# 最近傍ヒットを保持する

シーン内の球を順番に調べながら、各レーンで最も近い交点を保持します。

## 3 つの同期した状態

最近傍ヒットには、少なくとも次の情報があります。

- `closest.geometry`: 交点・法線・距離など
- `closest.material`: その交点を持つ物体の材質
- `closest_so_far`: 次の交差判定で使う距離の上限

今回調べた球がより近かったレーンだけ、この 3 つを候補側へ切り替えます。

```text
mask が真:
    geometry  = candidate.geometry
    material  = candidate.material
    distance  = candidate.t

mask が偽:
    以前の値を維持
```

## なぜ同じマスクを使うのか

異なるマスクを使うと、同じレーン内で状態が食い違います。

```text
交点と法線: 球 A
材質:       球 B
距離上限:   球 C
```

この状態では、球 A の位置で球 B の材質を使うなど、正しい散乱を計算できません。

## `candidate.geometry.hit` の意味

球の交差判定へ現在の `closest_so_far` を上限として渡しているため、このマスクは通常、単なる「球に当たった」ではなく、次を表します。

```text
ray_tmin < candidate.t < closest_so_far
```

つまり、**現在の最近傍より手前に有効な交点が見つかったレーン**です。

## 関連する操作

- [比較する](../operations/compare.md)
- [値を選択する](../operations/select.md)
- [材質をレーンごとに保持する](./material-select.md)
