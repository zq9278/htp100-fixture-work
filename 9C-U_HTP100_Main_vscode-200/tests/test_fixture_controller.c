#include "fixture_controller.h"
#include "fixture_config.h"
#include <math.h>
#include <string.h>
static uint32_t clock_ms;
static bool present,heater_ok,powered;
static float temperature;
static unsigned writes,heats,offs,marker_clears;
static uint8_t prepare_result,last[FX_FRAME_SIZE];
static uint8_t usable_result;
static uint8_t led_state;
static FxDiagnostics hardware_diag;
static uint16_t transaction;
static uint8_t function;
static uint32_t now(void){return clock_ms;}
static void off(void){powered=false;offs++;}
static bool probe(void){return present;}
static bool heater(float target,float *value){
    if(target!=43.0f) return false;
    *value=temperature; heats++; powered=heater_ok; return heater_ok;
}
static uint8_t prepare(bool demo){writes++; return demo==(function==FX_DEMO)?prepare_result:FX_BAD_COMMAND;}
static bool send(const uint8_t *p,uint16_t n){if(n==sizeof(last))memcpy(last,p,n);return true;}
static uint8_t check_usable(void){
    hardware_diag.marker=usable_result==FX_USED_EYE?1U:0xFFFFU;
    hardware_diag.flags=FX_DIAG_MARK_VALID; return usable_result;
}
static void status_led(uint8_t state){led_state=state;}
static void diagnostics(FxDiagnostics *out){if(out)*out=hardware_diag;else hardware_diag=(FxDiagnostics){0};}
static uint8_t clear_marker(void){marker_clears++;return prepare_result;}
static const FixturePort port={now,off,probe,heater,prepare,send,check_usable,status_led,diagnostics,clear_marker};
static void frame(uint8_t type,uint16_t id,uint8_t mode){
    uint8_t f[FX_FRAME_SIZE];Fx_Frame(f,type,id,mode);Fx_Seal(f);Fixture_Feed(f,sizeof(f));
}
static void query(void){frame(FX_START,transaction,function);}
static void reset(uint8_t mode){
    present=true;heater_ok=true;temperature=43;writes=heats=offs=marker_clears=0;powered=false;prepare_result=FX_OK;usable_result=FX_OK;
    clock_ms=100;transaction++;function=mode;Fixture_Init(&port);
    frame(FX_START,transaction,function);Fixture_Tick();query();
}
static void step(uint32_t ms){clock_ms+=ms;frame(FX_HEARTBEAT,transaction,function);Fixture_Tick();query();}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
int main(void){
    CHECK(Fx_Crc((const uint8_t *)"123456789",9)==0x4B37);
    reset(FX_INSPECT);CHECK(last[7]==FX_HEATING && writes==0);
    for(unsigned i=0;i<14;i++)step(150);
    CHECK(last[7]==FX_STABLE && powered && writes==0);
    step(150);CHECK(last[7]==FX_PASS && !powered && writes==0 && Fx_Get16(last+14)==2000);
    CHECK(led_state==FX_PASS);
    unsigned old=heats;step(1000);CHECK(heats==old); /* no automatic retest */
    reset(FX_INSPECT);for(unsigned i=0;i<10;i++)step(150);
    temperature=39.9f;step(150);CHECK(last[7]==FX_HEATING && Fx_Get16(last+14)==0);
    temperature=40.0f;for(unsigned i=0;i<14;i++)step(150);
    CHECK(last[7]==FX_STABLE);step(150);CHECK(last[7]==FX_PASS);
    reset(FX_INSPECT);present=false;step(150);CHECK(last[8]==FX_REMOVED && !powered);
    reset(FX_INSPECT);temperature=NAN;step(150);CHECK(last[8]==FX_TEMP_COMM && !powered);
    /* Exact threshold, exact 5s boundary, and no premature PASS while pending. */
    reset(FX_INSPECT);temperature=44.0f;step(150);
    CHECK(last[8]==FX_OK && last[7]==FX_STABLE);
    for(unsigned i=0;i<4;i++)step(1000);
    step(850);CHECK(last[8]==FX_OK && last[7]==FX_STABLE); /* 4850ms */
    step(150);CHECK(last[8]==FX_OVER_TEMP && !powered); /* 5000ms */
    /* A transient overshoot can recover; below 44C clears confirmation. */
    reset(FX_INSPECT);temperature=44.1f;step(150);step(1000);
    temperature=43.0f;step(150);CHECK(last[8]==FX_OK);
    step(1000);CHECK(last[7]==FX_PASS && !powered);
    reset(FX_INSPECT);temperature=44.5f;step(150);step(1000);step(1000);step(1000);
    temperature=39.9f;step(150);CHECK(last[7]==FX_HEATING && last[8]==FX_OK);
    temperature=44.5f;step(150);
    for(unsigned i=0;i<4;i++)step(1000);
    step(850);CHECK(last[8]==FX_OK && last[7]==FX_STABLE);
    step(150);CHECK(last[8]==FX_OVER_TEMP && !powered);
    /* Unsigned clock rollover must not shorten the 5s confirmation. */
    reset(FX_INSPECT);clock_ms=0xFFFFF000U;Fixture_Init(&port);
    transaction++;frame(FX_START,transaction,function);Fixture_Tick();temperature=44.0f;step(150);
    for(unsigned i=0;i<5;i++)step(1000);
    CHECK(last[8]==FX_OVER_TEMP && !powered);
    reset(FX_INSPECT);heater_ok=false;step(150);CHECK(last[8]==FX_PWM_ERROR && !powered);
    reset(FX_INSPECT);step(150);clock_ms+=3000;Fixture_Tick();query();CHECK(last[8]==FX_LINK_LOST && !powered);
    reset(FX_INSPECT);step(150);frame(FX_CANCEL,transaction,function);CHECK(last[8]==FX_CANCELLED && !powered);
    reset(FX_INSPECT);step(150);Fixture_PowerLost();query();CHECK(last[8]==FX_POWER_LOST && !powered);
    reset(FX_INSPECT);temperature=30;for(unsigned i=0;i<801;i++)step(150);
    CHECK(last[8]==FX_TIMEOUT && !powered && writes==0);
    reset(FX_INITIALIZE);CHECK(last[7]==FX_PASS && writes==1 && heats==0);
    for(unsigned i=0;i<20;i++)query();CHECK(writes==1); /* retries do not erase again */
    reset(FX_DEMO);CHECK(last[7]==FX_PASS && writes==1 && heats==0);
    reset(FX_NONE);CHECK(last[8]==FX_BAD_COMMAND && writes==0 && heats==0);
    reset(FX_INITIALIZE); /* then new transaction with no consumable */
    present=false;transaction++;frame(FX_START,transaction,function);
    for(unsigned i=0;i<201;i++)step(150);
    CHECK(last[8]==FX_NO_EYE && writes==1 && heats==0);
    reset(FX_INITIALIZE);transaction++;prepare_result=FX_WRITE_ERROR;frame(FX_START,transaction,function);Fixture_Tick();query();
    CHECK(last[8]==FX_WRITE_ERROR && !powered);
    reset(FX_DEMO);transaction++;prepare_result=FX_VERIFY_ERROR;frame(FX_START,transaction,function);Fixture_Tick();query();
    CHECK(last[8]==FX_VERIFY_ERROR);
    /* Noisy, fragmented, corrupt and coalesced frames. */
    reset(FX_INITIALIZE);transaction++;uint8_t f[FX_FRAME_SIZE];Fx_Frame(f,FX_START,transaction,FX_INSPECT);Fx_Seal(f);
    uint8_t noise[]={0,0xE7,0x7E,1,3,0xFF};Fixture_Feed(noise,sizeof(noise));
    for(unsigned i=0;i<sizeof(f);i++)Fixture_Feed(f+i,1);
    function=FX_INSPECT;Fixture_Tick();query();CHECK(last[7]==FX_HEATING && last[6]==FX_INSPECT);
    uint8_t bad[FX_FRAME_SIZE];memcpy(bad,f,sizeof(f));bad[4]++; /* invalid CRC ignored */
    Fixture_Feed(bad,sizeof(bad));query();CHECK(Fx_Get16(last+4)==transaction);
    reset(FX_INSPECT);step(150); /* stale/other transaction cannot cancel active job */
    frame(FX_CANCEL,transaction-1,function);query();CHECK(last[7]==FX_STABLE && powered);
    /* Tick wrap must preserve consecutive stability timing. */
    clock_ms=0xFFFFFF00U;Fixture_Init(&port);transaction++;frame(FX_START,transaction,function);Fixture_Tick();
    for(unsigned i=0;i<15;i++)step(150);CHECK(last[7]==FX_PASS && !powered);
    /* Finished consumables stay latched. A brief contact glitch cannot repeat
     * a write; removal for 600ms rearms only the selected function. */
    for(uint8_t selected=FX_INITIALIZE;selected<=FX_INSPECT;++selected) {
        reset(selected);
        if(selected==FX_INSPECT) for(unsigned i=0;i<15;i++)step(150);
        CHECK(last[7]==FX_PASS && !powered);
        unsigned completed_writes=writes,completed_heats=heats;
        step(1000); CHECK(last[7]==FX_PASS && writes==completed_writes && heats==completed_heats);
        present=false;step(150);step(300);present=true;step(150);
        CHECK(last[7]==FX_PASS); /* short dropout does not rearm */
        present=false;step(150);step(300);step(300);
        CHECK(last[7]==FX_WAIT && last[8]==FX_OK && !powered && Fx_Get16(last+14)==0);
        CHECK((int16_t)Fx_Get16(last+10)==FX_TEMP_INVALID);
        for(unsigned i=0;i<210;i++)step(150);
        CHECK(last[7]==FX_WAIT); /* batch waiting does not expire */
        present=true;step(150);
        if(selected==FX_INSPECT) {
            CHECK(last[7]==FX_HEATING && writes==0);
            for(unsigned i=0;i<15;i++)step(150);
            CHECK(last[7]==FX_PASS && !powered && writes==0);
        } else CHECK(last[7]==FX_PASS && writes==completed_writes+1 && heats==0);
    }
    reset(FX_INITIALIZE);frame(FX_CANCEL,transaction,function);
    present=false;for(unsigned i=0;i<8;i++)step(150);present=true;step(150);
    CHECK(last[8]==FX_CANCELLED && writes==1); /* returning home disarms the batch */
    CHECK(led_state==FX_IDLE); /* cancellation restores default blue, not red */
    reset(FX_DEMO);clock_ms+=3000;Fixture_Tick();query();
    CHECK(last[8]==FX_LINK_LOST);present=false;for(unsigned i=0;i<8;i++)step(150);
    CHECK(last[8]==FX_LINK_LOST && writes==1);
    reset(FX_INSPECT);temperature=NAN;step(150);CHECK(last[8]==FX_TEMP_COMM);
    present=false;step(150);step(300);step(300);CHECK(last[7]==FX_WAIT);
    /* Consumed or unreadable markers fail closed before the first heat step. */
    reset(FX_INSPECT);Fixture_Init(&port);usable_result=FX_USED_EYE;transaction++;
    frame(FX_START,transaction,function);Fixture_Tick();query();
    CHECK(last[7]==FX_FAIL && last[8]==FX_USED_EYE && heats==0 && writes==0 && !powered);
    CHECK(led_state==FX_FAIL);
    CHECK(Fx_Get16(last+20)==1U && (last[16]&FX_DIAG_MARK_VALID));
    hardware_diag.marker=0xBEEFU;step(150);CHECK(Fx_Get16(last+20)==1U); /* frozen failure */
    step(1000);CHECK(last[8]==FX_USED_EYE && heats==0);
    present=false;step(150);step(300);step(300);CHECK(last[7]==FX_WAIT);
    CHECK(led_state==FX_WAIT);
    present=true;usable_result=FX_OK;step(150);CHECK(last[7]==FX_HEATING);
    step(150);CHECK(heats==1 && writes==0);
    usable_result=FX_USED_EYE;step(150);CHECK(last[8]==FX_USED_EYE && !powered && heats==1);
    reset(FX_INSPECT);usable_result=FX_EYE_READ_ERROR;step(150);
    CHECK(last[8]==FX_EYE_READ_ERROR && heats==0 && writes==0 && !powered);
    /* The entire I2C failure snapshot survives later successful presence probes. */
    reset(FX_INSPECT);present=false;
    hardware_diag=(FxDiagnostics){.flags=FX_DIAG_IO_FAILED,.device=0x50,.reg=3,
        .status=1,.attempts=2,.lines=3,.operation=1,.error=4};
    step(150);CHECK(last[8]==FX_REMOVED && last[17]==0x50 && last[18]==3);
    CHECK(Fx_Get32(last+28)==4 && last[19]==1 && last[26]==2);
    CHECK(Fx_Get16(last+FX_FRAME_SIZE-2)==Fx_Crc(last,FX_FRAME_SIZE-2));
    hardware_diag=(FxDiagnostics){0};present=true;step(150);
    CHECK(last[17]==0x50 && last[18]==3 && Fx_Get32(last+28)==4);
    present=false;step(150);step(300);step(300);
    CHECK(last[7]==FX_WAIT && last[16]==0 && Fx_Get32(last+32)==0);
    reset(FX_CLEAR_MARKER);
    CHECK(last[7]==FX_PASS && marker_clears==1 && writes==0 && heats==0 && led_state==FX_PASS);
    for(unsigned i=0;i<10;i++)query();CHECK(marker_clears==1);
    present=false;step(150);step(300);step(300);CHECK(last[7]==FX_WAIT);
    present=true;step(150);CHECK(last[7]==FX_PASS && marker_clears==2 && writes==0 && heats==0);
    reset(FX_CLEAR_MARKER);transaction++;prepare_result=FX_WRITE_ERROR;
    frame(FX_START,transaction,function);Fixture_Tick();query();
    CHECK(last[8]==FX_WRITE_ERROR && marker_clears==2 && writes==0 && heats==0 && led_state==FX_FAIL);
    reset(FX_CLEAR_MARKER);transaction++;prepare_result=FX_VERIFY_ERROR;
    frame(FX_START,transaction,function);Fixture_Tick();query();
    CHECK(last[8]==FX_VERIFY_ERROR && marker_clears==2 && writes==0 && heats==0);
    return 0;
}
