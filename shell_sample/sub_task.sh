sleep 5 > /dev/null 2>&1

export gpuid=$7

script_task=$1
script_file=/home/task/$6/script/${script_task:0:8}/script_$1_$2.py
python_exec=/usr/bin/python27
output_file=/home/task/$6/user/${script_task:0:8}/$1/run_$2.log

echo ---------------------------- start ---------------------------- >> $output_file 2>&1

echo `date +%Y-%m-%d\ %H:%M:%S` >> $output_file 2>&1

echo "run.sh stop resource_monitor first time begin" >> $output_file 2>&1
ps -eo pid,cmd | grep resource_monitor | grep -v grep | awk '{print $1}' | xargs kill -9 > /dev/null 2>&1
echo "run.sh stop resource_monitor first time end" >> $output_file 2>&1

if [ ! -f $script_file ]; then
    echo "error: $script_file is not exist" >> $output_file 2>&1
    exit 101
fi

if [ ! -s $script_file ]; then
    echo "error: $script_file is empty" >> $output_file 2>&1
    exit 102
fi

echo $python_exec $script_file \"$1\" \"$2\" \"$3\" \"$4\" \"$5\" \"$8\" >> $output_file 2>&1

date_time_beg=`date +%s`

$python_exec $script_file \"$1\" \"$2\" \"$3\" \"$4\" \"$5\" \"$8\" >> $output_file 2>&1

result=$?

date_time_end=`date +%s`

min_delta_time=5
delta_time=$((date_time_end - date_time_beg))

echo "result is $result" >> $output_file 2>&1

echo "run.sh stop resource_monitor last time begin" >> $output_file 2>&1
ps -eo pid,cmd | grep resource_monitor | grep -v grep | awk '{print $1}' | xargs kill -9 > /dev/null 2>&1
echo "run.sh stop resource_monitor last time end" >> $output_file 2>&1

if [ ! -f $output_file ]; then
    echo "error: $output_file is not exist" >> $output_file 2>&1
    exit 103
fi

if [ $delta_time < $min_delta_time ]; then
    echo "error: script execute time $delta_time is too short" >> $output_file 2>&1
    exit 104
fi

echo "exit with $result" >> $output_file 2>&1

exit $result
