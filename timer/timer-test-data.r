#!/usr/bin/env Rscript

standard_out <- stdout()
sink(stderr())

l     <- 10000
count <- 100000

for (log.min.sleep in 3:6) {
  # NOTE: Calling format with all three at once will turn l and count into
  # floating-points.
  args <- sapply(c(l,count,0.1^log.min.sleep),function(x) format(x,scientific=FALSE))
  command <- paste(c('./timer-test-data',args),collapse=' ')
  write(command,file=standard_out)

  data.raw <- system(command,intern=TRUE)
  data <- data.frame(do.call(rbind,lapply(data.raw,function(x) as.numeric(strsplit(x,',')[[1]]))))
  names(data) <- c('expected','actual')
  data$diff <- data$expected-data$actual

  r.squared <- cor(data)^2
  write(capture.output(print(r.squared)),file=standard_out)

  png(paste('timer-data-plot',log.min.sleep,'.png',sep=''),width=800,height=800)
  plot(data[c(1,2)],pch='.',xlim=c(0,quantile(data$expected,0.99)),ylim=c(0,quantile(data$actual,0.99)),asp=1)
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
