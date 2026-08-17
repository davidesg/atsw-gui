# References — cointegration benchmark bibliography

All DOIs below were verified against the Semantic Scholar / Crossref registry on
2026-08-13. Each reference is the source of at least one dataset in `datasets/`
and one benchmark case in `benchmark/`.

## Primary

1. Johansen, S. and Juselius, K. (1990). "Maximum likelihood estimation and
   inference on cointegration — with applications to the demand for money."
   *Oxford Bulletin of Economics and Statistics*, 52(2), 169–210.
   DOI: `10.1111/j.1468-0084.1990.mp52002003.x`

2. Johansen, S. and Juselius, K. (1992). "Testing structural hypotheses in a
   multivariate cointegration analysis of the PPP and the UIP for UK."
   *Journal of Econometrics*, 53, 211–244.
   DOI: `10.1016/0304-4076(92)90086-7`

3. Mauricio, J.A. (2006). "Exact maximum likelihood estimation of partially
   nonstationary vector ARMA models." *Computational Statistics & Data
   Analysis*, 50, 3644–3662.
   DOI: `10.1016/j.csda.2005.07.012`

4. Lütkepohl, H. (2005). *New Introduction to Multiple Time Series Analysis.*
   Springer, Berlin.
   DOI: `10.1007/978-3-540-27752-1`

5. Pfaff, B. (2008). *Analysis of Integrated and Cointegrated Time Series with
   R.* Springer, New York.
   DOI: `10.1007/978-0-387-75967-8`

## Supporting

6. Nelson, C.R. and Plosser, C.I. (1982). "Trends and random walks in
   macroeconomic time series: some evidence and implications." *Journal of
   Monetary Economics*, 10, 139–162.
   DOI: `10.1016/0304-3932(82)90012-5`

7. Schotman, P.C. and van Dijk, H.K. (1991). "On Bayesian routes to unit
   roots." *Journal of Applied Econometrics*, 6, 387–401.
   DOI: `10.1002/jae.3950060406`

8. Rao, B. Bhaskara (ed.) (1994). *Cointegration for the Applied Economist.*
   St. Martin's Press, New York. (Dataset appendix tables D.1–D.6.)

9. Pokorny, M. (1987). *An Introduction to Econometrics.* Basil Blackwell,
   p. 408.

10. Jones, J.W. (1914). "Fur-farming in Canada." Commission of Conservation,
    Canada, pp. 209–214. (Source of the Mink–Muskrat data.)

11. Chan, W.-Y.T. and Wallis, K.F. (1978). "Multiple time series modelling:
    another look at the mink–muskrat interaction." *Journal of the Royal
    Statistical Society, Series C (Applied Statistics)*, 27(2), 168–175.
    JSTOR: `2346944`. (Independent exact-ML calibration of the Mink–Muskrat
    residual covariance; see `benchmark/README.md`. PDF in `literature/`.)

12. Jenkins, G.M. (1975). "The interaction between the muskrat and mink cycles
    in North Canada." *Proceedings of the 8th International Biometric
    Conference*, 55–71. Editura Academiei Republicii Socialiste Romania.
    (Cited via Chan & Wallis 1978; not held locally.)

13. Osborn, D.R. (1977). "Exact and approximate maximum likelihood estimators
    for vector moving average processes." *Journal of the Royal Statistical
    Society, Series B*, 39, 114–118. (The exact-ML vector-MA method Chan &
    Wallis used; cited via them, not held locally.)

## Software (reference implementation)

- R 4.3.3 (2024-02-29)
- `urca` 1.3.4 — `ca.jo()` (Johansen trace/eigen tests)
- `vars` 1.6.1 — `Canada` dataset, `ca.jo()` interface
- `tseries` 0.10.58

## BibTeX

```bibtex
@article{johansen1990,
  author  = {Johansen, S{\o}ren and Juselius, Katarina},
  title   = {Maximum likelihood estimation and inference on cointegration ---
             with applications to the demand for money},
  journal = {Oxford Bulletin of Economics and Statistics},
  volume  = {52}, number = {2}, pages = {169--210}, year = {1990},
  doi     = {10.1111/j.1468-0084.1990.mp52002003.x}
}

@article{johansen1992,
  author  = {Johansen, S{\o}ren and Juselius, Katarina},
  title   = {Testing structural hypotheses in a multivariate cointegration
             analysis of the PPP and the UIP for UK},
  journal = {Journal of Econometrics},
  volume  = {53}, pages = {211--244}, year = {1992},
  doi     = {10.1016/0304-4076(92)90086-7}
}

@article{mauricio2006,
  author  = {Mauricio, Jos{\'e} Alberto},
  title   = {Exact maximum likelihood estimation of partially nonstationary
             vector ARMA models},
  journal = {Computational Statistics & Data Analysis},
  volume  = {50}, pages = {3644--3662}, year = {2006},
  doi     = {10.1016/j.csda.2005.07.012}
}

@book{lutkepohl2005,
  author    = {L{\"u}tkepohl, Helmut},
  title     = {New Introduction to Multiple Time Series Analysis},
  publisher = {Springer}, address = {Berlin}, year = {2005},
  doi       = {10.1007/978-3-540-27752-1}
}

@book{pfaff2008,
  author    = {Pfaff, Bernhard},
  title     = {Analysis of Integrated and Cointegrated Time Series with R},
  publisher = {Springer}, address = {New York}, year = {2008},
  doi       = {10.1007/978-0-387-75967-8}
}

@article{nelson1982,
  author  = {Nelson, Charles R. and Plosser, Charles I.},
  title   = {Trends and random walks in macroeconomic time series: some
             evidence and implications},
  journal = {Journal of Monetary Economics},
  volume  = {10}, pages = {139--162}, year = {1982},
  doi     = {10.1016/0304-3932(82)90012-5}
}

@article{schotman1991,
  author  = {Schotman, Peter C. and van Dijk, Herman K.},
  title   = {On Bayesian routes to unit roots},
  journal = {Journal of Applied Econometrics},
  volume  = {6}, pages = {387--401}, year = {1991},
  doi     = {10.1002/jae.3950060406}
}

@book{rao1994,
  editor    = {Rao, B. Bhaskara},
  title     = {Cointegration for the Applied Economist},
  publisher = {St. Martin's Press}, address = {New York}, year = {1994}
}

@book{pokorny1987,
  author    = {Pokorny, Michael},
  title     = {An Introduction to Econometrics},
  publisher = {Basil Blackwell}, year = {1987}
}

@incollection{jones1914,
  author  = {Jones, J. W.},
  title   = {Fur-farming in Canada},
  booktitle = {Commission of Conservation}, pages = {209--214}, year = {1914}
}

@article{chanwallis1978,
  author  = {Chan, W.-Y. T. and Wallis, Kenneth F.},
  title   = {Multiple Time Series Modelling: Another Look at the
             Mink--Muskrat Interaction},
  journal = {Journal of the Royal Statistical Society, Series C
             (Applied Statistics)},
  volume  = {27}, number = {2}, pages = {168--175}, year = {1978}
}
```
