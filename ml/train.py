#!/usr/bin/env python3
"""
train.py — MLP clasificador ARMA con Adam, 3 capas, 100K muestras.
NumPy puro. Normalización Z-score. LR con cosine annealing.

Uso: python train.py [--epochs 150] [--samples 100000] [--lr 0.001]
"""

import numpy as np
import argparse, time, json
from collections import defaultdict

# =============================================================================
# 1. SIMULACIÓN (vectorizada)
# =============================================================================

def simulate_arma_fast(phi, theta, n, rng):
    p, q = len(phi), len(theta)
    innov = rng.normal(0, 1, n).astype(np.float64)
    series = innov.copy()
    for j in range(q):
        if j < n - 1:
            series[j+1:] -= theta[j] * innov[:n-j-1]
    for j in range(p):
        if j < n - 1:
            series[j+1:] += phi[j] * series[:n-j-1]
    return series.astype(np.float32)

def simulate_sarima_fast(phi, theta, Phi, Theta, s, d, D, n, rng):
    p, q, P, Q = len(phi), len(theta), len(Phi), len(Theta)
    total_n = n + d + D * s + 200
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
    pt = len(ar_total) - 1; qt = len(ma_total) - 1
    pt_arr = -ar_total[1:] if pt > 0 else np.array([], dtype=np.float64)
    qt_arr = -ma_total[1:] if qt > 0 else np.array([], dtype=np.float64)
    series = simulate_arma_fast(pt_arr, qt_arr, total_n, rng)
    for _ in range(d): series = series[1:] - series[:-1]
    for _ in range(D): series = series[s:] - series[:-s]
    return series[-n:]

# =============================================================================
# 2. ACF/PACF
# =============================================================================

def compute_acf(data, max_lags):
    n = len(data)
    acf = np.zeros(max_lags + 1, dtype=np.float32)
    acf[0] = 1.0
    mean = np.mean(data); variance = np.var(data)
    if variance < 1e-10: return acf
    data_c = data - mean
    for k in range(1, max_lags + 1):
        if k >= n: acf[k] = 0.0
        else:
            cov = np.dot(data_c[:n-k], data_c[k:])
            acf[k] = np.clip((cov / (n - k)) / variance, -1.0, 1.0)
    return acf

def compute_pacf(acf, max_lags):
    pacf = np.zeros(max_lags + 1, dtype=np.float32); pacf[0] = 1.0
    if max_lags < 1: return pacf
    pacf[1] = acf[1]
    po = np.zeros(max_lags + 1, dtype=np.float64)
    pn = np.zeros(max_lags + 1, dtype=np.float64)
    po[1] = acf[1]
    for nn in range(2, max_lags + 1):
        num, den = acf[nn], 1.0
        for j in range(1, nn):
            num -= po[j] * acf[nn - j]; den -= po[j] * acf[j]
        pn[nn] = 0.0 if abs(den) < 1e-10 else num / den
        pacf[nn] = pn[nn]
        for j in range(1, nn): pn[j] = po[j] - pn[nn] * po[nn - j]
        po[:nn+1], pn[:nn+1] = pn[:nn+1], po[:nn+1]
    return pacf

# =============================================================================
# 3. FEATURES (18 dims, matching C exactly)
# =============================================================================

THR = 1.96 / np.sqrt(200)

def gw(lag, d): return 1.0 if lag <= 0 else d ** (lag - 1)

def extract_pattern_features(acf, pacf, lags, s):
    # cutting lags
    acl = pcl = 0
    for i in range(1, lags):
        if not acl and abs(acf[i]) < THR and i+2 <= lags and abs(acf[i+1]) < THR and abs(acf[i+2]) < THR:
            acl = i
        if not pcl and abs(pacf[i]) < THR and i+2 <= lags and abs(pacf[i+1]) < THR and abs(pacf[i+2]) < THR:
            pcl = i
        if acl and pcl: break

    adr = pdr = adw = pdw = 0.0
    for i in range(2, min(8, lags)+1):
        if abs(acf[i-1]) > 1e-6:
            w = gw(i, 0.9); adr += abs(float(acf[i])/float(acf[i-1])) * w; adw += w
        if abs(pacf[i-1]) > 1e-6:
            w = gw(i, 0.9); pdr += abs(float(pacf[i])/float(pacf[i-1])) * w; pdw += w
    adr = adr/adw if adw > 0 else 0.0
    pdr = pdr/pdw if pdw > 0 else 0.0

    sas = sps = sc = 0.0
    if s > 1:
        for i in range(1, min(5, lags//s)+1):
            ml = i*s
            if ml > lags: break
            sas += abs(acf[ml]); sps += abs(pacf[ml]); sc += 1.0
            if ml-1 >= 1: sas += abs(acf[ml-1])*0.3; sps += abs(pacf[ml-1])*0.3
            if ml+1 <= lags: sas += abs(acf[ml+1])*0.3; sps += abs(pacf[ml+1])*0.3
        if sc > 0: sas /= sc; sps /= sc

    v = np.zeros(30, dtype=np.float32); idx = 0
    for i in range(1,13): v[idx] = float(acf[i]) if i <= lags else 0.0; idx += 1
    for i in range(1,13): v[idx] = float(pacf[i]) if i <= lags else 0.0; idx += 1
    v[idx] = float(acl); idx += 1
    v[idx] = float(pcl); idx += 1
    v[idx] = float(adr); idx += 1
    v[idx] = float(pdr); idx += 1
    v[idx] = float(sas); idx += 1
    v[idx] = float(sps); idx += 1
    return v

# =============================================================================
# 4. DATASET (100K)
# =============================================================================

def generate_dataset(n, rng):
    X, yp, yq, yP, yQ = [], [], [], [], []
    pool = [
        (1,0,0,0),(2,0,0,0),(3,0,0,0),(4,0,0,0),
        (0,1,0,0),(0,2,0,0),(0,3,0,0),(0,4,0,0),
        (1,1,0,0),(2,1,0,0),(1,2,0,0),(2,2,0,0),
        (3,1,0,0),(1,3,0,0),(3,2,0,0),(3,3,0,0),
        # AR(2) complex roots (pseudo-cyclical) — extra weight
        (2,0,0,0),(2,0,0,0),
        (0,0,1,0),(0,0,2,0),(0,0,0,1),(0,0,0,2),
        (0,0,1,1),(1,0,1,0),(2,0,1,0),
        (0,1,1,0),(1,1,1,0),(1,1,0,1),
    ]
    npo = (n // len(pool)) + 1
    t = 0
    for p,q,P,Q in pool:
        for _ in range(npo):
            if t >= n: break
            if p==2 and rng.random()<0.6:
                # Force complex roots: φ₁² + 4φ₂ < 0 (damped sine ACF)
                phi1 = rng.uniform(-0.6,0.6)
                phi2 = rng.uniform(-0.8, -0.15 - phi1*phi1/4.1)
                phi = np.array([phi1, phi2], dtype=np.float64)
            else:
                phi = rng.uniform(-0.7,0.7,p).astype(np.float64) if p>0 else np.array([],dtype=np.float64)
            if p>0 and np.sum(np.abs(phi))>=0.95: phi *= 0.8/np.sum(np.abs(phi))
            theta = rng.uniform(-0.7,0.7,q).astype(np.float64) if q>0 else np.array([],dtype=np.float64)
            if q>0 and np.sum(np.abs(theta))>=0.95: theta *= 0.8/np.sum(np.abs(theta))
            Phi = np.array([],dtype=np.float64)
            if P>0:
                Phi = rng.uniform(-0.7,0.7,P).astype(np.float64)
                if np.sum(np.abs(Phi))>=0.95: Phi *= 0.8/np.sum(np.abs(Phi))
            Theta = rng.uniform(0.2,0.7,Q).astype(np.float64) if Q>0 else np.array([],dtype=np.float64)
            s = rng.choice([4,12]) if (P>0 or Q>0) else 1
            nobs = rng.randint(150,400)
            if s==1:
                ser = simulate_arma_fast(phi,theta,nobs,rng)
            else:
                ser = simulate_sarima_fast(phi,theta,Phi,Theta,s,0,0,nobs,rng)
            lags = min(40, max(10, nobs//4))
            acf = compute_acf(ser, lags); pacf = compute_pacf(acf, lags)
            X.append(extract_pattern_features(acf,pacf,lags,s))
            yp.append(p); yq.append(q); yP.append(P); yQ.append(Q)
            t += 1
    return np.array(X,dtype=np.float32), np.array(yp), np.array(yq), np.array(yP), np.array(yQ)

# =============================================================================
# 5. MLP CON ADAM, 3 CAPAS
# =============================================================================

def _relu(x): return np.maximum(0, x)
def _relu_deriv(x): return (x > 0).astype(np.float32)

def _softmax(x):
    e = np.exp(x - np.max(x, axis=1, keepdims=True))
    return e / np.maximum(np.sum(e, axis=1, keepdims=True), 1e-12)

class AdamMLP:
    def __init__(self, input_dim=30, h1=128, h2=128, h3=64, rng=None):
        if rng is None: rng = np.random.RandomState(42)
        self.id = input_dim; self.h1 = h1; self.h2 = h2; self.h3 = h3
        self.fmean = np.zeros(input_dim, np.float32)
        self.fstd  = np.ones(input_dim, np.float32)

        s1 = np.sqrt(2.0/input_dim)
        self.W1 = (rng.randn(h1,input_dim)*s1).astype(np.float32); self.b1 = np.zeros(h1,np.float32)
        s2 = np.sqrt(2.0/h1)
        self.W2 = (rng.randn(h2,h1)*s2).astype(np.float32); self.b2 = np.zeros(h2,np.float32)
        s3 = np.sqrt(2.0/h2)
        self.W3 = (rng.randn(h3,h2)*s3).astype(np.float32); self.b3 = np.zeros(h3,np.float32)
        s4 = np.sqrt(2.0/h3)
        self.Wp = (rng.randn(6,h3)*s4).astype(np.float32); self.bp = np.zeros(6,np.float32)
        self.Wq = (rng.randn(6,h3)*s4).astype(np.float32); self.bq = np.zeros(6,np.float32)
        self.WP = (rng.randn(3,h3)*s4).astype(np.float32); self.bP = np.zeros(3,np.float32)
        self.WQ = (rng.randn(3,h3)*s4).astype(np.float32); self.bQ = np.zeros(3,np.float32)

        # Adam state
        self.m = {}; self.v = {}; self.t = 0
        for k in ['W1','b1','W2','b2','W3','b3','Wp','bp','Wq','bq','WP','bP','WQ','bQ']:
            w = getattr(self, k)
            self.m[k] = np.zeros_like(w)
            self.v[k] = np.zeros_like(w)

    def _norm(self, X): return (X - self.fmean) / np.maximum(self.fstd, 1e-8)

    def fit_normalizer(self, X):
        self.fmean = X.mean(axis=0).astype(np.float32)
        self.fstd  = X.std(axis=0).astype(np.float32)
        self.fstd[self.fstd < 1e-8] = 1.0

    def forward(self, X):
        Xn = self._norm(X)
        z1 = Xn @ self.W1.T + self.b1; a1 = _relu(z1)
        z2 = a1 @ self.W2.T + self.b2; a2 = _relu(z2)
        z3 = a2 @ self.W3.T + self.b3; a3 = _relu(z3)
        zp = a3 @ self.Wp.T + self.bp; zq = a3 @ self.Wq.T + self.bq
        zP = a3 @ self.WP.T + self.bP; zQ = a3 @ self.WQ.T + self.bQ
        pp = _softmax(zp); pq = _softmax(zq); pP = _softmax(zP); pQ = _softmax(zQ)
        cache = (Xn, z1,a1, z2,a2, z3,a3, zp,zq,zP,zQ)
        return pp,pq,pP,pQ,cache

    def _adam_update(self, name, grad, lr, beta1=0.9, beta2=0.999, eps=1e-8):
        self.m[name] = beta1 * self.m[name] + (1-beta1) * grad
        self.v[name] = beta2 * self.v[name] + (1-beta2) * grad * grad
        m_hat = self.m[name] / (1 - beta1**(self.t))
        v_hat = self.v[name] / (1 - beta2**(self.t))
        w = getattr(self, name)
        setattr(self, name, w - lr * m_hat / (np.sqrt(v_hat) + eps))

    def backward(self, cache, yp, yq, yP, yQ, lr):
        Xn,z1,a1,z2,a2,z3,a3,zp,zq,zP,zQ = cache
        N = Xn.shape[0]

        pp = _softmax(zp); pp[np.arange(N),yp] -= 1; pp /= N
        pq = _softmax(zq); pq[np.arange(N),yq] -= 1; pq /= N
        pP = _softmax(zP); pP[np.arange(N),yP] -= 1; pP /= N
        pQ = _softmax(zQ); pQ[np.arange(N),yQ] -= 1; pQ /= N

        self._adam_update('Wp', pp.T @ a3, lr); self._adam_update('bp', pp.sum(axis=0), lr)
        self._adam_update('Wq', pq.T @ a3, lr); self._adam_update('bq', pq.sum(axis=0), lr)
        self._adam_update('WP', pP.T @ a3, lr); self._adam_update('bP', pP.sum(axis=0), lr)
        self._adam_update('WQ', pQ.T @ a3, lr); self._adam_update('bQ', pQ.sum(axis=0), lr)

        da3 = pp @ self.Wp + pq @ self.Wq + pP @ self.WP + pQ @ self.WQ
        dz3 = da3 * _relu_deriv(z3)
        self._adam_update('W3', dz3.T @ a2, lr); self._adam_update('b3', dz3.sum(axis=0), lr)

        da2 = dz3 @ self.W3; dz2 = da2 * _relu_deriv(z2)
        self._adam_update('W2', dz2.T @ a1, lr); self._adam_update('b2', dz2.sum(axis=0), lr)

        da1 = dz2 @ self.W2; dz1 = da1 * _relu_deriv(z1)
        self._adam_update('W1', dz1.T @ Xn, lr); self._adam_update('b1', dz1.sum(axis=0), lr)

    def train(self, X, yp, yq, yP, yQ, epochs=150, bs=512, lr=0.001):
        self.fit_normalizer(X)
        N = X.shape[0]
        for epoch in range(epochs):
            # Cosine annealing
            clr = lr * 0.5 * (1 + np.cos(np.pi * epoch / epochs))
            self.t += 1
            perm = np.random.permutation(N)
            tl = 0.0
            for s in range(0, N, bs):
                e = min(s+bs, N); idx = perm[s:e]; Xb = X[idx]
                pp,pq,pP,pQ,cache = self.forward(Xb)
                loss = (_xent(pp,yp[idx]) + _xent(pq,yq[idx]) +
                        _xent(pP,yP[idx]) + _xent(pQ,yQ[idx]))
                tl += loss * (e-s)
                self.backward(cache, yp[idx],yq[idx],yP[idx],yQ[idx], clr)
            if (epoch+1) % 20 == 0:
                print(f"  Epoch {epoch+1}/{epochs} — loss: {tl/N:.4f}  lr: {clr:.6f}")

    def predict(self, X):
        pp,pq,pP,pQ,_ = self.forward(X)
        return (np.argmax(pp,1), np.argmax(pq,1), np.argmax(pP,1), np.argmax(pQ,1))

    def get_weights(self):
        return {
            'fc1_weight':self.W1,'fc1_bias':self.b1,
            'fc2_weight':self.W2,'fc2_bias':self.b2,
            'fc3_weight':self.W3,'fc3_bias':self.b3,
            'head_p_weight':self.Wp,'head_p_bias':self.bp,
            'head_q_weight':self.Wq,'head_q_bias':self.bq,
            'head_P_weight':self.WP,'head_P_bias':self.bP,
            'head_Q_weight':self.WQ,'head_Q_bias':self.bQ,
            'norm_mean':self.fmean,'norm_std':self.fstd,
            'h1':self.h1,'h2':self.h2,'h3':self.h3,
        }

def _xent(probs, y):
    eps = 1e-12; N = probs.shape[0]
    return -np.mean(np.log(probs[np.arange(N), y] + eps))

# =============================================================================
# 6. MAIN
# =============================================================================

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--epochs', type=int, default=150)
    p.add_argument('--samples', type=int, default=100000)
    p.add_argument('--lr', type=float, default=0.001)
    p.add_argument('-o', default='model_weights.json')
    a = p.parse_args()

    print("="*60)
    print("ART_18 — MLP Adam 30→128→128→64, 100K muestras")
    print("="*60)

    print(f"\n[1/3] Generando {a.samples} series...")
    t0 = time.time(); rng = np.random.RandomState(42)
    X,yp,yq,yP,yQ = generate_dataset(a.samples, rng)
    print(f"  {X.shape[0]} × {X.shape[1]}  ({time.time()-t0:.1f}s)")

    for nm,y in [('p',yp),('q',yq),('P',yP),('Q',yQ)]:
        u,c = np.unique(y,return_counts=True)
        print(f"  {nm}: {', '.join(f'{k}:{v}' for k,v in zip(u,c))}")

    nt = int(0.2*len(X)); idx = rng.permutation(len(X))
    Xtr,Xte = X[idx[:-nt]],X[idx[-nt:]]
    ytr = (yp[idx[:-nt]],yq[idx[:-nt]],yP[idx[:-nt]],yQ[idx[:-nt]])
    yte = (yp[idx[-nt:]],yq[idx[-nt:]],yP[idx[-nt:]],yQ[idx[-nt:]])

    print(f"\n[2/3] Adam 30→128→128→64, {a.epochs} épocas...")
    t0 = time.time()
    mlp = AdamMLP(30,128,128,64,rng)
    mlp.train(Xtr,*ytr,epochs=a.epochs,lr=a.lr)
    print(f"  {time.time()-t0:.1f}s")

    preds = mlp.predict(Xte)
    ap = (preds[0]==yte[0]).mean(); aq = (preds[1]==yte[1]).mean()
    aP = (preds[2]==yte[2]).mean(); aQ = (preds[3]==yte[3]).mean()
    aa = ((preds[0]==yte[0])&(preds[1]==yte[1])&(preds[2]==yte[2])&(preds[3]==yte[3])).mean()

    print(f"\n[3/3] Resultados:")
    print(f"  p={ap:.3f}  q={aq:.3f}  P={aP:.3f}  Q={aQ:.3f}  all={aa:.3f}")

    conf = defaultdict(int)
    for i in range(len(yte[0])):
        tk = (yte[0][i],yte[1][i],yte[2][i],yte[3][i])
        pk = (preds[0][i],preds[1][i],preds[2][i],preds[3][i])
        conf[(tk,pk)] += 1
    errs = [(k,v) for k,v in conf.items() if k[0]!=k[1]]
    errs.sort(key=lambda x:-x[1])
    print("  Errores top 8:")
    for (t,pd),c in errs[:8]: print(f"    True {t} → Pred {pd}: {c}")

    w = mlp.get_weights()
    wj = {}
    for k,v in w.items():
        if isinstance(v, np.ndarray): wj[k] = v.tolist()
        else: wj[k] = v
    with open(a.o,'w') as f: json.dump(wj, f)
    print(f"\n  Guardado: {a.o}")

if __name__=='__main__': main()
