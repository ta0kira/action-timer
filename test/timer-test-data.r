#!/usr/bin/env Rscript

standard_out <- stdout()
sink(stderr())

l     <- 10000
count <- 100000

for (log.min.sleep in 2:6) {
  # NOTE: Calling format with all three at once will turn l and count into
  # floating-points.
  args <- sapply(c(l,count,0.1^log.min.sleep),function(x) format(x,scientific=FALSE))
  command <- paste(c('./timer-test-data',args),collapse=' ')
  write(command,file=standard_out)

  data.raw <- system(command,intern=TRUE)
  data <- data.frame(do.call(rbind,lapply(data.raw,function(x) as.numeric(strsplit(x,',')[[1]]))))
  names(data) <- c('expected','actual')
  data$error <- data$expected-data$actual

  # NOTE: The mean isn't subtracted here, since they're supposed to be the same.
  data.mat <- as.matrix(data)
  data.norm <- data.mat%*%diag(1/sqrt(colSums(data.mat^2)))
  expected.vs.actual <- (t(data.norm[,1])%*%data.norm[,2])^2
  write(paste('  r-squared expected vs. actual:',format(expected.vs.actual)),file=standard_out)

  actual.vs.error <- cor(data$actual,data$error)^2
  write(paste('  r-squared actual vs. error:   ',format(actual.vs.error)),file=standard_out)

  mean.diff <- mean(data$error)
  write(paste('  mean expected vs. actual:     ',format(mean.diff)),file=standard_out)

  png(paste('timer-data-plot',log.min.sleep,'.png',sep=''),width=800,height=800)
  lim <- max(quantile(data$expected,0.99),quantile(data$actual,0.99))
  plot(data[c(1,2)],pch='.',cex=3,xlim=c(0,lim),ylim=c(0,lim),asp=1,col=rgb(0,0,0,0.05))
  lines(c(0,1),c(0,1),col='red',lwd=2)
  dev.off()

  png(paste('timer-data-hist',log.min.sleep,'.png',sep=''),width=800,height=800)
  hist(data$actual,xlim=c(0,quantile(data$actual,0.99)),breaks=500,freq=FALSE)
  lines.x <- (0:1000)/1000*quantile(data$actual,0.999)
  lines(lines.x,exp(-lines.x*l)*l,col='red',lwd=3)
  l.actual <- 1/mean(data$actual)
  lines(lines.x,exp(-lines.x*l.actual)*l.actual,col='green',lwd=3)
  dev.off()
}
