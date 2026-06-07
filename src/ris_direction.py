import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, FFMpegWriter
import os
from tqdm.auto import tqdm


class AngularRIS:
    def __init__(self, num_bin=4, smooth_sigma_deg=15, min_prob=0.01):
        self.num_bin = num_bin
        self.bin_width = 360.0 / num_bin

        # 各binがどれくらい良かったかを貯める場所
        # 最初はまだ何も学習していない
        self.score = np.zeros(num_bin, dtype=np.float64)

        self.sample_count = 0
        self.smooth_sigma_deg = smooth_sigma_deg
        eps = 1e-12
        self.min_prob = min(min_prob, (1.0 - eps) / num_bin)

    def bin_index(self, x_deg):
        x_deg = float(x_deg) % 360.0
        return int(x_deg / self.bin_width)

    def circular_smooth(self, x):
        x = np.asarray(x, dtype=np.float64)
        sigma_bins = self.smooth_sigma_deg / self.bin_width
        if sigma_bins <= 0:
            return x.copy()
        # index 0 を中心とした円環距離
        idx = np.arange(self.num_bin) # [0, 1, 2, ..., 5(num_bin-1)]
        dist_bins = np.minimum(idx, self.num_bin - idx) # [0, 1, 2, 3, 4, 5] -> [0, 1, 2, 3, 2, 1]
        kernel = np.exp(-0.5 * (dist_bins / sigma_bins) ** 2)
        kernel /= np.sum(kernel) # 正規化. sum(kernel)>1 or <1 になると総量が保存されなくなる
        y = np.zeros_like(x)
        # for center in range(self.num_bin):
        #     for src in range(self.num_bin):
        #         offset = (src - center) % self.num_bin # %num_bin で円環距離を考慮
        #         y[center] += x[src] * kernel[offset]
        # 
        #
        # FFT高速化
        #
        # 目的:
        # 角度bin上のスコア x を、円環ガウスカーネル k で平滑化した y を計算したい。
        # つまり、円環上の畳み込みを計算したい。
        #
        # 元式:
        #
        # $y_j = \sum_{m=0}^{N-1} x_{(j-m) \bmod N} k_m$
        #
        # 展開すると:
        #
        # $y_j = x_j k_0 + x_{j-1} k_1 + x_{j-2} k_2 + \cdots$
        #
        # つまり、出力bin j は、周囲の入力binをカーネル重みで足し合わせたもの。
        #
        #
        # 目的:
        # 元式をそのまま計算すると j と m の2重ループになり重い。
        # そこで、畳み込みをDFT空間での掛け算に変換したい。
        #
        # x のDFTを定義する:
        #
        # $X_\ell := \sum_{n=0}^{N-1} x_n \exp(-\frac{2\pi i \ell n}{N})$
        #
        # k のDFTを定義する:
        #
        # $K_\ell := \sum_{m=0}^{N-1} k_m \exp(-\frac{2\pi i \ell m}{N})$
        #
        # y のDFTを定義する:
        #
        # $Y_\ell := \mathrm{DFT}(y) = \sum_{j=0}^{N-1} y_j \exp(-\frac{2\pi i \ell j}{N})$
        #
        # 
        # $ y = \mathrm{IDFT}(Y) $
        #
        # 目的:
        # y のDFTである Y_l を、X_l と K_l で表したい。
        # そのために、Y_l の定義へ元式を代入する。
        #
        # $Y_\ell = \sum_{j=0}^{N-1} y_j \exp(-\frac{2\pi i \ell j}{N})$
        #
        #
        # $y_j = \sum_{m=0}^{N-1} x_{(j-m) \bmod N} k_m$
        #
        # 代入すると:
        #
        # $Y_\ell = \sum_{j=0}^{N-1} \left(\sum_{m=0}^{N-1} x_{(j-m) \bmod N} k_m\right) \exp(-\frac{2\pi i \ell j}{N})$
        #
        #
        # 目的:
        # k_m は j に依存しないので、和の順序を入れ替える。
        #
        # $Y_\ell = \sum_{m=0}^{N-1} k_m \sum_{j=0}^{N-1} x_{(j-m) \bmod N} \exp(-\frac{2\pi i \ell j}{N})$
        #
        #
        # 目的:
        # x の添字を普通の n に直して、DFTの形に近づける。
        #
        # $n = (j-m) \bmod N$
        #
        # $j = n + m \pmod N$
        #
        # よって:
        #
        # $Y_\ell = \sum_{m=0}^{N-1} k_m \sum_{n=0}^{N-1} x_n \exp(-\frac{2\pi i \ell (n+m)}{N})$
        #
        #
        # 目的:
        # 指数関数を n の部分と m の部分に分ける。
        #
        # $\exp(-\frac{2\pi i \ell (n+m)}{N}) = \exp(-\frac{2\pi i \ell n}{N}) \exp(-\frac{2\pi i \ell m}{N})$
        #
        # したがって:
        #
        # $Y_\ell = \sum_{m=0}^{N-1} k_m \sum_{n=0}^{N-1} x_n \exp(-\frac{2\pi i \ell n}{N}) \exp(-\frac{2\pi i \ell m}{N})$
        #
        #
        # 目的:
        # n に依存する部分と m に依存する部分を分離する。
        #
        # $Y_\ell = \left(\sum_{n=0}^{N-1} x_n \exp(-\frac{2\pi i \ell n}{N})\right) \left(\sum_{m=0}^{N-1} k_m \exp(-\frac{2\pi i \ell m}{N})\right)$
        #
        #
        # 目的:
        # DFTの定義と見比べる。
        #
        # 1つ目の括弧は x のDFTなので X_l:
        #
        # $X_\ell = \sum_{n=0}^{N-1} x_n \exp(-\frac{2\pi i \ell n}{N})$
        #
        # 2つ目の括弧は k のDFTなので K_l:
        #
        # $K_\ell = \sum_{m=0}^{N-1} k_m \exp(-\frac{2\pi i \ell m}{N})$
        #
        # よって:
        #
        # $Y_\ell = X_\ell K_\ell$
        #
        #
        # 目的:
        # DFT空間で得た Y_l を、元のbin空間の y に戻す。
        # IDFT = Inverse Discrete Fourier Transform 逆離散フーリエ変換
        #
        # $y = \mathrm{IDFT}(Y)$
        #
        # $Y = X K$
        #
        # よって:
        #
        # $y = \mathrm{IDFT}(X K)$
        #
        # つまり:
        #
        # $y = \mathrm{IDFT}(\mathrm{DFT}(x) \mathrm{DFT}(k))$
        #
        # NumPyでは次の1行になる。
        #
        # y = np.fft.ifft(
        #     np.fft.fft(x) * np.fft.fft(kernel)
        # ).real

        y = np.fft.ifft(
            np.fft.fft(x) * np.fft.fft(kernel)
        ).real
        return y

    def bin_probabilities(self):
        smoothed_score = self.circular_smooth(self.score)
        total = np.sum(smoothed_score)

        # まだ有効な学習結果がないなら一様分布
        if self.sample_count == 0 or total <= 0:
            return np.ones(self.num_bin) / self.num_bin

        learned = smoothed_score / total

        # すべてのbinに最低限の確率を残す
        # min_prob は「各binの最低確率」
        p = learned * (1.0 - self.min_prob * self.num_bin) + self.min_prob

        # 数値誤差対策
        p = np.nan_to_num(p, nan=0.0, posinf=0.0, neginf=0.0)
        p = np.maximum(p, 0.0)
        p /= np.sum(p)

        return p

    def probability(self, x_deg):
        bin_idx = self.bin_index(x_deg)
        bin_probs = self.bin_probabilities()

        # degree単位の確率密度
        return bin_probs[bin_idx] / self.bin_width

    def sample_direction(self, K):
        bin_probs = self.bin_probabilities()

        # 1. binを確率的に選ぶ
        bins = np.random.choice(
            self.num_bin,
            size=K,
            p=bin_probs
        )

        # 2. 選んだbinの中で一様に選ぶ
        offsets = np.random.uniform(
            0.0,
            self.bin_width,
            size=K
        )

        directions = bins * self.bin_width + offsets
        return directions.tolist()

    def update(self, directions, weighted_contributions):
        added = 0

        for d, w in zip(directions, weighted_contributions):
            if not np.isfinite(w) or w <= 0:
                continue

            bin_idx = self.bin_index(d)
            self.score[bin_idx] += w
            added += 1

        self.sample_count += added

    def print_status(self):
        print(f"Sample Count: {self.sample_count}")
        print(f"Bin Scores: {self.score}")
        print(f"Bin Probabilities: {self.bin_probabilities()}")


# def F(x_deg):
#     """
#     学習用の被積分関数。
#     0〜10度では意図的にNaNを返す。
#     """
#     x_deg = float(x_deg) % 360.0

#     if 0 <= x_deg < 10:
#         return np.nan

#     x_rad = np.radians(x_deg)
#     return np.sin(x_rad) + 1.0


def F(x_deg):
    """
    Path tracing っぽいテスト関数。

    要素:
    - diffuse: cos則っぽい広いローブ
    - glossy/specular: 狭く強いスパイク
    - caustic/firefly: さらに狭い超強スパイク
    - visibility: 遮蔽領域は0
    - invalid: 一部領域はNaN
    """
    x_deg = float(x_deg) % 360.0

    def angular_distance(a, b):
        return ((a - b + 180.0) % 360.0) - 180.0

    # 評価不能領域: NaN
    # これは「バグ・無効サンプル・未定義方向」のテスト用
    if 0.0 <= x_deg < 10.0:
        return np.nan

    # 遮蔽領域: 物理的には NaN ではなく 0 が自然
    # shadow ray が遮られた、visibility = 0 のイメージ
    if 210.0 <= x_deg < 260.0:
        return 0.0

    value = 0.02

    # diffuse / Lambertian cosine lobe
    # 法線方向を90度とする
    d_diffuse = np.deg2rad(angular_distance(x_deg, 90.0))
    diffuse = max(0.0, np.cos(d_diffuse))

    value += 1.0 * diffuse

    # glossy lobe
    # 粗めの反射ハイライト
    d_glossy = angular_distance(x_deg, 135.0)
    glossy = np.exp(-0.5 * (d_glossy / 8.0) ** 2)

    value += 8.0 * glossy

    # specular spike
    # 鏡面反射に近い鋭いピーク
    d_specular = angular_distance(x_deg, 155.0)
    specular = np.exp(-0.5 * (d_specular / 2.0) ** 2)

    value += 50.0 * specular

    # caustic / firefly spike
    # 低確率だけど当たると非常に大きい寄与
    d_firefly = angular_distance(x_deg, 320.0)
    firefly = np.exp(-0.5 * (d_firefly / 0.7) ** 2)

    value += 300.0 * firefly

    return value

def plot_angular_bins(ris, sample_index=None):
    bin_probs = ris.bin_probabilities()

    centers_deg = (np.arange(ris.num_bin) + 0.5) * ris.bin_width
    centers_rad = np.deg2rad(centers_deg)

    width = np.deg2rad(ris.bin_width)

    # bin確率を degree単位の密度に変換
    heights = bin_probs / ris.bin_width

    fig, ax = plt.subplots(subplot_kw={"projection": "polar"})

    ax.bar(
        centers_rad,
        heights,
        width=width,
        bottom=0.0,
        alpha=0.6,
        edgecolor="black",
        linewidth=1.0,
    )

    title_str = "Angular PDF bins"
    if sample_index is not None:
        title_str += f" (Sample {sample_index + 1})"

    ax.set_title(title_str)
    plt.show()

def save_angular_bins_video(
    history,
    ris,
    filename="angular_ris_learning.mp4",
    fps=8,
    frame_step=5,
    normalize_display=True,
    dpi=120,
    bitrate=3000,
    show_progress=True,
):
    history = history[::frame_step]

    centers_deg = (np.arange(ris.num_bin) + 0.5) * ris.bin_width
    centers_rad = np.deg2rad(centers_deg)
    width = np.deg2rad(ris.bin_width)

    # 真のPDF密度スケールでの最大値
    global_max_height = 0.0
    for frame in history:
        raw_heights = frame["bin_probs"] / ris.bin_width
        global_max_height = max(global_max_height, np.max(raw_heights))

    if global_max_height <= 0:
        global_max_height = 1.0

    fig, ax = plt.subplots(
        figsize=(8, 8),
        subplot_kw={"projection": "polar"},
    )

    def update(frame_index):
        ax.clear()

        frame = history[frame_index]

        bin_probs = frame["bin_probs"]

        # 真の確率密度
        raw_heights = bin_probs / ris.bin_width

        # 表示用だけ正規化する
        if normalize_display:
            frame_max = np.max(raw_heights)

            if frame_max > 0:
                heights = raw_heights / frame_max
            else:
                heights = raw_heights

            ylim_top = 1.35
            candidate_radius = 1.08
            selected_radius = 1.22

            scale_text = (
                f"display normalized, "
                f"max pdf={frame_max:.4g}, "
                f"max prob={np.max(bin_probs):.4g}"
            )
        else:
            heights = raw_heights
            ylim_top = global_max_height * 1.35
            candidate_radius = global_max_height * 1.08
            selected_radius = global_max_height * 1.22

            scale_text = (
                f"true scale, "
                f"max pdf={np.max(raw_heights):.4g}, "
                f"max prob={np.max(bin_probs):.4g}"
            )

        ax.bar(
            centers_rad,
            heights,
            width=width,
            bottom=0.0,
            alpha=0.85,
            edgecolor="none",
            linewidth=0.0,
            label="q(theta)",
        )

        # min_prob の床ライン
        min_prob_pdf = ris.min_prob / ris.bin_width
        if normalize_display:
            min_prob_height = min_prob_pdf / frame_max if frame_max > 0 else 0.0
        else:
            min_prob_height = min_prob_pdf

        theta_line = np.linspace(0.0, 2.0 * np.pi, 720)
        ax.plot(
            theta_line,
            np.full_like(theta_line, min_prob_height),
            linestyle="--",
            linewidth=1.5,
            label="min_prob floor",
        )

        # 候補方向
        directions_rad = np.deg2rad(frame["directions"])

        ax.scatter(
            directions_rad,
            np.full(len(directions_rad), candidate_radius),
            s=28,
            label="candidates",
        )

        # 選ばれた方向
        selected_rad = np.deg2rad(frame["selected_direction"])

        ax.scatter(
            [selected_rad],
            [selected_radius],
            s=100,
            marker="x",
            linewidths=2.0,
            label="selected",
        )

        ax.set_ylim(0.0, ylim_top)

        ax.set_title(
            f"Angular RIS learning\n"
            f"sample {frame['sample_index'] + 1}, "
            f"estimate={frame['estimate']:.3f}\n"
            f"{scale_text}"
        )

        ax.legend(
            loc="upper right",
            bbox_to_anchor=(1.35, 1.15),
        )

    ani = FuncAnimation(
        fig,
        update,
        frames=len(history),
        interval=1000 / fps,
        repeat=True,
    )

    directory = os.path.dirname(filename)
    if directory:
        os.makedirs(directory, exist_ok=True)

    writer = FFMpegWriter(
        fps=fps,
        codec="libx264",
        bitrate=bitrate,
        extra_args=["-pix_fmt", "yuv420p"],
    )

    total_frames = len(history)

    if show_progress and tqdm is not None:
        with tqdm(
            total=total_frames,
            desc=f"Saving {os.path.basename(filename)}",
            unit="frame",
        ) as pbar:
            ani.save(
                filename,
                writer=writer,
                dpi=dpi,
                progress_callback=lambda current_frame, total_frames: pbar.update(1),
            )
    else:
        if show_progress and tqdm is None:
            print("tqdm is not installed. Save progress bar is disabled.")

        ani.save(filename, writer=writer, dpi=dpi)

    plt.close(fig)

    print(f"Saved MP4: {filename}")

def save_angular_bins_png(
    frame,
    ris,
    filename="angular_ris_frame.png",
    normalize_display=True,
):
    centers_deg = (np.arange(ris.num_bin) + 0.5) * ris.bin_width
    centers_rad = np.deg2rad(centers_deg)
    width = np.deg2rad(ris.bin_width)

    bin_probs = frame["bin_probs"]

    # 真の確率密度
    raw_heights = bin_probs / ris.bin_width
    frame_max = np.max(raw_heights)

    if normalize_display:
        if frame_max > 0:
            heights = raw_heights / frame_max
        else:
            heights = raw_heights

        min_prob_height = (ris.min_prob / ris.bin_width) / frame_max if frame_max > 0 else 0.0

        ylim_top = 1.35
        candidate_radius = 1.08
        selected_radius = 1.22

        scale_text = (
            f"display normalized, "
            f"max pdf={frame_max:.4g}, "
            f"max prob={np.max(bin_probs):.4g}, "
            f"min prob={ris.min_prob:.4g}"
        )
    else:
        heights = raw_heights
        min_prob_height = ris.min_prob / ris.bin_width

        ymax = max(np.max(raw_heights), min_prob_height)
        if ymax <= 0:
            ymax = 1.0

        ylim_top = ymax * 1.35
        candidate_radius = ymax * 1.08
        selected_radius = ymax * 1.22

        scale_text = (
            f"true scale, "
            f"max pdf={np.max(raw_heights):.4g}, "
            f"max prob={np.max(bin_probs):.4g}, "
            f"min prob={ris.min_prob:.4g}"
        )

    fig, ax = plt.subplots(
        figsize=(8, 8),
        subplot_kw={"projection": "polar"},
    )

    ax.bar(
        centers_rad,
        heights,
        width=width,
        bottom=0.0,
        alpha=0.85,
        edgecolor="none",
        linewidth=0.0,
        label="q(theta)",
    )

    # min_prob の床ライン
    theta_line = np.linspace(0.0, 2.0 * np.pi, 720)

    ax.plot(
        theta_line,
        np.full_like(theta_line, min_prob_height),
        linestyle="--",
        linewidth=2.0,
        label="min_prob floor",
    )

    # 候補方向
    directions_rad = np.deg2rad(frame["directions"])

    ax.scatter(
        directions_rad,
        np.full(len(directions_rad), candidate_radius),
        s=28,
        label="candidates",
    )

    # 選ばれた方向
    selected_rad = np.deg2rad(frame["selected_direction"])

    ax.scatter(
        [selected_rad],
        [selected_radius],
        s=100,
        marker="x",
        linewidths=2.0,
        label="selected",
    )

    ax.set_ylim(0.0, ylim_top)

    ax.set_title(
        f"Angular RIS learning\n"
        f"sample {frame['sample_index'] + 1}, "
        f"estimate={frame['estimate']:.3f}\n"
        f"{scale_text}"
    )

    ax.legend(
        loc="upper right",
        bbox_to_anchor=(1.35, 1.15),
    )

    os.makedirs(os.path.dirname(filename), exist_ok=True)
    fig.savefig(filename, dpi=150, bbox_inches="tight")
    plt.close(fig)

    print(f"Saved PNG: {filename}")

def main():
    compute_mode = 'all_progress_long_video'
    # compute_mode = 'all_progress'
    # compute_mode = 'first_progress'

    
    np.random.seed(1)

    if compute_mode == 'first_progress':
        samples = 10
    elif compute_mode == 'all_progress' or compute_mode == 'all_progress_long_video':
        samples = 100
    sample_K = 16

    estimate_sum = 0.0
    valid_estimate_count = 0

    num_bin = 180
    ris = AngularRIS(
        num_bin=num_bin,
        min_prob=1.0/num_bin*0.10,
        smooth_sigma_deg=15.0,
    )

    history = []

    for sample_index in range(samples):
        print(f"\n=== Sample {sample_index + 1} ===")
        ris.print_status()

        directions = ris.sample_direction(sample_K)

        contributions = np.array([
            F(d) for d in directions
        ])

        # Importance sampling estimator = contribution / q(direction)
        weighted_contributions = np.array([
            c / ris.probability(d)
            for c, d in zip(contributions, directions)
        ])

        # NaN, inf, 0, 負値を無効扱いにする
        valid = np.isfinite(weighted_contributions) & (weighted_contributions > 0)

        # selection用にもestimate用にも、安全な値を使う
        safe_weights = np.where(valid, weighted_contributions, 0.0)

        weights_sum = np.sum(safe_weights)

        if weights_sum <= 0:
            # 有効候補が1つもない場合だけ一様選択
            probs = np.ones(len(directions)) / len(directions)
        else:
            probs = safe_weights / weights_sum

        selected_sample_index = np.random.choice(
            len(directions),
            p=probs
        )

        selected_direction = directions[selected_sample_index]
        selected_weight = weighted_contributions[selected_sample_index]
        selected_contribution = contributions[selected_sample_index]

        # estimateはK個全体の平均
        # 無効値は0として扱う
        estimate = np.mean(safe_weights)
        estimate_sum += estimate
        valid_estimate_count += 1

        # reservoir的に、選ばれた方向だけでqを更新
        ris.update(
            [selected_direction],
            [selected_weight]
        )

        history.append({
            "sample_index": sample_index,
            "bin_probs": ris.bin_probabilities().copy(),
            "directions": np.array(directions),
            "contributions": contributions.copy(),
            "weighted_contributions": weighted_contributions.copy(),
            "safe_weights": safe_weights.copy(),
            "probs": probs.copy(),
            "selected_direction": selected_direction,
            "selected_weight": selected_weight,
            "selected_contribution": selected_contribution,
            "estimate": estimate,
        })

        print("directions:", np.round(directions, 2))
        print("contributions:", contributions)
        print("weighted_contributions:", weighted_contributions)
        print("valid:", valid)
        print("safe_weights:", safe_weights)
        print("probs:", probs)
        print("selected_sample_index:", selected_sample_index)
        print("selected_direction:", selected_direction)
        print("estimate:", estimate)

        ris.print_status()

    print()
    print("Final Estimate:", estimate_sum / valid_estimate_count)

    if compute_mode == 'first_progress':
        video_fps = 1
        video_frame_step = 1
    elif compute_mode == 'all_progress':
        video_fps = 15
        video_frame_step = max(1, len(history) // 100)
    elif compute_mode == 'all_progress_long_video':
        video_fps = 60
        video_frame_step = 1
    save_angular_bins_video(
        history,
        ris,
        filename="angular_ris_learning.mp4",
        fps=video_fps,
        frame_step=video_frame_step,
        normalize_display=True,
        dpi=120,
        bitrate=5000,
    )


    if compute_mode == 'first_progress':
        png_frame_step = 1
    elif compute_mode == 'all_progress' or compute_mode == 'all_progress_long_video':
        png_frame_step = max(1, len(history) // 10)
    for frame in history[::png_frame_step]:
        save_angular_bins_png(
            frame,
            ris,
            filename=f"angular_ris/angular_ris_frame_{frame['sample_index'] + 1}_true_scale.png",
            normalize_display=False,
        )


if __name__ == "__main__":
    main()