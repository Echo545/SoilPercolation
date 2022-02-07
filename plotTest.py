#! /usr/bin/env python3
import serial
import time
import os
import threading
import queue
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib import style
import datetime
import re

global log
log = 0 # set log to 1 to save a log in RAM.

if os.path.exists('/dev/ttyACM0') == True:
    ser = serial.Serial(port='/dev/ttyACM0',baudrate = 9600,parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,bytesize=serial.EIGHTBITS,timeout=1)
if os.path.exists('/dev/ttyACM1') == True:
     ser = serial.Serial(port='/dev/ttyACM1',baudrate = 9600,parity=serial.PARITY_NONE,
     stopbits=serial.STOPBITS_ONE,bytesize=serial.EIGHTBITS,timeout=1)

if os.path.exists('/run/shm/example.txt'): # note log currently saved to RAM
   os.remove('/run/shm/example.txt')       # and gets deleted as you start the script

def thread_read():
   global ser
   global xs,ys, avg, ravg, rmax, max_count
   max_count = 1000
   axis = 0
   a = 0
   xs = []
   ys = []
   avg = []
   ravg = []
   rmax = []
   while True:
       #read data from arduino
       myData2 = ser.readline()
       myData2 = myData2.decode("utf-8","ignore")
       myData2 = re.findall("\d+\.\d+", myData2)
       if len(myData2) > 0 :
          a = float(myData2[0])
       # check incoming data
       if a > 0 :
           # save to log file
           if log == 1:
               now = datetime.datetime.now()
               timestamp = now.strftime("%y/%m/%d-%H:%M:%S")
               with open('/run/shm/example.txt', 'a') as f:
                   f.write(timestamp + "," + str(axis)+"," + str(a) + "\n")
           # write to lists
           xs.append(axis)
           ys.append(a)
           # delete old list values
           if len(xs) > max_count:
               del xs[0]
               del ys[0]
               del avg[0]
               del ravg[0]
               del rmax[0]
           # calculate average
           Sum = sum(ys)
           avg.append(Sum/len(ys))
           # check maximum value
           rmax.append(max(ys))
           # calculate rolling average
           if len(ys) > int(max_count/10):
               za = ys[len(ys)-int(max_count/10):len(ys)]
               Sum2 = sum(za)
               ravg.append(Sum2/int(max_count/10))
           else:
               Sum = sum(ys)
               ravg.append(Sum/len(ys))

       axis +=1

def thread_plot():
       global fig,animate, ax1
       fig = plt.figure()
       ax1 = fig.add_subplot(1,1,1)
       ani = animation.FuncAnimation(fig, animate, interval=10)
       plt.show()

def animate(i):
       global xs,ys,avg,ravg,rmax,max_count
       ax1.clear()
       plt.xlabel('Counter')
       plt.ylabel('Value')
       ax1.set_label('Label via method')
       ax1.plot(xs, ys, '-b', label='Actual')
       ax1.plot(xs, avg, '--r', label='Average')
       ax1.plot(xs, ravg, '-g', label='Rolling Average')
       ax1.plot(xs, rmax, '--y', label='Max')
       ax1.legend(loc='lower left')

read_thread = threading.Thread(target=thread_read)
read_thread.start()

plot_thread = threading.Thread(target=thread_plot)
plot_thread.start()
