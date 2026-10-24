"""Render the actual screen ELF in Unicorn; replace only LCD/UART hardware calls.
Dependencies: unicorn, pyelftools, Pillow. Run after building screen/Fixture.
"""
from pathlib import Path
import argparse
import struct
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.arm_const import *
from PIL import Image
root=Path(__file__).resolve().parents[1]
arguments=argparse.ArgumentParser()
arguments.add_argument('--variant',choices=['New','Old'],default='New')
variant=arguments.parse_args().variant
image_name='9C-HTP100-screen-old.elf' if variant=='Old' else '9C-HTP100-screen.elf'
path=root/'9C-U_HTP100_SCREEN_Picture -100/build/Fixture'/image_name
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for address,size in [(0,0x10000),(0x20000000,0x100000),(0x24000000,0x100000),(0x90000000,0x1000000),(0xC0000000,0x800000)]:u.mem_map(address,size)
with path.open('rb') as f:
    elf=ELFFile(f)
    for seg in elf.iter_segments():
        if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
    symbols={sym.name:sym['st_value'] for sym in elf.get_section_by_name('.symtab').iter_symbols()}
u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
def bad(uc,access,address,size,value,data):
    print('unmapped',hex(address),'PC',hex(uc.reg_read(UC_ARM_REG_PC)));return False
u.hook_add(UC_HOOK_MEM_INVALID,bad)
pixels=None
now_ms=0
buzzer_starts=0
buzzer_stops=0
def ret(uc,addr,size,data):uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
for name in ['LCD_Init','HAL_UART_Transmit','lv_draw_stm32_dma2d_init']:
    a=symbols[name]&~1;u.hook_add(UC_HOOK_CODE,ret,begin=a,end=a)
def tick(uc,addr,size,data):
    uc.reg_write(UC_ARM_REG_R0,now_ms);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
a=symbols['HAL_GetTick']&~1;u.hook_add(UC_HOOK_CODE,tick,begin=a,end=a)
def pwm_start(uc,addr,size,data):
    global buzzer_starts
    buzzer_starts+=1;ret(uc,addr,size,data)
def pwm_stop(uc,addr,size,data):
    global buzzer_stops
    buzzer_stops+=1;ret(uc,addr,size,data)
for name,fn in [('HAL_TIM_PWM_Start',pwm_start),('HAL_TIM_PWM_Stop',pwm_stop)]:
    a=symbols[name]&~1;u.hook_add(UC_HOOK_CODE,fn,begin=a,end=a)
def software_draw(uc,addr,size,data):
    uc.reg_write(UC_ARM_REG_PC,symbols['lv_draw_sw_init_ctx']|1)
a=symbols['lv_draw_stm32_dma2d_ctx_init']&~1;u.hook_add(UC_HOOK_CODE,software_draw,begin=a,end=a)
def flush(uc,addr,size,data):
    global pixels
    pixels=bytes(uc.mem_read(uc.reg_read(UC_ARM_REG_R2),480*640*2))
    uc.reg_write(UC_ARM_REG_PC,symbols['lv_disp_flush_ready']|1)
a=symbols['disp_flush']&~1;u.hook_add(UC_HOOK_CODE,flush,begin=a,end=a)
def call(name,*args):
    u.reg_write(UC_ARM_REG_SP,0x200F0000);u.reg_write(UC_ARM_REG_LR,0x8001)
    for i,v in enumerate(args):u.reg_write([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3][i],v)
    u.emu_start(symbols[name]|1,0x8000,count=150000000)
    assert u.reg_read(UC_ARM_REG_PC)==0x8000,f'{name} did not return, PC={u.reg_read(UC_ARM_REG_PC):x}'
def snapshot(name):
    global pixels
    pixels=None;call('lv_refr_now',0);assert pixels
    rgb=bytearray()
    for (v,) in struct.iter_unpack('<H',pixels):rgb.extend((((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
    if name in ('检测合格','检测失败','耗材已报废'):
        pos=(100*480+10)*3
        red,green,blue=rgb[pos:pos+3]
        if name=='检测合格':assert green>red*2 and green>blue,'PASS must be green'
        else:assert red>green*2 and red>blue*2,'FAIL must be red'
    out=root/'交付/界面预览'
    if variant=='Old':out=out/'旧屏'
    out.mkdir(parents=True,exist_ok=True)
    Image.frombytes('RGB',(480,640),bytes(rgb)).save(out/(name+'.png'))
call('lv_init');call('lv_port_disp_init');call('FixtureUi_Init');snapshot('功能选择')
# Invoke the actual button event through LVGL, then feed real protocol statuses.
# Read the home panel and its children; use lv_obj_get_child to obtain inspection button.
home=struct.unpack('<I',u.mem_read(symbols['home'],4))[0]
call('lv_obj_get_child',home,4);button=u.reg_read(UC_ARM_REG_R0)
call('lv_event_send',button,7,0) # LV_EVENT_CLICKED=7 in LVGL 8
snapshot('出厂检测等待')
seq=struct.unpack('<H',u.mem_read(symbols['seq'],2))[0] # static UI sequence symbol

def crc(data):
    c=65535
    for b in data:
        c^=b
        for _ in range(8):c=(c>>1)^(0xA001 if c&1 else 0)
    return c
def feed(state,reason,temp,stable):
    f=bytearray([0xE7,0x7E,1,0x81])+bytearray(struct.pack('<H',seq))+bytearray([3,state,reason,1])+bytearray(struct.pack('<hhH',temp,4300,stable))
    f+=struct.pack('<H',crc(f));u.mem_write(0x1000,bytes(f));call('FixtureUi_Feed',0x1000,18)
feed(4,0,4300,1500);snapshot('稳定计时')
feed(5,0,4300,2000);snapshot('检测合格')
assert buzzer_starts==1
stops=buzzer_stops
for _ in range(5):feed(5,0,4300,2000)
assert buzzer_starts==1 and buzzer_stops==stops,'Repeated PASS must not repeat sound'
for now_ms in (149,150,269,270,419,420):call('FixtureUi_Tick')
assert buzzer_starts==2 and buzzer_stops==stops+2,'Two short nonblocking beeps'
feed(1,0,-32768,0);snapshot('拔出后等待下一件')
feed(5,0,4300,2000);assert buzzer_starts==3,'Next item gets its own notification'
feed(6,3,-32768,0);snapshot('检测失败')
feed(6,13,-32768,0);snapshot('耗材已报废')
status=struct.unpack('<I',u.mem_read(symbols['status'],4))[0]
call('lv_label_get_text',status)
text=bytes(u.mem_read(u.reg_read(UC_ARM_REG_R0),80)).split(b'\0')[0].decode('utf-8')
assert text=='耗材已报废'
# Return from a terminal result must CANCEL and wait for its acknowledgement;
# a buffered terminal status must not be mistaken for that acknowledgement.
back_label=struct.unpack('<I',u.mem_read(symbols['back_label'],4))[0]
call('lv_obj_get_parent',back_label);back_button=u.reg_read(UC_ARM_REG_R0)
call('lv_event_send',back_button,7,0)
feed(5,0,4300,2000)
assert u.mem_read(symbols['mode'],1)==b'\x03'
feed(6,9,-32768,0)
assert u.mem_read(symbols['mode'],1)==b'\x00'
print('PASS: actual LVGL menu/results/reset, repeated PASS suppression, two-beep timing, next-item alert, cancel acknowledgement.')
