import numpy as np
import matplotlib.pyplot as plt
import sys

data = {}
for line in open(sys.argv[1]):
    elements = line.split(' ')
    time = float(elements[0][1:-1])
    x = data.get(int(time), 0) + 1
    data[int(time)] = x

plt.plot(data.keys(), data.values(), 'ro')
#plt.axis(sorted(data.keys()))
plt.show()
