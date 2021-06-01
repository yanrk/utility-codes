date_time_beg=`date +%s`
sleep $1 > /dev/null
date_time_end=`date +%s`
time_delta=$((date_time_end - date_time_beg))
echo $time_delta seconds
