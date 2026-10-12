#include "strela/system.h"
#include "state_json.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static strela_machine *machine;
static char state_output[8192];

static int octal_word(const char *text, uint64_t *word)
{
    uint64_t value=0;
    unsigned i;
    if (!text || strlen(text)!=15) return 0;
    for (i=0;i<15;++i) {
        if (text[i]<'0' || text[i]>'7') return 0;
        value=(value<<3)+(unsigned)(text[i]-'0');
    }
    if (value>UINT64_C(0x7ffffffffff)) return 0;
    *word=value;
    return 1;
}

static int decimal_u64(const char *text, uint64_t *value)
{
    char *end;
    unsigned long long parsed;
    if (!text || !*text) return 0;
    for (end=(char *)text;*end;++end)
        if (!isdigit((unsigned char)*end)) return 0;
    errno=0;
    parsed=strtoull(text,&end,10);
    if (errno==ERANGE || *end || !parsed) return 0;
    *value=(uint64_t)parsed;
    return 1;
}

int sb_new(void)
{
    strela_machine *fresh=strela_machine_create(NULL);
    if (!fresh) return 0;
    strela_machine_destroy(machine);
    machine=fresh;
    return 1;
}

void sb_destroy(void)
{
    strela_machine_destroy(machine);
    machine=NULL;
}

int sb_write_word(unsigned address, const char *text)
{
    uint64_t word;
    if (!machine || address>07777 || !octal_word(text,&word)) return 0;
    if (address<04000)
        return strela_machine_write_word(machine,(uint16_t)address,word);
    return strela_machine_set_constant(machine,(uint16_t)address,word);
}

int sb_set_pc(unsigned address)
{
    return machine && address<04000 &&
           strela_machine_set_pc(machine,(uint16_t)address);
}

int sb_mount_cards(const char *path)
{ return machine && path && strela_machine_load_cards(machine,path); }

int sb_mount_tape(unsigned unit, const char *path)
{ return machine && unit<2 && path && strela_machine_load_tape(machine,unit,path); }

int sb_pause(void)
{
    if (!machine) return 0;
    strela_machine_pause(machine);
    return 1;
}

int sb_reset(unsigned address)
{
    if (!machine || address>=04000) return 0;
    strela_machine_pause(machine);
    return strela_machine_reset(machine,(uint16_t)address);
}

int sb_check(int enabled)
{
    if (!machine || (enabled!=0 && enabled!=1)) return 0;
    strela_machine_pause(machine);
    strela_machine_set_check_stops(machine,enabled!=0);
    return 1;
}

int sb_step(unsigned count)
{
    unsigned i;
    if (!machine || !count || count>1000000) return 0;
    strela_machine_pause(machine);
    for (i=0;i<count;++i)
        if (strela_machine_step(machine)!=STRELA_SYS_PAUSED) break;
    return 1;
}

int sb_run_us(const char *budget_text)
{
    uint64_t budget;
    if (!machine || !decimal_u64(budget_text,&budget)) return 0;
    strela_machine_resume(machine);
    strela_machine_run_for_us(machine,budget);
    strela_machine_pause(machine);
    return 1;
}

int sb_save_snapshot(const char *path)
{ return machine && path && strela_machine_save_snapshot(machine,path); }

int sb_load_snapshot(const char *path)
{
    if (!path) return 0;
    if (!machine && !sb_new()) return 0;
    return strela_machine_load_snapshot(machine,path);
}

const char *sb_state_json(unsigned ram, unsigned zone)
{
    if (!machine ||
        strela_state_json(machine,ram,zone,state_output,sizeof(state_output))<0)
        return NULL;
    return state_output;
}

const char *sb_punched_json(unsigned start, unsigned limit)
{
    if (!machine ||
        strela_punched_json(machine,start,limit,state_output,sizeof(state_output))<0)
        return NULL;
    return state_output;
}