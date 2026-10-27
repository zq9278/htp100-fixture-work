"""Exercise actual firmware EEPROM functions with simulated HAL I2C transactions."""
from pathlib import Path
import struct
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import *
root=Path(__file__).resolve().parents[1]
path=root/'9C-U_HTP100_Main_vscode-200/build/Fixture/9C-HTP100-main-v2.elf'
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
for a,n in [(0,0x10000),(0x08000000,0x20000),(0x20000000,0x100000)]:u.mem_map(a,n)
with path.open('rb') as f:
    e=ELFFile(f)
    for seg in e.iter_segments():
        if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
    syms={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
eeprom=bytearray(range(256));writes=[];fail_write=False;fail_read=False;bad_verify=False
fail_register=None
fail_device=None

def finish(value=0):u.reg_write(UC_ARM_REG_R0,value);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
def read(uc,a,n,d):
    addr=uc.reg_read(UC_ARM_REG_R2);sp=uc.reg_read(UC_ARM_REG_SP)
    device=uc.reg_read(UC_ARM_REG_R1)
    if fail_read or (device==fail_device and addr==fail_register):finish(1);return
    buf,size=struct.unpack('<II',uc.mem_read(sp,8));data=b'\x2b\x00' if device==0x90 else bytes(eeprom[addr:addr+size])
    if bad_verify and writes:data=bytes([data[0]^1])+data[1:]
    uc.mem_write(buf,data);finish()
def write(uc,a,n,d):
    if fail_write:finish(1);return
    addr=uc.reg_read(UC_ARM_REG_R2);sp=uc.reg_read(UC_ARM_REG_SP)
    buf,size=struct.unpack('<II',uc.mem_read(sp,8));data=bytes(uc.mem_read(buf,size))
    eeprom[addr:addr+size]=data;writes.append((addr,data));finish()
def const(value):
    def cb(uc,a,n,d):finish(value)
    return cb
hooks={'HAL_I2C_Mem_Read':read,'HAL_I2C_Mem_Write':write,'vTaskDelay':const(0),
       'HAL_GetTick':const(100),'AppLog_Write':const(0),'HAL_GPIO_ReadPin':const(1),
       'HAL_I2C_GetError':const(4),'HAL_I2C_GetState':const(0x20)}
for name,fn in hooks.items():
    a=syms[name]&~1;u.hook_add(UC_HOOK_CODE,fn,begin=a,end=a)
def call(name,arg=0):
    u.reg_write(UC_ARM_REG_SP,0x200F0000);u.reg_write(UC_ARM_REG_LR,0x8001);u.reg_write(UC_ARM_REG_R0,arg)
    u.emu_start(syms[name]|1,0x8000,count=5000000)
    assert u.reg_read(UC_ARM_REG_PC)==0x8000
    return u.reg_read(UC_ARM_REG_R0)
before=bytes(eeprom);assert call('EyeDriver_FixtureProbe')==1 and bytes(eeprom)==before and not writes
assert call('EyeDriver_FixtureCheckUsable')==13 and bytes(eeprom)==before and not writes
assert call('EyeDriver_FixturePrepare',0)==0
assert eeprom==bytearray([255]*256) and len(writes)==256
assert all(data==b'\xff' for addr,data in writes) # no consumed marker ever written
writes.clear();assert call('EyeDriver_FixturePrepare',1)==0
assert writes==[(3,b'\x02'),(4,b'\x02')] and eeprom[1:3]==b'\xff\xff'
writes.clear();assert call('EyeDriver_FixtureCheckUsable')==0 and not writes
for marker in [b'\x00\x01',b'\x00\x00',b'\xff\xfe']:
    eeprom[1:3]=marker;before=bytes(eeprom)
    assert call('EyeDriver_FixtureCheckUsable')==13 and bytes(eeprom)==before and not writes
eeprom[1:3]=b'\xff\xff';before=bytes(eeprom);fail_read=True
assert call('EyeDriver_FixtureCheckUsable')==15 and bytes(eeprom)==before and not writes
fail_read=False
writes.clear();bad_verify=True;assert call('EyeDriver_FixturePrepare',1)==7
bad_verify=False;fail_write=True;writes.clear();assert call('EyeDriver_FixturePrepare',1)==6
fail_write=False
def diagnostics():
    call('EyeDriver_FixtureDiagnostics',0x1000)
    values=struct.unpack('<7BxHHH2xI',u.mem_read(0x1000,20))
    return dict(zip(['flags','device','reg','status','attempts','lines','operation','marker','service','temperature_raw','error'],values))
call('EyeDriver_FixtureDiagnostics',0)
assert diagnostics()['flags']==0
eeprom[1:3]=b'\x00\x01';assert call('EyeDriver_FixtureCheckUsable')==13
diag=diagnostics();assert diag['marker']==1 and diag['device']==0x50 and diag['reg']==1 and diag['flags']&1
eeprom[1:3]=b'\xff\xff';fail_device=0xA0;fail_register=3
assert call('EyeDriver_FixtureProbe')==0
diag=diagnostics();assert diag['device']==0x50 and diag['reg']==3 and diag['status']==1 and diag['attempts']==2 and diag['error']==4 and diag['flags']&8 and not diag['flags']&2
fail_device=None;fail_register=None
assert call('EyeDriver_ReadTemperature',0x2000)==1
assert struct.unpack('<f',u.mem_read(0x2000,4))[0]==43.0
diag=diagnostics();assert diag['temperature_raw']==0x2B00 and diag['flags']&4
fail_device=0x90;fail_register=0
assert call('EyeDriver_ReadTemperature',0x2000)==0
diag=diagnostics();assert diag['device']==0x48 and diag['reg']==0 and diag['attempts']==2 and diag['lines']==3 and diag['error']==4
assert diag['flags']&8 and not diag['flags']&4,'Failed temperature read must invalidate old raw data'
assert not writes
print('PASS: actual EEPROM driver: read-only probe, full FF initialization, demo only 03/04, write/verify errors, consumed flag never set.')
print('PASS: diagnostic register/value/flags, EEPROM 03 failure, TMP112 raw 2B00, sensor 00 NACK and stale-data invalidation.')
