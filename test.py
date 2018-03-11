from random import choice, randint

sizes = [1,2,4,8]

def getAddr():
    s = choice(sizes)
    a = randint(0, 1 << 26)
    addrs.append(a)
    return int(a / s) * s, s


def getSameAddr(addrs):
    a = choice(addrs)
    s = choice(sizes)
    return int(a / s) * s, s


def getRecord(i, getAddr, fh):
    a = getAddr()
    tick = randint(0,3)
    st = randint(0,1)
    if st == 1:
        if a[1] == 1:
            fh.write('{0} {1} {2} {3} {4} {5}\n'.format(
                tick, st, hex(a[0]), i+1, a[1], hex(i+1)))
        elif a[1] == 2:
            fh.write('{0} {1} {2} {3} {4} {5} 0x{6}\n'.format(
                tick, st, hex(a[0]), i+1, a[1], hex(i+1), 0))
        elif a[1] == 4:
            fh.write('{0} {1} {2} {3} {4} {5} 0x{6} 0x{7} 0x{8}\n'.format(
                tick, st, hex(a[0]), i+1, a[1], hex(i+1), 0, 0, 0))
        elif a[1] == 4:
            fh.write('{0} {1} {2} {3} {4} {5} 0x{6} 0x{7} 0x{8} 0x{9} 0x{10} 0x{11} 0x{12}\n'.format(
                tick, st, hex(a[0]), i+1, a[1], hex(i+1), 0, 0, 0, 0, 0, 0, 0))
    else:
        fh.write('{0} {1} {2} {3} {4}\n'.format(
            tick, st, hex(a[0]), i+1, a[1]))


addrs = []


for j in range(10):
    file = "test"+ str(j) +".txt"
    fh = open(file,"w")
    for i in range(25):
        getRecord(i, getAddr, fh)
    for i in range(25,50):
        getRecord(i, lambda: getSameAddr(addrs), fh)
    fh.close()

