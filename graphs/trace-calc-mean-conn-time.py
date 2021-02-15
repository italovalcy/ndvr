import sys
import numpy as np

# gen irp file: ./bin/bm InRangePrinter -f DDSN -r 60

filename = sys.argv[1]
num_nodes = int(sys.argv[2])

if len(sys.argv) > 3:
    max_duration = float(sys.argv[3])
else:
    max_duration = float('inf')

conn_list = {}
conn_time = []

f = open(filename, "r")
for line in f:
    elements = line.replace(' [', ',[').replace('][', '],[').split(',')
    time = float(elements[0])
    if time > max_duration:
        break
    for i in range(0, num_nodes):
        irp = elements[i+1].split(' ')[1:-1]
        # according to BonnMotion InRangePrint documentation, the node itself is ommited
        irp.insert(i, 0)
        for j in range(0, num_nodes):
            idx = '%d<->%d' % ((i,j) if i<j else (j,i))
            if irp[j] == '1':
                if idx not in conn_list:
                    conn_list[idx] = time
            else:
                if idx in conn_list:
                    delta = time - conn_list[idx]
                    #print "%s\t%s\t%.3f (start_conn_at=%f)\t%s" % (time, idx, delta, conn_list[idx], conn_list.keys())
                    print "%s\t%s\t%.3f (start_conn_at=%f)" % (time, idx, delta, conn_list[idx])
                    conn_time.append(delta)
                    del conn_list[idx]

print "avg conn time = ", np.mean(conn_time)
