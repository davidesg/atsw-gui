#!/usr/bin/env python3
"""
train_v2.py — Entrenamiento del estimador neuronal de coeficientes ARMA/SARIMA.

Mejoras sobre train.py:
  - Solo ruido Gaussiano iid (supuesto de modelos ARMA)
  - Outliers (5% de contaminación)
  - Coeficientes near-unit-root (φ ≥ 0.85)
  - Tamaños de muestra variables: 50–2000
  - MLP regresor con pérdida Huber
  - Exportación a ONNX

Uso: python train_v2.py [--epochs 200] [--samples 200000] [--lr 0.001]
"""

import numpy as np
from train import (simulate_arma_fast as _simulate_exact, sample_ar, sample_ma,
                   sample_sar, near_common_factor, theoretical_acf)  # 2026-09-30
import argparse
import time
import json
from collections import defaultdict

# =============================================================================
# 1. SIMULACIÓN MEJORADA
# =============================================================================

def simulate_arma(phi, theta, n, rng):
    """The exact ARMA recursion of train.py (2026-09-30): the vectorised
    version here was not recursive — an "AR(1)" came out as an MA(1)."""
    return _simulate_exact(phi, theta, n, rng)

def simulate_sarima_fast(phi, theta, Phi, Theta, s, d, D, n, rng):
    """SARIMA con innovaciones Gaussianas iid."""
    p, q, P, Q = len(phi), len(theta), len(Phi), len(Theta)
    total_n = n + d + D * s + 200

    # Convolve polynomials
    ar_reg = np.zeros(p + 1); ar_reg[0] = 1.0
    for i in range(p): ar_reg[i + 1] = -phi[i]
    ar_sea = np.zeros(P + 1); ar_sea[0] = 1.0
    for i in range(P): ar_sea[i + 1] = -Phi[i]
    ar_total = np.convolve(ar_reg, ar_sea)

    ma_reg = np.zeros(q + 1); ma_reg[0] = 1.0
    for i in range(q): ma_reg[i + 1] = -theta[i]
    ma_sea = np.zeros(Q + 1); ma_sea[0] = 1.0
    for i in range(Q): ma_sea[i + 1] = -Theta[i]
    ma_total = np.convolve(ma_reg, ma_sea)

    phi_total = -ar_total[1:] if len(ar_total) > 1 else np.array([], dtype=np.float64)
    theta_total = -ma_total[1:] if len(ma_total) > 1 else np.array([], dtype=np.float64)

    series = simulate_arma(phi_total, theta_total, total_n, rng)

    for _ in range(d): series = series[1:] - series[:-1]
    for _ in range(D): series = series[s:] - series[:-s]

    return series[-n:].astype(np.float32)


# =============================================================================
# 2. ACF/PACF (vectorizado)
# =============================================================================

def compute_acf(data, max_lags):
    n = len(data)
    acf = np.zeros(max_lags + 1, dtype=np.float32)
    acf[0] = 1.0
    mean = np.mean(data)
    variance = np.var(data)
    if variance < 1e-10:
        return acf
    data_c = data - mean
    for k in range(1, max_lags + 1):
        if k >= n:
            acf[k] = 0.0
        else:
            cov = np.dot(data_c[:n-k], data_c[k:])
            acf[k] = (cov / n) / variance            # cov/n, as the C (§1.1)
    return acf


def compute_pacf(acf, max_lags):
    pacf = np.zeros(max_lags + 1, dtype=np.float32)
    pacf[0] = 1.0
    if max_lags < 1:
        return pacf
    pacf[1] = acf[1]
    po = np.zeros(max_lags + 1, dtype=np.float64)
    pn = np.zeros(max_lags + 1, dtype=np.float64)
    po[1] = acf[1]
    for nn in range(2, max_lags + 1):
        num, den = acf[nn], 1.0
        for j in range(1, nn):
            num -= po[j] * acf[nn - j]
            den -= po[j] * acf[j]
        pn[nn] = 0.0 if abs(den) < 1e-10 else num / den
        pacf[nn] = pn[nn]
        for j in range(1, nn):
            pn[j] = po[j] - pn[nn] * po[nn - j]
        po[:nn+1], pn[:nn+1] = pn[:nn+1].copy(), po[:nn+1].copy()
    return pacf


# =============================================================================
# 3. MLP REGRESOR DE COEFICIENTES
# =============================================================================

def _relu(x): return np.maximum(0, x)
def _relu_deriv(x): return (x > 0).astype(np.float32)


class CoeffRegressor:
    """
    MLP: (ACF[1:41], PACF[1:41], p, q, P, Q) → coeficientes (φ, θ, Φ, Θ).

    Entrada: 40 + 40 + 4 = 84 dimensiones.
    Salida: 14 coeficientes (máx 4+4+2+1), con máscara de padding.
    """
    MAX_COEFFS = 14  # 4 AR + 4 MA + 2 SAR + 1 SMA + 3 padding

    def __init__(self, h1=256, h2=256, h3=128, rng=None):
        if rng is None:
            rng = np.random.RandomState(42)
        self.h1, self.h2, self.h3 = h1, h2, h3
        self.input_dim = 84  # 40 ACF + 40 PACF + 4 order flags

        s1 = np.sqrt(2.0 / self.input_dim)
        self.W1 = (rng.randn(h1, self.input_dim) * s1).astype(np.float32)
        self.b1 = np.zeros(h1, np.float32)
        s2 = np.sqrt(2.0 / h1)
        self.W2 = (rng.randn(h2, h1) * s2).astype(np.float32)
        self.b2 = np.zeros(h2, np.float32)
        s3 = np.sqrt(2.0 / h2)
        self.W3 = (rng.randn(h3, h2) * s3).astype(np.float32)
        self.b3 = np.zeros(h3, np.float32)
        s4 = np.sqrt(2.0 / h3)
        self.Wo = (rng.randn(self.MAX_COEFFS, h3) * s4).astype(np.float32)
        self.bo = np.zeros(self.MAX_COEFFS, np.float32)

        # Adam state
        self.m = {}
        self.v = {}
        self.t = 0
        for k in ['W1', 'b1', 'W2', 'b2', 'W3', 'b3', 'Wo', 'bo']:
            w = getattr(self, k)
            self.m[k] = np.zeros_like(w)
            self.v[k] = np.zeros_like(w)

        # Normalization
        self.fmean = np.zeros(self.input_dim, np.float32)
        self.fstd = np.ones(self.input_dim, np.float32)

    def fit_normalizer(self, X):
        self.fmean = X.mean(axis=0).astype(np.float32)
        self.fstd = X.std(axis=0).astype(np.float32)
        self.fstd[self.fstd < 1e-8] = 1.0

    def _norm(self, X):
        return (X - self.fmean) / np.maximum(self.fstd, 1e-8)

    def forward(self, X):
        Xn = self._norm(X)
        z1 = Xn @ self.W1.T + self.b1; a1 = _relu(z1)
        z2 = a1 @ self.W2.T + self.b2; a2 = _relu(z2)
        z3 = a2 @ self.W3.T + self.b3; a3 = _relu(z3)
        out = a3 @ self.Wo.T + self.bo
        cache = (Xn, z1, a1, z2, a2, z3, a3)
        return out, cache

    def _adam_update(self, name, grad, lr, beta1=0.9, beta2=0.999, eps=1e-8):
        self.m[name] = beta1 * self.m[name] + (1 - beta1) * grad
        self.v[name] = beta2 * self.v[name] + (1 - beta2) * grad * grad
        m_hat = self.m[name] / (1 - beta1 ** self.t)
        v_hat = self.v[name] / (1 - beta2 ** self.t)
        w = getattr(self, name)
        setattr(self, name, w - lr * m_hat / (np.sqrt(v_hat) + eps))

    @staticmethod
    def _huber_loss(pred, target, delta=0.1):
        diff = pred - target
        abs_diff = np.abs(diff)
        quad = np.minimum(abs_diff, delta)
        lin = abs_diff - quad
        return 0.5 * quad * quad + delta * lin

    @staticmethod
    def _huber_grad(pred, target, delta=0.1):
        diff = pred - target
        abs_diff = np.abs(diff)
        grad = np.where(abs_diff <= delta, diff, delta * np.sign(diff))
        return grad

    def backward(self, cache, X, y_true, y_mask, lr):
        Xn, z1, a1, z2, a2, z3, a3 = cache
        N = Xn.shape[0]

        # Output layer: only backprop through masked (valid) coefficients
        out = a3 @ self.Wo.T + self.bo
        grad_out = self._huber_grad(out, y_true, 0.1) * y_mask
        grad_out /= N

        self._adam_update('Wo', grad_out.T @ a3, lr)
        self._adam_update('bo', grad_out.sum(axis=0), lr)

        da3 = grad_out @ self.Wo
        dz3 = da3 * _relu_deriv(z3)
        self._adam_update('W3', dz3.T @ a2, lr)
        self._adam_update('b3', dz3.sum(axis=0), lr)

        da2 = dz3 @ self.W3
        dz2 = da2 * _relu_deriv(z2)
        self._adam_update('W2', dz2.T @ a1, lr)
        self._adam_update('b2', dz2.sum(axis=0), lr)

        da1 = dz2 @ self.W2
        dz1 = da1 * _relu_deriv(z1)
        self._adam_update('W1', dz1.T @ Xn, lr)
        self._adam_update('b1', dz1.sum(axis=0), lr)

    def train(self, X, y_true, y_mask, epochs=200, bs=512, lr=0.001):
        self.fit_normalizer(X)
        N = X.shape[0]
        for epoch in range(epochs):
            clr = lr * 0.5 * (1 + np.cos(np.pi * epoch / epochs))
            self.t += 1
            perm = np.random.permutation(N)
            tl = 0.0
            nb = 0
            for s in range(0, N, bs):
                e = min(s + bs, N)
                idx = perm[s:e]
                Xb, yb, mb = X[idx], y_true[idx], y_mask[idx]
                out, cache = self.forward(Xb)
                loss = self._huber_loss(out, yb, 0.1)
                # Only count loss on valid coefficients
                masked_loss = (loss * mb).sum() / max(mb.sum(), 1)
                tl += masked_loss * (e - s)
                nb += 1
                self.backward(cache, Xb, yb, mb, clr)
            if (epoch + 1) % 25 == 0:
                print(f"  Epoch {epoch+1}/{epochs} — loss: {tl/N:.6f}  lr: {clr:.6f}")

    def predict(self, X):
        out, _ = self.forward(X)
        return out

    def get_weights(self):
        return {
            'W1': self.W1, 'b1': self.b1,
            'W2': self.W2, 'b2': self.b2,
            'W3': self.W3, 'b3': self.b3,
            'Wo': self.Wo, 'bo': self.bo,
            'fmean': self.fmean, 'fstd': self.fstd,
            'h1': self.h1, 'h2': self.h2, 'h3': self.h3,
        }


# =============================================================================
# 4. GENERACIÓN DE DATASET
# =============================================================================

def build_input_vector(acf, pacf, max_lags, p, q, P, Q):
    """Construye vector de entrada: ACF[1:41] + PACF[1:41] + [p,q,P,Q]."""
    lags = min(max_lags, 40)
    vec = np.zeros(84, dtype=np.float32)
    vec[:lags] = acf[1:lags+1]
    vec[40:40+lags] = pacf[1:lags+1]
    vec[80] = p; vec[81] = q; vec[82] = P; vec[83] = Q
    return vec


def build_target_vector(phi, theta, Phi, Theta):
    """
    Construye vector de salida: [φ₁..φ₄, θ₁..θ₄, Φ₁..Φ₂, Θ₁, pad].
    Padding con ceros para órdenes no usados.
    """
    target = np.zeros(14, dtype=np.float32)
    mask = np.zeros(14, dtype=np.float32)

    for i, v in enumerate(phi[:4]):
        target[i] = v; mask[i] = 1.0
    for i, v in enumerate(theta[:4]):
        target[4 + i] = v; mask[4 + i] = 1.0
    for i, v in enumerate(Phi[:2]):
        target[8 + i] = v; mask[8 + i] = 1.0
    if len(Theta) > 0:
        target[10] = Theta[0]; mask[10] = 1.0
    # positions 11-13: reserved padding (mask=0)

    return target, mask


def generate_dataset(n_samples, rng):
    """Genera dataset de entrenamiento con variedad de modelos y condiciones."""
    X_list, y_list, mask_list = [], [], []

    # Pool de modelos (p, q, P, Q)
    model_pool = [
        # Pure AR
        (1,0,0,0), (2,0,0,0), (3,0,0,0), (4,0,0,0),
        # Pure MA
        (0,1,0,0), (0,2,0,0), (0,3,0,0), (0,4,0,0),
        # ARMA
        (1,1,0,0), (2,1,0,0), (1,2,0,0), (2,2,0,0),
        (3,1,0,0), (1,3,0,0), (3,2,0,0), (3,3,0,0),
        # Seasonal pure
        (0,0,1,0), (0,0,2,0), (0,0,0,1), (0,0,0,2),
        # Seasonal mixed
        (0,0,1,1), (1,0,1,0), (2,0,1,0), (0,1,1,0),
        (1,1,1,0), (1,1,0,1),
    ]

    n_per_model = max(1, n_samples // len(model_pool))

    count = 0
    for p, q, P, Q in model_pool:
        for _ in range(n_per_model):
            if count >= n_samples:
                break

            # Generate coefficients
            # AR and MA by their roots, inside the unit circle, no rescaling
            # (2026-09-30: the box plus sum|phi| rescale never produced the
            # cycles of economic series); near common factors rejected
            while True:
                phi = sample_ar(p, rng)
                theta = sample_ma(q, rng)
                if not near_common_factor(phi, theta):
                    break

            # Seasonal AR
            Phi = np.array([], dtype=np.float64)
            if P > 0:
                Phi = rng.uniform(-0.7, 0.7, P).astype(np.float64)
                if np.sum(np.abs(Phi)) >= 0.95:
                    Phi *= 0.8 / np.sum(np.abs(Phi))

            # Seasonal MA (positive, moderate)
            Theta = np.array([], dtype=np.float64)
            if Q > 0:
                Theta = rng.uniform(0.2, 0.7, Q).astype(np.float64)

            s = rng.choice([4, 12]) if (P > 0 or Q > 0) else 1
            nobs = rng.randint(50, 2000)

            # Generate series (Gaussian iid — assumption of ARMA models)
            if s == 1:
                series = simulate_arma(phi, theta, nobs, rng)
            else:
                series = simulate_sarima_fast(phi, theta, Phi, Theta, s, 0, 0, nobs, rng)

            # 5% outlier contamination
            if rng.random() < 0.05:
                n_outliers = max(1, int(nobs * 0.05))
                idx = rng.choice(nobs, n_outliers, replace=False)
                series[idx] += rng.normal(0, 5, n_outliers)

            # Compute ACF/PACF
            max_lags = min(40, max(10, nobs // 4))
            acf = compute_acf(series, max_lags)
            pacf = compute_pacf(acf, max_lags)

            # Build input/output
            X_vec = build_input_vector(acf, pacf, max_lags, p, q, P, Q)
            y_vec, m_vec = build_target_vector(phi, theta, Phi, Theta)

            X_list.append(X_vec)
            y_list.append(y_vec)
            mask_list.append(m_vec)
            count += 1

    X = np.array(X_list, dtype=np.float32)
    y = np.array(y_list, dtype=np.float32)
    m = np.array(mask_list, dtype=np.float32)

    return X, y, m


def generate_dataset_parallel(n_samples, seed=42, n_jobs=None, cache_dir="data"):
    """
    Genera dataset en paralelo usando multiprocessing.
    Si existe cache .npy, carga desde ahí.
    """
    import os
    from multiprocessing import Pool, cpu_count

    if n_jobs is None:
        n_jobs = min(cpu_count(), 8)

    # "_exactsim": the datasets cached before 2026-09-30 were simulated with the
    # non-recursive AR (an "AR" that was an MA); a cache must never bring them back
    cache_file = os.path.join(cache_dir, f"dataset_{n_samples}_{seed}_exactsim.npz")

    if os.path.exists(cache_file):
        print(f"  Loading cached dataset: {cache_file}")
        data = np.load(cache_file)
        return data['X'], data['y'], data['m']

    os.makedirs(cache_dir, exist_ok=True)

    print(f"  Generating {n_samples} samples with {n_jobs} workers...")
    chunk_size = n_samples // n_jobs

    args_list = []
    for i in range(n_jobs):
        size = chunk_size + (1 if i < n_samples % n_jobs else 0)
        args_list.append((size, seed + i * 1000))

    with Pool(n_jobs) as pool:
        results = pool.map(_generate_chunk_worker, args_list)

    X_chunks, y_chunks, m_chunks = zip(*results)
    X = np.concatenate(X_chunks).astype(np.float32)
    y = np.concatenate(y_chunks).astype(np.float32)
    m = np.concatenate(m_chunks).astype(np.float32)

    # Cache
    np.savez_compressed(cache_file, X=X, y=y, m=m)
    print(f"  Cached: {cache_file} ({os.path.getsize(cache_file)/1024/1024:.0f} MB)")

    return X, y, m


def _generate_chunk_worker(args):
    """Worker independiente: genera un chunk con su propia semilla."""
    chunk_size, seed = args
    rng = np.random.RandomState(seed)
    return generate_dataset(chunk_size, rng)


# =============================================================================
# 5. MAIN
# =============================================================================

def main():
    p = argparse.ArgumentParser(description="Coefficient Regressor Training")
    p.add_argument('--epochs', type=int, default=200)
    p.add_argument('--samples', type=int, default=200000)
    p.add_argument('--lr', type=float, default=0.001)
    p.add_argument('--seed', type=int, default=42)
    p.add_argument('-o', default='coeff_regressor_weights.json')
    a = p.parse_args()

    print("=" * 60)
    print("ART_19 — Coefficient Regressor (84→256→256→128→14)")
    print(f"Samples: {a.samples}  |  Epochs: {a.epochs}  |  LR: {a.lr}")
    print("=" * 60)

    rng = np.random.RandomState(a.seed)

    print(f"\n[1/3] Generating {a.samples} series...")
    t0 = time.time()
    X, y, mask = generate_dataset_parallel(a.samples, seed=a.seed)
    print(f"  {X.shape[0]} × {X.shape[1]} → {y.shape[1]}  ({time.time()-t0:.1f}s)")
    print(f"  Active coefficients: {mask.sum() / mask.size * 100:.1f}%")

    # Train/test split
    n_test = int(0.15 * len(X))
    idx = rng.permutation(len(X))
    Xtr, Xte = X[idx[:-n_test]], X[idx[-n_test:]]
    ytr, yte = y[idx[:-n_test]], y[idx[-n_test:]]
    mtr, mte = mask[idx[:-n_test]], mask[idx[-n_test:]]

    print(f"\n[2/3] Training MLP ({a.epochs} epochs)...")
    t0 = time.time()
    model = CoeffRegressor(256, 256, 128, rng)
    model.train(Xtr, ytr, mtr, epochs=a.epochs, lr=a.lr)
    print(f"  Training time: {time.time()-t0:.1f}s")

    # Evaluate
    print(f"\n[3/3] Evaluation...")
    preds = model.predict(Xte)

    # MAE per coefficient type
    mae_total = 0.0
    count_total = 0
    labels = ['φ', 'θ', 'Φ', 'Θ']
    offsets = [0, 4, 8, 10]
    limits = [4, 4, 2, 1]

    for name, off, lim in zip(labels, offsets, limits):
        active = mte[:, off:off+lim].sum() > 0
        if active.sum() > 0:
            diff = np.abs(preds[active][:, off:off+lim] - yte[active][:, off:off+lim])
            mask_active = mte[active][:, off:off+lim]
            mae = (diff * mask_active).sum() / max(mask_active.sum(), 1)
            print(f"  MAE {name}: {mae:.4f}")
            mae_total += (diff * mask_active).sum()
            count_total += mask_active.sum()

    overall_mae = mae_total / max(count_total, 1)
    print(f"  Overall MAE: {overall_mae:.4f}")

    # RMSE
    active_all = mte.sum(axis=1) > 0
    diff_all = (preds[active_all] - yte[active_all]) * mte[active_all]
    rmse = np.sqrt((diff_all**2).sum() / max(mte[active_all].sum(), 1))
    print(f"  Overall RMSE: {rmse:.4f}")

    # Save weights
    w = model.get_weights()
    wj = {}
    for k, v in w.items():
        if isinstance(v, np.ndarray):
            wj[k] = v.tolist()
        else:
            wj[k] = v
    with open(a.o, 'w') as f:
        json.dump(wj, f)
    print(f"\n  Saved: {a.o}")
    print(f"  File size: {__import__('os').path.getsize(a.o) / 1024:.0f} KB")


if __name__ == '__main__':
    main()
