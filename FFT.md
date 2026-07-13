# FFT

下記はDFT定義

$$
f_k = \sum_{n=0}^{N-1} x_n\exp\left(-i \frac{2 \pi k n}{N}\right)
$$

$$
\underbrace{
\begin{pmatrix}
f_0 \\
f_1 \\
f_2 \\
f_3
\end{pmatrix}
}_{\text{スペクトル}}
=
\begin{pmatrix}
e^{-i\frac{2\pi}{4}\cdot 0} &
e^{-i\frac{2\pi}{4}\cdot 0} &
e^{-i\frac{2\pi}{4}\cdot 0} &
e^{-i\frac{2\pi}{4}\cdot 0}
\\
e^{-i\frac{2\pi}{4}\cdot 0} &
e^{-i\frac{2\pi}{4}\cdot 1} &
e^{-i\frac{2\pi}{4}\cdot 2} &
e^{-i\frac{2\pi}{4}\cdot 3}
\\
e^{-i\frac{2\pi}{4}\cdot 0} &
e^{-i\frac{2\pi}{4}\cdot 2} &
e^{-i\frac{2\pi}{4}\cdot 4} &
e^{-i\frac{2\pi}{4}\cdot 6}
\\
e^{-i\frac{2\pi}{4}\cdot 0} &
e^{-i\frac{2\pi}{4}\cdot 3} &
e^{-i\frac{2\pi}{4}\cdot 6} &
e^{-i\frac{2\pi}{4}\cdot 9}
\end{pmatrix}
\underbrace{
\begin{pmatrix}
x_0 \\
x_1 \\
x_2 \\
x_3
\end{pmatrix}
}_{\text{元データ}}
$$

省略表記 $W_N := e^{-i \frac{2 \pi }{N}}$

$$
f_k = \sum_{n=0}^{N-1} x_n W^{kn}_N
$$

$W_N$ の定義より、$W_N^{k+N}=W_N^k$ が周期性から成り立つ。
なぜなら...

$$
\begin{aligned}
W^{k+N}_N
&= e^{-i\frac{2\pi (k+N) }{N}} \\
&= e^{-i\frac{2\pi k }{N} - 2\pi i} \\
&= e^{-i\frac{2\pi k }{N}} e^{- 2\pi i} \\
&= e^{-i\frac{2\pi k }{N}} \\
&= W^{k}_N
\end{aligned}
$$

$$
\boxed{
W^{k+N}_N = W^{k}_N
}
$$

$$
\begin{pmatrix}
f_0 \\
f_1 \\
f_2 \\
f_3
\end{pmatrix}
=
\begin{pmatrix}
W^0_4 & W^0_4 & W^0_4 & W^0_4 \\
W^0_4 & W^1_4 & W^2_4 & W^3_4 \\
W^0_4 & W^2_4 & W^4_4 & W^6_4 \\
W^0_4 & W^3_4 & W^6_4 & W^9_4
\end{pmatrix}
\begin{pmatrix}
x_0 \\
x_1 \\
x_2 \\
x_3
\end{pmatrix}
$$

ここで、$\mathbf{M}_N$, $\mathbf{M}^{(K)}_N$ を定義

$$
\mathbf{M}_N
:=
\mathbf{M}^{(N)}_N
$$

$$
\mathbf{M}^{(K)}_N
:=
\begin{bmatrix}
W^{\frac{N}{K}kn}_N
\end{bmatrix}^{K-1}_{k,n=0}
=
\begin{pmatrix}
W_N^{0\cdot 0\cdot \frac{N}{K}} &
W_N^{0\cdot 1\cdot \frac{N}{K}} &
\cdots &
W_N^{0\cdot (K-1)\cdot \frac{N}{K}}
\\
W_N^{1\cdot 0\cdot \frac{N}{K}} &
W_N^{1\cdot 1\cdot \frac{N}{K}} &
\cdots &
W_N^{1\cdot (K-1)\cdot \frac{N}{K}}
\\
\vdots &
\vdots &
\ddots &
\vdots
\\
W_N^{(K-1)\cdot 0\cdot \frac{N}{K}} &
W_N^{(K-1)\cdot 1\cdot \frac{N}{K}} &
\cdots &
W_N^{(K-1)(K-1)\frac{N}{K}}
\end{pmatrix}
$$

さらに後で再帰計算で使われる性質も示しておきます

$$
\begin{aligned}
W^{\frac{N}{K}}_N
&= \exp\left(-i \frac{2\pi}{N} \frac{N}{K}\right) \\
&= \exp\left(-i \frac{2\pi}{K}\right) \\
&= W_K
\end{aligned}
$$

したがって、

$$
\begin{aligned}
\mathbf{M}^{(K)}_N
&=
\begin{bmatrix}
W^{\frac{N}{K}kn}_N
\end{bmatrix}^{K-1}_{k,n=0} \\
&=
\begin{bmatrix}
W^{kn}_K
\end{bmatrix}^{K-1}_{k,n=0} \\
&=
\mathbf{M}_K
\end{aligned}
$$

$$
\boxed{
\mathbf{M}_N^{(K)}
=
\mathbf{M}_K
}
$$

> 例えば、$\mathbf{M}_4^{(2)} = \mathbf{M}_2$
> 円上の4等分を2ステップ進んだ位置 と 2等分を1ステップ進んだ位置 は同等

よって、4つの入力データに対するフーリエ変換は下記の様に書ける

$$
\mathbf{f}
=
\mathbf{M}_4
\mathbf{x}
$$

オイラーによる奇数と偶数の計算結果の再利用のために、
元データの前半を偶数データ、後半を奇数データに移動させます

> TODO: ここ修正

$$
\begin{pmatrix}
x_0 \\
x_1 \\
x_2 \\
x_3
\end{pmatrix}
\rightarrow
\begin{pmatrix}
x_0 \\
x_2 \\
x_1 \\
x_3
\end{pmatrix}
$$

$$
\begin{pmatrix}
f_0 \\
f_1 \\
f_2 \\
f_3
\end{pmatrix}
=
\begin{pmatrix}
W^0_4 & W^0_4 & W^0_4 & W^0_4 \\
W^0_4 & W^2_4 & W^1_4 & W^3_4 \\
W^0_4 & W^4_4 & W^2_4 & W^6_4 \\
W^0_4 & W^6_4 & W^3_4 & W^9_4
\end{pmatrix}
\begin{pmatrix}
x_0 \\
x_2 \\
x_1 \\
x_3
\end{pmatrix}
$$

さらに再利用のために塊を定義していきます

## 1. W行列の左上ブロック

下記のW行列の左上ブロックを $\mathbf{M}^{(2)}_4$で定義します

$$
\text{(左上ブロック)}
=
\mathbf{M}^{(2)}_4
=
\begin{pmatrix}
W^0_4 & W^0_4 \\
W^0_4 & W^2_4
\end{pmatrix}
$$

## 2. 左下ブロック

$W_N^{k+N}=W_N^k$から下記が成り立つ

$$
\text{(左下ブロック)}
\quad
\begin{pmatrix}
W^0_4 & W^4_4 \\
W^0_4 & W^6_4
\end{pmatrix}
=
\begin{pmatrix}
W^0_4 & W^0_4 \\
W^0_4 & W^2_4
\end{pmatrix}
=
\mathbf{M}^{(2)}_4
$$

## 3. 右上ブロック

ここで、$\mathbf{D}_N$ を定義

$$
\mathbf{D}^{(K)}_N
:=
\begin{pmatrix}
W^0_N & 0 & \cdots & 0 \\
0 & W^1_N & \cdots & 0 \\
\vdots & \vdots & \ddots & \vdots \\
0 & 0 & \cdots & W^{K-1}_N
\end{pmatrix}
$$

$$
\begin{aligned}
\text{(右上ブロック)}
&=
\begin{pmatrix}
W^0_4 & W^0_4 \\
W^1_4 & W^3_4
\end{pmatrix} \\
&=
\begin{pmatrix}
W^0_4 & 0 \\
0 & W^1_4
\end{pmatrix}
\begin{pmatrix}
W^0_4 & W^0_4 \\
W^0_4 & W^2_4
\end{pmatrix} \\
&=
\mathbf{D}^{(2)}_4 \mathbf{M}^{(2)}_4
\end{aligned}
$$

## 4. 右下ブロック

$$
\begin{aligned}
\text{(右下ブロック)}
&=
\begin{pmatrix}
W^2_4 & W^6_4 \\
W^3_4 & W^9_4
\end{pmatrix} \\
&=
\begin{pmatrix}
W^2_4 & W^2_4 \\
W^3_4 & W^1_4
\end{pmatrix} \\
&=
W^2_4
\begin{pmatrix}
W^0_4 & W^0_4 \\
W^1_4 & W^3_4
\end{pmatrix} \\
&=
W^2_4 D^{(2)}_4 M^{(2)}_4
\end{aligned}
$$

$$
W^2_4 = e^{-i \frac{2 \cdot 2 \pi}{4}} = -1
$$

$$
\begin{aligned}
\text{(右下ブロック)}
&=
-D^{(2)}_4 M^{(2)}_4
\end{aligned}
$$

4つのブロックにより

$$
\begin{pmatrix}
f_0 \\
f_1 \\
f_2 \\
f_3
\end{pmatrix}
=
\begin{pmatrix}
M^{(2)}_4 & D^{(2)}_4 M^{(2)}_4 \\
M^{(2)}_4 & -D^{(2)}_4 M^{(2)}_4
\end{pmatrix}
\begin{pmatrix}
x_0 \\
x_2 \\
x_1 \\
x_3
\end{pmatrix}
$$

さらに、$\mathbf{x}$も $\mathbf{x}_e$偶数, $\mathbf{x}_o$奇数に分けちゃいます。

$$
\begin{aligned}
\begin{pmatrix}
f_0 \\
f_1 \\
f_2 \\
f_3
\end{pmatrix}
&=
\begin{pmatrix}
M^{(2)}_4 & D^{(2)}_4 M^{(2)}_4 \\
M^{(2)}_4 & -D^{(2)}_4 M^{(2)}_4
\end{pmatrix}
\begin{pmatrix}
\mathbf{x}_e \\
\mathbf{x}_o
\end{pmatrix} \\
&=
\begin{pmatrix}
\color{blue}{M^{(2)}_4 \mathbf{x}_e} + D^{(2)}_4 \color{green}{M^{(2)}_4 \mathbf{x}_o} \\
\color{blue}{M^{(2)}_4 \mathbf{x}_e} - D^{(2)}_4 \color{green}{M^{(2)}_4 \mathbf{x}_o}
\end{pmatrix}
\end{aligned}
$$

$$
\mathbf{M}^{(K)}_N
=
\begin{pmatrix}
W_N^{0\cdot 0\cdot \frac{N}{K}} &
W_N^{0\cdot 1\cdot \frac{N}{K}} &
\cdots &
W_N^{0\cdot (K-1)\cdot \frac{N}{K}}
\\
W_N^{1\cdot 0\cdot \frac{N}{K}} &
W_N^{1\cdot 1\cdot \frac{N}{K}} &
\cdots &
W_N^{1\cdot (K-1)\cdot \frac{N}{K}}
\\
\vdots &
\vdots &
\ddots &
\vdots
\\
W_N^{(K-1)\cdot 0\cdot \frac{N}{K}} &
W_N^{(K-1)\cdot 1\cdot \frac{N}{K}} &
\cdots &
W_N^{(K-1)(K-1)\frac{N}{K}}
\end{pmatrix},
\qquad
W_N:=e^{-i\frac{2\pi}{N}}
$$

$$
\mathbf{D}^{(K)}_N
=
\begin{pmatrix}
W^0_N & 0 & \cdots & 0 \\
0 & W^1_N & \cdots & 0 \\
\vdots & \vdots & \ddots & \vdots \\
0 & 0 & \cdots & W^{K-1}_N
\end{pmatrix}
$$

青と緑が再利用できる！

# 再帰計算

$\mathbf{M}_4^{(2)} \mathbf{x}_e$ を分解せずに、再帰的に解きます。ここもFFTのミソ

定義から、2個の入力データに対するフーリエ変換としてみなせる

$$
\mathbf{M}_4^{(2)} \mathbf{x}_e
=
\mathbf{M}_2 \mathbf{x}_e
$$

$$
\begin{aligned}
\begin{pmatrix}
f_0^{\prime} \\
f_1^{\prime}
\end{pmatrix}
&=
\begin{pmatrix}
W^0_2 & W^0_2 \\
W^0_2 & W^1_2
\end{pmatrix}
\begin{pmatrix}
x_{ee} \\
x_{eo}
\end{pmatrix} \\
&=
\begin{pmatrix}
\mathbf{M}^{(1)}_2 & \mathbf{D}^{(1)}_2 \mathbf{M}^{(1)}_2 \\
\mathbf{M}^{(1)}_2 & -\mathbf{D}^{(1)}_2 \mathbf{M}^{(1)}_2
\end{pmatrix}
\begin{pmatrix}
x_{ee} \\
x_{eo}
\end{pmatrix} \\
&=
\begin{pmatrix}
\mathbf{M}^{(1)}_2 x_{ee} + \mathbf{D}^{(1)}_2 \mathbf{M}^{(1)}_2 x_{eo} \\
\mathbf{M}^{(1)}_2 x_{ee} -\mathbf{D}^{(1)}_2 \mathbf{M}^{(1)}_2 x_{eo}
\end{pmatrix}
\end{aligned}
$$

$\mathbf{M}$を分解

$$
\begin{aligned}
\mathbf{M}^{(1)}_2 x_{ee}
&= \mathbf{M}_1 x_{ee}, \\
\mathbf{M}_1
&= \begin{pmatrix} W^{0 \cdot 0}_1 \end{pmatrix} \\
&= \begin{pmatrix} \exp\left(-i \frac{2\pi}{1} 0 \cdot 0\right) \end{pmatrix} \\
&= \begin{pmatrix} 1 \end{pmatrix}
\end{aligned}
$$

$\mathbf{D}$を分解

$$
\begin{aligned}
\mathbf{D}^{(1)}_2
&= \begin{pmatrix} W^{0}_2 \end{pmatrix} \\
&= \begin{pmatrix} 1 \end{pmatrix}
\end{aligned}
$$

$$
\begin{aligned}
\begin{pmatrix}
f_0^{\prime} \\
f_1^{\prime}
\end{pmatrix}
&=
\begin{pmatrix}
x_{ee} + x_{eo} \\
x_{ee} - x_{eo}
\end{pmatrix}
\end{aligned}
$$

Nが偶数の時に再帰計算を一般化すると

$$
\begin{aligned}
f_k
&=
\sum_{n=0}^{N-1}
x_n
\exp\left(-i\frac{2\pi kn}{N}\right),
\qquad
k=0,1,\ldots,N-1,
\\
W_N
&=
\exp\left(-i\frac{2\pi}{N}\right),
\\
f_k
&=
\sum_{n=0}^{N-1}x_nW_N^{kn},
\\[1em]
\mathbf{f}
&=
\begin{bmatrix}
f_0\\
f_1\\
\vdots\\
f_{N-1}
\end{bmatrix},
\qquad
\mathbf{x}
=
\begin{bmatrix}
x_0\\
x_1\\
\vdots\\
x_{N-1}
\end{bmatrix},
\\[1em]
\mathbf{M}
&=
\begin{bmatrix}
1 & 1 & \cdots & 1\\
1 & W_N & \cdots & W_N^{N-1}\\
1 & W_N^2 & \cdots & W_N^{2(N-1)}\\
\vdots & \vdots & \ddots & \vdots\\
1 & W_N^{N-1} & \cdots & W_N^{(N-1)^2}
\end{bmatrix},
\\
M_{kn}
&=
W_N^{kn},
\qquad
k,n=0,1,\ldots,N-1,
\\[1em]
\mathbf{f}
&=
\mathbf{M}\mathbf{x}.
\end{aligned}
$$

$$
\mathbf{x}_e =
\begin{pmatrix}
x_0 \\
x_2 \\
\vdots \\
x_{N-2}
\end{pmatrix}, \qquad

\mathbf{x}_o =
\begin{pmatrix}
x_1 \\
x_3 \\
\vdots \\
x_{N-1}
\end{pmatrix} \\


\text{FFT}_N(\mathbf{x}) = 
\left\{
\begin{array}{ll}
\begin{pmatrix}
\mathbf{M}_{N/2} \mathbf{x}_e
+ \mathbf{D}^{(N/2)}_N \mathbf{M}_{N/2} \mathbf{x}_o \\
\mathbf{M}_{N/2} \mathbf{x}_e 
- \mathbf{D}^{(N/2)}_N \mathbf{M}_{N/2} \mathbf{x}_o \\
\end{pmatrix} 
,& N>1 \\
\mathbf{x} ,& N=1
\end{array}
\right. \\

\mathbf{M}_{N/2} \mathbf{x}_e = \text{FFT}_{N/2}(\mathbf{x}_e) \\
\mathbf{M}_{N/2} \mathbf{x}_o = \text{FFT}_{N/2}(\mathbf{x}_o) \\


\text{FFT}_N(\mathbf{x}) = 
\left\{
\begin{array}{ll}
\begin{pmatrix}
\text{FFT}_{N/2}(\mathbf{x}_e) 
+ \mathbf{D}^{(N/2)}_N \text{FFT}_{N/2}(\mathbf{x}_o) \\
\text{FFT}_{N/2}(\mathbf{x}_e) 
- \mathbf{D}^{(N/2)}_N \text{FFT}_{N/2}(\mathbf{x}_o) \\
\end{pmatrix} 
,& N>1 \\
\mathbf{x} ,& N=1
\end{array}
\right.

\\



$$

# 反復計算

再帰FFTでは、入力を偶数番目と奇数番目に分割し、入力数が1になるまで再帰的に処理します。しかし、実際のプログラムでは関数呼び出しのオーバーヘッドを避けるため、木構造の下（葉）から上（根）へ向かって配列を直接書き換えながら結合していく**反復計算（インプレース計算）**が一般的です。

\(N=8\) の場合、再帰呼び出しの構造は以下のようになります。

**図1：8点FFTの再帰呼び出し木**

```text
FFT8(x0,x1,x2,x3,x4,x5,x6,x7)
├─ FFT4(x0,x2,x4,x6)
│  ├─ FFT2(x0,x4)
│  │  ├─ FFT1(x0) → x0
│  │  └─ FFT1(x4) → x4
│  └─ FFT2(x2,x6)
│     ├─ FFT1(x2) → x2
│     └─ FFT1(x6) → x6
└─ FFT4(x1,x3,x5,x7)
   ├─ FFT2(x1,x5)
   │  ├─ FFT1(x1) → x1
   │  └─ FFT1(x5) → x5
   └─ FFT2(x3,x7)
      ├─ FFT1(x3) → x3
      └─ FFT1(x7) → x7
```

反復計算では、図1の最下層（葉）を左から順に格納したものを初期配列 $A^{(0)}$ とします。上付きの数字は、計算が完了したステージ数を表します。

再帰処理で「偶数番目」と「奇数番目」への振り分けを繰り返した結果、初期配列に格納されるデータの順序は、元の添字を2進数で表してビットを反転（左右反転）させた順序と完全に一致します。
これを配列を操作する視点から見ると、**配列の添字 $k$ を2進数で表してビット反転させた値が、その場所に格納されるデータの「元の添字」になる**という明確な規則になります。

| 配列の添字 $k$ | $k$ の2進数表現 | ビット反転後 (元の添字) | 格納される値 $A^{(0)}[k]$ |
| :---: | :---: | :---: | :---: |
| 0 | 000 | 000 = 0 | $x_0$ |
| 1 | 001 | 100 = 4 | $x_4$ |
| 2 | 010 | 010 = 2 | $x_2$ |
| 3 | 011 | 110 = 6 | $x_6$ |
| 4 | 100 | 001 = 1 | $x_1$ |
| 5 | 101 | 101 = 5 | $x_5$ |
| 6 | 110 | 011 = 3 | $x_3$ |
| 7 | 111 | 111 = 7 | $x_7$ |

このように並べ替えた初期配列 $A^{(0)}$ をベースに、隣り合う要素を結合していく計算をステージごとに進めます。



## ステージ1：1点FFTから2点FFTを作る

再帰木の最下層では、1点FFTは入力値そのものです。

$$
\operatorname{FFT}_1(x_n)=x_n
$$

ステージ1では、配列上で隣り合う二つの1点FFTを結合し、一つの2点FFTを作ります。
図1を見ると、結合されるデータのペア（**結合添字対**）は $(x_0, x_4)$、$(x_2, x_6)$、$(x_1, x_5)$、$(x_3, x_7)$ です。
これを初期配列 $A^{(0)}$ の添字で表すと、以下の4ペアになります。

$$
(0,1), \quad (2,3), \quad (4,5), \quad (6,7)
$$

最初のペア $(0,1)$ について、2点FFTの計算を行います。

$$
\begin{aligned}
\operatorname{FFT}_2
\begin{pmatrix}
x_0 \\
x_4
\end{pmatrix}
&=
\begin{pmatrix}
x_0 + x_4 \\
x_0 - x_4
\end{pmatrix}
\end{aligned}
$$

これを配列 $A$ の更新として表すと、次のようになります。

$$
\begin{aligned}
A^{(1)}[0] &= A^{(0)}[0] + A^{(0)}[1] = x_0 + x_4 \\
A^{(1)}[1] &= A^{(0)}[0] - A^{(0)}[1] = x_0 - x_4
\end{aligned}
$$

残りのペア $(2,3)$、$(4,5)$、$(6,7)$ についても全く同じ計算（和と差）を適用します。
これにより配列の全要素が更新され、ステージ1終了後の配列 $A^{(1)}$ は次のようになります。

$$
A^{(1)} =
\begin{bmatrix}
x_0+x_4 & x_0-x_4 & x_2+x_6 & x_2-x_6 & x_1+x_5 & x_1-x_5 & x_3+x_7 & x_3-x_7
\end{bmatrix}
$$

$A^{(1)}$ には、2要素ずつに区切られた4つの2点FFTの結果が順番に格納されていることがわかります。

## ステージ2：2点FFTから4点FFTを作る

ステージ2では、直前のステージで作られた二つの2点FFTを結合し、4点FFTを作ります。

配列の前半部分に着目すると、$A^{(1)}$ には次の二つの結果が格納されています。

$$
\begin{pmatrix}
A^{(1)}[0] \\
A^{(1)}[1]
\end{pmatrix}
=
\operatorname{FFT}_2
\begin{pmatrix}
x_0 \\
x_4
\end{pmatrix},
\quad
\begin{pmatrix}
A^{(1)}[2] \\
A^{(1)}[3]
\end{pmatrix}
=
\operatorname{FFT}_2
\begin{pmatrix}
x_2 \\
x_6
\end{pmatrix}
$$

これらを $N=4$ の再帰計算の式に当てはめると、次のように結合されます。

$$
\begin{pmatrix}
A^{(2)}[0] \\
A^{(2)}[1] \\
A^{(2)}[2] \\
A^{(2)}[3]
\end{pmatrix}
=
\begin{pmatrix}
\begin{pmatrix}
A^{(1)}[0] \\
A^{(1)}[1]
\end{pmatrix}
+
\mathbf{D}^{(2)}_4
\begin{pmatrix}
A^{(1)}[2] \\
A^{(1)}[3]
\end{pmatrix}
\\[1em]
\begin{pmatrix}
A^{(1)}[0] \\
A^{(1)}[1]
\end{pmatrix}
-
\mathbf{D}^{(2)}_4
\begin{pmatrix}
A^{(1)}[2] \\
A^{(1)}[3]
\end{pmatrix}
\end{pmatrix}
$$

$\mathbf{D}^{(2)}_4$ は対角行列であるため、要素の計算は2つずつのペアで行われます。ここでは、結合する添字の距離が $m/2 = 2$ となります。
要素ごとに展開すると、次のようになります。

$$
\begin{aligned}
A^{(2)}[0] &= A^{(1)}[0] + W_4^0 A^{(1)}[2] \\
A^{(2)}[2] &= A^{(1)}[0] - W_4^0 A^{(1)}[2]
\end{aligned}
$$

$$
\begin{aligned}
A^{(2)}[1] &= A^{(1)}[1] + W_4^1 A^{(1)}[3] \\
A^{(2)}[3] &= A^{(1)}[1] - W_4^1 A^{(1)}[3]
\end{aligned}
$$

配列の後半4要素（$A^{(1)}[4]$〜$A^{(1)}[7]$）についても全く同じ処理を行います。
結果として、ステージ2で計算される結合添字対は以下の4ペアになります。

$$
(0,2), \quad (1,3), \quad (4,6), \quad (5,7)
$$

## ステージ3：4点FFTから8点FFTを作る

ステージ3の再帰木の最上位では、二つの4点FFTを結合して8点FFTを作ります。

前半の4要素（$A^{(2)}[0 \dots 3]$）と後半の4要素（$A^{(2)}[4 \dots 7]$）を使って計算を行います。ここでは添字の距離が $m/2 = 4$ となります。

$$
\begin{pmatrix}
A^{(3)}[0 \dots 7]
\end{pmatrix}
=
\begin{pmatrix}
A^{(2)}[0 \dots 3] + \mathbf{D}^{(4)}_8 A^{(2)}[4 \dots 7] \\
A^{(2)}[0 \dots 3] - \mathbf{D}^{(4)}_8 A^{(2)}[4 \dots 7]
\end{pmatrix}
$$

先ほどと同様に各要素をペアで計算していくと、結合添字対は距離4で結ばれた次の4ペアになります。

$$
(0,4), \quad (1,5), \quad (2,6), \quad (3,7)
$$

各ペアに対して、回転因子 $W_8^k$ を掛けた加減算を行います。例えば最初のペア $(0,4)$ であれば以下のようになります。

$$
\begin{aligned}
A^{(3)}[0] &= A^{(2)}[0] + W_8^0 A^{(2)}[4] \\
A^{(3)}[4] &= A^{(2)}[0] - W_8^0 A^{(2)}[4]
\end{aligned}
$$

ステージ3のすべての計算が完了した配列 $A^{(3)}$ が、最終的な8点FFTの計算結果となります。

## 各ステージの規則と反復計算の一般化

ここまでの $N=8$ の各ステージの動きをまとめると、次のような規則性が見えてきます。

| ステージ $s$ | 作成するFFTの長さ $m=2^s$ | 結合する添字の距離 $m/2$ | 対応する入力添字のペア                  |
|:--------:|:------------------:|:---------------:|:---------------------------- |
| 1        | 2                  | 1               | $(0,1), (2,3), (4,5), (6,7)$ |
| 2        | 4                  | 2               | $(0,2), (1,3), (4,6), (5,7)$ |
| 3        | 8                  | 4               | $(0,4), (1,5), (2,6), (3,7)$ |

各ステージでは、長さ $m/2$ のFFTを二つ結合し、長さ $m$ のFFTを作ります（$m$ は $2, 4, 8, \dots, N$ と倍増していきます）。

この計算を一般化します。
計算範囲（ブロック）の先頭添字を $b$、ブロック内のオフセット（順番）を $j$ と置きます。
結合する二つの要素は、距離 $m/2$ だけ離れた次のペアになります。

$$
\left( b+j, \quad b+j+\frac{m}{2} \right)
$$

この2つの要素を用いた一連の加減算は**バタフライ演算**と呼ばれ、次のように表されます。

$$
\begin{aligned}
u &= A^{(s-1)}[b+j] \\
v &= W_m^j A^{(s-1)}\left[b+j+\frac{m}{2}\right] \\
A^{(s)}[b+j] &= u + v \\
A^{(s)}\left[b+j+\frac{m}{2}\right] &= u - v
\end{aligned}
$$

（※ここで $j = 0, 1, \dots, \frac{m}{2}-1$ です。）

これをプログラムとして実装するためには、以下のような3重のループ構造を構築します。

1. **外側のループ**: 部分FFTの長さ $m$ を $2, 4, 8, \dots, N$ と倍々で変化させる。
2. **中間のループ**: 各ステージで、ブロックの先頭添字 $b$ を $0, m, 2m, \dots, N-m$ と変化させる。
3. **内側のループ**: ブロック内の要素 $j$ を $0$ から $m/2 - 1$ まで変化させ、バタフライ演算を行う。

この規則を擬似コードで記述すると、次のようになります。

```text
Aをビット反転順に並べ替える

m = 2
while m <= N:
    for b = 0; b < N; b += m:
        for j = 0; j < m / 2; j++:
            u = A[b + j]
            v = W_m^j * A[b + j + m / 2]

            A[b + j]         = u + v
            A[b + j + m / 2] = u - v

    m = 2 * m
```

https://qiita.com/TumoiYorozu/items/5855d75a47ef2c7e62c8

https://www.youtube.com/watch?v=ltMQVCQAtrY&t=2s

https://www.youtube.com/watch?v=9mQ0Ycu4sF0&t=617s
