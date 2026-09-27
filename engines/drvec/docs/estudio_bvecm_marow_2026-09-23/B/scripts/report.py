import json, sys, math
S = __file__.rsplit('/', 2)[0]
from scipy.stats import chi2
NP = {'none': 0, 'leg': 1, 'mawarma': 1, 'diag': 2, 'marow': 2, 'matri': 3, 'free': 4}
def table(which):
    R = json.load(open(f'{S}/work/evidence_{which}.json'))
    keys = sorted({(r['ds'], r['p'], r['ar']) for r in R}, key=lambda k: (k[0], k[1], k[2]))
    for k in keys:
        rr = {r['cls']: r for r in R if (r['ds'], r['p'], r['ar']) == k}
        f = rr['free']['logL']
        print(f"\n## {k[0]} p={k[1]} AR={k[2]} {'conv' if rr['free']['conv'] else ''}")
        print(f"{'class':8} {'nMA':>3} {'logL':>12} {'LRvsFree':>8} {'p':>6} {'term':>5} {'spread':>7}  MA (est/sd) [drvec names a=dY2<-dY2 b=dY2<-W c=W<-dY2 d=W<-W]  MAinvmod  best-src")
        for c in ['none', 'leg', 'mawarma', 'diag', 'marow', 'matri', 'free']:
            r = rr[c]; lr = 2 * (f - r['logL']); df = 4 - NP[c]
            pv = 1 - chi2.cdf(max(lr, 0), df) if df else float('nan')
            ma = ' '.join(f"{n}={v:+.3f}({s:.3f})" for n, (v, s) in sorted(r['ma'].items()))
            print(f"{c:8} {NP[c]:3d} {r['logL']:12.4f} {lr:8.3f} {pv:6.3f} {r['term']:>5} {r['spread']:7.3f}  {ma:60} {','.join(f'{m:.3f}' for m in r['ma_invmod'])}  {r['src']}")
table(sys.argv[1])
