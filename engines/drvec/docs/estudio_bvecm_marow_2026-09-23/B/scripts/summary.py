"""Q4 summary: the LRs that bear on the dY2-row moving average."""
import json, sys
from scipy.stats import chi2
S = __file__.rsplit('/', 2)[0]
def pv(lr, df): return 1 - chi2.cdf(lr, df) if lr > 0 else 1.0
def flag(r, k):
    v = r['ma'].get(k); return '' if v is None else ('*' if abs(v[0]) > 0.995 else '')
print(f"{'set':10} {'data':11} {'p':>1} {'AR':4} | {'a(leg) t':>9} {'LR leg/none':>11} | {'LR tri/row':>10} {'p':>5} | {'LR free/row':>11} {'p':>5} | {'L(leg)-L(row)':>13} | {'LR free/tri':>11} | marow term")
for which in ['legconf', 'legdata', 'pairs']:
    R = json.load(open(f'{S}/work/evidence_{which}.json'))
    keys = sorted({(r['ds'], r['p'], r['ar']) for r in R})
    for k in keys:
        rr = {r['cls']: r for r in R if (r['ds'], r['p'], r['ar']) == k}
        L = {c: rr[c]['logL'] for c in rr}
        a = rr['leg']['ma'].get('a', (float('nan'), float('nan')))
        t = a[0] / a[1] if a[1] else float('inf')
        x1 = 2 * (L['leg'] - L['none']); x2 = 2 * (L['matri'] - L['marow']); x3 = 2 * (L['free'] - L['marow']); x5 = 2 * (L['free'] - L['matri'])
        print(f"{which:10} {k[0]:11} {k[1]} {k[2]:4} | {a[0]:+.2f}{flag(rr['leg'],'a'):1}{t:5.1f} {x1:11.2f} | {x2:10.2f} {pv(x2,1):5.3f} | {x3:11.2f} {pv(x3,2):5.3f} | {L['leg']-L['marow']:13.2f} | {x5:11.2f} | {rr['marow']['term']}{' d=1' if flag(rr['marow'],'d') else ''}")
