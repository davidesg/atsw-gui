"""Run legacy drv (original or instrumented) and drvec; parse results.  Study B."""
import os, re, subprocess, shutil, math
S = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DRV_ORIG = '/home/david/Dropbox/SRC/drv_project/bin/drv'
DRV_X = os.path.join(S, 'legacy_x/bin/drv')
DRVEC = '/home/david/Dropbox/SRC/drvec/bin/drvec'
WORK = os.path.join(S, 'work')

def _dir(tag):
    d = os.path.join(WORK, tag); os.makedirs(d, exist_ok=True); return d

def run_legacy(inp4, tag, p, ar='full', ma='leg', beta_fixed=False, conv=False, orig=False, maseed=None, a22seed=None):
    """inp4: path of a legacy 4-column .inp (dL, A, L, X)."""
    d = _dir(tag); shutil.copy(inp4, os.path.join(d, 'm.inp'))
    env = dict(os.environ)
    if not orig:
        env['DRV_AR'] = ar; env['DRV_MA'] = ma
        if maseed is not None: env['DRV_MASEED'] = str(maseed)
        if a22seed is not None: env['DRV_A22SEED'] = str(a22seed)
    args = [DRV_ORIG if orig else DRV_X, 'm', str(p)]
    if beta_fixed: args.append('beta_fijo')
    if not conv: args.append('no_conv')
    args.append('1')
    subprocess.run(args, cwd=d, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
    return parse_legacy(os.path.join(d, 'm.out'))

def parse_legacy(f):
    t = open(f, encoding='latin-1').read()
    r = {'file': f}
    m = re.search(r'logelf:\s*(\S+)', t); r['logL'] = float(m.group(1)) if m else None
    r['npar'] = int(re.search(r'Parameters :\s*(\d+)', t).group(1))
    m = re.findall(r'\*\*\*\* (NORM OF SCALED GRADIENT|SCALED DISTANCE|LAST GLOBAL STEP|ITERATION LIMIT|FIVE CONSECUTIVE)', t)
    r['term'] = {'NORM OF SCALED GRADIENT': 'grad', 'SCALED DISTANCE': 'step', 'LAST GLOBAL STEP': 'lower',
                 'ITERATION LIMIT': 'maxit', 'FIVE CONSECUTIVE': 'maxstep'}.get(m[-1] if m else '', '?')
    r['x'] = [float(v) for v in re.findall(r'x\[\s*\d+\]=\s*(\S+);', t)]
    r['sd'] = [float(v) for v in re.findall(r'sd\[\s*\d+\]:\s*(\S+)', t)]
    th = re.search(r'Matrix theta\( 1\):\n(.*)\n(.*)\n', t)
    r['theta_leg'] = [[float(v) for v in th.group(i).split()] for i in (1, 2)] if th else None
    ph = re.findall(r'Matrix phi\(\s*(\d+)\):\n(.*)\n(.*)\n', t)
    r['phi_leg'] = {int(k): [[float(v) for v in a.split()], [float(v) for v in b.split()]] for k, a, b in ph}
    mr = re.search(r'Inverse roots of det\[theta\(B\)\] = 0:\n((?:.*modulus.*\n)*)', t)
    r['ma_invmod'] = [float(v) for v in re.findall(r'modulus:\s*(\S+)\)', mr.group(1))] if mr else []
    mu = re.search(r'Vector mu:\n\s*(\S+)', t); r['mu'] = float(mu.group(1)) if mu else None
    return r

def run_drvec(inpD, tag, p, q, cls, extra=(), differenced=True):
    d = _dir(tag); shutil.copy(inpD, os.path.join(d, 'm.inp'))
    args = [DRVEC, 'm', str(p), str(q), '1', '-case', '2'] + (['-differenced'] if differenced else []) + ([cls] if cls else []) + list(extra)
    subprocess.run(args, cwd=d, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
    return parse_drvec(os.path.join(d, 'm.out'))

def parse_drvec(f):
    t = open(f, encoding='latin-1').read()
    r = {'file': f}
    m = re.search(r'logelf\s*:\s*(\S+)', t); r['logL'] = float(m.group(1)) if m else None
    m = re.search(r'npar\s*:\s*(\d+)', t); r['npar'] = int(m.group(1)) if m else None
    m = re.search(r'Convergence criterion: (\S+)', t)
    r['term'] = {'norm': 'grad', 'scaled': 'step', 'last': 'lower', 'iteration': 'maxit', 'five': 'maxstep'}.get(m.group(1), m.group(1)) if m else '?'
    m = re.search(r'beta_2 matrix \(s x r\):\n\s*(\S+)', t); r['B2'] = float(m.group(1)) if m else None
    m = re.search(r'MA \(Theta\)\s+(.*)', t)
    r['ma_roots'] = [float(v.rstrip('*')) for v in m.group(1).split() if v != 'inf'] if m else []
    m = re.search(r'Rank condition \(Granger\): sigma_min\(.*?\) = (\S+)', t); r['G'] = float(m.group(1)) if m else None
    th = re.search(r'theta\(1\) matrix:\n(.*)\n(.*)\n', t)
    r['theta_vec'] = [[float(v) for v in th.group(i).split()] for i in (1, 2)] if th else None
    return r
