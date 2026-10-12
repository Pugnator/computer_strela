#include "strela.h"
#include "strela/system.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *status_name(strela_status status)
{
    switch (status) {
    case STRELA_RUNNING: return "running";
    case STRELA_STOPPED: return "normal stop";
    case STRELA_BAD_ADDRESS: return "bad address";
    case STRELA_BAD_OPCODE: return "unknown opcode";
    case STRELA_ARITHMETIC_STOP: return "arithmetic overflow";
    case STRELA_IO_UNAVAILABLE: return "I/O failure";
    case STRELA_CONTROL_STOP: return "control stop";
    case STRELA_MISMATCH_STOP: return "comparison mismatch";
    case STRELA_UNKNOWN_CONSTANT: return "unknown constant";
    case STRELA_DOMAIN_STOP: return "numeric domain stop";
    case STRELA_MEDIA_ABSENT: return "media absent";
    case STRELA_MEDIA_EXHAUSTED: return "media exhausted";
    case STRELA_CHECKSUM_MISMATCH: return "checksum mismatch";
    case STRELA_TIME_OVERFLOW: return "simulated time overflow";
    }
    return "unknown status";
}

static int octal_field(const char *s, size_t width, unsigned *out)
{
    size_t n;
    unsigned value=0;
    if (strlen(s)!=width) return 0;
    for (n=0;n<width;++n) {
        if (s[n]<'0' || s[n]>'7') return 0;
        value=(value<<3)+(unsigned)(s[n]-'0');
    }
    *out=value;
    return 1;
}

static int decimal_u64(const char *s, uint64_t *out)
{
    char *end;
    unsigned long long value;
    if (!*s) return 0;
    for (end=(char *)s;*end;++end) if (!isdigit((unsigned char)*end)) return 0;
    errno=0;
    value=strtoull(s,&end,10);
    if (*end || errno==ERANGE) return 0;
    *out=(uint64_t)value;
    return 1;
}

static int parse_listing(const char *path, uint64_t words[STRELA_RAM_WORDS],
                         unsigned char seen[STRELA_RAM_WORDS],
                         unsigned *start, int *has_start)
{
    FILE *f=fopen(path,"rb");
    char line[256];
    unsigned line_number=0;
    if (!f) { perror(path); return 0; }
    memset(words,0,STRELA_RAM_WORDS*sizeof(*words));
    memset(seen,0,STRELA_RAM_WORDS);
    *has_start=0;
    while (fgets(line,sizeof(line),f)) {
        char t[6][16], extra;
        unsigned address,a,b,c,check,opcode;
        strela_instruction i;
        char *p=line;
        ++line_number;
        if (!strchr(line,'\n') && !feof(f)) goto invalid;
        while (isspace((unsigned char)*p)) ++p;
        if (!*p || *p=='#') continue;
        if (strncmp(p,"START ",6)==0) {
            char value[16];
            if (*has_start || sscanf(p+6,"%15s %c",value,&extra)!=1 ||
                !octal_field(value,4,start) || *start>=STRELA_RAM_WORDS)
                goto invalid;
            *has_start=1;
            continue;
        }
        if (sscanf(p,"%15s %15s %15s %15s %15s %15s %c",
                   t[0],t[1],t[2],t[3],t[4],t[5],&extra)!=6 ||
            !octal_field(t[0],4,&address) || address>=STRELA_RAM_WORDS ||
            !octal_field(t[1],4,&a) || !octal_field(t[2],4,&b) ||
            !octal_field(t[3],4,&c) || !octal_field(t[4],1,&check) ||
            check>1 || !octal_field(t[5],2,&opcode) || seen[address])
            goto invalid;
        i.a=(uint16_t)a; i.b=(uint16_t)b; i.c=(uint16_t)c;
        i.check=(uint8_t)check; i.opcode=(uint8_t)opcode;
        words[address]=strela_pack(i);
        seen[address]=1;
    }
    if (ferror(f)) goto invalid;
    fclose(f);
    return 1;
invalid:
    fprintf(stderr,"%s:%u: invalid or duplicate octal listing entry\n",
            path,line_number);
    fclose(f);
    return 0;
}

static void print_word(unsigned address, uint64_t word)
{
    strela_instruction i=strela_unpack(word);
    printf("%04o: %04o %04o %04o %u %02o (binary interpretation %.12g)\n",
           address,i.a,i.b,i.c,i.check,i.opcode,strela_to_double(word));
}

static void print_legacy(const strela_t *m)
{
    printf("%s at %04o after %" PRIu64 " instructions, %" PRIu64
           " nominal intervals (%" PRIu64 " us simulated)\n",
           status_name(m->status),m->pc,m->instructions,m->cycles,m->elapsed_us);
    printf("last %04o next %04o omega=%u display=%013" PRIo64 "/%013" PRIo64
           " main-tact=%u us nominal-instruction=%u us\n",
           m->last_location,m->pc,m->omega,m->display_a,m->display_b,
           m->main_tact_us,m->instruction_us);
}

static int positional(int argc, char **argv)
{
    uint64_t words[STRELA_RAM_WORDS];
    unsigned char seen[STRELA_RAM_WORDS];
    strela_t m;
    unsigned start=0,dump=0;
    unsigned long steps=0,limit=1000000;
    int has_start,dumping=0;
    unsigned n;
    if (argc<2 || argc>5) return 2;
    if (!parse_listing(argv[1],words,seen,&start,&has_start)) return 2;
    if (argc>=3 && (!octal_field(argv[2],4,&start) || start>=STRELA_RAM_WORDS))
        return 2;
    if (argc>=4) {
        uint64_t value;
        if (!decimal_u64(argv[3],&value) || !value || value>1000000000u) return 2;
        limit=(unsigned long)value;
    }
    if (argc==5) {
        if (!octal_field(argv[4],4,&dump) || dump>=STRELA_RAM_WORDS) return 2;
        dumping=1;
    }
    strela_init(&m);
    for (n=0;n<STRELA_RAM_WORDS;++n)
        if (seen[n]) strela_write(&m,(uint16_t)n,words[n]);
    m.pc=(uint16_t)start;
    while (m.status==STRELA_RUNNING && steps++<limit) strela_step(&m);
    print_legacy(&m);
    if (dumping) print_word(dump,m.ram[dump]);
    n=(unsigned)m.status;
    strela_destroy(&m);
    if (n==STRELA_RUNNING) return 3;
    return (n==STRELA_STOPPED || n==STRELA_CONTROL_STOP) ? 0 : 1;
}

/* The first word is the stop reason recorded in the machine, so a snapshot
   saved after a stop (and therefore paused) still reports that stop. */
static void print_system(const strela_machine *m, strela_system_status result,
                         int dumping, unsigned dump)
{
    strela_machine_state state;
    strela_machine_read_state(m,&state);
    printf("%s at %04o after %" PRIu64 " instructions, %" PRIu64
           " nominal intervals (%" PRIu64 " us simulated)\n",
           result==STRELA_SYS_BUDGET ? "budget" :
           state.legacy_reason!=STRELA_RUNNING ?
           status_name((strela_status)state.legacy_reason) : "paused",
           state.pc,state.instructions,state.cycles,state.elapsed_us);
    printf("last %04o next %04o omega=%u display=%013" PRIo64 "/%013" PRIo64
           " main-tact=%u us nominal-instruction=%u us estimate-flags=%u\n",
           state.last_location,state.pc,state.omega,
           state.display_a,state.display_b,state.main_tact_us,
           state.instruction_us,state.timing_estimate_flags);
    printf("check-switch=%u group-left=%u batch-left=%u block-left=%u pending-events=%u delivered=%" PRIu64 "\n",
           state.check_stops ? 1u : 0u,state.group_remaining,
           state.batch_remaining,state.block_remaining,state.pending_events,state.events_delivered);
    if (dumping) {
        uint64_t word;
        if (strela_machine_read_word(m,(uint16_t)dump,&word)) print_word(dump,word);
        else printf("%04o: unknown or invalid address\n",dump);
    }
}

static int explicit_run(int argc, char **argv)
{
    uint64_t words[STRELA_RAM_WORDS],budget=UINT64_C(500000000);
    unsigned char seen[STRELA_RAM_WORDS];
    unsigned start=0,dump=0,n;
    int has_start,dumping=0,check_stops=0,j;
    const char *cards=NULL,*tape1=NULL,*tape2=NULL;
    const char *out_cards=NULL,*out_tape1=NULL,*out_tape2=NULL,*snapshot=NULL;
    strela_timing_profile profile=strela_timing_default();
    strela_machine *m;
    strela_machine_state state;
    strela_system_status result;
    if (argc<3 || !parse_listing(argv[2],words,seen,&start,&has_start)) return 2;
    for (j=3;j<argc;++j) {
        const char *key=argv[j],*value;
        if (strcmp(key,"--check-stops")==0) {
            check_stops=1;
            if (j+1<argc && (strcmp(argv[j+1],"0")==0 || strcmp(argv[j+1],"1")==0))
                check_stops=argv[++j][0]=='1';
            continue;
        }
        if (++j>=argc) { fprintf(stderr,"missing value for %s\n",key); return 2; }
        value=argv[j];
        if (strcmp(key,"--start")==0) {
            if (!octal_field(value,4,&start) || start>=STRELA_RAM_WORDS) return 2;
        } else if (strcmp(key,"--dump")==0) {
            if (!octal_field(value,4,&dump) || dump>=STRELA_RAM_WORDS) return 2;
            dumping=1;
        } else if (strcmp(key,"--max-us")==0) {
            if (!decimal_u64(value,&budget) || !budget) return 2;
        } else if (strcmp(key,"--instruction-us")==0) {
            uint64_t x; if (!decimal_u64(value,&x) || !x || x>UINT32_MAX) return 2;
            profile.instruction_us=(uint32_t)x;
        } else if (strcmp(key,"--main-tact-us")==0) {
            uint64_t x; if (!decimal_u64(value,&x) || !x || x>UINT32_MAX) return 2;
            profile.main_tact_us=(uint32_t)x;
        } else if (strcmp(key,"--cards")==0) cards=value;
        else if (strcmp(key,"--tape1")==0) tape1=value;
        else if (strcmp(key,"--tape2")==0) tape2=value;
        else if (strcmp(key,"--out-cards")==0) out_cards=value;
        else if (strcmp(key,"--out-tape1")==0) out_tape1=value;
        else if (strcmp(key,"--out-tape2")==0) out_tape2=value;
        else if (strcmp(key,"--save-snapshot")==0) snapshot=value;
        else { fprintf(stderr,"unknown option: %s\n",key); return 2; }
    }
    m=strela_machine_create(&profile);
    if (!m) return 2;
    for (n=0;n<STRELA_RAM_WORDS;++n)
        if (seen[n] && !strela_machine_write_word(m,(uint16_t)n,words[n])) goto input_error;
    if (!strela_machine_set_pc(m,(uint16_t)start) ||
        (cards && !strela_machine_load_cards(m,cards)) ||
        (tape1 && !strela_machine_load_tape(m,0,tape1)) ||
        (tape2 && !strela_machine_load_tape(m,1,tape2))) goto input_error;
    strela_machine_set_check_stops(m,check_stops!=0);
    strela_machine_resume(m);
    result=strela_machine_run_for_us(m,budget);
    strela_machine_read_state(m,&state);
    print_system(m,result,dumping,dump);
    strela_machine_pause(m);
    if ((out_cards && !strela_machine_save_punched(m,out_cards)) ||
        (out_tape1 && !strela_machine_save_tape(m,0,out_tape1)) ||
        (out_tape2 && !strela_machine_save_tape(m,1,out_tape2)) ||
        (snapshot && !strela_machine_save_snapshot(m,snapshot))) goto input_error;
    strela_machine_destroy(m);
    if (result==STRELA_SYS_BUDGET) return 3;
    return (state.legacy_reason==STRELA_STOPPED ||
            state.legacy_reason==STRELA_CONTROL_STOP) ? 0 : 1;
input_error:
    fprintf(stderr,"invalid input media or output path\n");
    strela_machine_destroy(m);
    return 2;
}

static int resume_or_inspect(int argc, char **argv, int inspect)
{
    strela_machine *m;
    strela_machine_state state;
    strela_system_status result;
    uint64_t budget=UINT64_C(500000000);
    const char *snapshot=NULL,*out_cards=NULL,*out_tape1=NULL,*out_tape2=NULL;
    unsigned dump=0,zone=0;
    int dumping=0,zone_requested=0,j;
    if (argc<3) return 2;
    for (j=3;j<argc;++j) {
        const char *key=argv[j],*value;
        if (++j>=argc) { fprintf(stderr,"missing value for %s\n",key); return 2; }
        value=argv[j];
        if (strcmp(key,"--max-us")==0 && !inspect) {
            if (!decimal_u64(value,&budget) || !budget) return 2;
        } else if ((strcmp(key,inspect ? "--ram" : "--dump")==0)) {
            if (!octal_field(value,4,&dump) || dump>=STRELA_RAM_WORDS) return 2;
            dumping=1;
        } else if (inspect && (strcmp(key,"--tape1-zone")==0 ||
                               strcmp(key,"--tape2-zone")==0)) {
            unsigned expected=strcmp(key,"--tape1-zone")==0 ? 04000 : 05000;
            if (!octal_field(value,4,&zone) ||
                (zone&07000)!=expected || !(zone&0777)) return 2;
            zone_requested=1;
        } else if (!inspect && strcmp(key,"--save-snapshot")==0) snapshot=value;
        else if (!inspect && strcmp(key,"--out-cards")==0) out_cards=value;
        else if (!inspect && strcmp(key,"--out-tape1")==0) out_tape1=value;
        else if (!inspect && strcmp(key,"--out-tape2")==0) out_tape2=value;
        else { fprintf(stderr,"unknown option: %s\n",key); return 2; }
    }
    m=strela_machine_create(NULL);
    if (!m) return 2;
    if (!strela_machine_load_snapshot(m,argv[2])) {
        fprintf(stderr,"invalid snapshot: %s\n",argv[2]);
        strela_machine_destroy(m); return 2;
    }
    if (inspect) {
        unsigned i,n;
        uint64_t word;
        print_system(m,STRELA_SYS_PAUSED,dumping,dump);
        if (zone_requested) {
            n=strela_machine_zone_length(m,(uint16_t)zone);
            printf("zone %04o: %u words\n",zone,n);
            for (i=0;i<n;++i)
                if (strela_machine_read_zone_word(m,(uint16_t)zone,i,&word))
                    print_word(i,word);
        }
        strela_machine_destroy(m); return 0;
    }
    strela_machine_resume(m);
    result=strela_machine_run_for_us(m,budget);
    print_system(m,result,dumping,dump);
    strela_machine_read_state(m,&state);
    strela_machine_pause(m);
    if ((out_cards && !strela_machine_save_punched(m,out_cards)) ||
        (out_tape1 && !strela_machine_save_tape(m,0,out_tape1)) ||
        (out_tape2 && !strela_machine_save_tape(m,1,out_tape2)) ||
        (snapshot && !strela_machine_save_snapshot(m,snapshot))) {
        fprintf(stderr,"cannot write snapshot or output media\n");
        strela_machine_destroy(m); return 2;
    }
    strela_machine_destroy(m);
    if (result==STRELA_SYS_BUDGET) return 3;
    return (state.legacy_reason==STRELA_STOPPED ||
            state.legacy_reason==STRELA_CONTROL_STOP) ? 0 : 1;
}


static int octal_word(const char *s, uint64_t *word)
{
    uint64_t x=0;
    unsigned i;
    if (strlen(s)!=15) return 0;
    for (i=0;i<15;++i) {
        if (s[i]<'0' || s[i]>'7') return 0;
        x=(x<<3)+(unsigned)(s[i]-'0');
    }
    if (x>STRELA_WORD_MASK) return 0;
    *word=x;
    return 1;
}
static int operate_snapshot(int argc, char **argv)
{
    strela_machine *m;
    strela_system_status result=STRELA_SYS_PAUSED;
    const char *save=NULL;
    unsigned dump=0;
    int dumping=0,j;
    if (argc<3) return 2;
    m=strela_machine_create(NULL);
    if (!m) return 2;
    if (!strela_machine_load_snapshot(m,argv[2])) {
        fprintf(stderr,"invalid snapshot: %s\n",argv[2]);
        strela_machine_destroy(m); return 2;
    }
    strela_machine_pause(m);
    for (j=3;j<argc;++j) {
        const char *key=argv[j];
        if (strcmp(key,"--set")==0) {
            unsigned address;
            uint64_t word;
            if (j+2>=argc || !octal_field(argv[j+1],4,&address) ||
                !octal_word(argv[j+2],&word) ||
                !(address<STRELA_RAM_WORDS ?
                  strela_machine_write_word(m,(uint16_t)address,word) :
                  strela_machine_set_constant(m,(uint16_t)address,word)))
                goto invalid;
            j+=2;
        } else if (strcmp(key,"--reset")==0) {
            unsigned address;
            if (++j>=argc || !octal_field(argv[j],4,&address) ||
                !strela_machine_reset(m,(uint16_t)address)) goto invalid;
            result=STRELA_SYS_PAUSED;
        } else if (strcmp(key,"--step")==0) {
            uint64_t count,k;
            if (++j>=argc || !decimal_u64(argv[j],&count) ||
                !count || count>1000000) goto invalid;
            for (k=0;k<count;++k) {
                result=strela_machine_step(m);
                if (result!=STRELA_SYS_PAUSED) break;
            }
        } else if (strcmp(key,"--check-stops")==0) {
            if (++j>=argc || (strcmp(argv[j],"0")!=0 &&
                               strcmp(argv[j],"1")!=0)) goto invalid;
            strela_machine_set_check_stops(m,argv[j][0]=='1');
        } else if (strcmp(key,"--dump")==0) {
            if (++j>=argc || !octal_field(argv[j],4,&dump) ||
                dump>=STRELA_RAM_WORDS) goto invalid;
            dumping=1;
        } else if (strcmp(key,"--save-snapshot")==0) {
            if (++j>=argc) goto invalid;
            save=argv[j];
        } else goto invalid;
    }
    print_system(m,result,dumping,dump);
    if (save && !strela_machine_save_snapshot(m,save)) goto invalid;
    strela_machine_destroy(m);
    return result==STRELA_SYS_FAULT ? 1 : 0;
invalid:
    fprintf(stderr,"invalid operator action or output path\n");
    strela_machine_destroy(m);
    return 2;
}

static int prepare_snapshot(int argc, char **argv)
{
    uint64_t words[STRELA_RAM_WORDS];
    unsigned char seen[STRELA_RAM_WORDS];
    unsigned start=0,n;
    int has_start,j;
    const char *output=NULL,*cards=NULL,*tape1=NULL,*tape2=NULL;
    strela_machine *m;
    if (argc<4 || !parse_listing(argv[2],words,seen,&start,&has_start))
        return 2;
    for (j=3;j<argc;++j) {
        const char *key=argv[j];
        if (++j>=argc) return 2;
        if (strcmp(key,"--start")==0) {
            if (!octal_field(argv[j],4,&start) || start>=STRELA_RAM_WORDS)
                return 2;
        } else if (strcmp(key,"--save-snapshot")==0) output=argv[j];
        else if (strcmp(key,"--cards")==0) cards=argv[j];
        else if (strcmp(key,"--tape1")==0) tape1=argv[j];
        else if (strcmp(key,"--tape2")==0) tape2=argv[j];
        else return 2;
    }
    if (!output) return 2;
    m=strela_machine_create(NULL);
    if (!m) return 2;
    for (n=0;n<STRELA_RAM_WORDS;++n)
        if (seen[n] && !strela_machine_write_word(m,(uint16_t)n,words[n]))
            goto failed;
    if (!strela_machine_set_pc(m,(uint16_t)start) ||
        (cards && !strela_machine_load_cards(m,cards)) ||
        (tape1 && !strela_machine_load_tape(m,0,tape1)) ||
        (tape2 && !strela_machine_load_tape(m,1,tape2)) ||
        !strela_machine_save_snapshot(m,output)) goto failed;
    strela_machine_destroy(m);
    return 0;
failed:
    strela_machine_destroy(m);
    return 2;
}
int main(int argc, char **argv)
{
    if (argc<2 || strcmp(argv[1],"--help")==0) {
        puts("usage: strela_run listing.txt [start-octal] [step-limit] [dump-octal]");
        puts("       strela_run prepare listing.txt --save-snapshot session.sts [--start 0010]");
        puts("         [--cards deck.cards] [--tape1 image.stt] [--tape2 image.stt]");
        puts("       strela_run run listing.txt [--start 0010] [--max-us 5000000]");
        puts("         [--cards deck.cards] [--tape1 image.stt] [--tape2 image.stt]");
        puts("         [--out-cards output.cards] [--out-tape1 output.stt]");
        puts("         [--out-tape2 output.stt] [--dump 0100] [--check-stops [0|1]]");
        puts("         [--instruction-us 500] [--main-tact-us 220] [--save-snapshot file.sts]");
        puts("       strela_run resume file.sts [--max-us 5000000] [--save-snapshot next.sts]");
        puts("         [--dump 0100] [--out-cards file] [--out-tape1 file] [--out-tape2 file]");
        puts("       strela_run inspect file.sts [--ram 0100] [--tape1-zone 4001]");
        puts("       strela_run operate file.sts [--reset 0010] [--set 0100 000000000000001]");
        puts("         [--check-stops 1] [--step 1] [--dump 0100] [--save-snapshot next.sts]");
        puts("exit: 0 normal/control stop, 1 machine fault, 2 input/output error, 3 budget/limit");
        return argc<2 ? 2 : 0;
    }
    if (strcmp(argv[1],"prepare")==0) return prepare_snapshot(argc,argv);
    if (strcmp(argv[1],"run")==0) return explicit_run(argc,argv);
    if (strcmp(argv[1],"resume")==0) return resume_or_inspect(argc,argv,0);
    if (strcmp(argv[1],"inspect")==0) return resume_or_inspect(argc,argv,1);
    if (strcmp(argv[1],"operate")==0) return operate_snapshot(argc,argv);
    return positional(argc,argv);
}

