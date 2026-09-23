"""Q4: MA-structure evidence. For each data set, AR mode and MA class, the best of several starts
(instrumented legacy with MA seeds; drvec default and -seedjoh when the class exists in drvec).
MA entries named in drvec's Ybar order (dY2, W):  a = dY2<-A.dY2, b = dY2<-A.W, c = W<-A.dY2, d = W<-A.W."""
import sys, json, itertools
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, __file__.rsplit('/', 1)[0]); from runlib import *
D = os.path.join(S, 'data')
# legacy mask (i,j) in legacy order (1=W, 2=dY2) -> drvec-name
LEGPOS = {(1, 1): 'd', (1, 2): 'c', (2, 1): 'b', (2, 2): 'a'}
CLASSES = {'none': '', 'leg': 'a', 'mawarma': 'd', 'diag': 'ad', 'marow': 'cd', 'matri': 'acd', 'free': 'abcd'}
LEGMA = {'none': 'none', 'leg': 'leg', 'mawarma': 'ww', 'diag': 'diag', 'marow': 'wrow', 'matri': 'tri', 'free': 'free'}
VECCLS = {'none': (0, None), 'mawarma': (1, '-mawarma'), 'marow': (1, '-marow'), 'matri': (1, '-matri'), 'free': (1, '-mafree')}

def nar(p, ar):
    n = 0
    for k in range(1, p + 1):
        for i in (1, 2):
            for j in (1, 2):
                n += not ((ar == 'vec' and k == p and j == 2) or (ar == 'warma' and j == 2))
    return n

def ma_params(r, p, ar, cls):
    """read MA estimates and sds from the legacy x vector"""
    if not r['x'] or cls == 'none': return {}
    k = 3 + nar(p, ar); out = {}
    for (i, j) in [(1, 1), (1, 2), (2, 1), (2, 2)]:
        nm = LEGPOS[(i, j)]
        if nm in CLASSES[cls]:
            out[nm] = (r['x'][k], r['sd'][k] if k < len(r['sd']) else float('nan')); k += 1
    return out

def one(ds, p, ar, cls, conv=False, bfix=False, levels=False):
    tagb = f'ev_{ds}_p{p}_{ar}_{cls}_{"c" if conv else "n"}{"b" if bfix else ""}'
    legf = f'{D}/{ds}.inp' if not levels else f'{D}/P_{ds}.inp'
    runs = []
    for sd, sa in ((None, None), (0.3, None), (-0.3, None), (0.6, None), (None, 0.5), (0.3, 0.8), (-0.3, -0.3)):
        r = run_legacy(legf, f'{tagb}_s{sd}_a{sa}', p, ar, LEGMA[cls], beta_fixed=bfix, conv=conv, maseed=sd, a22seed=sa)
        r['src'] = f'leg(seed {sd},{sa})'; r['ma'] = ma_params(r, p, ar, cls); runs.append(r)
    if ar == 'vec' and not conv and not bfix and cls in VECCLS:
        q, c = VECCLS[cls]
        vf = f'{D}/D{ds}.inp' if not levels else f'{D}/V_{ds}.inp'
        for ex in ((), ('-seedjoh',)):
            r = run_drvec(vf, f'{tagb}_v{len(ex)}', p, q, c, ex, differenced=not levels)
            r['src'] = 'drvec' + (' -seedjoh' if ex else ''); runs.append(r)
    ok = [r for r in runs if r['logL'] is not None]
    best = max(ok, key=lambda r: r['logL'])
    bestleg = max([r for r in ok if r['src'].startswith('leg')], key=lambda r: r['logL'])
    return dict(ds=ds, p=p, ar=ar, cls=cls, conv=conv, bfix=bfix, logL=best['logL'], src=best['src'], term=best['term'],
                npar_leg=bestleg['npar'], leg_logL=bestleg['logL'], leg_term=bestleg['term'], ma=bestleg['ma'],
                ma_invmod=bestleg['ma_invmod'], spread=max(r['logL'] for r in ok) - min(r['logL'] for r in ok),
                all=[(r['src'], r['logL'], r['term']) for r in ok])

if __name__ == '__main__':
    which = sys.argv[1]
    tasks = []
    if which == 'legconf':      # the legacy's own published configurations (convergence operator on)
        for ds, p, bf in [('AL', 2, True), ('PL', 2, True), ('SL', 2, False), ('VL', 4, False)]:
            for cls in CLASSES: tasks.append((ds, p, 'full', cls, True, bf, False))
    elif which == 'legdata':    # legacy data, no convergence operator, beta free, both AR modes
        for ds in ['AL', 'PL', 'SL', 'VL']:
            for p in (1, 2):
                for ar in ('full', 'vec'):
                    for cls in CLASSES: tasks.append((ds, p, ar, cls, False, False, False))
    elif which == 'pairs':      # drvec's eight wheat pairs (1700-1812), levels layout
        for ds in ['milan', 'strasbourg', 'utrecht', 'vienna', 'aix', 'arevalo', 'angers', 'penn']:
            for p in (1, 2):
                for ar in ('full', 'vec'):
                    for cls in CLASSES: tasks.append((ds, p, ar, cls, False, False, True))
    with ThreadPoolExecutor(12) as ex:
        res = list(ex.map(lambda t: one(*t), tasks))
    json.dump(res, open(f'{S}/work/evidence_{which}.json', 'w'), indent=1)
    print(len(res), 'fits')
