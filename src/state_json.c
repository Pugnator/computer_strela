#include "state_json.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    int failed;
} json_buffer;

static void add(json_buffer *b, const char *format, ...)
{
    va_list args;
    int n;
    if (b->failed) return;
    va_start(args,format);
    n=vsnprintf(b->data+b->length,b->capacity-b->length,format,args);
    va_end(args);
    if (n<0 || (size_t)n>=b->capacity-b->length) {
        b->failed=1;
        return;
    }
    b->length+=(size_t)n;
}

static const char *status_name(strela_system_status status)
{
    switch (status) {
    case STRELA_SYS_RUNNING: return "running";
    case STRELA_SYS_NORMAL_STOP: return "stopped";
    case STRELA_SYS_FAULT: return "fault";
    case STRELA_SYS_BUDGET: return "budget";
    case STRELA_SYS_PAUSED: return "paused";
    }
    return "unknown";
}

static void instruction(json_buffer *b, uint64_t word)
{
    add(b,"{\"raw\":\"%015" PRIo64 "\",\"a\":\"%04o\",\"b\":\"%04o\","
          "\"c\":\"%04o\",\"check\":%u,\"opcode\":\"%02o\"}",
        word,(unsigned)((word>>31)&07777),(unsigned)((word>>19)&07777),
        (unsigned)((word>>7)&07777),(unsigned)((word>>6)&1),
        (unsigned)(word&077));
}

int strela_state_json(const strela_machine *m, unsigned ram, unsigned zone,
                      char *output, size_t capacity)
{
    strela_machine_state s;
    json_buffer b={output,0,capacity,0};
    uint64_t word;
    unsigned i,n;
    if (!m || !output || !capacity || ram>=04000 ||
        (zone && !(((zone&07000)==04000 || (zone&07000)==05000) &&
                   (zone&0777) && zone<=05777))) return -1;
    output[0]='\0';
    strela_machine_read_state(m,&s);
    add(&b,"{\"status\":\"%s\",\"reason\":\"%s\",\"reasonCode\":%d,"
           "\"pc\":\"%04o\",\"omega\":%u,\"checkStops\":%s,"
           "\"instructions\":\"%" PRIu64 "\",\"intervals\":\"%" PRIu64 "\","
           "\"elapsedUs\":\"%" PRIu64 "\",\"mainTactUs\":%u,"
           "\"instructionUs\":%u,\"timingEstimateFlags\":%u,"
           "\"lastLocation\":\"%04o\",\"displayA\":\"%015" PRIo64 "\","
           "\"displayB\":\"%015" PRIo64 "\","
           "\"groupRemaining\":%u,\"batchRemaining\":%u,"
           "\"blockRemaining\":%u,\"cardPosition\":%zu,"
           "\"punchedWords\":%zu,\"tapeSupplied\":[\"%04o\",\"%04o\"],"
           "\"pendingEvents\":%u,\"eventsDelivered\":\"%" PRIu64 "\",",
        status_name(s.status),strela_machine_reason_name(m),s.legacy_reason,
        s.pc,s.omega,s.check_stops?"true":"false",s.instructions,
        s.cycles,s.elapsed_us,s.main_tact_us,s.instruction_us,
        s.timing_estimate_flags,s.last_location,s.display_a,s.display_b,
        s.group_remaining,s.batch_remaining,s.block_remaining,
        strela_machine_card_position(m),strela_machine_punched_words(m),
        s.tape_supplied[0],s.tape_supplied[1],s.pending_events,
        s.events_delivered);
    add(&b,"\"lastInstruction\":");
    instruction(&b,s.last_instruction);
    add(&b,",\"nextInstruction\":");
    if (strela_machine_read_word(m,s.pc,&word)) instruction(&b,word);
    else add(&b,"null");
    add(&b,",\"ramStart\":\"%04o\",\"ram\":[",ram);
    for (i=ram;i<ram+16 && i<04000;++i) {
        if (i!=ram) add(&b,",");
        if (strela_machine_read_word(m,(uint16_t)i,&word))
            add(&b,"{\"address\":\"%04o\",\"word\":\"%015" PRIo64 "\"}",i,word);
        else add(&b,"{\"address\":\"%04o\",\"word\":null}",i);
    }
    add(&b,"],\"zone\":");
    if (zone) {
        n=strela_machine_zone_length(m,(uint16_t)zone);
        add(&b,"{\"address\":\"%04o\",\"length\":%u,\"words\":[",zone,n);
        for (i=0;i<n && i<16;++i) {
            if (i) add(&b,",");
            if (strela_machine_read_zone_word(m,(uint16_t)zone,i,&word))
                add(&b,"\"%015" PRIo64 "\"",word);
            else add(&b,"null");
        }
        add(&b,"]}");
    } else add(&b,"null");
    add(&b,"}");
    return b.failed ? -1 : (int)b.length;
}

int strela_punched_json(const strela_machine *m, size_t start, unsigned limit,
                        char *output, size_t capacity)
{
    json_buffer b={output,0,capacity,0};
    size_t count,index,end;
    uint64_t word;
    if (!m || !output || !capacity || !limit || limit>128) return -1;
    output[0]='\0';
    count=strela_machine_punched_words(m);
    if (start>count) return -1;
    end=count-start<limit ? count : start+limit;
    add(&b,"{\"total\":%zu,\"start\":%zu,\"words\":[",count,start);
    for (index=start;index<end;++index) {
        if (index!=start) add(&b,",");
        if (!strela_machine_read_punched(m,index,&word)) return -1;
        add(&b,"\"%015" PRIo64 "\"",word);
    }
    add(&b,"]}");
    return b.failed ? -1 : (int)b.length;
}