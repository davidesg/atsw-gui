suppressMessages(library(urca))
# trace test: stat[k] corresponds to hypothesis r <= m-k ; cval[k,2] its 5% cv
# rank = smallest r that is NOT rejected (start r=0, increment while rejected)
rank5 <- function(stat, cval5) {
  m <- length(stat)
  r <- 0
  while (r < m) {
    k <- m - r
    if (stat[k] > cval5[k]) r <- r + 1 else break
  }
  r
}
cases <- c("urca_denmark","urca_finland","urca_nporg","urca_npext","urca_UKpppuip","vars_Canada",
  "urca_Raotbl1","urca_Raotbl2","urca_Raotbl3","urca_Raotbl4","urca_Raotbl5","urca_Raotbl6","urca_Raotbl7",
  "urca_UKconinc","urca_UKconsumption","urca_ecb")
for (nm in cases) {
  d <- read.csv(file.path("datasets", paste0(nm, ".csv")))
  num <- d[sapply(d, is.numeric)]
  tj <- tryCatch(summary(ca.jo(num, type = "trace", K = 2, ecdet = "const", spec = "longrun")), error = function(e) NULL)
  if (is.null(tj)) { cat(sprintf("%-22s %3d x %2d  NA\n", nm, nrow(num), ncol(num))); next }
  st <- tj@teststat; cv <- tj@cval[, 2]
  r <- rank5(st, cv)
  cat(sprintf("%-22s %3d x %2d  r=%d  trace=[%s]\n", nm, nrow(num), ncol(num), r, paste(round(st, 2), collapse = ",")))
}
