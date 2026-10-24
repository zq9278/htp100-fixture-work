#include "fixture_controller.h"
#include "fixture_config.h"
#include <math.h>
static const FixturePort *io;
static FxParser parser;
static uint16_t seq;
static uint8_t mode,state,reason;
static bool have_seq,in_band,eye;
static bool rearm_after_removal,removal_pending,continuous_wait;
static uint32_t removal_started;
static bool over_temp_pending;
static uint32_t over_temp_started;
static float measured;
static uint32_t started,last_link,last_heat,last_send,band_started,last_probe;
static bool active(void) { return state>=FX_WAIT && state<=FX_STABLE; }
static void update_status_led(void) {
    /* User cancellation/return is not a failed consumable. */
    io->status_led(state==FX_FAIL && reason==FX_CANCELLED?FX_IDLE:state);
}
static void finish(uint8_t why) {
    io->off(); reason=why; state=why==FX_OK?FX_PASS:FX_FAIL; in_band=false;
    over_temp_pending=false;
    removal_pending=false;
    rearm_after_removal=mode>=FX_INITIALIZE && mode<=FX_INSPECT &&
        why!=FX_CANCELLED && why!=FX_LINK_LOST && why!=FX_POWER_LOST &&
        why!=FX_NO_EYE && why!=FX_BAD_COMMAND;
    last_probe=io->now();
    update_status_led();
}
static void report(void) {
    uint8_t f[FX_FRAME_SIZE];
    Fx_Frame(f,FX_STATUS,seq,mode); f[7]=state; f[8]=reason; f[9]=(uint8_t)eye;
    Fx_Put16(f+10,isfinite(measured)?(uint16_t)(int16_t)(measured*100.0f):(uint16_t)FX_TEMP_INVALID);
    Fx_Put16(f+12,(uint16_t)(FIXTURE_TARGET_C*100.0f));
    uint32_t stable=in_band?io->now()-band_started:0;
    if(stable>FIXTURE_STABLE_MS) stable=FIXTURE_STABLE_MS;
    if(state==FX_PASS && mode==FX_INSPECT) stable=FIXTURE_STABLE_MS;
    Fx_Put16(f+14,(uint16_t)stable); Fx_Seal(f); (void)io->send(f,sizeof(f));
    last_send=io->now();
}
void Fixture_Init(const FixturePort *port) {
    io=port; parser.used=0; seq=0; have_seq=false; mode=FX_NONE; state=FX_IDLE;
    reason=FX_OK; eye=false; measured=NAN; in_band=false;
    rearm_after_removal=removal_pending=continuous_wait=false;
    over_temp_pending=false;
    started=last_link=last_heat=last_send=last_probe=io->now(); io->off();
    update_status_led();
}
void Fixture_Feed(const uint8_t *data,size_t size) {
    uint8_t f[FX_FRAME_SIZE];
    while(size--) {
        if(!Fx_Parse(&parser,*data++,f)) continue;
        uint16_t incoming=Fx_Get16(f+4);
        if(f[3]==FX_START) {
            if(have_seq && incoming==seq) { report(); continue; } /* Never repeat EEPROM writes. */
            if(active()) { report(); continue; }
            io->off(); seq=incoming; have_seq=true; mode=f[6]; reason=FX_OK;
            measured=NAN; in_band=false; eye=false;
            rearm_after_removal=removal_pending=continuous_wait=false;
            over_temp_pending=false;
            started=last_link=io->now(); last_probe=started-100U; last_heat=started-150U;
            if(mode<FX_INITIALIZE || mode>FX_INSPECT) finish(FX_BAD_COMMAND);
            else state=FX_WAIT;
            report();
        } else if(have_seq && incoming==seq && f[6]==mode) {
            if(f[3]==FX_HEARTBEAT) last_link=io->now();
            else if(f[3]==FX_CANCEL) { finish(FX_CANCELLED); report(); }
        }
    }
}
void Fixture_Tick(void) {
    uint32_t now=io->now();
    /* Keep the result until sustained removal; never repeat a finished item.
     * The same transaction stays open across a batch, under its live heartbeat. */
    if(rearm_after_removal) {
        if(now-last_link>=FIXTURE_LINK_TIMEOUT_MS) finish(FX_LINK_LOST);
        else if(now-last_probe>=100U) {
            last_probe=now; eye=io->probe();
            if(eye) removal_pending=false;
            else if(!removal_pending) { removal_pending=true; removal_started=io->now(); }
            else if(io->now()-removal_started>=FIXTURE_REMOVE_CONFIRM_MS) {
                io->off(); state=FX_WAIT; reason=FX_OK; measured=NAN; in_band=false;
                over_temp_pending=false;
                rearm_after_removal=removal_pending=false; continuous_wait=true;
                started=last_heat=io->now(); report();
            }
        }
    }
    if(active()) {
        if(now-last_link>=FIXTURE_LINK_TIMEOUT_MS) finish(FX_LINK_LOST);
        else if(state==FX_WAIT) {
            if(now-last_probe>=100U) {
                last_probe=now; eye=io->probe();
                if(eye) {
                    if(mode==FX_INSPECT) {
                        uint8_t usable=io->check_usable();
                        if(usable!=FX_OK) finish(usable);
                        else { state=FX_HEATING; started=io->now(); }
                    }
                    else { state=FX_WRITE; report(); finish(io->prepare(mode==FX_DEMO)); }
                }
            }
            if(state==FX_WAIT && !continuous_wait && io->now()-started>=FIXTURE_WAIT_MS) finish(FX_NO_EYE);
        } else if(mode==FX_INSPECT && now-last_heat>=150U) {
            /* Probe immediately before every PID update, rather than using a stale debounce state. */
            last_heat=now; eye=io->probe();
            uint8_t usable=eye?io->check_usable():FX_REMOVED;
            if(usable!=FX_OK) finish(usable);
            else if(now-started>=FIXTURE_HEAT_TIMEOUT_MS) finish(FX_TIMEOUT);
            else {
                measured=NAN;
                bool ok=io->heater(FIXTURE_TARGET_C,&measured);
                now=io->now();
                if(!isfinite(measured)) finish(FX_TEMP_COMM);
                else if(measured < -40.0f || measured>125.0f) finish(FX_TEMP_COMM);
                else if(!ok) finish(FX_PWM_ERROR);
                else {
                    if(measured>=FIXTURE_MAX_SAFE_C) {
                        if(!over_temp_pending) { over_temp_pending=true; over_temp_started=now; }
                    } else over_temp_pending=false;
                    if(over_temp_pending && now-over_temp_started>=FIXTURE_OVER_TEMP_CONFIRM_MS) {
                        finish(FX_OVER_TEMP);
                    } else if(fabsf(measured-FIXTURE_TARGET_C)<=FIXTURE_TOLERANCE_C) {
                        if(!in_band) { in_band=true; band_started=now; }
                        state=FX_STABLE;
                        /* Do not announce PASS during the 5s overtemperature
                         * confirmation window, even inside the +/-3C band. */
                        if(now-band_started>=FIXTURE_STABLE_MS && !over_temp_pending) finish(FX_OK);
                    } else { in_band=false; state=FX_HEATING; }
                }
            }
        }
    }
    if(io->now()-last_send>=200U) report();
    update_status_led();
}
void Fixture_PowerLost(void) { if(active() || rearm_after_removal) finish(FX_POWER_LOST); else io->off(); }
