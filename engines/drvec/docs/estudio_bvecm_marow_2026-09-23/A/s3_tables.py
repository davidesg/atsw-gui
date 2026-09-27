"""s3 -- internal consistency of the article's empirical numbers.

T1 = BVECM_models.tex Table 1 (l.536-546, break dates 1839/1843/1841/1847)
R1 = results_Wheat_1848.tex Table 1 (l.23-33, break 1848)
"""
import sympy as sp
T1 = {  # pair, spec: (omega, se, delta, se, mu, g, gse, l, lse)
 ("A/L","b=1"):(3.35,2.61,0.924,0.096,-35.03,43.81,25.23,13.09,16.49),
 ("A/L","b=.769"):(3.63,1.68,0.93,0.06,-38.89,50.23,20.60,13.84,11.61),
 ("V/L","b=1"):(12.11,8.78,0.78,0.17,-71.63,55.86,9.63,4.61,3.64),
 ("V/L","b=.946"):(12.18,8.81,0.79,0.17,-72.18,56.72,10.10,4.66,3.72),
 ("S/L","b=1"):(8.45,11.71,0.77,0.35,-40.89,37.33,12.11,4.42,6.75),
 ("S/L","b=.733"):(12.62,11.82,0.70,0.29,-45.10,42.62,8.97,3.38,3.35),
 ("P/L","b=1"):(7.90,4.41,0.83,0.10,-41.23,45.78,5.45,5.80,3.44),
 ("P/L","b=1.163"):(7.01,3.87,0.85,0.10,-39.97,45.63,6.89,6.51,4.07)}
R1 = {
 ("A/L","b=1"):(6.62,4.26,0.821,0.127,-33.50,36.94,10.31,5.58,3.96),
 ("A/L","b=.807"):(8.84,6.07,0.777,0.145,-36.71,41.09,9.63,4.65,3.13),
 ("V/L","b=1"):(12.85,12.56,0.772,0.254,-69.65,55.68,12.44,4.33,4.76),
 ("V/L","b=.961"):(13.35,12.89,0.763,0.260,-70.15,56.14,13.21,4.21,4.59),
 ("S/L","b=1"):(3.77,3.88,0.914,0.136,-38.60,43.90,31.14,11.65,18.42),
 ("S/L","b=.796"):(5.40,4.28,0.880,0.123,-41.74,45.29,18.15,8.38,8.65),
 ("P/L","b=1"):(7.35,3.71,0.840,0.090,-40.89,46.06,6.73,6.27,3.54),
 ("P/L","b=1.167"):(6.73,3.44,0.853,0.085,-39.64,45.83,7.03,6.81,3.95)}
for name, tab in [("BVECM_models.tex Table 1", T1), ("results_Wheat_1848 Table 1", R1)]:
    print("==", name)
    print("pair spec | l=1/(1-d) recomputed vs reported | delta implied by l | g=w/(1-d_impl) vs rep | t(g) | nu'(1)/nu(1)=d/(1-d) | half-life ln.5/ln d")
    import math
    for k, (w, ws, d, ds, mu, g, gs, l, ls) in tab.items():
        d_impl = 1 - 1 / l
        print(k, "| %.2f vs %.2f | %.4f | %.2f vs %.2f | %.2f | %.2f | %.2f" % (
            1 / (1 - d), l, d_impl, w / (1 - d_impl), g, g / gs, d_impl / (1 - d_impl),
            math.log(.5) / math.log(d_impl)))
print("\nText BVECM l.561: 'estimates of phi all above 0.77' -> T1 S/L b=.733 has delta=0.70;")
print("'g significant except S/L under b=1' -> in T1, S/L b=1 t=%.2f (significant), A/L b=1 t=%.2f (not)" % (37.33/12.11, 43.81/25.23))
print("In R1: S/L b=1 t=%.2f (not significant), A/L b=1 t=%.2f -> the sentence matches R1, not T1" % (43.90/31.14, 36.94/10.31))
print("\nWeak exogeneity of London (z2), p-values:")
print(" BVECM Table 2 (l.585-586) V/L: 0.025, 0.035   text l.607: 'weakly exogenous ... p > 0.05 under both'  -> contradicts")
print(" results_1848 Table 2 (l.70-71) V/L: 0.059, 0.067 -> the text matches the 1848 run")
print(" presentation l.438-440: V/L 'Yes (p=0.025)'  -> contradicts its own p")
print(" addon l.409-410: A/L 'weakly exogenous (Wald p=0.035, borderline rejection)' -> rejects at 5%")

print("\nAddon numerical illustration (l.377-403):")
phi11, phi21, beta = 0.651, 0.044, 1.0
print(" phi11(1)=0.651 is Table 1's 'AR root' column (A/L b=1, l.536); phi21(1)=0.044 is Table 2's sum alpha2 (l.582)")
print(" alpha1 = %.3f  vs Table 2 sum alpha1 (A/L b=1) = -0.457" % (phi11 - 1 + beta * phi21))

print("\nAddon D_t (l.495, 545, 578) vs Appendix Cor. 3 (D_t = (dc;0) - sum M_l c_{t-l}):")
a11, a12, a21, a22, b, c0, c1 = sp.symbols('phi11 phi12 phi21 phi22 beta c_t c_tm1')
al1 = a11 - 1 + b * a21; al2 = a21
D1_true = (c0 - c1) - al1 * c1; D2_true = -al2 * c1
D1_step2 = (c0 - c1) - (a11 + b * a21) * c1
print(" true D1 =", sp.expand(D1_true), "; addon Step-2 D1 =", sp.expand(D1_step2),
      "; difference =", sp.simplify(D1_step2 - D1_true))
print(" addon Step-3 writes -(alpha1+1, alpha2)' c_{t-1} = -M1 c_{t-1}: but M1 = (alpha1, alpha2)' (p*=1), so off by c_{t-1} in row 1")

print("\nAddon orthogonalisation (l.213-236): p21 = s12/s11 of Sigma_a, applied to u=(a1+b a2, a2):")
s11, s12, s22 = sp.symbols('s11 s12 s22')
cov_u12 = b * s22 + s12; var_u1 = s11 + 2 * b * s12 + b**2 * s22
# P^{-1} = [[1,0],[-p21,1]] applied to u: e2 = u2 - p21 u1 ; Cov(e1,e2) = Cov(u1,u2) - p21 Var(u1)
p21 = s12 / s11
print(" Cov(e1,e2) with the addon's p21 =", sp.simplify(cov_u12 - p21 * var_u1), " (non-zero in general)")
