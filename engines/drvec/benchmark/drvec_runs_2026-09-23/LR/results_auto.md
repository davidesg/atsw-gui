# drvec external validation — Lütkepohl e1/e3 and Rao tables 1–7

Spec everywhere: K=2 (ca.jo) = 1 lagged difference = drvec `p=2`; ecdet='const' (restricted constant) = drvec `-case 2`; q=0 for the rank.

Johansen = independent numpy RRR (identical to ca.jo and statsmodels VECM(ci) to 1e-9, see johansen_ref.py). drvec LR is the λ-max form 2[L(r+1)−L(r)] and stops at r=M−1.


## e1  (M=3, n=92, reference rank r=1; Y1 = lcons)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=90) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 59.553 | 58.652 | 721.199 | 713.960 | grad/18 |
| 1 | 9.368 | 6.021 | 750.525 | 743.737 | grad/71 |
| 2 | 6.082 | — | 753.535 | 748.421 | tc3/89 |


Johansen trace (= ca.jo): [75.003, 15.450, 6.082]

drvec verdicts: r=0: reject H0 at 1%; r=1: H0 not rejected

### Fit at r=1 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | 743.737 (cond.) | — | closed form | 0 | 0 | 0 | [0.2207] |
| drvec q=0 | 750.5247084752 | 20 | grad/71 — clean convergence, on the scaled gradient | 0.007192 | 0.01224 | 0.01308 | [0.2216] |
| drvec q=0 multistart 20 | 750.5247084752 | 20 | grad/71,grad/248,grad/252,tc3/31,grad/3,grad/8,grad/6,grad/98,tc3/36,tc3/16,tc3/42,grad/19,grad/29,tc3/17,tc3/19,grad/9,tc3/19 — NOT a convergence: the line search failed to improve | 0.007192 | 0.01224 | 0.01308 | [0.2216] |
| drvec q=0 -seedjoh | 750.5247084752 | 20 | grad/61 — clean convergence, on the scaled gradient | 0.007192 | 0.01224 | 0.01308 | [0.2216] |
| drvec q=1 (default MA, marow) | 754.8732639196 | 23 | grad/136 — clean convergence, on the scaled gradient | 0.07561 | 0.2485 | 0.2485 | [0.2018] |

Johansen β' (rows = relations):
```
[[-0.0352 -0.9279  1.    ]]
```
Johansen α (M×r):
```
[[ 0.0474]
 [-0.2259]
 [-0.2941]]
```
drvec q=0 β':
```
[[-0.028  -0.9335  1.    ]]
```
drvec q=0 α:
```
[[ 0.0421]
 [-0.233 ]
 [-0.3063]]
```

## e3  (M=4, n=136, reference rank r=1; Y1 = lM1)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=134) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 56.713 | 47.261 | 1967.130 | 1951.582 | grad/26 |
| 1 | 19.549 | 18.624 | 1990.760 | 1979.938 | grad/76 |
| 2 | 12.228 | 7.979 | 2000.072 | 1989.713 | grad/92 |
| 3 | 2.522 | — | 2004.062 | 1995.826 | grad/84 |


Johansen trace (= ca.jo): [91.011, 34.299, 14.749, 2.522]

drvec verdicts: r=0: reject H0 at 1%; r=1: H0 not rejected; r=2: H0 not rejected

### Fit at r=1 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | 1979.938 (cond.) | — | closed form | 0 | 0 | 0 | [2.7647] |
| drvec q=0 | 1990.7601702020 | 33 | grad/76 — clean convergence, on the scaled gradient | 2.759 | 0.0114 | 0.05604 | [2.4955] |
| drvec q=0 multistart 20 | 1990.7601702021 | 33 | grad/76,grad/70,grad/74,grad/76,grad/373,tc3/446,tc3/11,grad/93,tc3/41,grad/137,tc3/29,tc3/36,grad/16,tc3/19,grad/54 — clean convergence, on the scaled gradient | 2.759 | 0.0114 | 0.05603 | [2.4955] |
| drvec q=0 -seedjoh | 1990.7601702021 | 33 | grad/74 — clean convergence, on the scaled gradient | 2.759 | 0.0114 | 0.05603 | [2.4955] |
| drvec q=1 (default MA, marow) | 1997.8408915899 | 37 | tc3/108 — NOT a convergence: the line search failed to improve | 3.887 | 0.04313 | 0.3743 | [2.4170] |

Johansen β' (rows = relations):
```
[[-0.5    11.6167 -6.0931  1.    ]]
```
Johansen α (M×r):
```
[[-0.0375]
 [ 0.0012]
 [ 0.0014]
 [-0.0105]]
```
drvec q=0 β':
```
[[-0.5301  8.8581 -3.5251  1.    ]]
```
drvec q=0 α:
```
[[-0.0489]
 [ 0.0008]
 [ 0.0014]
 [-0.0137]]
```

## rao1  (M=5, n=143, reference rank r=2; Y1 = rgnp, k)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=141) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 50.318 | 44.776 | 1712.743 | 1700.495 | grad/109 |
| 1 | 32.422 | 24.250 | 1735.131 | 1725.654 | tc3/160 |
| 2 | 10.774 | 10.169 | 1747.256 | 1741.865 | tc3/280 |
| 3 | 8.018 | 6.364 | 1752.341 | 1747.252 | grad/189 |
| 4 | 5.666 | — | 1755.523 | 1751.261 | grad/139 |


Johansen trace (= ca.jo): [107.198, 56.880, 24.458, 13.684, 5.666]

drvec verdicts: r=0: reject H0 at 1%; r=1: H0 not rejected; r=2: H0 not rejected; r=3: H0 not rejected

### Fit at r=2 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | 1741.865 (cond.) | — | closed form | 0 | 0 | 0 | [13.2487, 1.6678] |
| drvec q=0 | 1747.2560542898 | 57 | tc3/280 — NOT a convergence: the line search failed to improve | 0.8523 | 0.2485 | 0.2502 | [11.8003, 0.9271] |
| drvec q=0 multistart 20 | 1747.2560542898 | 57 | tc3/280,grad/399,tc3/269,tc3/266,tc3/284,tc3/192,tc3/113,tc3/286,tc3/21,tc3/255,tc3/418,tc3/221,tc3/28,iteration limit reached/500,tc3/108 — NOT a convergence: the line search failed to improve | 0.8523 | 0.2485 | 0.2502 | [11.8003, 0.9271] |
| drvec q=0 -seedjoh | 1744.9477904777 | 57 | tc3/59 — NOT a convergence: the line search failed to improve | 3.744 | 0.2933 | 0.2933 | [20.4511, 1.0773] |
| drvec q=1 (default MA, marow) | 1766.8936595469 | 67 | iteration limit reached/500 — NOT a convergence: the optimiser gave up | 1.994 | 0.09888 | 0.09888 | [10.6103, 0.7492] |

Johansen β' (rows = relations):
```
[[-3.7119 -2.1282  2.8769  1.      0.    ]
 [-1.7436 -0.7747  1.1109  0.      1.    ]]
```
Johansen α (M×r):
```
[[ 0.0001 -0.0078]
 [-0.0145  0.3114]
 [-0.0005 -0.0299]
 [ 0.0088  0.0021]
 [ 0.0009 -0.0136]]
```
drvec q=0 β':
```
[[-2.8596 -1.6697  2.0997  1.      0.    ]
 [-1.4397 -0.4421  0.6253  0.      1.    ]]
```
drvec q=0 α:
```
[[ 0.0004 -0.012 ]
 [-0.0234  0.5599]
 [ 0.0089 -0.0691]
 [ 0.013  -0.0022]
 [ 0.0036 -0.0348]]
```

## rao2  (M=4, n=144, reference rank r=3; Y1 = mbp, nm1m2p, m1p)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=142) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 49.433 | 31.330 | 2303.195 | 2289.821 | grad/36 |
| 1 | 32.765 | 30.634 | 2318.860 | 2314.538 | grad/100 |
| 2 | 23.291 | 11.891 | 2334.177 | 2330.920 | grad/111 |
| 3 | 4.865 | — | 2340.123 | 2342.566 | tc3/230 |


Johansen trace (= ca.jo): [110.354, 60.921, 28.156, 4.865]

drvec verdicts: r=0: reject H0 at 5%; r=1: reject H0 at 1%; r=2: H0 not rejected

### Fit at r=3 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | 2342.566 (cond.) | — | closed form | 0 | 0 | 0 | [9.9105, -8.8250, 20.3314] |
| drvec q=0 | 2340.1228389721 | 43 | tc3/230 — NOT a convergence: the line search failed to improve | 0.418 | 0.03524 | 0.03524 | [12.1712, -12.1140, 18.0748] |
| drvec q=0 multistart 20 | 2340.1228389721 | 43 | tc3/230,tc3/28 — NOT a convergence: the line search failed to improve | 0.418 | 0.03524 | 0.03524 | [12.1712, -12.1140, 18.0748] |
| drvec q=0 -seedjoh | 2341.1949106760 | 43 | step/240 — stopped on steptol, NOT on the gradient | 3.524 | 0.03474 | 0.05612 | [-7.4693, 4.9358, -6.8515] |
| drvec q=1 (default MA, marow) | 2351.1324436026 | 55 | tc3/162 — NOT a convergence: the line search failed to improve | 1.904 | 0.2097 | 0.2654 | [2.7419, -3.4407, 6.0853] |

Johansen β' (rows = relations):
```
[[ 0.5909  1.     -0.      0.    ]
 [-2.1042 -0.      1.      0.    ]
 [ 1.8183  0.      0.      1.    ]]
```
Johansen α (M×r):
```
[[-0.0862 -0.0237  0.0354]
 [-0.1908 -0.0927  0.0676]
 [-0.0665 -0.0137  0.026 ]
 [-0.1794 -0.1187  0.0533]]
```
drvec q=0 β':
```
[[ 0.8763  1.      0.      0.    ]
 [-2.5223  0.      1.      0.    ]
 [ 1.5217  0.      0.      1.    ]]
```
drvec q=0 α:
```
[[-0.0845 -0.0336  0.0427]
 [-0.1556 -0.0802  0.0704]
 [-0.0723 -0.0286  0.0335]
 [-0.1902 -0.1296  0.0649]]
```

## rao3  (M=6, n=98, reference rank r=3; Y1 = dd792, dd883, lc)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=96) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 94.059 | 31.266 | 897.426 | 887.036 | grad/199 |
| 1 | 85.305 | 38.535 | 913.059 | 934.065 | tc3/15 |
| 2 | 44.206 | 147.595 | 932.326 | 976.718 | tc3/42 |
| 3 | 15.400 | 20.778 | 1006.124 | 998.821 | grad/274 |
| 4 | 6.120 | — | 1016.513 | 1006.521 | grad/155 |
| 5 | 1.673 | — | 1015.510 | 1009.581 | grad/206 |


Johansen trace (= ca.jo): [246.762, 152.703, 67.398, 23.193, 7.793, 1.673]

drvec verdicts: r=0: H0 not rejected; r=1: reject H0 at 5%; r=2: reject H0 at 1%; r=3: reject H0 at 10%

### Fit at r=3 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | 998.821 (cond.) | — | closed form | 0 | 0 | 0 | [-0.1343, -19.7809, -0.5371] |
| drvec q=0 | 1006.1238007386 | 86 | grad/274 — clean convergence, on the scaled gradient | 45.55 | 0.5375 | 0.5375 | [-0.0078, -15.2796, -0.2862] |
| drvec q=0 multistart 20 | 1006.1238007386 | 86 | grad/274,grad/12,grad/165,grad/455,tc3/20,grad/10,grad/3,tc3/355,tc3/20,tc3/59,tc3/16,tc3/15,tc3/11 — NOT a convergence: the line search failed to improve | 45.55 | 0.5375 | 0.5375 | [-0.0078, -15.2796, -0.2862] |
| drvec q=0 -seedjoh | 998.7409018188 | 86 | tc3/2 — NOT a convergence: the line search failed to improve | 29.78 | 0.5757 | 0.5757 | [-0.3802, -27.1464, -0.7237] |
| drvec q=1 (default MA, marow) | 975.3679622428 | 104 | tc3/32 — NOT a convergence: the line search failed to improve | 65.36 | 0.6402 | 1.861 | [0.0121, -12.9236, -0.0941] |

Johansen β' (rows = relations):
```
[[  0.0115  -0.0205  -1.6303   1.      -0.       0.    ]
 [  0.1395  -1.6726 -65.4251  -0.       1.      -0.    ]
 [ -0.9058  -0.1123  -3.5958   0.       0.       1.    ]]
```
Johansen α (M×r):
```
[[ 0.0011 -0.0186  0.3426]
 [-0.0526 -0.0197  0.3758]
 [-0.1223  0.0311  0.0107]
 [-2.046   0.0455  0.0942]
 [ 0.0595 -0.098   1.7799]
 [-0.0481 -0.0143  0.2754]]
```
drvec q=0 β':
```
[[  0.0052  -0.0051  -0.5045   1.       0.       0.    ]
 [ -0.204   -1.0239 -19.8731   0.       1.       0.    ]
 [ -0.9221  -0.0786  -1.1366   0.       0.       1.    ]]
```
drvec q=0 α:
```
[[ 0.0018 -0.0196  0.3579]
 [-0.0505 -0.0213  0.3803]
 [-0.1377  0.0649  0.5482]
 [-2.0322  0.046   0.0719]
 [ 0.0563 -0.1023  1.8341]
 [-0.0469 -0.0152  0.2674]]
```

## rao4  (M=6, n=87, reference rank r=2; Y1 = ger, aus)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=85) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 72.635 | 39.521 | 836.093 | 827.643 | grad/39 |
| 1 | 31.638 | 61.008 | 855.853 | 863.961 | tc3/8 |
| 2 | 19.712 | 11.109 | 886.357 | 879.779 | tc3/66 |
| 3 | 19.134 | 28.152 | 891.912 | 889.636 | tc3/25 |
| 4 | 8.190 | 7.484 | 905.988 | 899.202 | grad/121 |
| 5 | 4.072 | — | 909.730 | 903.297 | grad/102 |


Johansen trace (= ca.jo): [155.381, 82.745, 51.108, 31.395, 12.262, 4.072]

drvec verdicts: r=0: reject H0 at 10%; r=1: reject H0 at 1%; r=2: H0 not rejected; r=3: reject H0 at 1%; r=4: H0 not rejected

### Fit at r=2 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | 879.779 (cond.) | — | closed form | 0 | 0 | 0 | [1.9822, -2.0525] |
| drvec q=0 | 886.3574640497 | 78 | tc3/66 — NOT a convergence: the line search failed to improve | 0.2965 | 0.08143 | 0.08143 | [1.4521, -1.7559] |
| drvec q=0 multistart 20 | 886.3574640497 | 78 | tc3/66,tc3/20,tc3/3,tc3/95,tc3/18,tc3/23,grad/19,tc3/47,tc3/12,tc3/8,grad/6,grad/35,grad/7,tc3/2,tc3/6,tc3/7 — NOT a convergence: the line search failed to improve | 0.2965 | 0.08143 | 0.08143 | [1.4521, -1.7559] |
| drvec q=0 -seedjoh | 866.4986285883 | 78 | tc3/3 — NOT a convergence: the line search failed to improve | 0.4105 | 0.1557 | 0.1557 | [2.9296, -2.2323] |
| drvec q=1 (default MA, marow) | 861.2554081813 | 90 | tc3/3 — NOT a convergence: the line search failed to improve | 0.8735 | 0.2473 | 0.2475 | [1.3230, -0.8553] |

Johansen β' (rows = relations):
```
[[-0.081   1.5502 -1.7655 -0.5868  1.      0.    ]
 [-0.6065 -0.7734  0.5628 -0.2229  0.      1.    ]]
```
Johansen α (M×r):
```
[[ 0.1005  0.3574]
 [ 0.0709  0.1305]
 [ 0.103  -0.0605]
 [ 0.1148  0.0767]
 [-0.1857 -0.3733]
 [ 0.0829 -0.0531]]
```
drvec q=0 β':
```
[[-0.1085  1.2537 -1.483  -0.5951  1.      0.    ]
 [-0.5914 -0.6321  0.4168 -0.2081  0.      1.    ]]
```
drvec q=0 α:
```
[[ 0.1106  0.3712]
 [ 0.0721  0.1078]
 [ 0.0887 -0.0808]
 [ 0.0989  0.0447]
 [-0.202  -0.4547]
 [ 0.0692 -0.0973]]
```

## rao5  (M=5, n=117, reference rank r=2; Y1 = usa, ita)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=115) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 72.711 | 68.412 | 1069.204 | 1059.665 | grad/25 |
| 1 | 28.612 | — | 1103.410 | 1096.020 | grad/93 |
| 2 | 11.750 | 54.991 | 1093.921 | 1110.326 | tc3/21 |
| 3 | 10.547 | 8.102 | 1121.416 | 1116.201 | grad/176 |
| 4 | 6.060 | — | 1125.467 | 1121.475 | grad/114 |


Johansen trace (= ca.jo): [129.680, 56.969, 28.357, 16.607, 6.060]

drvec verdicts: r=0: reject H0 at 1%; r=2: reject H0 at 1%; r=3: H0 not rejected

### Fit at r=2 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | 1110.326 (cond.) | — | closed form | 0 | 0 | 0 | [0.3808, 2.2535] |
| drvec q=0 | 1093.9206432074 | 57 | tc3/21 — NOT a convergence: the line search failed to improve | 3.752 | 0.1015 | 0.2511 | [0.9894, 7.5633] |
| drvec q=0 multistart 20 | 1116.4268369084 | 57 | tc3/21,grad/107,tc3/123,tc3/76,tc3/29,tc3/46,grad/43,tc3/40,grad/25,grad/39,grad/30,grad/10,grad/60,grad/12,grad/88,tc3/68,grad/30,grad/11 — clean convergence, on the scaled gradient | 1.289 | 0.01428 | 0.02503 | [1.0907, 3.9230] |
| drvec q=0 -seedjoh | 1116.4268369082 | 57 | grad/93 — clean convergence, on the scaled gradient | 1.289 | 0.01428 | 0.02503 | [1.0907, 3.9230] |
| drvec q=1 (default MA, marow) | 1126.9980145945 | 67 | tc3/115 — NOT a convergence: the line search failed to improve | 1.893 | 0.125 | 0.2506 | [1.4329, 4.5099] |

Johansen β' (rows = relations):
```
[[ 0.7533 -0.9039 -1.4847  1.      0.    ]
 [-2.7514  4.5199 -4.0699  0.      1.    ]]
```
Johansen α (M×r):
```
[[-0.0605  0.0463]
 [ 0.071  -0.0062]
 [-0.0396  0.0248]
 [ 0.0527 -0.009 ]
 [-0.0561  0.0347]]
```
drvec q=0 β':
```
[[ 0.8879 -1.4303 -0.9853  1.      0.    ]
 [-1.1318  0.768  -0.6163  0.      1.    ]]
```
drvec q=0 α:
```
[[ 0.041   0.0934]
 [ 0.1004  0.031 ]
 [ 0.0045  0.013 ]
 [ 0.0295 -0.0203]
 [-0.0259 -0.0066]]
```

## rao6  (M=8, n=123, reference rank r=5; Y1 = gdf, gdfim, gdfcf, gdfce, rgnp)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=121) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 85.958 | — | — | -1144.451 | — |
| 1 | 71.722 | — | — | -1101.472 | — |
| 2 | 50.480 | — | — | -1065.611 | — |
| 3 | 43.605 | — | — | -1040.371 | — |
| 4 | 26.650 | — | — | -1018.568 | — |
| 5 | 16.401 | — | — | -1005.243 | — |
| 6 | 8.391 | — | — | -997.043 | — |
| 7 | 6.430 | — | — | -992.848 | — |


Johansen trace (= ca.jo): [309.637, 223.678, 151.957, 101.476, 57.871, 31.221, 14.821, 6.430]

### Fit at r=5 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | -1005.243 (cond.) | — | closed form | 0 | 0 | 0 | [-3.3733, 23.4230, -2.7495, -20.4736, -578.5245] |
| drvec q=0 | None | None |  — ? | — | — | — | — |
| drvec q=0 multistart 20 | -1147.9741242976 | 159 | tc3/12,tc3/12,tc3/23,tc3/2,tc3/12 — NOT a convergence: the line search failed to improve | 36.55 | 1.087 | 1.087 | [3.9215, -1.7715, -7.5580, 21.7638, -210.4509] |
| drvec q=0 -seedjoh | None | None |  — ? | — | — | — | — |
| drvec q=1 (default MA, marow) | None | None |  — ? | — | — | — | — |

Johansen β' (rows = relations):
```
[[ -0.0112  -0.5403  -0.1278   1.       0.       0.       0.      -0.    ]
 [  0.0073   0.0939  -0.6872   0.       1.      -0.      -0.      -0.    ]
 [ -0.0196   0.0241  -0.63     0.       0.       1.       0.      -0.    ]
 [ -0.0261  -4.4799   4.9385   0.       0.       0.       1.       0.    ]
 [ -1.0749 -18.7697  18.6274   0.       0.       0.       0.       1.    ]]
```
Johansen α (M×r):
```
[[-0.0762 -0.0149 -0.2142 -0.0319  0.003 ]
 [-0.4415  0.0329  0.3042  0.0157  0.0068]
 [ 0.025   0.0206 -0.0234 -0.0361  0.0027]
 [-0.1415  0.0138  0.0674  0.0092  0.0015]
 [-1.0895 -0.2374  0.4747  0.0686  0.002 ]
 [-0.1127  0.0169  0.0034  0.022   0.0001]
 [ 0.5412 -0.0458  0.3354 -0.109   0.0033]
 [-6.5574 -0.1838 -4.179   1.534  -0.0931]]
```

## rao7  (M=4, n=132, reference rank r=2; Y1 = r, m1)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=130) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 83.718 | 7.257 | -1772.730 | -1759.275 | tc3/1 |
| 1 | 48.902 | 61.276 | -1769.101 | -1717.416 | tc3/1 |
| 2 | 11.528 | 0.207 | -1738.463 | -1692.965 | tc3/1 |
| 3 | 5.738 | — | -1738.360 | -1687.201 | tc3/1 |


Johansen trace (= ca.jo): [149.887, 66.168, 17.266, 5.738]

drvec verdicts: r=0: H0 not rejected; r=1: reject H0 at 1%; r=2: H0 not rejected

### Fit at r=2 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | -1692.965 (cond.) | — | closed form | 0 | 0 | 0 | [-5.2400, 4861.3294] |
| drvec q=0 | -1738.4629524056 | 39 | tc3/1 — NOT a convergence: the line search failed to improve | — | 40.38 | 1405 | [0.9105, -5746.1010] |
| drvec q=0 multistart 20 | -1738.4629524056 | 39 | tc3/1,tc3/3,tc3/1,tc3/1,tc3/3,tc3/3,tc3/4,tc3/18,tc3/4,tc3/18,tc3/1,tc3/7 — NOT a convergence: the line search failed to improve | — | 40.38 | 1405 | [0.9105, -5746.1010] |
| drvec q=0 -seedjoh | -1740.0693877557 | 39 | step/1 — stopped on steptol, NOT on the gradient | — | 51.67 | 3085 | [-14.9358, 19670.8948] |
| drvec q=1 (default MA, marow) | -1738.4629524056 | 47 | tc3/1 — NOT a convergence: the line search failed to improve | — | 40.38 | 1405 | [0.9105, -5746.1010] |

Johansen β' (rows = relations):
```
[[    14.2902     -0.0001      1.          0.    ]
 [-39145.7596      0.1375      0.          1.    ]]
```
Johansen α (M×r):
```
[[   0.0011    0.    ]
 [-734.1954   -0.5004]
 [  -0.1462   -0.0002]
 [-102.9392   -0.0748]]
```

## rao7_sc  (M=4, n=132, reference rank r=2; Y1 = r, m1)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=130) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 83.718 | 80.092 | -587.543 | -583.175 | grad/27 |
| 1 | 48.902 | 39.090 | -547.497 | -541.316 | grad/145 |
| 2 | 11.528 | 11.837 | -527.952 | -516.865 | tc3/153 |
| 3 | 5.738 | — | -522.034 | -511.101 | grad/136 |


Johansen trace (= ca.jo): [149.887, 66.168, 17.266, 5.738]

drvec verdicts: r=0: reject H0 at 1%; r=1: reject H0 at 1%; r=2: H0 not rejected

### Fit at r=2 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | -516.865 (cond.) | — | closed form | 0 | 0 | 0 | [-4.6997, 12.8296] |
| drvec q=0 | -527.9523299293 | 39 | tc3/153 — NOT a convergence: the line search failed to improve | 2.468 | 0.03414 | 0.03414 | [9.9911, -39.0897] |
| drvec q=0 multistart 20 | -526.1188664350 | 39 | tc3/153,tc3/151,tc3/159,tc3/169,grad/173,grad/251,grad/245,grad/261,grad/361,tc3/413,grad/244,tc3/269 — NOT a convergence: the line search failed to improve | 0.6331 | 0.01667 | 0.01667 | [-0.9000, -1.1341] |
| drvec q=0 -seedjoh | -535.4367069601 | 39 | tc3/102 — NOT a convergence: the line search failed to improve | 8.641 | 0.02121 | 0.02121 | [-13.7355, 49.7816] |
| drvec q=1 (default MA, marow) | -507.6212313219 | 47 | tc3/286 — NOT a convergence: the line search failed to improve | 3.509 | 0.4697 | 0.4697 | [17.9439, -66.8410] |

Johansen β' (rows = relations):
```
[[ 0.0953 -0.353   1.     -0.    ]
 [-0.7686  0.9801  0.      1.    ]]
```
Johansen α (M×r):
```
[[ 0.1689  0.0536]
 [-0.303  -0.0702]
 [-0.1462 -0.0551]
 [-0.3029 -0.0748]]
```
drvec q=0 β':
```
[[-0.1622  0.3458  1.      0.    ]
 [ 0.1404 -1.4877  0.      1.    ]]
```
drvec q=0 α:
```
[[ 0.1798  0.0451]
 [-0.3116 -0.1043]
 [-0.1352 -0.0234]
 [-0.2958 -0.0954]]
```

## rao3_3v  (M=3, n=98, reference rank r=1; Y1 = lc)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=96) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 34.391 | 33.592 | 714.303 | 706.328 | grad/20 |
| 1 | 10.416 | 4.850 | 731.099 | 723.523 | grad/40 |
| 2 | 2.316 | — | 733.524 | 728.731 | grad/50 |


Johansen trace (= ca.jo): [47.124, 12.733, 2.316]

drvec verdicts: r=0: reject H0 at 1%; r=1: H0 not rejected

### Fit at r=1 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | 723.523 (cond.) | — | closed form | 0 | 0 | 0 | [-0.2650] |
| drvec q=0 | 731.0988871732 | 20 | grad/40 — clean convergence, on the scaled gradient | 0.007794 | 0.03943 | 0.03943 | [-0.2297] |

Johansen β' (rows = relations):
```
[[-0.9532 -0.0501  1.    ]]
```
Johansen α (M×r):
```
[[0.4999]
 [0.3124]
 [0.2412]]
```
drvec q=0 β':
```
[[-0.9454 -0.054   1.    ]]
```
drvec q=0 α:
```
[[0.5366]
 [0.3519]
 [0.2438]]
```

## rao6_sc  (M=8, n=123, reference rank r=5; Y1 = gdf, gdfim, gdfcf, gdfce, rgnp)

### Rank

| r | Johansen λmax | drvec LR | drvec logL(r) | Johansen llf(r) (cond., T=121) | drvec optimizer |
|---|---|---|---|---|---|
| 0 | 85.958 | — | — | -648.825 | — |
| 1 | 71.722 | — | — | -605.846 | — |
| 2 | 50.480 | — | — | -569.985 | — |
| 3 | 43.605 | — | — | -544.745 | — |
| 4 | 26.650 | — | — | -522.942 | — |
| 5 | 16.401 | — | — | -509.617 | — |
| 6 | 8.391 | — | — | -501.417 | — |
| 7 | 6.430 | — | — | -497.221 | — |


Johansen trace (= ca.jo): [309.637, 223.678, 151.957, 101.476, 57.871, 31.221, 14.821, 6.430]

### Fit at r=5 (β normalised on Y1; α in the same normalisation; drvec E[W] ≙ −Johansen const)

| source | logL | npar | optimizer | max|Δβ| vs Joh | max|Δα| vs Joh | max|ΔΠ| vs Joh | E[W] |
|---|---|---|---|---|---|---|---|
| Johansen RRR | -509.617 (cond.) | — | closed form | 0 | 0 | 0 | [-5.9716, 18.3983, -3.5696, -10.5716, -21.2514] |

Johansen β' (rows = relations):
```
[[-0.0559 -1.1231 -0.1411  1.      0.      0.      0.      0.    ]
 [ 0.0161  0.0866 -0.3366  0.      1.      0.      0.      0.    ]
 [-0.0716  0.0368 -0.5101  0.      0.      1.      0.     -0.    ]
 [-0.0379 -2.7162  1.5903  0.      0.      0.      1.      0.    ]
 [-0.111  -0.8096  0.4267  0.      0.      0.     -0.      1.    ]]
```
Johansen α (M×r):
```
[[-0.0153 -0.0067 -0.0587 -0.022   0.0286]
 [-0.2124  0.0356  0.1995  0.0259  0.1575]
 [ 0.0226  0.042  -0.0289 -0.1121  0.1163]
 [-0.1415  0.0312  0.0919  0.0315  0.0715]
 [-0.4834 -0.2374  0.2872  0.1043  0.0436]
 [-0.0826  0.0279  0.0034  0.0554  0.0022]
 [ 0.1579 -0.0301  0.1334 -0.109   0.0463]
 [-0.1361 -0.0086 -0.1182  0.1091 -0.0931]]
```
