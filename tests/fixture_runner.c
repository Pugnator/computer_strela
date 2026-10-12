#include "strela.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned parse(const char *s, int base)
{
    char *end;
    unsigned long n = strtoul(s, &end, base);
    if (*end || n > 65535ul) { fprintf(stderr, "invalid argument: %s\n", s); exit(2); }
    return (unsigned)n;
}

int main(int argc, char **argv)
{
    strela_t m;
    FILE *f;
    char line[256];
    unsigned line_no = 0, steps = 0, a, b, c, check, op, addr;
    uint64_t expected, actual;
    strela_instruction i;
    if (argc != 14) { fprintf(stderr, "fixture arguments\n"); return 2; }
    f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    strela_init(&m);
    m.pc = (uint16_t)parse(argv[2], 8);
    while (fgets(line, sizeof(line), f)) {
        char extra;
        ++line_no;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%o %o %o %o %o %o %c",
                   &addr, &a, &b, &c, &check, &op, &extra) != 6 ||
            addr >= STRELA_RAM_WORDS || a > 07777 || b > 07777 ||
            c > 07777 || check > 1 || op > 077) {
            fprintf(stderr, "%s:%u invalid listing\n", argv[1], line_no);
            return 2;
        }
        i.a=(uint16_t)a; i.b=(uint16_t)b; i.c=(uint16_t)c;
        i.check=(uint8_t)check; i.opcode=(uint8_t)op;
        strela_write(&m, (uint16_t)addr, strela_pack(i));
    }
    fclose(f);
    while (m.status == STRELA_RUNNING && steps++ < 1000) strela_step(&m);
    i.a=(uint16_t)parse(argv[8],8); i.b=(uint16_t)parse(argv[9],8);
    i.c=(uint16_t)parse(argv[10],8);
    i.check=(uint8_t)parse(argv[11],8); i.opcode=(uint8_t)parse(argv[12],8);
    expected=strela_pack(i);
    if (!strela_read(&m,(uint16_t)parse(argv[7],8),&actual)) return 2;
    if (m.status != (strela_status)parse(argv[3],10) ||
        m.pc != parse(argv[4],8) || m.omega != parse(argv[5],10) ||
        m.cycles != parse(argv[6],10) ||
        m.elapsed_us != strtoull(argv[13], NULL, 10) || actual != expected) {
        fprintf(stderr,"status=%d pc=%04o omega=%u cycles=%" PRIu64
                " us=%" PRIu64 " word=%013" PRIo64 "\n",
                m.status,m.pc,m.omega,m.cycles,m.elapsed_us,actual);
        return 1;
    }
    strela_destroy(&m);
    return 0;
}
