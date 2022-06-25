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

plot \
"fast_ori.csv" using 1:3 with linespoints linewidth 1 title "with original ubignum-sub()", \
"fast_pos.csv" using 1:3 with linespoints linewidth 1 title "with in-place ubignum-sub()", \
