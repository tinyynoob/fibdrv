reset                                                                           
set xlabel 'n'
set ylabel 'average time at converting (ns)'
set title 'time measurement'
set term png enhanced font 'Times_New_Roman, 10'
set output 'time.png'
#set xtic 1000
set xtics rotate by 45 right
set datafile separator ","
set key left
set grid

plot \
"fast_ori.csv" using 1:3 with linespoints linewidth 1 title "old ubignum-divby-ten()", \
"fast_pos.csv" using 1:3 with linespoints linewidth 1 title "with memory-pool improvement", \