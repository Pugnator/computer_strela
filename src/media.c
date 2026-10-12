#include "strela_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

bool strela_tape_address(uint16_t address, unsigned *tape, unsigned *zone)
{
    unsigned base = address & 07000;
    unsigned number = address & 0777;
    if ((base != 04000 && base != 05000) || number == 0) return false;
    *tape = (base == 05000);
    *zone = number;
    return true;
}


bool strela_media_mount_cards(strela_t *m, const uint64_t *words, size_t count)
{
    uint64_t *copy;
    size_t n;
    if (!m || (count && !words) || count % 12 || count > SIZE_MAX / sizeof(*copy)) return false;
    copy = count ? (uint64_t *)malloc(count * sizeof(*copy)) : NULL;
    if (count && !copy) return false;
    for (n = 0; n < count; ++n) copy[n] = words[n] & STRELA_WORD_MASK;
    free(m->card_deck);
    m->card_deck = copy;
    m->card_deck_words = count;
    m->card_position = 0;
    m->card_mounted = true;
    return true;
}

unsigned strela_media_read_cards(strela_t *m, uint64_t *words, unsigned count)
{
    size_t available;
    unsigned n, take;
    if (m->card_mounted) {
        available = m->card_deck_words - m->card_position;
        take = available < count ? (unsigned)available : count;
        for (n = 0; n < take; ++n)
            words[n] = m->card_deck[m->card_position++];
        if (take < count) m->status = STRELA_MEDIA_EXHAUSTED;
        return take;
    }
    if (!m->card_read) { m->status = STRELA_MEDIA_ABSENT; return 0; }
    if (!m->card_read(m->card_user, words, count)) {
        m->status = STRELA_MEDIA_EXHAUSTED; return 0;
    }
    for (n = 0; n < count; ++n) words[n] &= STRELA_WORD_MASK;
    return count;
}

bool strela_media_punch(strela_t *m, const uint64_t *words, unsigned count)
{
    uint64_t *grown;
    unsigned n;
    size_t total;
    if (m->card_write) {
        if (!m->card_write(m->card_user, words, count)) {
            m->status = STRELA_IO_UNAVAILABLE; return false;
        }
        return true;
    }
    total = m->card_output_words + count;
    if (total < m->card_output_words || total > SIZE_MAX / sizeof(*grown)) {
        m->status = STRELA_IO_UNAVAILABLE; return false;
    }
    grown = (uint64_t *)realloc(m->card_output, total * sizeof(*grown));
    if (!grown) { m->status = STRELA_IO_UNAVAILABLE; return false; }
    m->card_output = grown;
    for (n = 0; n < count; ++n)
        m->card_output[m->card_output_words + n] = words[n] & STRELA_WORD_MASK;
    m->card_output_words = total;
    return true;
}

bool strela_media_store_zone(strela_t *m, unsigned tape, unsigned zone,
                             const uint64_t *words, unsigned count)
{
    uint64_t *copy;
    unsigned n;
    uint32_t old;
    if (!m || tape >= 2 || zone == 0 || zone >= 512 ||
        count == 0 || count > STRELA_RAM_WORDS || !words) return false;
    old = m->tape_length[tape][zone];
    if (m->tape_total_words[tape] - old + count > 100000) return false;
    copy = (uint64_t *)malloc(count * sizeof(*copy));
    if (!copy) return false;
    for (n = 0; n < count; ++n) copy[n] = words[n] & STRELA_WORD_MASK;
    free(m->tape[tape][zone]);
    m->tape[tape][zone] = copy;
    m->tape_length[tape][zone] = (uint16_t)count;
    m->tape_total_words[tape] = m->tape_total_words[tape] - old + count;
    return true;
}

bool strela_media_finalize_output(strela_t *m)
{
    size_t remainder, padded;
    uint64_t *grown;
    if (!m) return false;
    remainder = m->card_output_words % 12;
    if (!remainder) return true;
    padded = m->card_output_words + 12 - remainder;
    if (padded > SIZE_MAX / sizeof(*grown)) return false;
    grown = (uint64_t *)realloc(m->card_output, padded * sizeof(*grown));
    if (!grown) return false;
    m->card_output = grown;
    while (m->card_output_words < padded) m->card_output[m->card_output_words++] = 0;
    return true;
}

/* Text media images use the octal command-field spelling for every raw word.
   A parser builds a complete replacement before changing mounted media. */
static int media_line(FILE *f, char *line, size_t capacity)
{
    size_t n;
    if (!fgets(line,(int)capacity,f)) return feof(f) ? 0 : -1;
    n=strlen(line);
    if (n && line[n-1]=='\n') line[--n]=0;
    else if (!feof(f)) return -1;
    if (n && line[n-1]=='\r') line[--n]=0;
    return 1;
}

static bool tail_blank(const char *s)
{
    while (*s) { if (!isspace((unsigned char)*s)) return false; ++s; }
    return true;
}

static bool octal_token(const char *s, size_t width, unsigned *value)
{
    size_t i;
    unsigned v=0;
    if (strlen(s)!=width) return false;
    for (i=0;i<width;++i) {
        if (s[i]<'0' || s[i]>'7') return false;
        v=(v<<3)+(unsigned)(s[i]-'0');
    }
    *value=v;
    return true;
}

static bool parse_media_word(const char *line, uint64_t *word)
{
    char label[16],ta[16],tb[16],tc[16],tx[16],to[16],extra;
    unsigned a,b,c,check,opcode;
    strela_instruction i;
    if (sscanf(line,"%15s %15s %15s %15s %15s %15s %c",
               label,ta,tb,tc,tx,to,&extra)!=6 ||
        strcmp(label,"WORD")!=0 ||
        !octal_token(ta,4,&a) || !octal_token(tb,4,&b) ||
        !octal_token(tc,4,&c) || !octal_token(tx,1,&check) ||
        !octal_token(to,2,&opcode) || check>1) return false;
    i.a=(uint16_t)a; i.b=(uint16_t)b; i.c=(uint16_t)c;
    i.check=(uint8_t)check; i.opcode=(uint8_t)opcode;
    *word=strela_pack(i);
    return true;
}

static bool write_media_word(FILE *f, uint64_t word)
{
    strela_instruction i=strela_unpack(word);
    return fprintf(f,"WORD %04o %04o %04o %u %02o\n",
                   i.a,i.b,i.c,i.check,i.opcode)>0;
}

static FILE *begin_image_write(const char *path, char **temp_path)
{
    size_t n;
    FILE *f;
    if (!path) return NULL;
    n=strlen(path);
    if (n>SIZE_MAX-5) return NULL;
    *temp_path=(char *)malloc(n+5);
    if (!*temp_path) return NULL;
    memcpy(*temp_path,path,n);
    memcpy(*temp_path+n,".tmp",5);
    f=fopen(*temp_path,"wb");
    if (!f) { free(*temp_path); *temp_path=NULL; }
    return f;
}

static bool finish_image_write(FILE *f, char *temp, const char *path, bool ok)
{
    if (fclose(f)!=0) ok=false;
    if (ok) {
#ifdef _WIN32
        ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        ok=rename(temp,path)==0;
#endif
    }
    if (!ok) remove(temp);
    free(temp);
    return ok;
}

bool strela_media_load_cards_text(strela_t *m, const char *path)
{
    FILE *f;
    char line[256];
    uint64_t *words=NULL;
    size_t count=0;
    int result;
    bool ok=false;
    if (!m || !path || !(f=fopen(path,"rb"))) return false;
    result=media_line(f,line,sizeof(line));
    if (result!=1 || strcmp(line,"STRELA-CARDS 1")!=0) goto done;
    while ((result=media_line(f,line,sizeof(line)))==1) {
        uint64_t *grown;
        unsigned k;
        if (strcmp(line,"CARD")!=0 || count>SIZE_MAX/sizeof(*words)-12) goto done;
        grown=(uint64_t *)realloc(words,(count+12)*sizeof(*words));
        if (!grown) goto done;
        words=grown;
        for (k=0;k<12;++k) {
            if (media_line(f,line,sizeof(line))!=1 ||
                !parse_media_word(line,&words[count+k])) goto done;
        }
        count+=12;
    }
    if (result!=0) goto done;
    ok=strela_media_mount_cards(m,words,count);
done:
    free(words);
    fclose(f);
    return ok;
}

bool strela_media_save_cards_text(const strela_t *m, const char *path)
{
    FILE *f;
    char *temp=NULL;
    size_t n=0;
    bool ok=true;
    if (!m || !(f=begin_image_write(path,&temp))) return false;
    if (fprintf(f,"STRELA-CARDS 1\n")<0) ok=false;
    while (ok && n+12<=m->card_output_words) {
        unsigned k;
        if (fprintf(f,"CARD\n")<0) ok=false;
        for (k=0;k<12 && ok;++k)
            ok=write_media_word(f,m->card_output[n+k]);
        n+=12;
    }
    if (ok && n<m->card_output_words) {
        size_t partial=m->card_output_words-n;
        if (fprintf(f,"CARD-PARTIAL %u\n",(unsigned)partial)<0) ok=false;
        while (ok && n<m->card_output_words)
            ok=write_media_word(f,m->card_output[n++]);
    }
    return finish_image_write(f,temp,path,ok);
}

bool strela_media_load_tape_text(strela_t *m, unsigned unit, const char *path)
{
    FILE *f;
    char line[256];
    uint64_t *zones[512]={0};
    uint16_t lengths[512]={0};
    unsigned header_unit, prior=0, total=0, zone, count, k, tape, number;
    int result, end;
    bool ok=false;
    if (!m || unit>=2 || !path || !(f=fopen(path,"rb"))) return false;
    if (media_line(f,line,sizeof(line))!=1) goto done;
    end=0;
    if (sscanf(line,"STRELA-TAPE 1 UNIT=%u CAPACITY=HISTORICAL-100000%n",
               &header_unit,&end)!=1 || !end || !tail_blank(line+end) ||
        header_unit!=unit+1) goto done;
    while ((result=media_line(f,line,sizeof(line)))==1) {
        end=0;
        if (sscanf(line,"ZONE %o %u%n",&zone,&count,&end)!=2 || !end ||
            !tail_blank(line+end) || zone>07777 || !strela_tape_address((uint16_t)zone,&tape,&number) ||
            tape!=unit || number<=prior || !count || count>STRELA_RAM_WORDS ||
            count>100000-total) goto done;
        zones[number]=(uint64_t *)malloc(count*sizeof(uint64_t));
        if (!zones[number]) goto done;
        lengths[number]=(uint16_t)count;
        for (k=0;k<count;++k)
            if (media_line(f,line,sizeof(line))!=1 ||
                !parse_media_word(line,&zones[number][k])) goto done;
        total+=count; prior=number;
    }
    if (result!=0) goto done;
    for (k=0;k<512;++k) {
        free(m->tape[unit][k]);
        m->tape[unit][k]=zones[k]; zones[k]=NULL;
        m->tape_length[unit][k]=lengths[k];
    }
    m->tape_total_words[unit]=total;
    m->tape_supplied[unit]=0;
    ok=true;
done:
    for (k=0;k<512;++k) free(zones[k]);
    fclose(f);
    return ok;
}

bool strela_media_save_tape_text(const strela_t *m, unsigned unit, const char *path)
{
    FILE *f;
    char *temp=NULL;
    unsigned zone,k;
    bool ok=true;
    if (!m || unit>=2 || !(f=begin_image_write(path,&temp))) return false;
    if (fprintf(f,"STRELA-TAPE 1 UNIT=%u CAPACITY=HISTORICAL-100000\n",unit+1)<0)
        ok=false;
    for (zone=1;zone<512 && ok;++zone) {
        if (!m->tape[unit][zone]) continue;
        if (fprintf(f,"ZONE %04o %u\n",(unit?05000:04000)+zone,
                    m->tape_length[unit][zone])<0) ok=false;
        for (k=0;k<m->tape_length[unit][zone] && ok;++k)
            ok=write_media_word(f,m->tape[unit][zone][k]);
    }
    return finish_image_write(f,temp,path,ok);
}
