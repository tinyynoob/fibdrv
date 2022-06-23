reset                                                                           
set ylabel 'number of decimal digits'
set xlabel 'n'
set title 'The length of Fibonacci numbers'
set term png enhanced font 'Times_New_Roman, 10'
set output 'digit.png'
#set xtic 1000
set xtics rotate by 45 right
set datafile separator ","
set key left

plot \
"digit.csv" using 1:2 with linespoints linewidth 1 title "length of fib(n)", \
