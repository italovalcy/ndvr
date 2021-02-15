import sys
import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
data = []
f = open(sys.argv[1])
for l in f:
    data.append(int(l.strip()))
confidence = 0.95
mean = np.average(data)
intv = sem(data) * t.ppf((1 + confidence) / 2, len(data) - 1)
print(mean,intv)
