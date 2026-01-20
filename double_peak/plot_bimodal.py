# plot_bimodal.py
# Usage:
#   python3 plot_bimodal.py out.csv

import sys
import pandas as pd
import matplotlib.pyplot as plt

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 plot_bimodal.py out.csv")
        sys.exit(1)

    path = sys.argv[1]
    df = pd.read_csv(path)

    # Basic sanity
    if not {"i","thrash","cycles"}.issubset(df.columns):
        raise ValueError("CSV must have columns: i,thrash,cycles")

    # Optionally remove extreme outliers (interrupts etc.)
    # Keep it mild so you still see the real structure.
    lo = df["cycles"].quantile(0.001)
    hi = df["cycles"].quantile(0.999)
    dff = df[(df["cycles"] >= lo) & (df["cycles"] <= hi)].copy()

    # --------- Plot 1: Histogram (overall + grouped) ----------
    plt.figure()
    bins = 200

    # overall
    plt.hist(dff["cycles"], bins=bins, alpha=0.5, label="all")

    # grouped
    d0 = dff[dff["thrash"] == 0]["cycles"]
    d1 = dff[dff["thrash"] == 1]["cycles"]
    plt.hist(d0, bins=bins, alpha=0.5, label="thrash=0")
    plt.hist(d1, bins=bins, alpha=0.5, label="thrash=1")

    plt.xlabel("cycles")
    plt.ylabel("count")
    plt.title("Cycle distribution (look for bimodality)")
    plt.legend()
    plt.tight_layout()

    # --------- Plot 2: Time series ----------
    plt.figure()
    plt.plot(dff["i"], dff["cycles"], linewidth=0.5)
    plt.xlabel("sample index")
    plt.ylabel("cycles")
    plt.title("Cycles over time (clusters often visible)")
    plt.tight_layout()

    plt.show()

if __name__ == "__main__":
    main()
