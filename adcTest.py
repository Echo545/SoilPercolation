# Analog to Digital Converter Test using MCP3008
from MCP3008 import MCP3008
import time

adc = MCP3008()

while True:

    value = adc.read( channel = 0 )
    print("Applied voltage: %.2f" % (value / 1023.0 * 3.3) )
    time.sleep(.1)