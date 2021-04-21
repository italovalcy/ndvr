cd ~/ndvr
cp scenarios/sbrc2021/run-*.sh ~/mini-ndn/
sudo cp scenarios/sbrc2021/nlsr.conf.sample-* /usr/local/etc/ndn/
cd ~/mini-ndn 
mkdir ~/results

# apply changes on NLSR config
cp /usr/local/etc/ndn/nlsr.conf.sample-int1 /usr/local/etc/ndn/nlsr.conf.sample
for i in $(seq 1 10); do for type in run-ndvr-*.sh run-nlsr-*.sh; do ./$type; sleep 60; done; done 

# compute data for Latency and Loss
for run in ~/results/ndvr-*/2021* ~/results/nlsr-*/2021*; do echo "####### $run";  python ~/ndvr-emu/graphs/process_log_ndnping.py --log $(ls -1  $run/*/ping-data/*.txt | egrep -w -v "ce|wu") --output $run/data-ndnping.json; done | tee /tmp/summary-latency
awk 'BEGIN{type=""}{OFS="\t"; if ($0 ~ /#####/ ) {split($2,a,"/"); if (a[5] != type) {print a[5]; type=a[5]}} else {print $13,$17,$15,"","","","",$9}}' /tmp/summary-latency

# compute data for CPU
cd ~/
for run in ~/results/ndvr-*/2021* ~/results/nlsr-*/2021*; do echo "####### $run"; START_TIME=$(grep 'Each node will ping' $run/mndn.log | cut -d, -f1 | cut -d' ' -f2); python ~/calc_cpu_usage.py --log $run/top --start_time $START_TIME --max_count 180 --output $run/data-top.json ; done | tee /tmp/summary-cpu
awk 'BEGIN{type=""}{OFS="\t"; if ($0 ~ /#####/ ) {split($2,a,"/"); if (a[5] != type) {print a[5]; type=a[5]}} else {print $NF}}' /tmp/summary-cpu

# compute data for overhead:
for run in ~/results/ndvr-*/2021* ~/results/nlsr-*/2021*; do echo "####### $run"; python ~/ndvr-emu/graphs/process_log_nfd_mndn.py --log $(ls -1  $run/*/log/nfd.log | egrep -w -v "ce|wu") --output $run/data-nfd.json; done | tee /tmp/summary-nfd
awk 'BEGIN{type=""}{OFS="\t"; if ($0 ~ /#####/ ) {split($2,a,"/"); if (a[5] != type) {print a[5]; type=a[5]}} else {tot=$3+$5+$7+$9; proto=$7+$9; print $7,$9,100.0*proto/tot}}' /tmp/summary-nfd

# TODO: document factorial project spreeadsheet
# TODO: document graphs plotting
# TODO: document the experiments using NLSR with 30s intervals (/usr/local/etc/ndn/nlsr.conf.sample-int30)
