# References

Works cited across this documentation. Entries were verified against the sources
themselves or against the reference list of Mauricio (2006); unpublished items
are identified as such.

## Estimation of partially nonstationary VARMA models

**Mauricio, J. A.** (2006). Exact maximum likelihood estimation of partially
nonstationary vector ARMA models. *Computational Statistics & Data Analysis*,
50(12), 3644–3662.
— The model, the transformation and the estimation procedure this program
implements.

**Mauricio, J. A.** (2005). *Additional material on exact maximum likelihood
estimation of partially nonstationary vector ARMA models*. Unpublished
manuscript. — Closed-form expressions for the bivariate case with one
cointegrating relation, and a second worked example.

**Mélard, G., Roy, R. and Saidi, A.** (2004). *Exact maximum likelihood
estimation of structured or unit root multivariate time series models*.
Technical Report 0444, Interuniversity Attraction Pole Statistics Network,
Université Libre de Bruxelles. — The state-space alternative to the same problem,
by Kalman filtering; the only prior treatment Mauricio (2006) identifies.

**Luceño, A.** (1994). A fast algorithm for the exact likelihood of stationary
and partially nonstationary vector autoregressive moving average processes.
*Biometrika*, 81(3), 551–565.

**Ma, C.** (1997). On the exact likelihood function of a multivariate
autoregressive moving average model. *Biometrika*, 84(4), 957–964.

## The exact likelihood of a stationary VARMA

**Mauricio, J. A.** (1995). Exact maximum likelihood estimation of stationary
vector autoregressive moving average models. *Journal of the American
Statistical Association*, 90(429), 282–291.
— Includes the quasi-Newton optimiser used here.

**Mauricio, J. A.** (1997). Algorithm AS 311: the exact likelihood function of a
vector autoregressive moving average model. *Applied Statistics*, 46(1),
157–171. — The likelihood routine at the core of this program, used unmodified.

**Hillmer, S. C. and Tiao, G. C.** (1979). Likelihood function of stationary
multiple autoregressive moving average models. *Journal of the American
Statistical Association*, 74(367), 652–660.

**Shea, B. L.** (1989). Algorithm AS 242: the exact likelihood of a vector
autoregressive moving average model. *Applied Statistics*, 38(1), 161–204.

**Mélard, G.** (1984). Algorithm AS 197: a fast algorithm for the exact
likelihood of autoregressive-moving average models. *Applied Statistics*, 33(1),
104–114.

## Cointegration: representation, estimation and inference

**Phillips, P. C. B.** (1991a). Optimal inference in cointegrated systems.
*Econometrica*, 59(2), 283–306.
— The triangular system error-correction representation, the LAMN result, and
the condition on which valid inference for the cointegrating coefficients rests.

**Phillips, P. C. B.** (1991b). Error correction and long-run equilibrium in
continuous time. *Econometrica*, 59(4), 967–980.

**Johansen, S.** (1988). Statistical analysis of cointegration vectors. *Journal
of Economic Dynamics and Control*, 12(2–3), 231–254.

**Johansen, S.** (1991). Estimation and hypothesis testing of cointegration
vectors in Gaussian vector autoregressive models. *Econometrica*, 59(6),
1551–1580. — Mixed-Gaussian limits for the maximum likelihood estimator and χ²
tests of hypotheses on the cointegrating relations.

**Johansen, S. and Swensen, A. R.** (2024). Adjustment coefficients and exact
rational expectations in cointegrated vector autoregressive models. *Journal of
Time Series Analysis*, 45(2), 248–268.
— The class of linear restrictions α = Aψ implemented here, and the degrees of
freedom of the associated likelihood-ratio test.

**Cappuccio, N. and Lubian, D.** (1996). Triangular representation and error
correction mechanism in cointegrated systems. *Oxford Bulletin of Economics and
Statistics*, 58(2), 431–439. — Establishes that no reparameterisation of a
cointegrated ARMA system yields a vector error-correction model with independent
errors.

**Engle, R. F. and Granger, C. W. J.** (1987). Co-integration and error
correction: representation, estimation, and testing. *Econometrica*, 55(2),
251–276.

**Ahn, S. K. and Reinsel, G. C.** (1990). Estimation for partially nonstationary
multivariate autoregressive models. *Journal of the American Statistical
Association*, 85(411), 813–823. — Reduced-rank estimation imposing the unit-root
structure, for the purely autoregressive case.

**Yap, S. F. and Reinsel, G. C.** (1995). Estimation and testing for unit roots
in a partially nonstationary vector autoregressive moving average model.
*Journal of the American Statistical Association*, 90(429), 253–267.
— Moving-average terms do not affect the asymptotic distribution of the rank
test statistics.

**Reinsel, G. C.** (1997). *Elements of Multivariate Time Series Analysis*, 2nd
edition. Springer, New York.

**Luukkonen, R., Ripatti, A. and Saikkonen, P.** (1999). Testing for a valid
normalization of cointegrating vectors in vector autoregressive processes.
*Journal of Business and Economic Statistics*, 17(2), 195–204.

**Kurozumi, E.** (2005). The rank of a submatrix of cointegration.
*Econometric Theory*, 21(2), 299–325.

**Gonzalo, J. and Granger, C. W. J.** (1995). Estimation of common long-memory
components in cointegrated systems. *Journal of Business and Economic
Statistics*, 13(1), 27–35.

**Vahid, F. and Engle, R. F.** (1993). Common trends and common cycles.
*Journal of Applied Econometrics*, 8(4), 341–360.

**Pham, D. T., Roy, R. and Cédras, L.** (2003). Tests for non-correlation of two
cointegrated ARMA time series. *Journal of Time Series Analysis*, 24(5),
553–577.

## Critical values for the rank test

**Osterwald-Lenum, M.** (1992). A note with quantiles of the asymptotic
distribution of the maximum likelihood cointegration rank test statistics.
*Oxford Bulletin of Economics and Statistics*, 54(3), 461–472.
— Source of the tabulated values reported by default.

**MacKinnon, J. G., Haug, A. A. and Michelis, L.** (1999). Numerical
distribution functions of likelihood ratio tests for cointegration. *Journal of
Applied Econometrics*, 14(5), 563–577.

**Trenkler, C.** (2004). *Determining p-values for systems cointegration tests
with a prior adjustment for deterministic terms*. Discussion Paper 2004-37,
Center for Applied Statistics and Economics, Humboldt-Universität zu Berlin.
— Response-surface approximations giving arbitrary p-values, for statistics of
the Saikkonen–Lütkepohl family rather than of the Johansen family.

## Data sources

**Chan, W.-Y. T. and Wallis, K. F.** (1978). Multiple time series modelling:
another look at the mink–muskrat interaction. *Journal of the Royal Statistical
Society, Series C (Applied Statistics)*, 27(2), 168–175.

**Hillmer, S. C. and Tiao, G. C.** (1979), as above, for the Census housing
series used in Mauricio (2005).

**Johansen, S. and Juselius, K.** (1990). Maximum likelihood estimation and
inference on cointegration, with applications to the demand for money. *Oxford
Bulletin of Economics and Statistics*, 52(2), 169–210. — Danish money demand.

**Pfaff, B.** (2008). *Analysis of Integrated and Cointegrated Time Series with
R*, 2nd edition. Springer, New York. — The `urca` package, used for external
comparison of rank decisions.

**Seabold, S. and Perktold, J.** (2010). statsmodels: econometric and
statistical modeling with Python. *Proceedings of the 9th Python in Science
Conference*. — Used for the Johansen comparison.

## Numerical methods

**Dennis, J. E. and Schnabel, R. B.** (1983). *Numerical Methods for
Unconstrained Optimization and Nonlinear Equations*. Prentice-Hall, Englewood
Cliffs. — The modified Cholesky factorisation and the quasi-Newton framework.

**Anderson, T. W.** (1951). Estimating linear restrictions on regression
coefficients for multivariate normal distributions. *Annals of Mathematical
Statistics*, 22(3), 327–351. — Reduced-rank regression.

## Unpublished material of the project

**Guerrero, D. E. and García-Hiernaux, A.** (2026). *Cointegration analysis with
gradual adjustment dynamics: a unified framework from triangular representation
to behavioral error correction models*. Unpublished manuscript, 2 May 2026.
— The article accompanying the experimental predecessor of this program; the
source of the parametric-bootstrap prescription for the rank test and of the
convergence-operator extension that lies outside the present scope.

**Treadway, A. B.** (2001). *DRVUS: manual de usuario*. Unpublished user manual.
— The origin and the authoritative description of the `.inp` file format.

The estimation suite of which this program forms part — comprising the
univariate, transfer-function and stationary multivariate programs — is
unpublished software by A. B. Treadway, J. A. Mauricio and D. E. Guerrero
(1995–2026).
