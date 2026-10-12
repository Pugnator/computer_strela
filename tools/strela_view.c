#include "strela/system.h"
#include "state_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static int address4(const char *s, unsigned max, unsigned *out)
{
    unsigned v=0,i;
    if (strlen(s)!=4) return 0;
    for (i=0;i<4;++i) {
        if (s[i]<'0' || s[i]>'7') return 0;
        v=(v<<3)+(unsigned)(s[i]-'0');
    }
    if (v>max) return 0;
    *out=v;
    return 1;
}

int main(int argc, char **argv)
{
    strela_machine *m;
    char output[8192];
    unsigned ram=0,zone=0,i,card_count=128;
    size_t card_start=0;
    int cards=0;
    if (argc<2) return 2;
    for (i=2;i<(unsigned)argc;++i) {
        if (++i>=(unsigned)argc) return 2;
        if (!strcmp(argv[i-1],"--ram")) {
            if (!address4(argv[i],03777,&ram)) return 2;
        } else if (!strcmp(argv[i-1],"--zone")) {
            if (!address4(argv[i],05777,&zone) ||
                !((zone&07000)==04000 || (zone&07000)==05000) ||
                !(zone&0777)) return 2;
        } else if (!strcmp(argv[i-1],"--cards") || !strcmp(argv[i-1],"--count")) {
            char *end;
            unsigned long value;
            const char *digit=argv[i];
            if (!*digit) return 2;
            for (;*digit;++digit) if (*digit<'0' || *digit>'9') return 2;
            errno=0;
            value=strtoul(argv[i],&end,10);
            if (errno || *end) return 2;
            if (!strcmp(argv[i-1],"--cards")) { card_start=value; cards=1; }
            else {
                if (!value || value>128) return 2;
                card_count=(unsigned)value;
            }
        } else return 2;
    }
    m=strela_machine_create(NULL);
    if (!m || !strela_machine_load_snapshot(m,argv[1])) {
        fprintf(stderr,"invalid snapshot\n");
        strela_machine_destroy(m);
        return 2;
    }
    if ((cards ? strela_punched_json(m,card_start,card_count,output,sizeof(output)) :
                 strela_state_json(m,ram,zone,output,sizeof(output)))<0) {
        fprintf(stderr,"cannot serialize machine state\n");
        strela_machine_destroy(m);
        return 2;
    }
    puts(output);
    strela_machine_destroy(m);
    return 0;
}
