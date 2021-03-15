import sys
import matplotlib.pyplot as plt
import numpy as np

class Parser:
    def __init__(self, filepath):
        print('openning ' + filepath)
        self.fd = open(filepath, 'r')
        self.add_start = []
        self.add_end = []
        self.read_start = []
        self.read_end = []
        self.last_read = (0, 0)
        self.last_add = (0, 0)

    def parse(self):
        for line in self.fd:
            arr = line.strip().split()
            if arr[0] == 'Adding':
                self.parse_add(arr)
            elif arr[0] == 'Reading':
                self.parse_read(arr)
    
    def parse_add(self, arr):
        [start, end] = arr[9].split('-')
        self.last_add = (int(start), int(end))

    def parse_read(self, arr):
        samples = int(arr[1])
        start = int(arr[5])
        self.last_read = (start, start + samples)
        if self.last_add[1] == 0:
            return
        self.add_start.append(self.last_add[0])
        self.add_end.append(self.last_add[1])
        self.read_start.append(self.last_read[0])
        self.read_end.append(self.last_read[1])

    def log(self):
        for i in range(len(self.add_start)):
            print(f"({self.add_start[i]}-{self.add_end[i]}) ({self.read_start[i]}-{self.read_end[i]})")
    
    def plot(self):
        t = np.arange(len(self.add_start))
        # red dashes, blue squares and green triangles
        plt.plot(
            t, self.add_start, 'r', 
            t, self.add_end, 'g', 
            t, self.read_start, 'b', 
            t, self.read_end, 'y')
        plt.show()

if __name__ == '__main__':
    parser = Parser(sys.argv[1])
    parser.parse()
    parser.plot()
