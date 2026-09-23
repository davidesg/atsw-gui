suppressMessages(library(urca))
rd <- function(f){ x <- readLines(f); i <- grep("^<",x); read.table(text=x[(i+1):length(x)], header=TRUE)}
D <- "/home/david/Dropbox/SRC/drvec/datasets/lutkepohl/"
e1 <- log(rd(paste0(D,"e1.dat"))); cat("e1 nobs", nrow(e1), "\n")
e3 <- rd(paste0(D,"e3.dat")); cat("e3 nobs", nrow(e3), "\n"); e3$M1 <- log(e3$M1); e3$gnp <- log(e3$gnp)
for (nm in c("e1","e3")) { d <- get(nm)
 for (ec in c("const","none")) {
  t <- ca.jo(d, type="trace", K=2, ecdet=ec, spec="transitory")
  cat("\n==", nm, "ecdet=",ec," K=2\n"); cat("trace:", rev(round(t@teststat,3)), "\n")
  e <- ca.jo(d, type="eigen", K=2, ecdet=ec, spec="transitory"); cat("eigen:", rev(round(e@teststat,3)), "\n")
  cat("lambda:", round(t@lambda,5), "\n")
  print(round(t@cval,2))
  cat("beta1 normalized on var1:\n"); print(round(t@V[,1]/t@V[1,1],5))
  cat("alpha1 (scaled):\n"); print(round(t@W[,1]*t@V[1,1],5))
 }}
# e1 subsample 1960-1978 as in Lutkepohl ch.3 (76 obs)
d <- e1[1:76,]; t <- ca.jo(d, type="trace", K=2, ecdet="const"); cat("\ne1 1960-78 const trace:", rev(round(t@teststat,3)),"\n")
