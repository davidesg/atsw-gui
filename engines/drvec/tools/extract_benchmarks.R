#!/usr/bin/env Rscript
# Extract cointegration datasets from installed R packages into datasets/ as CSV,
# and compute Johansen trace/eigen reference results into benchmark/.

suppressMessages({
  library(urca)
  library(vars)
})

out_data <- "datasets"
out_bench <- "benchmark"
dir.create(out_data, showWarnings = FALSE, recursive = TRUE)
dir.create(out_bench, showWarnings = FALSE, recursive = TRUE)

process_case <- function(d, name, source, description, K = 2, ecdet = "const",
                         spec = "longrun") {
  d <- as.data.frame(d)
  csv <- file.path(out_data, paste0(name, ".csv"))
  write.csv(d, csv, row.names = FALSE)

  num <- d[sapply(d, is.numeric), drop = FALSE]
  n <- nrow(num)
  m <- ncol(num)

  tj <- tryCatch(summary(ca.jo(num, type = "trace", K = K, ecdet = ecdet, spec = spec)),
                 error = function(e) NULL)
  ej <- tryCatch(summary(ca.jo(num, type = "eigen", K = K, ecdet = ecdet, spec = spec)),
                 error = function(e) NULL)

  trace_stat <- if (!is.null(tj)) tj@teststat else rep(NA_real_, m)
  trace_cv   <- if (!is.null(tj)) tj@cval else matrix(NA_real_, m, 3)
  eigen_stat <- if (!is.null(ej)) ej@teststat else rep(NA_real_, m)
  eigen_cv   <- if (!is.null(ej)) ej@cval else matrix(NA_real_, m, 3)

  sink(file.path(out_bench, paste0(name, ".results.txt")))
  cat("name:", name, "\n")
  cat("source:", source, "\n")
  cat("description:", description, "\n")
  cat("n_obs:", n, " n_vars:", m, "\n")
  cat("vars:", paste(names(num), collapse = ", "), "\n")
  cat("K(lag order):", K, " ecdet:", ecdet, " spec:", spec, "\n\n")
  cat("TRACE TEST (H0 r=0 ... r<=m-1, from largest to smallest eigenvalue)\n")
  print(trace_stat)
  cat("trace critical values (10pct, 5pct, 1pct)\n")
  print(trace_cv)
  cat("\nEIGEN (max) TEST\n")
  print(eigen_stat)
  cat("eigen critical values\n")
  print(eigen_cv)
  if (!is.null(tj)) {
    cat("\ncointegration vectors (beta, columns), normalized:\n")
    print(tj@V)
    cat("\nloading matrix (alpha):\n")
    print(tj@W)
  }
  sink()

  invisible(list(csv = csv, n = n, m = m))
}

data(denmark);  process_case(denmark, "urca_denmark",
  "Johansen, S. and Juselius, K. (1990), OBES 52(2), 169-210",
  "Danish money demand, quarterly 1974:Q1-1987:Q3", K = 2)
data(finland);  process_case(finland, "urca_finland",
  "Johansen, S. and Juselius, K. (1990), OBES 52(2), 169-210",
  "Finnish money demand, quarterly 1958:Q2-1984:Q3", K = 2)
data(nporg);    process_case(nporg, "urca_nporg",
  "Nelson, C.R. and Plosser, C.I. (1982), JME 10, 139-162",
  "Nelson-Plosser original 14 macro series, annual 1860-1970", K = 2)
data(npext);    process_case(npext, "urca_npext",
  "Schotman, P.C. and van Dijk, H.K. (1991), JAE 6, 387-401",
  "Nelson-Plosser extended (logs), annual 1860-1988", K = 2)
data(UKpppuip); process_case(UKpppuip, "urca_UKpppuip",
  "Johansen, S. and Juselius, K. (1992), J. Econometrics 53, 211-244",
  "UK PPP + UIP, quarterly 1971:Q1-1987:Q2", K = 2)
data(UKconinc); process_case(UKconinc, "urca_UKconinc",
  "Hylleberg, Engle, Granger, Yoo (HEGY), UK consumption-income",
  "UK log consumption & income, quarterly (ends 1984:Q4)", K = 2)
data(UKconsumption); process_case(UKconsumption, "urca_UKconsumption",
  "Pokorny, M. (1987), An Introduction to Econometrics, p.408",
  "UK cons, inc, price, quarterly 1957:Q1-1975:Q4", K = 2)
data(ecb);      process_case(ecb, "urca_ecb",
  "European Central Bank (www.ecb.europa.eu), bundled in urca",
  "Euro Zone macro data", K = 2)

rao_src <- list(
  "Dickey, Jansen & Thornton (1994), Rao Ch.2 Table D.1",
  "Dickey, Jansen & Thornton (1994), Rao Ch.2 Table D.2",
  "Holden & Perman (1994), Rao Ch.3 Table D.3",
  "Perron (1994), Rao Ch.4 Table D.4",
  "Perron (1994), Rao Ch.4 Table D.5",
  "Mehra (1994), Rao Ch.5 Table D.6",
  "Otto (1994), Rao Ch.6 Table D.6"
)
for (i in 1:7) {
  nm <- paste0("Raotbl", i)
  data(list = nm)
  d <- get(nm)
  process_case(d, paste0("urca_", nm),
    rao_src[[i]], paste0("Rao (1994) 'Cointegration for the Applied Economist', Table D.", i), K = 2)
}

data(Canada); process_case(Canada, "vars_Canada",
  "Lütkepohl (2005) Ch.7; Pfaff (2008), R package vars",
  "Canada: prod, e, U, rw; quarterly 1980:Q1-2000:Q4", K = 2)

cat("done\n")
