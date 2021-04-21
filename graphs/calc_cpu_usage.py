import re
import sys
import argparse
import json

parser = argparse.ArgumentParser(description='Process log file to parse data')
parser.add_argument('--log', metavar='FILE', required=True,
                    help='filename containing the logs')
parser.add_argument('--output', metavar='FILE', required=False,
                    help='Filename to save the results')
parser.add_argument('--max_count', type=int, required=False, default=float('inf'),
                    help='max count')
parser.add_argument('--start_time', required=False,
                    help='start time')
parser.add_argument('--debug', action="store_true", default=False, help='print more details')

args = parser.parse_args()

content = open(args.log).read()

start_seconds = 0
if args.start_time:
    (h,m,s) = [int(x) for x in args.start_time.split(":")]
    first_h = int(content[6:8])
    if h < first_h:
        h = h + 24
    start_seconds = s + m*60 + h*60*60

total_ticks = 0
sum_tot_time = 0.0
data = {}
last_time = None
prev_h = 0
for record in content[6:].split("\n\ntop - "):
    count = 0
    cur_time = {}
    for line in record.split("\n"):
        elements = re.split(" +", line.strip())
        if elements[-1] in ["ndvrd", "nlsr"] and elements[7] != "Z":
            count += 1
            a = [int(x) for x in re.split("[:.]", elements[-2])]
            cur_time[elements[0]] = a[0]*60 + a[1] + a[2]/100.0
    if last_time is None:
        last_time = cur_time
        continue
    tot_time = 0.0
    for k in cur_time:
        if k not in last_time:
            continue
        tot_time += cur_time[k] - last_time[k]
    ticks = tot_time*100

    time_str = record[:8]
    (h,m,s) = [int(x) for x in time_str.split(":")]
    if h < prev_h:
        h = h + 24
    seconds = s + m*60 + h*60*60
    if seconds >= start_seconds and seconds < start_seconds + args.max_count:
        delta = seconds - start_seconds + 1
        data[delta] = {'time': time_str, 'num_process': count, 'tot_time': tot_time, 'ticks': ticks}
        if args.debug:
            print("%d %s num_process=%s tot_time=%.2f cpu_ticks=%.0f" % (delta, time_str, count, tot_time, ticks))
        total_ticks += ticks
        sum_tot_time += tot_time
    last_time = cur_time
    prev_h = h

if args.output:
    f = open(args.output, "w")
    f.write(json.dumps(data))
    
print("SUMMARY: total_time %.2f total_ticks %d" % (sum_tot_time, total_ticks))
