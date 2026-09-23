# Johansen reference via urca::ca.jo, emitted as JSON for bench.py
# usage: Rscript johansen.R data.csv K ecdet season(0|4) out.json [dumvar.csv]
suppressMessages({library(urca); library(jsonlite)})
a <- commandArgs(TRUE)
Y <- as.matrix(read.csv(a[1]))
K <- as.integer(a[2]); ecdet <- a[3]; season <- as.integer(a[4]); out <- a[5]
dv <- NULL
if (length(a) >= 6) dv <- as.matrix(read.csv(a[6]))
seas <- if (season > 0) season else NULL
tr <- ca.jo(Y, type = "trace", ecdet = ecdet, K = K, spec = "longrun",
            season = seas, dumvar = dv)
ei <- ca.jo(Y, type = "eigen", ecdet = ecdet, K = K, spec = "longrun",
            season = seas, dumvar = dv)
M <- ncol(Y)
lam <- tr@lambda[1:M]
T <- nrow(tr@R0)
S00 <- crossprod(tr@R0) / T
ll <- sapply(0:M, function(r) {
  -T/2 * (M * log(2*pi) + M + log(det(S00)) + (if (r > 0) sum(log(1 - lam[1:r])) else 0))
})
# ca.jo reports statistics in reverse order (r<=M-1 first); flip to r=0..M-1
res <- list(
  M = M, T = T, K = K, ecdet = ecdet, season = season,
  lambda = lam,
  trace = rev(tr@teststat), cv_trace = tr@cval[nrow(tr@cval):1, , drop = FALSE],
  eigen = rev(ei@teststat), cv_eigen = ei@cval[nrow(ei@cval):1, , drop = FALSE],
  V = unclass(tr@V), W = unclass(tr@W), rownamesV = rownames(tr@V),
  logL = ll)
writeLines(toJSON(res, digits = 12, matrix = "rowmajor", auto_unbox = TRUE), out)
