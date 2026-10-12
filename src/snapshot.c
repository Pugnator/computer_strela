#include "system_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* STRELAST, version 1; FNV-1a-64 over a sectioned little-endian payload. */
typedef struct { unsigned char *p; size_t n, cap; bool ok; } writer;
typedef struct { const unsigned char *p; size_t n, pos; bool ok; } reader;
static const unsigned char magic[8] = {'S','T','R','E','L','A','S','T'};
enum { MAX_SNAPSHOT_BYTES = 32 * 1024 * 1024, MAX_CARD_WORDS = 1000000 };

static void bytes(writer *w, const void *p, size_t n)
{
    unsigned char *q;
    size_t cap;
    if (!w->ok || n > MAX_SNAPSHOT_BYTES - w->n) { w->ok=false; return; }
    if (w->n + n > w->cap) {
        cap=w->cap ? w->cap : 256;
        while (cap < w->n+n) cap*=2;
        q=(unsigned char *)realloc(w->p,cap);
        if (!q) { w->ok=false; return; }
        w->p=q; w->cap=cap;
    }
    memcpy(w->p+w->n,p,n);
    w->n+=n;
}
static void put(writer *w, uint64_t x, unsigned n)
{
    unsigned char b[8];
    unsigned i;
    for (i=0;i<n;++i) { b[i]=(unsigned char)x; x>>=8; }
    bytes(w,b,n);
}
static uint64_t take(reader *r, unsigned n)
{
    uint64_t x=0;
    unsigned i;
    if (!r->ok || n>r->n-r->pos) { r->ok=false; return 0; }
    for (i=0;i<n;++i) x|=(uint64_t)r->p[r->pos++] << (i*8);
    return x;
}
static size_t section(writer *w, unsigned id)
{
    size_t start=w->n;
    put(w,id,2); put(w,0,2); put(w,0,4);
    return start;
}
static void section_end(writer *w, size_t start)
{
    uint32_t length;
    unsigned i;
    if (!w->ok) return;
    length=(uint32_t)(w->n-start-8);
    for (i=0;i<4;++i) w->p[start+4+i]=(unsigned char)(length>>(8*i));
}
static uint64_t checksum(const unsigned char *p, size_t n)
{
    uint64_t h=UINT64_C(14695981039346656037);
    size_t i;
    for (i=0;i<n;++i) { h^=p[i]; h*=UINT64_C(1099511628211); }
    return h;
}
static bool atomic_file(const char *path, const unsigned char *p, size_t n)
{
    FILE *f;
    char *tmp;
    size_t len;
    bool ok;
    if (!path) return false;
    len=strlen(path);
    if (len>SIZE_MAX-5) return false;
    tmp=(char *)malloc(len+5);
    if (!tmp) return false;
    memcpy(tmp,path,len);
    memcpy(tmp+len,".tmp",5);
    f=fopen(tmp,"wb");
    if (!f) { free(tmp); return false; }
    ok=fwrite(p,1,n,f)==n;
    if (fclose(f)!=0) ok=false;
    if (ok) {
#ifdef _WIN32
        ok=MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        ok=rename(tmp,path)==0;
#endif
    }
    if (!ok) remove(tmp);
    free(tmp);
    return ok;
}
static bool read_file(const char *path, unsigned char **p, size_t *n)
{
    FILE *f;
    long size;
    unsigned char *data;
    if (!path || !(f=fopen(path,"rb"))) return false;
    if (fseek(f,0,SEEK_END)!=0 || (size=ftell(f))<0 ||
        size>MAX_SNAPSHOT_BYTES+22 || fseek(f,0,SEEK_SET)!=0) {
        fclose(f); return false;
    }
    data=(unsigned char *)malloc(size ? (size_t)size : 1);
    if (!data) { fclose(f); return false; }
    if (fread(data,1,(size_t)size,f)!=(size_t)size || fgetc(f)!=EOF) {
        free(data); fclose(f); return false;
    }
    fclose(f);
    *p=data; *n=(size_t)size; return true;
}
static void save_core(writer *w, const strela_machine *m)
{
    const strela_t *c=&m->core;
    size_t s=section(w,1);
    put(w,m->paused,1); put(w,c->pc,2); put(w,c->omega,1);
    put(w,c->check_stops,1); put(w,c->status,1);
    put(w,c->instructions,8); put(w,c->cycles,8); put(w,c->elapsed_us,8);
    put(w,c->instruction_us,4); put(w,c->main_tact_us,4);
    put(w,c->group_word,8); put(w,c->group_location,2);
    put(w,c->group_remaining,2); put(w,c->group_increment,1);
    put(w,c->last_location,2); put(w,c->last_instruction,8);
    put(w,c->display_a,8); put(w,c->display_b,8);
    put(w,c->card_start_us,4); put(w,c->card_word_us,4);
    put(w,c->tape_start_us,4); put(w,c->tape_word_us,4);
    put(w,c->tape_supply_us,4); put(w,c->timing_estimate_flags,1);
    put(w,c->batch_word,8); put(w,c->batch_location,2);
    put(w,c->batch_count,2); put(w,c->batch_index,2);
    put(w,c->batch_remaining,2); put(w,c->batch_a,2);
    put(w,c->batch_c,2); put(w,c->batch_opcode,1);
    section_end(w,s);
}
static void save_store(writer *w, const strela_t *c)
{
    unsigned i;
    size_t s=section(w,2);
    for (i=0;i<STRELA_RAM_WORDS;++i) put(w,c->ram[i],8);
    for (i=0;i<256;++i) put(w,c->cdd[i],8);
    for (i=0;i<256;++i) put(w,c->cdd_valid[i],1);
    section_end(w,s);
}
static void save_cards(writer *w, const strela_t *c)
{
    size_t i,s=section(w,3);
    put(w,c->card_mounted,1);
    put(w,c->card_deck_words,4); put(w,c->card_position,4);
    put(w,c->card_output_words,4);
    for (i=0;i<c->card_deck_words;++i) put(w,c->card_deck[i],8);
    for (i=0;i<c->card_output_words;++i) put(w,c->card_output[i],8);
    section_end(w,s);
}
static void save_tape(writer *w, const strela_t *c, unsigned unit)
{
    unsigned z,i,count=0;
    size_t s=section(w,4+unit);
    for (z=1;z<512;++z) if (c->tape_length[unit][z]) ++count;
    put(w,count,2);
    for (z=1;z<512;++z) if (c->tape_length[unit][z]) {
        put(w,z,2); put(w,c->tape_length[unit][z],2);
        for (i=0;i<c->tape_length[unit][z];++i)
            put(w,c->tape[unit][z][i],8);
    }
    section_end(w,s);
}
static void save_events(writer *w, const strela_t *c)
{
    unsigned i;
    size_t s=section(w,6);
    put(w,c->tape_supplied[0],2); put(w,c->tape_supplied[1],2);
    put(w,c->event_count,1);
    put(w,c->next_event_sequence,8); put(w,c->events_delivered,8);
    for (i=0;i<c->event_count;++i) {
        put(w,c->events[i].due_us,8);
        put(w,c->events[i].sequence,8);
        put(w,c->events[i].zone,2);
        put(w,c->events[i].unit,1);
    }
    section_end(w,s);
}
static void save_batch(writer *w, const strela_t *c)
{
    unsigned i;
    size_t s=section(w,7);
    for (i=0;i<c->batch_count;++i) put(w,c->batch_source[i],8);
    section_end(w,s);
}
static void save_block(writer *w, const strela_t *c)
{
    unsigned i;
    size_t s=section(w,8);
    put(w,c->block_word,8);
    put(w,c->block_location,2);
    put(w,c->block_count,2);
    put(w,c->block_index,2);
    put(w,c->block_remaining,2);
    put(w,c->block_a,2); put(w,c->block_c,2);
    put(w,c->block_source_zone,2); put(w,c->block_dest_zone,2);
    put(w,c->block_opcode,1);
    put(w,c->block_source_kind,1); put(w,c->block_dest_kind,1);
    put(w,c->block_media_kind,1);
    put(w,c->block_source_tape,1); put(w,c->block_dest_tape,1);
    put(w,c->block_recorded,8); put(w,c->block_calculated,8);
    for (i=0;i<c->block_count;++i) put(w,c->block_words[i],8);
    section_end(w,s);
}
bool strela_machine_save_snapshot(const strela_machine *m, const char *path)
{
    writer w={0};
    size_t i;
    uint64_t hash;
    bool ok;
    if (!m || m->core.card_read || m->core.card_write ||
        m->core.card_deck_words>MAX_CARD_WORDS ||
        m->core.card_output_words>MAX_CARD_WORDS) return false;
    w.ok=true;
    bytes(&w,magic,8); put(&w,1,2); put(&w,0,4); put(&w,0,8);
    save_core(&w,m); save_store(&w,&m->core); save_cards(&w,&m->core);
    save_tape(&w,&m->core,0); save_tape(&w,&m->core,1);
    save_events(&w,&m->core);
    save_batch(&w,&m->core);
    save_block(&w,&m->core);
    if (!w.ok) { free(w.p); return false; }
    for (i=0;i<4;++i) w.p[10+i]=(unsigned char)((w.n-22)>>(8*i));
    hash=checksum(w.p+22,w.n-22);
    for (i=0;i<8;++i) w.p[14+i]=(unsigned char)(hash>>(8*i));
    ok=atomic_file(path,w.p,w.n);
    free(w.p);
    return ok;
}
static bool core_ok(const strela_t *c)
{
    if (c->pc>=STRELA_RAM_WORDS || c->omega>1 || c->status>STRELA_TIME_OVERFLOW ||
        !c->instruction_us || !c->main_tact_us ||
        c->group_word>STRELA_WORD_MASK || c->group_location>=STRELA_RAM_WORDS ||
        c->group_remaining>4096 || c->group_increment>7 ||
        c->last_location>=STRELA_RAM_WORDS ||
        c->last_instruction>STRELA_WORD_MASK ||
        c->display_a>STRELA_WORD_MASK || c->display_b>STRELA_WORD_MASK ||
        c->timing_estimate_flags>7 || c->batch_word>STRELA_WORD_MASK ||
        c->batch_location>=STRELA_RAM_WORDS ||
        c->batch_count>STRELA_RAM_WORDS ||
        c->batch_index>c->batch_count ||
        c->batch_remaining>c->batch_count ||
        c->batch_index+c->batch_remaining!=c->batch_count ||
        c->batch_a>=STRELA_RAM_WORDS || c->batch_c>=STRELA_RAM_WORDS ||
        (c->batch_count && ((unsigned)c->batch_a+c->batch_count>STRELA_RAM_WORDS ||
                            (unsigned)c->batch_c+c->batch_count>STRELA_RAM_WORDS)))
        return false;
    return true;
}
static bool load_core(reader *r, strela_machine *m)
{
    strela_t *c=&m->core;
    uint64_t b;
    b=take(r,1); if (b>1) return false; m->paused=b!=0;
    c->pc=(uint16_t)take(r,2);
    c->omega=(uint8_t)take(r,1);
    b=take(r,1); if (b>1) return false; c->check_stops=b!=0;
    c->status=(strela_status)take(r,1);
    c->instructions=take(r,8); c->cycles=take(r,8); c->elapsed_us=take(r,8);
    c->instruction_us=(uint32_t)take(r,4);
    c->main_tact_us=(uint32_t)take(r,4);
    c->group_word=take(r,8);
    c->group_location=(uint16_t)take(r,2);
    c->group_remaining=(uint16_t)take(r,2);
    c->group_increment=(uint8_t)take(r,1);
    c->last_location=(uint16_t)take(r,2);
    c->last_instruction=take(r,8);
    c->display_a=take(r,8); c->display_b=take(r,8);
    c->card_start_us=(uint32_t)take(r,4);
    c->card_word_us=(uint32_t)take(r,4);
    c->tape_start_us=(uint32_t)take(r,4);
    c->tape_word_us=(uint32_t)take(r,4);
    c->tape_supply_us=(uint32_t)take(r,4);
    c->timing_estimate_flags=(uint8_t)take(r,1);
    c->batch_word=take(r,8);
    c->batch_location=(uint16_t)take(r,2);
    c->batch_count=(uint16_t)take(r,2);
    c->batch_index=(uint16_t)take(r,2);
    c->batch_remaining=(uint16_t)take(r,2);
    c->batch_a=(uint16_t)take(r,2);
    c->batch_c=(uint16_t)take(r,2);
    c->batch_opcode=(uint8_t)take(r,1);
    return r->ok && core_ok(c);
}
static bool load_store(reader *r, strela_t *c)
{
    unsigned i;
    for (i=0;i<STRELA_RAM_WORDS;++i)
        if ((c->ram[i]=take(r,8))>STRELA_WORD_MASK) return false;
    for (i=0;i<256;++i)
        if ((c->cdd[i]=take(r,8))>STRELA_WORD_MASK) return false;
    for (i=0;i<256;++i)
        if ((c->cdd_valid[i]=(uint8_t)take(r,1))>2) return false;
    return r->ok && c->ram[0]==0;
}
static bool load_cards(reader *r, strela_t *c)
{
    uint64_t mounted=take(r,1);
    uint32_t deck=(uint32_t)take(r,4), pos=(uint32_t)take(r,4);
    uint32_t output=(uint32_t)take(r,4),i;
    if (!r->ok || mounted>1 || deck>MAX_CARD_WORDS ||
        output>MAX_CARD_WORDS || deck%12 || pos>deck ||
        (!mounted && (deck || pos)) ||
        (size_t)(deck+output)>((r->n-r->pos)/8)) return false;
    c->card_mounted=mounted!=0;
    c->card_deck_words=deck; c->card_position=pos;
    c->card_output_words=output;
    if (deck) {
        c->card_deck=(uint64_t *)malloc((size_t)deck*8);
        if (!c->card_deck) return false;
    }
    if (output) {
        c->card_output=(uint64_t *)malloc((size_t)output*8);
        if (!c->card_output) return false;
    }
    for (i=0;i<deck;++i)
        if ((c->card_deck[i]=take(r,8))>STRELA_WORD_MASK) return false;
    for (i=0;i<output;++i)
        if ((c->card_output[i]=take(r,8))>STRELA_WORD_MASK) return false;
    return r->ok;
}
static bool load_tape(reader *r, strela_t *c, unsigned unit)
{
    unsigned count=(unsigned)take(r,2),k,prev=0,total=0;
    if (!r->ok || count>511) return false;
    for (k=0;k<count;++k) {
        unsigned z=(unsigned)take(r,2), n=(unsigned)take(r,2),i;
        if (!r->ok || z<=prev || z>=512 || !n ||
            n>STRELA_RAM_WORDS || n>100000-total ||
            (size_t)n>(r->n-r->pos)/8) return false;
        c->tape[unit][z]=(uint64_t *)malloc((size_t)n*8);
        if (!c->tape[unit][z]) return false;
        c->tape_length[unit][z]=(uint16_t)n;
        for (i=0;i<n;++i)
            if ((c->tape[unit][z][i]=take(r,8))>STRELA_WORD_MASK) return false;
        prev=z; total+=n;
    }
    c->tape_total_words[unit]=total;
    return r->ok;
}
static bool load_events(reader *r, strela_t *c)
{
    unsigned count,i;
    uint64_t prev_due=0,prev_sequence=0;
    c->tape_supplied[0]=(uint16_t)take(r,2);
    c->tape_supplied[1]=(uint16_t)take(r,2);
    count=(unsigned)take(r,1);
    c->next_event_sequence=take(r,8);
    c->events_delivered=take(r,8);
    if (!r->ok || c->tape_supplied[0]>=512 ||
        c->tape_supplied[1]>=512 || count>2 ||
        c->events_delivered>c->next_event_sequence) return false;
    c->event_count=(uint8_t)count;
    for (i=0;i<count;++i) {
        uint64_t due=take(r,8), sequence=take(r,8);
        unsigned zone=(unsigned)take(r,2), unit=(unsigned)take(r,1),j;
        if (!r->ok || !zone || zone>=512 || unit>=2 ||
            due<=c->elapsed_us || !sequence ||
            sequence>c->next_event_sequence ||
            (i && (due<prev_due || (due==prev_due && sequence<=prev_sequence))))
            return false;
        for (j=0;j<i;++j) if (c->events[j].unit==unit) return false;
        c->events[i].due_us=due;
        c->events[i].sequence=sequence;
        c->events[i].zone=(uint16_t)zone;
        c->events[i].unit=(uint8_t)unit;
        prev_due=due; prev_sequence=sequence;
    }
    return r->ok;
}
static bool load_batch(reader *r, strela_t *c)
{
    unsigned i;
    for (i=0;i<c->batch_count;++i)
        if ((c->batch_source[i]=take(r,8))>STRELA_WORD_MASK) return false;
    return r->ok;
}
static bool load_block(reader *r, strela_t *c)
{
    unsigned i;
    c->block_word=take(r,8);
    c->block_location=(uint16_t)take(r,2);
    c->block_count=(uint16_t)take(r,2);
    c->block_index=(uint16_t)take(r,2);
    c->block_remaining=(uint16_t)take(r,2);
    c->block_a=(uint16_t)take(r,2); c->block_c=(uint16_t)take(r,2);
    c->block_source_zone=(uint16_t)take(r,2);
    c->block_dest_zone=(uint16_t)take(r,2);
    c->block_opcode=(uint8_t)take(r,1);
    c->block_source_kind=(uint8_t)take(r,1);
    c->block_dest_kind=(uint8_t)take(r,1);
    c->block_media_kind=(uint8_t)take(r,1);
    c->block_source_tape=(uint8_t)take(r,1);
    c->block_dest_tape=(uint8_t)take(r,1);
    c->block_recorded=take(r,8); c->block_calculated=take(r,8);
    if (!r->ok || c->block_word>STRELA_WORD_MASK ||
        c->block_location>=STRELA_RAM_WORDS ||
        c->block_count>STRELA_RAM_WORDS ||
        c->block_index>c->block_count ||
        c->block_remaining>c->block_count ||
        c->block_index+c->block_remaining!=c->block_count ||
        c->block_a>07777 || c->block_c>07777 ||
        c->block_source_zone>=512 || c->block_dest_zone>=512 ||
        c->block_source_kind>2 || c->block_dest_kind>2 ||
        c->block_media_kind>2 ||
        c->block_source_tape>1 || c->block_dest_tape>1 ||
        c->block_recorded>STRELA_WORD_MASK ||
        c->block_calculated>STRELA_WORD_MASK ||
        (size_t)c->block_count>(r->n-r->pos)/8) return false;
    for (i=0;i<c->block_count;++i)
        if ((c->block_words[i]=take(r,8))>STRELA_WORD_MASK) return false;
    return r->ok;
}
bool strela_machine_load_snapshot(strela_machine *m, const char *path)
{
    unsigned char *data=NULL;
    size_t n;
    reader header,body;
    strela_machine *fresh;
    strela_t old;
    bool old_paused,ok=true;
    unsigned id;
    if (!m || !read_file(path,&data,&n)) return false;
    header.p=data; header.n=n; header.pos=0; header.ok=true;
    if (n<22 || memcmp(data,magic,8)!=0) { free(data); return false; }
    header.pos=8;
    if (take(&header,2)!=1 || take(&header,4)!=n-22 ||
        take(&header,8)!=checksum(data+22,n-22)) {
        free(data); return false;
    }
    fresh=strela_machine_create(NULL);
    if (!fresh) { free(data); return false; }
    body.p=data+22; body.n=n-22; body.pos=0; body.ok=true;
    for (id=1;id<=7 && ok;++id) {
        unsigned got=(unsigned)take(&body,2);
        unsigned flags=(unsigned)take(&body,2);
        uint32_t len=(uint32_t)take(&body,4);
        reader sec;
        if (!body.ok || got!=id || flags || len>body.n-body.pos) {
            ok=false; break;
        }
        sec.p=body.p+body.pos; sec.n=len; sec.pos=0; sec.ok=true;
        switch (id) {
        case 1: ok=load_core(&sec,fresh); break;
        case 2: ok=load_store(&sec,&fresh->core); break;
        case 3: ok=load_cards(&sec,&fresh->core); break;
        case 4: case 5: ok=load_tape(&sec,&fresh->core,id-4); break;
        case 6: ok=load_events(&sec,&fresh->core); break;
        case 7: ok=load_batch(&sec,&fresh->core); break;
        }
        if (!sec.ok || sec.pos!=sec.n) ok=false;
        body.pos+=len;
    }
    if (ok && body.pos<body.n) {
        unsigned got=(unsigned)take(&body,2);
        unsigned flags=(unsigned)take(&body,2);
        uint32_t len=(uint32_t)take(&body,4);
        reader sec;
        if (!body.ok || got!=8 || flags || len>body.n-body.pos)
            ok=false;
        else {
            sec.p=body.p+body.pos; sec.n=len; sec.pos=0; sec.ok=true;
            ok=load_block(&sec,&fresh->core);
            if (!sec.ok || sec.pos!=sec.n) ok=false;
            body.pos+=len;
        }
    }
    if (body.pos!=body.n) ok=false;
    if (ok) {
        old=m->core; old_paused=m->paused;
        m->core=fresh->core; m->paused=fresh->paused;
        fresh->core=old; fresh->paused=old_paused;
    }
    strela_machine_destroy(fresh);
    free(data);
    return ok;
}

