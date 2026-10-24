"""Test actual firmware WS2812 GRB encoding and busy-DMA state changes."""
from pathlib import Path
import struct
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import *
r=Path(__file__).resolve().parents[1]
p=r/'9C-U_HTP100_Main_vscode-200/build/Fixture/9C-HTP100-main-v2.elf'
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
for a,n in [(0,0x10000),(0x08000000,0x20000),(0x20000000,0x100000)]:u.mem_map(a,n)
with p.open('rb') as f:
 e=ELFFile(f)
 for seg in e.iter_segments():
  if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
 sym={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
colors=[]
def ret(uc,a,n,d):uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
def dma(uc,a,n,d):
 ptr=uc.reg_read(UC_ARM_REG_R2);length=uc.reg_read(UC_ARM_REG_R3);assert length==510
 bits=struct.unpack('<510H',uc.mem_read(ptr,1020));color=0
 for bit in bits[:24]:
  assert bit in (16,52);color=(color<<1)|(bit==52)
 assert all(bits[i*24:(i+1)*24]==bits[:24] for i in range(15))
 assert all(x==0 for x in bits[360:]);colors.append(color);ret(uc,a,n,d)
for name,fn in [('HAL_GetTick',ret),('AppLog_Write',ret),('HAL_TIM_PWM_Start_DMA',dma),('HAL_TIM_PWM_Stop_DMA',ret)]:
 a=sym[name]&~1;u.hook_add(UC_HOOK_CODE,fn,begin=a,end=a)
def call(name,arg=0):
 u.reg_write(UC_ARM_REG_SP,0x200F0000);u.reg_write(UC_ARM_REG_LR,0x8001);u.reg_write(UC_ARM_REG_R0,arg)
 u.emu_start(sym[name]|1,0x8000,count=2000000);assert u.reg_read(UC_ARM_REG_PC)==0x8000
call('fixture_status_led',5);call('Ws2812Driver_Tick');assert colors==[0x300000] # green in GRB
call('Ws2812Driver_Tick');assert len(colors)==1
call('fixture_status_led',6);call('Ws2812Driver_Tick');assert len(colors)==1 # busy buffer retained
call('Board_Ws2812FinishedFromIsr');call('Ws2812Driver_Tick');assert colors[-1]==0x003000 # red
call('fixture_status_led',1);call('Board_Ws2812FinishedFromIsr');call('Ws2812Driver_Tick');assert colors[-1]==0x000020 # rearmed blue
for state in (0,1,2,3,4):
 call('fixture_status_led',state);call('Board_Ws2812FinishedFromIsr');call('Ws2812Driver_Tick');assert colors[-1]==0x000020
print('PASS: WS2812 green PASS, red FAIL, blue IDLE/WAIT/WRITE/HEATING/STABLE, all 15 LEDs and DMA retry.')
