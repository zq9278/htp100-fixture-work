"""Render the actual screen ELF in Unicorn; replace only LCD/UART hardware calls.
Dependencies: unicorn, pyelftools, Pillow, PC test dependencies. Build PressureMonitor first.
"""
from pathlib import Path
import argparse
import struct
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.arm_const import *
from PIL import Image
root=Path(__file__).resolve().parents[1]
arguments=argparse.ArgumentParser()
arguments.add_argument('--variant',choices=['New','Old'],default='New')
variant=arguments.parse_args().variant
image_name='9C-HTP100-screen-old.elf' if variant=='Old' else '9C-HTP100-screen.elf'
path=root/'9C-U_HTP100_SCREEN_Picture -100/build/PressureMonitor'/image_name
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
# MAX supplies the double-precision FPv5 instructions used by this H750 build;
# Unicorn's Cortex-M7 preset exposes only the single-precision FPU variant.
u.ctl_set_cpu_model(UC_CPU_ARM_MAX)
u.reg_write(UC_ARM_REG_C1_C0_2,0x00F00000)
for address,size in [(0,0x10000),(0x20000000,0x100000),(0x24000000,0x100000),(0x90000000,0x1000000),(0xC0000000,0x800000)]:u.mem_map(address,size)
with path.open('rb') as f:
    elf=ELFFile(f)
    for seg in elf.iter_segments():
        if seg['p_type']=='PT_LOAD':u.mem_write(seg['p_vaddr'],seg.data())
    symbols={sym.name:sym['st_value'] for sym in elf.get_section_by_name('.symtab').iter_symbols()}
# Match Reset_Handler: NOLOAD-segment alignment can otherwise place ELF headers
# into BSS when a debugger/test loads program headers directly.
u.mem_write(symbols['_sbss'],bytes(symbols['_ebss']-symbols['_sbss']))
u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
def bad(uc,access,address,size,value,data):
    print('unmapped',hex(address),'PC',hex(uc.reg_read(UC_ARM_REG_PC)));return False
u.hook_add(UC_HOOK_MEM_INVALID,bad)
pixels=None
now_ms=0
buzzer_starts=0
buzzer_stops=0
def ret(uc,addr,size,data):uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
for name in ['LCD_Init','lv_draw_stm32_dma2d_init']:
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
    try:u.emu_start(symbols[name]|1,0x8000,count=150000000)
    except Exception as error:raise RuntimeError(f'{name} PC={u.reg_read(UC_ARM_REG_PC):x}') from error
    assert u.reg_read(UC_ARM_REG_PC)==0x8000,f'{name} did not return, PC={u.reg_read(UC_ARM_REG_PC):x}, LR={u.reg_read(UC_ARM_REG_LR):x}, R4={u.reg_read(UC_ARM_REG_R4):x}'
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
    out=root/'交付/压力传感器检测/H7界面预览（仿真）'
    if variant=='Old':out=out/'旧屏'
    out.mkdir(parents=True,exist_ok=True)
    Image.frombytes('RGB',(480,640),bytes(rgb)).save(out/(name+'.png'))

# Use the same production G0 ARM simulator as the PC integration tests. Only
# physical LCD, UART and buzzer calls are replaced in the actual H7 ELF.
import runpy
ArmSerial=runpy.run_path(str(root/'tools/pressure_sensor_host/tests/test_gui_arm_integration.py'))['ArmSerial']
cable=ArmSerial(pc_port=False)
def uart_send(uc,addr,size,data):
    frame=bytes(uc.mem_read(uc.reg_read(UC_ARM_REG_R1),uc.reg_read(UC_ARM_REG_R2)))
    cable.write(frame);ret(uc,addr,size,data)
a=symbols['HAL_UART_Transmit']&~1;u.hook_add(UC_HOOK_CODE,uart_send,begin=a,end=a)

def pointer(name):return struct.unpack('<I',u.mem_read(symbols[name],4))[0]
def click(name):
    if name in symbols:target=pointer(name)
    else:
        parent,label=('live','砝码校准') if name=='pm_cal_button' else ('calibration_page','返回')
        call('lv_obj_get_child_cnt',pointer(parent));count=u.reg_read(UC_ARM_REG_R0);target=None
        for index in range(count):
            call('lv_obj_get_child',pointer(parent),index);child=u.reg_read(UC_ARM_REG_R0)
            call('lv_obj_check_type',child,symbols['lv_btn_class'])
            if not u.reg_read(UC_ARM_REG_R0):continue
            call('lv_obj_get_child',child,0);text=u.reg_read(UC_ARM_REG_R0)
            call('lv_label_get_text',text);p=u.reg_read(UC_ARM_REG_R0)
            if bytes(u.mem_read(p,64)).split(b'\0')[0].decode('utf-8')==label:target=child;break
        assert target,name
    call('lv_event_send',target,7,0)
def set_text(name,value):
    u.mem_write(0x1000,value.encode()+b'\0');call('lv_textarea_set_text',pointer(name),0x1000)
def label_text(name):
    call('lv_label_get_text',pointer(name));p=u.reg_read(UC_ARM_REG_R0)
    return bytes(u.mem_read(p,256)).split(b'\0')[0].decode('utf-8')
def advance(ms,grams=0):
    global now_ms
    for _ in range(ms//50):
        now_ms+=50;cable.advance(50,grams);call('lv_tick_inc',50);call('PressureUi_Tick')
        data=cable.read(cable.in_waiting)
        if data:
            u.mem_write(0x1000,data);call('PressureUi_Feed',0x1000,len(data))
        call('PressureUi_Tick')

call('lv_init');call('lv_port_disp_init');call('PressureUi_Init')
advance(250);snapshot('01-实时待校准')
click('pm_tare_button');advance(1200);assert '0.0' in label_text('value_label')
click('pm_cal_button')
for grams in (0,100,300,500):
    set_text('mass_input',str(grams));click('pm_point_button');advance(1200,grams)
click('pm_fit_button');advance(100,500);snapshot('02-砝码多点校准')
click('pm_apply_button');advance(200,0);click('pm_save_button');advance(200,0)
assert '已保存' in label_text('config_label'),label_text('config_label')
snapshot('03-校准参数已保存')
# Touching the field brings up the actual numeric keyboard; READY hides it.
call('lv_event_send',pointer('mass_input'),7,0);snapshot('04-数字键盘')
import re
event_header=(root/'9C-U_HTP100_SCREEN_Picture -100/lvgl/src/core/lv_event.h').read_text(encoding='utf-8')
event_names=re.findall(r'^\s*(LV_EVENT_\w+)(?:\s*=\s*\d+)?\s*,',event_header,re.M)
call('lv_event_send',pointer('keyboard'),event_names.index('LV_EVENT_READY'),0)
call('lv_obj_has_flag',pointer('keyboard'),1);assert u.reg_read(UC_ARM_REG_R0),'READY must close the keyboard'
click('pm_back_button');set_text('time_input','0');click('pm_start_button');advance(500,350)
assert '0.1' in label_text('message_label'),'Input validation must remain readable after redraw'
snapshot('08-输入校验')
set_text('time_input','1.0');click('pm_start_button')
advance(350,300);advance(200,500);advance(650,350)
assert '433.7' in label_text('peak_label'),label_text('peak_label')
assert '4.903' in label_text('peak_load_label') and '500.0' in label_text('peak_load_label')
assert '3.432' in label_text('load_label') and '350.0' in label_text('load_label')
snapshot('05-实时压力与峰值')
now_ms+=1500;call('PressureUi_Tick');snapshot('06-数据过期')
assert '--.-' in label_text('value_label')
assert '---.---' in label_text('load_label'),'Stale data must clear N and g too'
now_ms+=3000;call('PressureUi_Tick');snapshot('07-两板通讯断开')
print(f'PASS: {variant} H7 production LVGL ELF + real G0 measurement core; touch, tare, 4 mass points, fit/apply/save, keyboard, peak and stale/link states rendered.')
