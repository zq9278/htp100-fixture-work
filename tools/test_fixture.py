"""Run the real portable C state machine as ARM Thumb code using Unicorn.
Dependencies: pip install unicorn pyelftools; arm-none-eabi-gcc on PATH.
"""
from pathlib import Path
import subprocess, struct, shutil
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0
root=Path(__file__).resolve().parents[1]
project=root/'9C-U_HTP100_Main_vscode-200'
out=project/'build/fixture-tests';out.mkdir(parents=True,exist_ok=True)
gcc=shutil.which('arm-none-eabi-gcc')
elf=out/'fixture-tests.elf'
subprocess.run([gcc,'-mcpu=cortex-m0','-mthumb','-O1','-g','-nostartfiles','--specs=nosys.specs',
    '-Wl,-Ttext=0x10000','-Wl,-Tdata=0x20000000','-Wl,-e,main','-I'+str(project/'App/Inc'),
    str(project/'tests/test_fixture_controller.c'),str(project/'App/Src/fixture_controller.c'),'-lm','-o',str(elf)],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
u.mem_map(0,0x200000);u.mem_map(0x20000000,0x100000)
with elf.open('rb') as f:
    e=ELFFile(f)
    for segment in e.iter_segments():
        if segment['p_type']=='PT_LOAD':u.mem_write(segment['p_vaddr'],segment.data())
    main=e.get_section_by_name('.symtab').get_symbol_by_name('main')[0]['st_value']
u.reg_write(UC_ARM_REG_SP,0x200F0000);u.reg_write(UC_ARM_REG_LR,0x8001)
u.emu_start(main|1,0x8000,count=50000000)
result=u.reg_read(UC_ARM_REG_R0)
assert result==0,f'C assertion failed at line {result}'
print('PASS: ARM C state machine, stability/wrap, all 3 batch modes, removal debounce/rearm, cancel/link disarm, no repeated item writes, fault exits and stream recovery.')
