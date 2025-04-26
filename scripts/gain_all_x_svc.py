#!/usr/bin/python
``` 
set gain all channels by array, either on SITE or, if SITE==0, all sites with gain
sample data:
set.site 0 gain_all_x 000000000000000300000000000000030000000000000003
set.site 0 gain_all_x 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3
```
import glob
import os
import re
import select

import epics

HN = os.getenv("IOC_HOST")
print(HN)

SFIFOS = glob.glob('/etc/acq400/[1-6]/gain_all_x')
MFIFO = glob.glob('/etc/acq400/0/gain_all_x')

print(SFIFOS)
print(MFIFO)

FIFO_PATHS=MFIFO
FIFO_PATHS.extend(SFIFOS)
print(FIFO_PATHS)

FIFOS = {}
PVS = {}
NCHAN = {}
SEQ = 0
SSITES = []

ssites = []
for path in SFIFOS:
    ssites.append(int(path.split('/')[3]))

SSITES = sorted(ssites)
print(SSITES)

for site in SSITES:
    pvname = f'{HN}:{site}:GAIN:ALL:X'
    print(f'create PV {pvname}')
    PVS[site] = epics.PV(pvname)
    with open(f'/etc/acq400/{site}/NCHAN', 'r') as fd:
        NCHAN[site] = int(fd.readline().strip())
     

def handle_site(site, gains):
    global PVS, NCHAN, SEQ, SSITES
    SEQ = (SEQ+1)%256
#    print(f'handle_site {SEQ} {site} {gains}')
    if site == 0:
        c0 = 0
        for site in SSITES:
            cn = c0 + NCHAN[site]  
            handle_site(site, gains[c0:cn])
            c0 = cn
    else:
        ar = [ SEQ, ]
        ar.extend(gains)
        print(f'put[{site}]({ar})')
        PVS[site].put(ar)
        


poller = select.poll()

for path in FIFO_PATHS:
    site = int(path.split('/')[3])
    print(f'path {path} rb')
    fd = os.open(path, os.O_RDWR)
    poller.register(fd, select.POLLIN)
    FIFOS[fd] = site

print('into event loop')

while True:
    for fd, event in poller.poll():
         if event & select.POLLIN:
            site = FIFOS[fd]
            print(f'request read from {site}')
            txt = os.read(fd, 1024)
            print(f'read {txt}')
            string_data = txt.decode('utf-8')
            gains = re.split(r'[ ,]', string_data)
#            gains = string_data.split(r'[ ,]')
            if len(gains) == 1:
                gains = list(string_data)
            handle_site(site, [int(x) for x in gains])
         else:
            print(f'empty event {event}')


         
   


