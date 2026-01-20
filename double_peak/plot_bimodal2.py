#!/usr/bin/env python3
# plot_bimodal2.py
# Usage examples:
#   python3 plot_bimodal2.py out.csv
#   python3 plot_bimodal2.py out.csv --qlo 0.01 --qhi 0.99
#   python3 plot_bimodal2.py out.csv --bins 200 --kde
#
# CSV columns required: i, thrash, cycles

import argparse
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def gaussian_kde_1d(x: np.ndarray, grid: np.ndarray, bw: float | None = None) -> np.ndarray:
    """
    Lightweight Gaussian KDE (no scipy). Returns density on 'grid'.
    bw: bandwidth in same unit as x.
    """
    x = x.astype(np.float64)
    n = x.size
    if n == 0:
        return np.zeros_like(grid, dtype=np.float64)

    # Silverman's rule of thumb
    if bw is None:
        std = np.std(x)
        if std == 0:
            std = 1.0
        bw = 1.06 * std * (n ** (-1.0 / 5.0))
        if bw <= 0:
            bw = 1.0

    # Vectorized KDE: sum exp(-(grid-x)^2 / (2*bw^2))
    # Beware: O(n*m). Use modest grid size.
    diffs = (grid[:, None] - x[None, :]) / bw
    dens = np.exp(-0.5 * diffs * diffs).sum(axis=1)
    dens /= (n * bw * np.sqrt(2.0 * np.pi))
    return dens


def describe_series(name: str, s: pd.Series) -> str:
    arr = s.to_numpy(dtype=np.float64)
    if arr.size == 0:
        return f"{name}: (no data)\n"

    q = np.quantile(arr, [0.0, 0.01, 0.05, 0.10, 0.25, 0.50, 0.75, 0.90, 0.95, 0.99, 1.0])
    iqr = q[6] - q[4]
    mean = float(np.mean(arr))
    std = float(np.std(arr, ddof=1)) if arr.size > 1 else 0.0

    # Robust width: MAD (median absolute deviation) scaled
    med = q[5]
    mad = float(np.median(np.abs(arr - med)))
    mad_scaled = 1.4826 * mad

    lines = []
    lines.append(f"{name}:")
    lines.append(f"  count={arr.size}")
    lines.append(f"  mean={mean:.3f}  std={std:.3f}")
    lines.append(f"  min={q[0]:.3f}  p01={q[1]:.3f}  p05={q[2]:.3f}  p10={q[3]:.3f}")
    lines.append(f"  p25={q[4]:.3f}  p50={q[5]:.3f}  p75={q[6]:.3f}  IQR={iqr:.3f}")
    lines.append(f"  p90={q[7]:.3f}  p95={q[8]:.3f}  p99={q[9]:.3f}  max={q[10]:.3f}")
    lines.append(f"  MAD*1.4826={mad_scaled:.3f}")
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", help="input CSV produced by bimodal_bench2 (columns: i,thrash,cycles)")
    ap.add_argument("--qlo", type=float, default=0.001, help="lower quantile cutoff (default 0.001)")
    ap.add_argument("--qhi", type=float, default=0.999, help="upper quantile cutoff (default 0.999)")
    ap.add_argument("--bins", type=int, default=200, help="histogram bins (default 200)")
    ap.add_argument("--kde", action="store_true", help="overlay simple KDE curves (no scipy)")
    ap.add_argument("--grid", type=int, default=400, help="KDE grid points (default 400)")
    args = ap.parse_args()

    df = pd.read_csv(args.csv)
    required = {"i", "thrash", "cycles"}
    if not required.issubset(df.columns):
        raise ValueError(f"CSV must have columns: {sorted(required)}")

    # Ensure types
    df["thrash"] = df["thrash"].astype(int)
    df["cycles"] = df["cycles"].astype(np.float64)

    # Quantile trimming (helps remove interrupt/SMI spikes)
    qlo = max(0.0, min(1.0, args.qlo))
    qhi = max(0.0, min(1.0, args.qhi))
    if qhi <= qlo:
        print("Error: qhi must be > qlo", file=sys.stderr)
        sys.exit(1)

    lo = df["cycles"].quantile(qlo)
    hi = df["cycles"].quantile(qhi)
    dff = df[(df["cycles"] >= lo) & (df["cycles"] <= hi)].copy()

    d0 = dff[dff["thrash"] == 0]["cycles"]
    d1 = dff[dff["thrash"] == 1]["cycles"]
    dall = dff["cycles"]

    # ---- Print stats ----
    print(f"Quantile trim: [{qlo:.3f}, {qhi:.3f}] -> keep cycles in [{lo:.3f}, {hi:.3f}]")
    print(describe_series("all", dall))
    print(describe_series("thrash=0", d0))
    print(describe_series("thrash=1", d1))

    # Also print separation metrics
    if len(d0) > 0 and len(d1) > 0:
        m0 = float(np.median(d0))
        m1 = float(np.median(d1))
        p900 = float(np.quantile(d0, 0.90))
        p101 = float(np.quantile(d1, 0.10))
        print("Separation quick checks:")
        print(f"  median(thrash=1) - median(thrash=0) = {m1 - m0:.3f} cycles")
        print(f"  p10(thrash=1) - p90(thrash=0)       = {p101 - p900:.3f} cycles  (positive => well separated)")
        print()

    # ---- Common binning range ----
    rmin = float(dall.min())
    rmax = float(dall.max())
    bins = max(40, args.bins)

    # ---- Plot 1: overlay hist ----
    plt.figure()
    plt.hist(dall, bins=bins, range=(rmin, rmax), alpha=0.35, label="all")
    plt.hist(d0,   bins=bins, range=(rmin, rmax), alpha=0.55, label="thrash=0")
    plt.hist(d1,   bins=bins, range=(rmin, rmax), alpha=0.55, label="thrash=1")
    plt.title("Cycle distribution (overlay)")
    plt.xlabel("cycles")
    plt.ylabel("count")
    plt.legend()
    plt.tight_layout()

    # Optional KDE overlay (density, rescaled to match histogram height roughly)
    if args.kde:
        grid = np.linspace(rmin, rmax, max(100, args.grid))
        # heuristic bandwidth: based on overall std
        std_all = float(np.std(dall.to_numpy()))
        bw = 0.02 * std_all if std_all > 0 else None  # a bit sharper than Silverman
        dens0 = gaussian_kde_1d(d0.to_numpy(), grid, bw=bw)
        dens1 = gaussian_kde_1d(d1.to_numpy(), grid, bw=bw)

        # rescale densities to histogram counts (approx)
        # count ≈ density * N * bin_width
        bin_width = (rmax - rmin) / bins
        scale0 = len(d0) * bin_width
        scale1 = len(d1) * bin_width

        ax = plt.gca()
        ax.plot(grid, dens0 * scale0, linewidth=1.2)
        ax.plot(grid, dens1 * scale1, linewidth=1.2)

    # ---- Plot 2: separate histograms ----
    fig, axs = plt.subplots(1, 2, figsize=(12, 4))
    axs[0].hist(d0, bins=bins, range=(rmin, rmax), alpha=0.8)
    axs[0].set_title("thrash=0 distribution")
    axs[0].set_xlabel("cycles")
    axs[0].set_ylabel("count")

    axs[1].hist(d1, bins=bins, range=(rmin, rmax), alpha=0.8)
    axs[1].set_title("thrash=1 distribution")
    axs[1].set_xlabel("cycles")
    axs[1].set_ylabel("count")

    plt.tight_layout()

    # ---- Plot 3: time series ----
    plt.figure()
    plt.plot(dff["i"], dff["cycles"], linewidth=0.4)
    plt.title("Cycles over time (clusters as bands)")
    plt.xlabel("sample index")
    plt.ylabel("cycles")
    plt.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()
