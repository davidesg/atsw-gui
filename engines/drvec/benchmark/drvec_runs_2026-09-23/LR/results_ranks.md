# Rank synthesis (q=0, case 2, p=2)

logL columns: `def` = plain `-lrtest`; `sj` = `-lrtest -seedjoh`; `fit` = best standalone fit at the reference rank (default / multistart 20 / seedjoh); `exact` = independent Kalman exact ML (exact_ml.py, BFGS from Johansen's point, and from drvec's where available). Tags: grad = gradient convergence, tc3 = termcode 3 stop, step = steptol stop.


## e1 (M=3, reference r=1)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | 713.960 | 721.199 grad | 721.199 grad | — | 721.199 | -0.000 |
| 1 | 743.737 | 750.525 grad | 750.525 grad | 750.525 | 750.525 | +0.000 |
| 2 | 748.421 | 753.535 tc3 | 753.535 grad | — | 753.535 | +0.000 |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 59.553 | 58.652 | 58.652 | 22.00 | reject | reject | reject |
| 1 | 9.368 | 6.021 | 6.021 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=1, drvec (best drvec fits) r=1, best known exact max r=1.


## e3 (M=4, reference r=1)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | 1951.582 | 1967.130 grad | 1967.130 grad | — | 1967.130 | -0.000 |
| 1 | 1979.938 | 1990.760 grad | 1990.760 grad | 1990.760 | 1990.760 | +0.000 |
| 2 | 1989.713 | 2000.072 grad | 2000.072 grad | — | 2000.072 | +0.000 |
| 3 | 1995.826 | 2004.062 grad | 2004.062 tc3 | — | 2004.328 | -0.266 |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 56.713 | 47.261 | 47.261 | 28.14 | reject | reject | reject |
| 1 | 19.549 | 18.624 | 18.624 | 22.00 | accept | accept | accept |
| 2 | 12.228 | 7.979 | 8.512 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=1, drvec (best drvec fits) r=1, best known exact max r=1.


## rao1 (M=5, reference r=2)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | 1700.495 | 1712.743 grad | 1712.743 grad | — | 1712.743 | +0.000 |
| 1 | 1725.654 | 1735.131 tc3 | 1735.672 grad | — | 1734.510 | +1.162 |
| 2 | 1741.865 | 1747.256 tc3 | 1744.948 tc3 | 1747.256 | 1747.256 | +0.000 |
| 3 | 1747.252 | 1752.341 grad | 1752.185 grad | — | — | — |
| 4 | 1751.261 | 1755.523 grad | 1755.523 grad | — | — | — |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 50.318 | 45.858 | 45.858 | 34.40 | reject | reject | reject |
| 1 | 32.422 | 23.168 | 23.168 | 28.14 | reject | accept | accept |
| 2 | 10.774 | 10.169 | 10.169 | 22.00 | accept | accept | accept |
| 3 | 8.018 | 6.364 | 6.364 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=2, drvec (best drvec fits) r=1, best known exact max r=1.


## rao2 (M=4, reference r=3)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | 2289.821 | 2303.195 grad | 2303.195 grad | — | 2303.196 | -0.000 |
| 1 | 2314.538 | 2318.860 grad | 2318.860 step | — | 2315.831 | +3.029 |
| 2 | 2330.920 | 2334.177 grad | 2334.177 grad | — | 2334.177 | +0.000 |
| 3 | 2342.566 | 2340.123 tc3 | 2341.195 step | 2341.195 | 2335.400 | +5.795 |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 49.433 | 31.330 | 31.330 | 28.14 | reject | reject | reject |
| 1 | 32.765 | 30.634 | 30.634 | 22.00 | reject | reject | reject |
| 2 | 23.291 | 14.035 | 14.035 | 15.67 | reject | accept | accept |

Sequential λmax rank at 5%: Johansen r=≥3, drvec (best drvec fits) r=2, best known exact max r=2.


## rao3 (M=6, reference r=3)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | 887.036 | 897.426 grad | 897.426 grad | — | — | — |
| 1 | 934.065 | 913.059 tc3 | 940.734 tc3 | — | — | — |
| 2 | 976.718 | 932.326 tc3 | 981.677 tc3 | — | — | — |
| 3 | 998.821 | 1006.124 grad | 998.741 tc3 | 1006.124 | — | — |
| 4 | 1006.521 | 1016.513 grad | 1016.513 grad | — | — | — |
| 5 | 1009.581 | 1015.510 grad | 1015.510 grad | — | — | — |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 94.059 | 86.616 | 86.616 | 40.30 | reject | reject | reject |
| 1 | 85.305 | 81.887 | 81.887 | 34.40 | reject | reject | reject |
| 2 | 44.206 | 48.893 | 48.893 | 28.14 | reject | reject | reject |
| 3 | 15.400 | 20.778 | 20.778 | 22.00 | accept | accept | accept |
| 4 | 6.120 | -2.006 | -2.006 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=3, drvec (best drvec fits) r=3, best known exact max r=3.


## rao4 (M=6, reference r=2)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | 827.643 | 836.093 grad | 836.093 grad | — | 836.093 | -0.000 |
| 1 | 863.961 | 855.853 tc3 | 869.201 tc3 | — | 871.971 | -2.770 |
| 2 | 879.779 | 886.357 tc3 | 866.499 tc3 | 886.357 | 886.388 | -0.030 |
| 3 | 889.636 | 891.912 tc3 | 893.322 tc3 | — | 894.955 | -1.633 |
| 4 | 899.202 | 905.988 grad | 905.988 grad | — | 905.988 | +0.000 |
| 5 | 903.297 | 909.730 grad | 909.730 grad | — | — | — |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 72.635 | 66.215 | 71.756 | 40.30 | reject | reject | reject |
| 1 | 31.638 | 34.314 | 28.834 | 34.40 | accept | accept | accept |
| 2 | 19.712 | 13.929 | 17.135 | 28.14 | accept | accept | accept |
| 3 | 19.134 | 25.332 | 22.066 | 22.00 | accept | reject | reject |
| 4 | 8.190 | 7.484 | 7.484 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=1, drvec (best drvec fits) r=1, best known exact max r=1.


## rao5 (M=5, reference r=2)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | 1059.665 | 1069.204 grad | 1069.204 grad | — | 1069.204 | -0.000 |
| 1 | 1096.020 | 1103.410 grad | 1103.410 grad | — | 1103.410 | +0.000 |
| 2 | 1110.326 | 1093.921 tc3 | 1116.427 grad | 1116.427 | 1116.427 | +0.000 |
| 3 | 1116.201 | 1121.416 grad | 1121.416 grad | — | — | — |
| 4 | 1121.475 | 1125.467 grad | 1125.467 grad | — | — | — |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 72.711 | 68.412 | 68.412 | 34.40 | reject | reject | reject |
| 1 | 28.612 | 26.034 | 26.034 | 28.14 | reject | accept | accept |
| 2 | 11.750 | 9.979 | 9.979 | 22.00 | accept | accept | accept |
| 3 | 10.547 | 8.102 | 8.102 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=2, drvec (best drvec fits) r=1, best known exact max r=1.


## rao7 (M=4, reference r=2)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | -1759.275 | -1772.730 tc3 | — | — | -1772.725 | -0.005 |
| 1 | -1717.416 | -1769.101 tc3 | — | — | -1732.734 | -36.368 |
| 2 | -1692.965 | -1738.463 tc3 | — | -1738.463 | — | — |
| 3 | -1687.201 | -1738.360 tc3 | — | — | -1989.857 | +251.497 |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 83.718 | 7.257 | 79.982 | 28.14 | reject | accept | reject |
| 1 | 48.902 | 61.276 | -11.459 | 22.00 | reject | reject | accept |
| 2 | 11.528 | 0.207 | 0.207 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=2, drvec (best drvec fits) r=0, best known exact max r=1.


## rao7_sc (M=4, reference r=2)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | -583.175 | -587.543 grad | -587.543 grad | — | -587.544 | +0.000 |
| 1 | -541.316 | -547.497 grad | -547.497 grad | — | -547.497 | -0.000 |
| 2 | -516.865 | -527.952 tc3 | -535.437 tc3 | -526.119 | -527.952 | +1.833 |
| 3 | -511.101 | -522.034 grad | -522.034 tc3 | — | -522.034 | -0.000 |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 83.718 | 80.092 | 80.092 | 28.14 | reject | reject | reject |
| 1 | 48.902 | 42.757 | 42.757 | 22.00 | reject | reject | reject |
| 2 | 11.528 | 8.170 | 8.170 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=2, drvec (best drvec fits) r=2, best known exact max r=2.


## rao3_3v (M=3, reference r=1)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | 706.328 | 714.303 grad | 714.303 grad | — | 714.303 | +0.000 |
| 1 | 723.523 | 731.099 grad | 731.099 grad | 731.099 | 731.099 | +0.000 |
| 2 | 728.731 | 733.524 grad | 733.524 grad | — | 733.524 | +0.000 |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 34.391 | 33.592 | 33.592 | 22.00 | reject | reject | reject |
| 1 | 10.416 | 4.850 | 4.850 | 15.67 | accept | accept | accept |

Sequential λmax rank at 5%: Johansen r=1, drvec (best drvec fits) r=1, best known exact max r=1.


## rao6_sc (M=8, reference r=5)

| r | Joh llf (cond) | drvec def | drvec sj | drvec fit | exact indep. | best drvec − exact |
|---|---|---|---|---|---|---|
| 0 | -648.825 | — | — | — | — | — |
| 1 | -605.846 | — | — | — | — | — |
| 2 | -569.985 | — | — | — | — | — |
| 3 | -544.745 | — | — | — | — | — |
| 4 | -522.942 | — | — | — | — | — |
| 5 | -509.617 | — | — | — | — | — |
| 6 | -501.417 | — | — | — | — | — |
| 7 | -497.221 | — | — | — | — | — |

| H0 r | Johansen λmax | drvec LR (best drvec fits) | LR (best known max: drvec ∪ indep. exact) | 5% cv | Johansen | drvec best | best known |
|---|---|---|---|---|---|---|---|
| 0 | 85.958 | — | — | 52.00 | reject | — | — |
| 1 | 71.722 | — | — | 46.45 | reject | — | — |
| 2 | 50.480 | — | — | 40.30 | reject | — | — |
| 3 | 43.605 | — | — | 34.40 | reject | — | — |
| 4 | 26.650 | — | — | 28.14 | accept | — | — |
| 5 | 16.401 | — | — | 22.00 | accept | — | — |
| 6 | 8.391 | — | — | 15.67 | accept | — | — |

Sequential λmax rank at 5%: Johansen r=4, drvec (best drvec fits) r=≥7, best known exact max r=≥7.

