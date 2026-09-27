"""Q2: numeric check of the class correspondence legacy(instrumented) <-> drvec, same data, same p."""
import sys, json
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, __file__.rsplit('/', 1)[0]); from runlib import *
R = os.path.join(S, 'runs')
pairs = ['AL', 'PL', 'SL', 'VL']
P = int(sys.argv[1]) if len(sys.argv) > 1 else 2
specs = [  # (label, legacy ar, legacy ma, drvec q, drvec class)
 ('free',    'vec',   'free', 1, '-mafree'),
 ('matri',   'vec',   'tri',  1, '-matri'),
 ('marow',   'vec',   'wrow', 1, '-marow'),
 ('mawarma', 'vec',   'ww',   1, '-mawarma'),
 ('q0',      'vec',   'none', 0, None),
 ('warma',   'warma', 'ww',   1, '-warma'),
]
jobs = []
with ThreadPoolExecutor(8) as ex:
    for pr in pairs:
        for lab, ar, ma, q, cls in specs:
            jobs.append((pr, lab, ex.submit(run_legacy, f'{R}/{pr}.inp', f'eq_p{P}_{pr}_{lab}_leg', P, ar, ma),
                         ex.submit(run_drvec, f'{R}/D{pr}.inp', f'eq_p{P}_{pr}_{lab}_vec', P, q, cls)))
    out = []
    print(f'p={P}  {"pair":4} {"class":8} {"logL legacy_x":>15} {"logL drvec":>15} {"diff":>9}  npar(leg/vec) term(leg/vec)')
    for pr, lab, fl, fd in jobs:
        a, b = fl.result(), fd.result()
        dif = (a['logL'] - b['logL']) if a['logL'] is not None and b['logL'] is not None else float('nan')
        print(f'      {pr:4} {lab:8} {a["logL"]:15.6f} {b["logL"]:15.6f} {dif:9.2e}  {a["npar"]}/{b["npar"]}  {a["term"]}/{b["term"]}')
        out.append(dict(pair=pr, cls=lab, leg=a, vec=b))
json.dump(out, open(f'{S}/work/equiv_p{P}.json', 'w'), indent=1)
