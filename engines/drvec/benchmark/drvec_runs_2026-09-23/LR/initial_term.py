#!/usr/bin/env python3
"""How much of drvec's exact logL is the first observation's stationary density (the
term Johansen's conditional likelihood does not have), at drvec's own optimum."""
import numpy as np, exact_ml as E, warnings; warnings.filterwarnings("ignore")
from scipy.linalg import solve_discrete_lyapunov
PTS=[("e1",1,"e1_q0r1"),("e3",1,"e3_q0r1"),("rao2",1,"rao2_q0r1"),("rao2",3,"rao2_q0r3_sj"),("rao1",1,"rao1_q0r1sj"),("rao1",2,"rao1_q0r2"),("rao5",2,"rao5_q0r2_sj"),("rao4",2,"rao4_q0r2"),("rao7_sc",2,"rao7_sc_q0r2_ms"),("rao3_3v",1,"rao3_3v_q0r1")]
print("| fit | logL | first-obs term | max|eig| of state VAR | sd of W_1 (stationary) / sd of W over sample |\n|---|---|---|---|---|")
for c,r,stem in PTS:
    y=E.read_inp(c+".inp"); n,M=y.shape; a,b2,G,mu,S,ll=E.drvec_point(stem+".out",M,r)
    P=np.eye(M)[list(range(M-r,M))+list(range(M-r))]; G=P.T@G@P; S=P.T@S@P
    beta=np.vstack([b2,np.eye(r)]); A=np.block([[G,a],[beta.T@G,np.eye(r)+beta.T@a]]); D=np.vstack([np.eye(M),beta.T])
    Pst=solve_discrete_lyapunov(A,D@S@D.T); s=M-r
    Sel=np.zeros((M,M+r)); Sel[:s,:s]=np.eye(s); Sel[s:,M:]=np.eye(r)
    F=Sel@Pst@Sel.T; W=(y@beta)[1:]-mu; z=np.concatenate([np.diff(y,axis=0)[0,:s],W[0]])
    l1=-0.5*(M*np.log(2*np.pi)+np.linalg.slogdet(F)[1]+z@np.linalg.solve(F,z))
    ratio=np.sqrt(np.diag(Pst)[M:])/W.std(0)
    print("| %s | %.3f | %.3f | %.4f | %s |"%(stem,ll,l1,abs(np.linalg.eigvals(A)).max()," ".join("%.2f"%v for v in ratio)))
