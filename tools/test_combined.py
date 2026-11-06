"""Exercise the real combined G0 protocol/router, fixture and pressure cores on ARM."""
from pathlib import Path
import runpy
import struct
import math

root=Path(__file__).resolve().parents[1]
ArmSerial=runpy.run_path(str(root/'tools/pressure_sensor_host/tests/test_gui_arm_integration.py'))['ArmSerial']
import protocol as P

def fx(kind,seq,mode,payload=b''):
    f=bytearray(48);struct.pack_into('<2sBBHB',f,0,b'\xe7\x7e',2,kind,seq,mode)
    f[10:10+len(payload)]=payload
    struct.pack_into('<H',f,46,P.crc16(f[:46]));return bytes(f)

def read(c):
    data=c.read(c.in_waiting);pm=[];fixture=[]
    while data:
        if data[:2]==b'\xe7\x7e':
            f,data=data[:48],data[48:];assert len(f)==48 and P.crc16(f[:46])==int.from_bytes(f[46:],'little');fixture.append(f)
        else:
            assert data[:2]==b'\x7b\xb7',data[:10]
            n=data[6]+10;f,data=data[:n],data[n:];decoded=P.Decoder().feed(f);assert len(decoded)==1;pm.extend(decoded)
    return pm,fixture

def scalar(c,name):return int.from_bytes(c.u.mem_read(c.symbols[name],4),'little')
def pm(c,cmd,seq,payload=b''):
    c.write(P.frame(cmd,seq,payload));packets,_=read(c)
    return packets
def ack(packets,cmd,status=0):assert any(p.command==P.ACK and p.payload==bytes([cmd,status]) for p in packets)
def select(c,seq,mode):
    c.write(fx(4,seq,mode));_,frames=read(c)
    assert any(f[6:9]==bytes([mode,0,0]) for f in frames)
def status(c,grams):
    c.advance(100,grams);packets,_=read(c)
    return P.Telemetry.decode([p for p in packets if p.command==P.STATUS][-1].payload)

c=ArmSerial(pc_port=False,combined=True)
c.advance(100);read(c)
ack(pm(c,P.TARE,1),P.TARE,2) # Main menu cannot mutate pressure calibration.
c.write(fx(1,2,3));c.advance(300);read(c)
assert scalar(c,'sim_heater_on')==1 and scalar(c,'sim_write_count')==0
ack(pm(c,P.SET_CONFIG,3,struct.pack('<iffB',1000000,-0.0025,84.8,3)),P.SET_CONFIG,2)
select(c,4,5);assert scalar(c,'sim_heater_on')==0
c.write(fx(1,2,3));c.advance(500);read(c) # Late fixture start may not re-enable heat.
assert scalar(c,'sim_heater_on')==0
pm(c,P.TARE,5);c.advance(1100);packets,_=read(c);ack(packets,P.TARE)
ack(pm(c,P.SET_CONFIG,6,struct.pack('<iffB',1000000,-0.0025,84.8,3)),P.SET_CONFIG)
before=status(c,100);assert abs(before.peak-86.7405)<0.01
POLARITY=10
ack(pm(c,POLARITY,7),POLARITY)
after=status(c,100);assert abs(after.pressure+before.pressure)<0.001 and after.count>0
assert abs(after.peak-after.pressure)<0.001
# Duplicate polarity transaction must not flip back.
ack(pm(c,POLARITY,7),POLARITY);assert status(c,100).pressure<0
# A complete pressure frame inside a valid fixture payload is not a command.
c.write(fx(2,4,5,P.frame(POLARITY,8)));read(c);assert status(c,100).pressure<0
positive=status(c,-100);assert abs(positive.pressure-before.pressure)<0.001
ack(pm(c,P.SAVE,9),P.SAVE)
c.call('Sim_Reset');status(c,0)
config=[p for p in pm(c,P.GET_CONFIG,10) if p.command==P.CONFIG][0]
assert P.Config.decode(config.payload).grams_per_count>0 # Saved reversed coefficient survives reboot.
select(c,11,5)
pm(c,P.POINT,12);ack(pm(c,POLARITY,13),POLARITY,2)
c.advance(1100);read(c)
pm(c,P.TARE,14);c.advance(300,50);read(c)
select(c,15,0);assert scalar(c,'sim_heater_on')==0
c.advance(1100,50);read(c)
config=[p for p in pm(c,P.GET_CONFIG,16) if p.command==P.CONFIG][0]
assert P.Config.decode(config.payload).zero==1000000 # Leaving cancels an in-flight tare.
# All four original consumable tools remain reachable after returning.
for mode in (1,2,4):
    c.write(fx(1,20+mode,mode));c.advance(150);read(c)
    assert scalar(c,'sim_heater_on')==0
    select(c,30+mode,0)
assert scalar(c,'sim_write_count')==2 and scalar(c,'sim_clear_count')==1
c.write(fx(1,40,3));c.advance(300);read(c);assert scalar(c,'sim_heater_on')==1
c.advance(4000);read(c);assert scalar(c,'sim_heater_on')==0 # Existing link-loss interlock.
print('PASS: real combined ARM cores: heater handover/late-start rejection, mode ownership, all four consumable tools, link-loss off, framed multiplexing, polarity magnitude/peak/duplicate/busy/persistence, calibration and continuous sampling.')
