import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import streamlit as st
from dataclasses import dataclass


APP_VERSION = "v8-one-reservoir-for-restir-di-2026-06-06"


# ============================================================
# Layer 0: data containers
# ============================================================

@dataclass
class SimulationParams:
    seed: int
    steps: int
    observe_step: int
    candidates_per_reservoir: int
    memory_size: int
    bandwidth: float
    learning_strength: float
    x_grid_size: int = 2048


@dataclass
class StepTrace:
    step_index: int
    memory_before: np.ndarray
    q_before: np.ndarray
    uniform_x: np.ndarray
    uniform_fx: np.ndarray
    candidates_x: np.ndarray
    candidates_fx: np.ndarray
    candidates_qx: np.ndarray
    weights: np.ndarray
    probabilities: np.ndarray
    selected_idx: int
    selected_x: float
    selected_fx: float
    reservoir_estimate: float
    memory_after: np.ndarray
    uniform_estimate_after: float
    ris_estimate_after: float


@dataclass
class SimulationResult:
    params: SimulationParams
    x_grid: np.ndarray
    f_grid: np.ndarray
    true_integral: float
    eval_history: np.ndarray
    uniform_history: np.ndarray
    ris_history: np.ndarray
    final_q: np.ndarray
    final_memory: np.ndarray
    trace: StepTrace


# ============================================================
# Layer 1: mathematical primitives
# ============================================================


def integrate_trapezoid(y: np.ndarray, x: np.ndarray) -> float:
    """Utility: numerical integral on a grid."""
    if hasattr(np, "trapezoid"):
        return float(np.trapezoid(y, x))
    return float(np.trapz(y, x))


def gaussian(x: np.ndarray, mu: float, sigma: float) -> np.ndarray:
    """Utility: Gaussian bump without normalization."""
    return np.exp(-0.5 * ((x - mu) / sigma) ** 2)


def target_function(x: np.ndarray) -> np.ndarray:
    """Step 1 Target Definition: f(x), the integrand. The algorithm only queries f at sampled points."""
    return (
        0.12
        + 5.0 * gaussian(x, 0.18, 0.012)
        + 2.8 * gaussian(x, 0.58, 0.035)
        + 1.7 * gaussian(x, 0.86, 0.018)
    )


def make_x_grid(size: int) -> np.ndarray:
    """Utility: domain grid for plotting and PDF construction."""
    return np.linspace(0.0, 1.0, size)


# ============================================================
# Layer 2: PDF construction and sampling
# ============================================================


def normalize_pdf(pdf_unnormalized: np.ndarray, x_grid: np.ndarray) -> np.ndarray:
    """Step 2 utility: normalize non-negative values into a PDF."""
    pdf_unnormalized = np.maximum(pdf_unnormalized, 0.0)
    area = integrate_trapezoid(pdf_unnormalized, x_grid)
    if not np.isfinite(area) or area <= 0.0:
        pdf = np.ones_like(x_grid)
        return pdf / integrate_trapezoid(pdf, x_grid)
    return pdf_unnormalized / area


def build_adaptive_proposal(
    x_grid: np.ndarray,
    memory_x: np.ndarray,
    bandwidth: float,
    learning_strength: float,
) -> np.ndarray:
    """
    Step 2 Proposal Build.

    Educational toy model:
      q(x) = (1 - learning_strength) * uniform
           + learning_strength * KDE(memory)

    This is not how ReSTIR DI usually constructs proposals. It is used here
    only to show how previous selected samples can influence later sampling.
    """
    uniform_pdf = normalize_pdf(np.ones_like(x_grid), x_grid)

    if len(memory_x) == 0:
        return uniform_pdf

    dx = x_grid[1] - x_grid[0]
    n = len(x_grid)

    indices = np.clip(np.round(memory_x / dx).astype(int), 0, n - 1)
    impulses = np.bincount(indices, minlength=n).astype(float)

    radius = max(1, int(np.ceil(4.0 * bandwidth / dx)))
    offsets = np.arange(-radius, radius + 1) * dx
    kernel = np.exp(-0.5 * (offsets / bandwidth) ** 2)

    kde = np.convolve(impulses, kernel, mode="same")
    kde_pdf = normalize_pdf(kde, x_grid)

    q_unnormalized = (
        (1.0 - learning_strength) * uniform_pdf
        + learning_strength * kde_pdf
    )
    return normalize_pdf(q_unnormalized, x_grid)


def make_cdf_from_pdf(pdf: np.ndarray, x_grid: np.ndarray) -> np.ndarray:
    """Step 3 utility: build CDF for inverse transform sampling."""
    cdf = np.zeros_like(x_grid)
    cdf[1:] = np.cumsum(0.5 * (pdf[:-1] + pdf[1:]) * np.diff(x_grid))
    if cdf[-1] <= 0.0:
        raise ValueError("PDF cannot be converted to CDF because its integral is zero.")
    return cdf / cdf[-1]


def sample_from_pdf(
    rng: np.random.Generator,
    x_grid: np.ndarray,
    pdf: np.ndarray,
    size: int,
) -> np.ndarray:
    """Step 3 Candidate Generation: draw candidate positions x_i from q(x)."""
    cdf = make_cdf_from_pdf(pdf, x_grid)
    u = rng.random(size)
    return np.interp(u, cdf, x_grid)


# ============================================================
# Layer 3: RIS core operations for one reservoir
# ============================================================


def sample_candidates_from_proposal(
    rng: np.random.Generator,
    x_grid: np.ndarray,
    q_grid: np.ndarray,
    candidates_per_reservoir: int,
) -> np.ndarray:
    """Step 3 Candidate Generation: create K candidate positions x_i for one reservoir."""
    return sample_from_pdf(
        rng=rng,
        x_grid=x_grid,
        pdf=q_grid,
        size=candidates_per_reservoir,
    )


def evaluate_candidates(
    x_grid: np.ndarray,
    q_grid: np.ndarray,
    candidates_x: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    """Step 4 Candidate Evaluation: compute f(x_i) and q(x_i)."""
    candidates_fx = target_function(candidates_x)
    candidates_qx = np.interp(candidates_x, x_grid, q_grid)
    return candidates_fx, candidates_qx


def compute_importance_weights(
    candidates_fx: np.ndarray,
    candidates_qx: np.ndarray,
) -> np.ndarray:
    """Step 5 Weighting: w_i = f(x_i) / q(x_i)."""
    eps = 1e-12
    return candidates_fx / np.maximum(candidates_qx, eps)


def normalize_resampling_probabilities(weights: np.ndarray) -> np.ndarray:
    """Step 6 Probability Normalize: p_i = w_i / sum_j w_j."""
    s = float(np.sum(weights))
    return weights / max(s, 1e-12)


def resample_candidates(
    rng: np.random.Generator,
    candidates_x: np.ndarray,
    probabilities: np.ndarray,
) -> tuple[int, float]:
    """Step 7 Resampling: choose one representative selected_x from K candidates."""
    cumulative = np.cumsum(probabilities)
    selected_idx = int(np.searchsorted(cumulative, rng.random(), side="right"))
    selected_idx = min(selected_idx, len(candidates_x) - 1)
    selected_x = float(candidates_x[selected_idx])
    return selected_idx, selected_x


def estimate_integral_from_weights(weights: np.ndarray) -> float:
    """
    Step 8 Estimator: one reservoir produces mean_i f(x_i)/q(x_i).

    Conditional on q, E_q[f(x)/q(x)] = integral f(x) dx.
    """
    return float(np.mean(weights))


def update_memory(
    memory_x: np.ndarray,
    selected_x: float,
    memory_size: int,
) -> np.ndarray:
    """Step 9 Memory Update: append selected_x, which will shape later q(x)."""
    updated = np.concatenate([memory_x, np.array([selected_x], dtype=float)])
    if len(updated) > memory_size:
        updated = updated[-memory_size:]
    return updated


# ============================================================
# Layer 4: one-step pipelines
# ============================================================


def run_uniform_step(
    rng: np.random.Generator,
    evals_per_step: int,
) -> tuple[np.ndarray, np.ndarray, float]:
    """Baseline pipeline: uniform samples and their per-step estimate."""
    x = rng.random(evals_per_step)
    fx = target_function(x)
    step_estimate = float(np.mean(fx))
    return x, fx, step_estimate


def run_one_adaptive_ris_step(
    rng: np.random.Generator,
    x_grid: np.ndarray,
    memory_before: np.ndarray,
    params: SimulationParams,
) -> dict:
    """
    RIS pipeline for one observed shading point.

    This step has exactly one reservoir.
    K candidates enter that reservoir, and one candidate is selected for memory.
    """
    q_before = build_adaptive_proposal(
        x_grid=x_grid,
        memory_x=memory_before,
        bandwidth=params.bandwidth,
        learning_strength=params.learning_strength,
    )

    candidates_x = sample_candidates_from_proposal(
        rng=rng,
        x_grid=x_grid,
        q_grid=q_before,
        candidates_per_reservoir=params.candidates_per_reservoir,
    )

    candidates_fx, candidates_qx = evaluate_candidates(
        x_grid=x_grid,
        q_grid=q_before,
        candidates_x=candidates_x,
    )

    weights = compute_importance_weights(candidates_fx, candidates_qx)
    probabilities = normalize_resampling_probabilities(weights)
    selected_idx, selected_x = resample_candidates(rng, candidates_x, probabilities)
    reservoir_estimate = estimate_integral_from_weights(weights)
    memory_after = update_memory(memory_before, selected_x, params.memory_size)

    return {
        "q_before": q_before,
        "candidates_x": candidates_x,
        "candidates_fx": candidates_fx,
        "candidates_qx": candidates_qx,
        "weights": weights,
        "probabilities": probabilities,
        "selected_idx": selected_idx,
        "selected_x": selected_x,
        "selected_fx": float(target_function(np.array([selected_x]))[0]),
        "reservoir_estimate": reservoir_estimate,
        "memory_after": memory_after,
    }


# ============================================================
# Layer 5: full simulation pipeline
# ============================================================


@st.cache_data(show_spinner=False)
def run_simulation(params: SimulationParams) -> SimulationResult:
    rng = np.random.default_rng(params.seed)

    x_grid = make_x_grid(params.x_grid_size)
    f_grid = target_function(x_grid)
    true_integral = integrate_trapezoid(f_grid, x_grid)

    memory_x = np.array([], dtype=float)

    uniform_sum = 0.0
    uniform_n = 0
    ris_sum = 0.0
    ris_n = 0
    eval_count = 0

    eval_history = []
    uniform_history = []
    ris_history = []

    trace = None
    evals_per_step = params.candidates_per_reservoir

    for step_index in range(1, params.steps + 1):
        memory_before = memory_x.copy()

        uniform_x, uniform_fx, _ = run_uniform_step(rng, evals_per_step)
        uniform_sum += float(np.sum(uniform_fx))
        uniform_n += evals_per_step
        uniform_estimate_after = uniform_sum / uniform_n

        ris_step = run_one_adaptive_ris_step(
            rng=rng,
            x_grid=x_grid,
            memory_before=memory_before,
            params=params,
        )

        memory_x = ris_step["memory_after"]
        ris_sum += float(ris_step["reservoir_estimate"])
        ris_n += 1
        ris_estimate_after = ris_sum / ris_n

        eval_count += evals_per_step
        eval_history.append(eval_count)
        uniform_history.append(uniform_estimate_after)
        ris_history.append(ris_estimate_after)

        if step_index == params.observe_step:
            trace = StepTrace(
                step_index=step_index,
                memory_before=memory_before,
                q_before=ris_step["q_before"],
                uniform_x=uniform_x,
                uniform_fx=uniform_fx,
                candidates_x=ris_step["candidates_x"],
                candidates_fx=ris_step["candidates_fx"],
                candidates_qx=ris_step["candidates_qx"],
                weights=ris_step["weights"],
                probabilities=ris_step["probabilities"],
                selected_idx=ris_step["selected_idx"],
                selected_x=ris_step["selected_x"],
                selected_fx=ris_step["selected_fx"],
                reservoir_estimate=ris_step["reservoir_estimate"],
                memory_after=ris_step["memory_after"],
                uniform_estimate_after=uniform_estimate_after,
                ris_estimate_after=ris_estimate_after,
            )

    if trace is None:
        raise RuntimeError("observe_step was outside the simulated range.")

    final_q = build_adaptive_proposal(
        x_grid=x_grid,
        memory_x=memory_x,
        bandwidth=params.bandwidth,
        learning_strength=params.learning_strength,
    )

    return SimulationResult(
        params=params,
        x_grid=x_grid,
        f_grid=f_grid,
        true_integral=true_integral,
        eval_history=np.array(eval_history),
        uniform_history=np.array(uniform_history),
        ris_history=np.array(ris_history),
        final_q=final_q,
        final_memory=memory_x,
        trace=trace,
    )


# ============================================================
# Layer 6: visualization
# ============================================================


def close_fig(fig):
    plt.close(fig)
    return fig


def plot_result_overview(result: SimulationResult):
    fig, axes = plt.subplots(1, 2, figsize=(14, 4.8))

    ax = axes[0]
    ax.plot(result.x_grid, result.f_grid, label="f(x)")
    ax.plot(
        result.x_grid,
        result.final_q * result.true_integral,
        linestyle="--",
        label="q_next(x): proposal rebuilt from final memory, scaled",
    )
    if len(result.final_memory) > 0:
        ax.hist(
            result.final_memory,
            bins=70,
            density=True,
            alpha=0.25,
            label="final memory: selected samples across steps",
        )
    ax.set_title("Learned proposal after all steps")
    ax.set_xlabel("x")
    ax.set_ylabel("value / density")
    ax.set_xlim(0.0, 1.0)
    ax.legend()

    ax = axes[1]
    ax.axhline(result.true_integral, linestyle=":", linewidth=2, label="true integral ∫f(x)dx")
    ax.plot(result.eval_history, result.uniform_history, label="Uniform MC")
    ax.plot(result.eval_history, result.ris_history, label="Adaptive RIS")
    ax.set_title("Convergence with the same number of f(x_i) evaluations")
    ax.set_xlabel("number of f(x) evaluations")
    ax.set_ylabel("estimated integral")
    ax.set_xscale("log")
    ax.legend()

    fig.tight_layout()
    return close_fig(fig)


def plot_step_2_proposal(result: SimulationResult):
    trace = result.trace
    fig, axes = plt.subplots(1, 2, figsize=(14, 4.6))

    ax = axes[0]
    ax.plot(result.x_grid, result.f_grid / result.true_integral, label="normalized f(x): reference only")
    ax.plot(result.x_grid, trace.q_before, linestyle="--", label="q(x): used in this step")
    if len(trace.memory_before) > 0:
        ax.hist(trace.memory_before, bins=60, density=True, alpha=0.3, label="memory at step start")
    ax.set_title("Step 2: build q(x) from memory")
    ax.set_xlabel("x")
    ax.set_ylabel("density")
    ax.set_xlim(0, 1)
    ax.legend()

    ax = axes[1]
    ax.plot(result.x_grid, trace.q_before, label="q(x)")
    ax.set_title("This q(x) generates the K candidates in Step 3")
    ax.set_xlabel("x")
    ax.set_ylabel("probability density")
    ax.set_xlim(0, 1)
    ax.legend()

    fig.tight_layout()
    return close_fig(fig)


def plot_step_3_candidates(result: SimulationResult):
    trace = result.trace
    fig, axes = plt.subplots(1, 2, figsize=(14, 4.6))

    ax = axes[0]
    ax.plot(result.x_grid, result.f_grid, label="f(x)")
    ax.plot(result.x_grid, trace.q_before * result.true_integral, linestyle="--", label="q(x), scaled")
    ax.scatter(
        trace.candidates_x,
        trace.candidates_fx,
        s=90,
        label=f"K={len(trace.candidates_x)} candidates in the one reservoir",
    )
    for i, x in enumerate(trace.candidates_x):
        ax.text(x, trace.candidates_fx[i], f" {i}", fontsize=9)
    ax.set_title("Step 3: exactly K candidates enter one reservoir")
    ax.set_xlabel("x")
    ax.set_ylabel("f(x)")
    ax.set_xlim(0, 1)
    ax.legend()

    ax = axes[1]
    ax.plot(result.x_grid, trace.q_before, linestyle="--", label="q(x)")
    ymax = max(np.max(trace.q_before), 1.0)
    for i, x in enumerate(trace.candidates_x):
        ax.vlines(x, 0, ymax * 0.12, linewidth=2)
        ax.text(x, ymax * 0.14, str(i), fontsize=9, ha="center")
    ax.set_title("Candidate positions as tick marks, not a histogram")
    ax.set_xlabel("x")
    ax.set_ylabel("density")
    ax.set_xlim(0, 1)
    ax.legend()

    fig.tight_layout()
    return close_fig(fig)


def candidate_table(trace: StepTrace) -> pd.DataFrame:
    selected = trace.selected_idx
    return pd.DataFrame({
        "candidate i": np.arange(len(trace.candidates_x)),
        "x_i": trace.candidates_x,
        "f(x_i)": trace.candidates_fx,
        "q(x_i)": trace.candidates_qx,
        "w_i = f/q": trace.weights,
        "p_i = w/sum(w)": trace.probabilities,
        "selected for memory?": ["YES" if i == selected else "" for i in range(len(trace.candidates_x))],
    })


def plot_step_4_6_weights(result: SimulationResult):
    trace = result.trace
    xs = trace.candidates_x
    ws = trace.weights
    ps = trace.probabilities
    selected = trace.selected_idx

    fig, axes = plt.subplots(1, 3, figsize=(16, 4.4))

    ax = axes[0]
    ax.plot(result.x_grid, result.f_grid, label="f(x)")
    ax.scatter(xs, trace.candidates_fx, s=70, label="candidate x_i")
    ax.scatter(xs[selected], trace.candidates_fx[selected], s=150, marker="x", label="selected candidate")
    for i, x in enumerate(xs):
        ax.text(x, trace.candidates_fx[i], str(i), fontsize=9)
    ax.set_title("Step 4: evaluate f(x_i)")
    ax.set_xlabel("x")
    ax.set_ylabel("f(x)")
    ax.set_xlim(0, 1)
    ax.legend()

    ax = axes[1]
    ax.bar(np.arange(len(ws)), ws)
    ax.set_title("Step 5: weights w_i = f(x_i) / q(x_i)")
    ax.set_xlabel("candidate i")
    ax.set_ylabel("weight")

    ax = axes[2]
    ax.bar(np.arange(len(ps)), ps)
    ax.axvline(selected, linestyle="--", label="selected index")
    ax.set_title("Step 6: selection probabilities p_i")
    ax.set_xlabel("candidate i")
    ax.set_ylabel("probability")
    ax.legend()

    fig.tight_layout()
    return close_fig(fig)


def plot_step_7_8_resampling_and_estimator(result: SimulationResult):
    trace = result.trace
    xs = trace.candidates_x
    selected = trace.selected_idx

    fig, axes = plt.subplots(1, 2, figsize=(14, 4.6))

    ax = axes[0]
    ax.plot(result.x_grid, result.f_grid, label="f(x)")
    ax.scatter(xs, trace.candidates_fx, s=70, alpha=0.65, label="K candidates")
    ax.scatter(trace.selected_x, trace.selected_fx, s=170, marker="x", label="selected sample stored in memory")
    ax.set_title("Step 7: one candidate is selected")
    ax.set_xlabel("x")
    ax.set_ylabel("f(x)")
    ax.set_xlim(0, 1)
    ax.legend()

    ax = axes[1]
    ax.bar(np.arange(len(trace.weights)), trace.weights, label="candidate weights")
    ax.axhline(trace.reservoir_estimate, linestyle="--", label=f"mean_i w_i = {trace.reservoir_estimate:.5f}")
    ax.axhline(result.true_integral, linestyle=":", linewidth=2, label=f"true integral = {result.true_integral:.5f}")
    ax.set_title("Step 8: estimator uses the mean of all K weights")
    ax.set_xlabel("candidate i")
    ax.set_ylabel("weight")
    ax.legend()

    fig.tight_layout()
    return close_fig(fig)


def plot_step_9_memory_update(result: SimulationResult):
    trace = result.trace
    fig, axes = plt.subplots(1, 2, figsize=(14, 4.6))

    ax = axes[0]
    ax.plot(result.x_grid, result.f_grid / result.true_integral, label="normalized f(x): reference only")
    if len(trace.memory_before) > 0:
        ax.hist(trace.memory_before, bins=60, density=True, alpha=0.35, label="memory at step start")
    ax.set_title("Step 9a: memory before adding selected sample")
    ax.set_xlabel("x")
    ax.set_ylabel("density")
    ax.set_xlim(0, 1)
    ax.legend()

    ax = axes[1]
    ax.plot(result.x_grid, result.f_grid / result.true_integral, label="normalized f(x): reference only")
    if len(trace.memory_after) > 0:
        ax.hist(trace.memory_after, bins=60, density=True, alpha=0.35, label="memory after this step")
    ax.scatter(
        np.array([trace.selected_x]),
        np.array([trace.selected_fx / result.true_integral]),
        s=120,
        marker="x",
        label="sample added this step",
    )
    ax.set_title("Step 9b: memory after adding selected sample")
    ax.set_xlabel("x")
    ax.set_ylabel("density")
    ax.set_xlim(0, 1)
    ax.legend()

    fig.tight_layout()
    return close_fig(fig)


def step_summary_dataframe(result: SimulationResult) -> pd.DataFrame:
    t = result.trace
    return pd.DataFrame([
        {
            "named step": "1. Target definition",
            "function": "target_function",
            "main data": "f(x)",
            "why it matters": "未知の被積分関数。サンプラーは点ごとの値だけ評価できる。",
        },
        {
            "named step": "2. Proposal Build",
            "function": "build_adaptive_proposal",
            "main data": f"q(x), memory size at step start = {len(t.memory_before)}",
            "why it matters": "このstepでK個の候補を出す分布 q(x) を作る。",
        },
        {
            "named step": "3. Candidate Generation",
            "function": "sample_candidates_from_proposal",
            "main data": f"K candidates, candidates_x shape = {t.candidates_x.shape}",
            "why it matters": "1つのreservoirへK個の候補位置 x_i を入れる。",
        },
        {
            "named step": "4. Candidate Evaluation",
            "function": "evaluate_candidates",
            "main data": "f(x_i), q(x_i)",
            "why it matters": "候補の寄与と、その候補がどれくらい出やすかったかを測る。",
        },
        {
            "named step": "5. Weighting",
            "function": "compute_importance_weights",
            "main data": "w_i = f(x_i) / q(x_i)",
            "why it matters": "q(x)で出やすい/出にくい差を補正する。",
        },
        {
            "named step": "6. Probability Normalize",
            "function": "normalize_resampling_probabilities",
            "main data": "p_i = w_i / sum(w)",
            "why it matters": "候補の重みを、選択確率に変換する。",
        },
        {
            "named step": "7. Resampling",
            "function": "resample_candidates",
            "main data": "selected sample",
            "why it matters": "K個の候補から、memoryに入れる候補を1つ選ぶ。",
        },
        {
            "named step": "8. Estimator",
            "function": "estimate_integral_from_weights",
            "main data": "mean_i w_i",
            "why it matters": "積分推定には、選ばれた1点ではなく候補重みの平均を使う。",
        },
        {
            "named step": "9. Memory Update",
            "function": "update_memory",
            "main data": f"memory size after this step = {len(t.memory_after)}",
            "why it matters": "選ばれた候補を保存し、次stepの q(x) をmemoryから作れるようにする。",
        },
    ])


def explain_result_message(result: SimulationResult) -> tuple[str, str]:
    true_value = result.true_integral
    uniform_final = float(result.uniform_history[-1])
    ris_final = float(result.ris_history[-1])
    uniform_error = abs(uniform_final - true_value)
    ris_error = abs(ris_final - true_value)

    if ris_error < uniform_error:
        ratio = uniform_error / max(ris_error, 1e-12)
        return (
            "Adaptive RIS is better for this run",
            f"最終誤差は Uniform MC が {uniform_error:.6f}, Adaptive RIS が {ris_error:.6f} です。"
            f"この設定では RIS の誤差が約 {ratio:.2f} 倍小さくなっています。",
        )

    return (
        "Adaptive RIS is not better for this particular random run",
        f"最終誤差は Uniform MC が {uniform_error:.6f}, Adaptive RIS が {ris_error:.6f} です。"
        "MCなのでseedやstep数によって逆転することがあります。stepsを増やすか、seedを変えて観察してください。",
    )


# ============================================================
# Layer 7: Streamlit view
# ============================================================


st.set_page_config(page_title="RIS Learning Demo", layout="wide")
st.title("RIS Learning Demo for ReSTIR DI: one observed point, one reservoir")
st.caption(f"Version: {APP_VERSION}")

st.markdown(
    """
この版では、`reservoirs per step R` を削除しました。  
ReSTIR DIの最小形に合わせて、**1つの観察点 / pixel / shading point が、1つのreservoirを持つ** として説明します。
"""
)

with st.sidebar:
    st.header("Simulation parameters")
    seed = st.number_input("random seed", value=7, step=1, help="乱数を固定する値。同じ値なら同じ結果を再現します。")
    steps = st.slider("total steps: 実行するRIS step数", min_value=10, max_value=1000, value=240, step=10)
    observe_step = st.number_input("詳しく見るstep", min_value=1, max_value=steps, value=min(1, steps), step=1)
    candidates_per_reservoir = st.slider("K: candidates in the one reservoir", min_value=2, max_value=128, value=16, step=1)
    memory_size = st.slider("memory size: 保存する候補の最大数", min_value=20, max_value=5000, value=800, step=20)
    bandwidth = st.slider("proposal bandwidth: memory周辺へ広げる幅", min_value=0.002, max_value=0.150, value=0.024, step=0.002, format="%.3f")
    learning_strength = st.slider("learning strength: qにmemoryを混ぜる強さ", min_value=0.0, max_value=0.98, value=0.72, step=0.01)

params = SimulationParams(
    seed=int(seed),
    steps=int(steps),
    observe_step=int(observe_step),
    candidates_per_reservoir=int(candidates_per_reservoir),
    memory_size=int(memory_size),
    bandwidth=float(bandwidth),
    learning_strength=float(learning_strength),
)

result = run_simulation(params)

# ------------------------------------------------------------
# 1. RIS overview
# ------------------------------------------------------------

st.header("1. RISの概要")
st.markdown(
    """
RIS, Resampled Importance Sampling, は **複数の候補を評価し、その中から代表候補を1つ残す** 方法です。  
このデモでは、次の積分値を推定します。

```text
I = ∫_0^1 f(x) dx
```

`f(x)` はスパイクを持つ被積分関数です。  
サンプラーは最初から `f(x)` の全体形状を知っているわけではなく、選んだ点 `x_i` で **その場で `f(x_i)` を評価できるだけ**、という想定です。
"""
)

st.subheader("1.1 ReSTIR DIを意識した最小単位")
st.markdown(
    """
この版では、観察する単位を1つに固定します。

```text
1 observed pixel / shading point
  -> 1 reservoir
  -> K candidates
  -> 1 selected sample
```

つまり、この教材内に `R = reservoirs per step` はありません。  
画面全体のReSTIR DIではpixelごとにreservoirを持ちますが、この教材ではまず **1 pixelぶんのreservoir更新** だけを見ます。
"""
)

st.subheader("1.2 このデモで使う用語")
st.dataframe(
    pd.DataFrame([
        {
            "用語": "f(x)",
            "意味": "積分したい被積分関数。形状は未知だが、選んだ点 x_i では f(x_i) を評価できる。",
            "このコードでの対応": "target_function",
        },
        {
            "用語": "q(x)",
            "意味": "候補 x_i を出すための確率密度。最初は一様分布。memory が増えると、その周辺に寄る。",
            "このコードでの対応": "build_adaptive_proposal",
        },
        {
            "用語": "candidate x_i",
            "意味": "このstepの q(x) から生成した候補位置。1つのreservoirに K 個入る。",
            "このコードでの対応": "sample_candidates_from_proposal",
        },
        {
            "用語": "reservoir",
            "意味": "K個の候補から代表候補を1つ残す箱。この教材では常に1つだけ。",
            "このコードでの対応": "run_one_adaptive_ris_step",
        },
        {
            "用語": "memory",
            "意味": "過去stepで残った代表候補のリスト。次stepの q(x) を作る材料になる。",
            "このコードでの対応": "update_memory",
        },
    ]),
    use_container_width=True,
    hide_index=True,
)

st.subheader("1.3 q(x) の初期状態と更新")
st.markdown(
    """
`q(x)` は **候補位置 `x_i` をどこから出すか** を決める確率密度です。

最初のstepでは `memory` が空なので、`q(x)` は一様分布です。

```text
step 1: memory = empty
        q(x) = uniform
```

その後、RISで選ばれた候補が `memory` に蓄積されます。  
次のstepでは、memory周辺に山を置いて、候補生成で使う `q(x)` を作ります。

```text
q(x) = (1 - learning_strength) * uniform
     + learning_strength * KDE(memory)
```

これはReSTIR DIの実装そのものではなく、**過去の代表候補が次の候補生成に影響する**ことを見るための1D教材モデルです。
"""
)

st.subheader("1.4 RISの1stepで起きること")
st.markdown(
    """
1stepでは、1つのreservoirに対して次を行います。

```text
1. step開始時点の memory から、このstepの q(x) を作る
2. その q(x) から K 個の候補位置 x_i を出す
3. 各候補で f(x_i) と q(x_i) を評価する
4. w_i = f(x_i) / q(x_i) を計算する
5. p_i = w_i / sum(w) に変換する
6. p_i に比例して、K 個の候補から1つを選ぶ
7. 選ばれた候補を memory に入れて、次stepの q(x) の材料にする
```

「候補を1つ残す」とは、**K個の候補の中から、memoryに入れる代表サンプルを1つ選ぶ**という意味です。  
積分推定そのものには、選ばれた1点の `f(x)` だけではなく、候補全体の重み平均を使います。

```text
reservoir estimate = mean_i f(x_i) / q(x_i)
```
"""
)

# ------------------------------------------------------------
# 2. Result comparison
# ------------------------------------------------------------

st.header("2. 一様ランダムと比べた結果優位性")

true_value = result.true_integral
uniform_final = float(result.uniform_history[-1])
ris_final = float(result.ris_history[-1])
uniform_error = abs(uniform_final - true_value)
ris_error = abs(ris_final - true_value)

a, b, c, d = st.columns(4)
a.metric("True integral", f"{true_value:.6f}")
b.metric("Uniform MC", f"{uniform_final:.6f}", delta=f"error {uniform_error:.6f}", delta_color="inverse")
c.metric("Adaptive RIS", f"{ris_final:.6f}", delta=f"error {ris_error:.6f}", delta_color="inverse")
if ris_error > 0:
    d.metric("Uniform error / RIS error", f"{uniform_error / ris_error:.2f}x")
else:
    d.metric("Uniform error / RIS error", "inf")

status_title, status_body = explain_result_message(result)
st.subheader(status_title)
st.write(status_body)
st.pyplot(plot_result_overview(result), clear_figure=True)

# ------------------------------------------------------------
# 3. Step by step explanation
# ------------------------------------------------------------

st.header("3. RIS step-by-step: 中間データを見て理解する")
st.markdown(
    f"""
ここでは **step {result.trace.step_index}** を1つだけ観察します。  
このstepには **1つのreservoirだけ** があり、その中に `K={params.candidates_per_reservoir}` 個の候補が入ります。
"""
)

step_tabs = st.tabs([
    "Step 2: Proposal Build",
    "Step 3: Candidate Generation",
    "Step 4-6: Weights and Probabilities",
    "Step 7-8: Resampling and Estimator",
    "Step 9: Memory Update",
])

with step_tabs[0]:
    st.subheader("Step 2: Proposal Build")
    st.markdown(
        """
**対応する関数**: `build_adaptive_proposal`  

このstepでは、step開始時点の `memory` から、このstepで使う `q(x)` を作ります。  
`q(x)` が高い場所ほど、次の Step 3 で候補 `x_i` が出やすくなります。
"""
    )
    st.pyplot(plot_step_2_proposal(result), clear_figure=True)

with step_tabs[1]:
    st.subheader("Step 3: Candidate Generation")
    st.markdown(
        f"""
**対応する関数**: `sample_candidates_from_proposal` → `sample_from_pdf`  

`K` は **1つのreservoirに入る候補数** です。  
この版ではreservoirは1つだけなので、図に出る候補も正確に `K={params.candidates_per_reservoir}` 個です。
"""
    )
    st.pyplot(plot_step_3_candidates(result), clear_figure=True)

with step_tabs[2]:
    st.subheader("Step 4-6: Candidate Evaluation, Weighting, Probability Normalize")
    st.markdown(
        """
**対応する関数**:  
- `evaluate_candidates`: `f(x_i)` と `q(x_i)` を計算  
- `compute_importance_weights`: `w_i = f(x_i) / q(x_i)`  
- `normalize_resampling_probabilities`: `p_i = w_i / sum(w)`  

RISでは、`f(x_i)` が大きいだけでなく、`q(x_i)` でどれくらい出やすかったかも重みに入ります。
"""
    )
    st.pyplot(plot_step_4_6_weights(result), clear_figure=True)
    st.dataframe(candidate_table(result.trace), use_container_width=True)

with step_tabs[3]:
    st.subheader("Step 7-8: Resampling and Estimator")
    st.markdown(
        """
**対応する関数**:  
- `resample_candidates`: `p_i` に比例して候補を1つ残す  
- `estimate_integral_from_weights`: このreservoirの積分推定値を作る  

重要なのは、**積分推定値は選ばれた1点の `f(x)` ではない** ということです。  
このデモでは、1つのreservoirの推定値として次を使います。

```text
estimate = mean_i f(x_i) / q(x_i)
```

選ばれた候補は、次のproposalを作るためのmemoryに入れます。
"""
    )
    st.pyplot(plot_step_7_8_resampling_and_estimator(result), clear_figure=True)

with step_tabs[4]:
    st.subheader("Step 9: Memory Update")
    st.markdown(
        """
**対応する関数**: `update_memory`  

resamplingで選ばれた候補をmemoryに追加します。  
次のstepでは、更新後のmemoryから新しい `q(x)` が作られ、このmemory周辺に候補が出やすくなります。
"""
    )
    st.pyplot(plot_step_9_memory_update(result), clear_figure=True)

# ------------------------------------------------------------
# 4. Function map and code hierarchy
# ------------------------------------------------------------

st.header("4. 手順名・中間データ・対応関数の対応表")
st.dataframe(step_summary_dataframe(result), use_container_width=True)

with st.expander("計算コードの階層構造"):
    st.markdown(
        """
```text
Layer 5: full simulation pipeline
  run_simulation
    ├─ run_uniform_step
    └─ run_one_adaptive_ris_step
         ├─ build_adaptive_proposal
         ├─ sample_candidates_from_proposal
         │    └─ sample_from_pdf
         │         └─ make_cdf_from_pdf
         ├─ evaluate_candidates
         │    └─ target_function
         ├─ compute_importance_weights
         ├─ normalize_resampling_probabilities
         ├─ resample_candidates
         ├─ estimate_integral_from_weights
         └─ update_memory
```
"""
    )

with st.expander("この教材で意図的に単純化している点"):
    st.markdown(
        """
- 実際のReSTIR DIでは、候補はlight sampleであり、BRDF、距離減衰、visibility、light pdfなどが入ります。
- このデモでは1D関数にして、RISの核である **候補生成 → 重み → resampling → memory更新** に絞っています。
- `normalized f(x)` は説明用の参照として描画しています。実際のアルゴリズムがこの形を事前に知っているわけではありません。
- `q(x) = uniform + KDE(memory)` は教育用のtoy proposalです。ReSTIR DIでは通常、Gaussian KDEでqを明示的に作るわけではありません。
- この版では、観察対象を **1 pixel / shading point = 1 reservoir** に固定しています。
"""
    )
