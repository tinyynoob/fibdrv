reset                                                                           
set xlabel 'length of the string copied'
set ylabel 'average time (ns)'
set title 'time measurement'
set term png enhanced font 'Times_New_Roman, 10'
set output 'time.png'
#set xtic 1000
set xtics rotate by 45 right
set datafile separator ","
set key left

plot \
"fast.csv" using 1:2 with linespoints linewidth 1 title "copy-to-user consumption", \
