#include "strela/system.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *status_name(strela_system_status s)
{
    switch (s) {
    case STRELA_SYS_RUNNING: return "running";
    case STRELA_SYS_NORMAL_STOP: return "stopped";
    case STRELA_SYS_FAULT: return "fault";
    case STRELA_SYS_BUDGET: return "budget";
    case STRELA_SYS_PAUSED: return "paused";
    }
    return "unknown";
}

static int octal(const char *s, size_t digits, uint64_t max, uint64_t *out)
{
    uint64_t v = 0;
    size_t i;
    if (!s || strlen(s) != digits) return 0;
    for (i = 0; i < digits; ++i) {
        if (s[i] < '0' || s[i] > '7') return 0;
        v = (v << 3) | (unsigned)(s[i] - '0');
    }
    if (v > max) return 0;
    *out = v;
    return 1;
}

static int decimal(const char *s, uint64_t *out)
{
    char *end;
    unsigned long long v;
    if (!s || !*s) return 0;
    for (end = (char *)s; *end; ++end)
        if (!isdigit((unsigned char)*end)) return 0;
    errno = 0;
    v = strtoull(s, &end, 10);
    if (errno == ERANGE || *end) return 0;
    *out = (uint64_t)v;
    return 1;
}

static void word_line(const char *label, unsigned address, uint64_t word)
{
    printf("%s %04o: %015" PRIo64 "  %04o %04o %04o %o %02o\n",
           label, address, word,
           (unsigned)((word >> 31) & 07777),
           (unsigned)((word >> 19) & 07777),
           (unsigned)((word >> 7) & 07777),
           (unsigned)((word >> 6) & 1),
           (unsigned)(word & 077));
}

static void show(const strela_machine *m, unsigned ram, int zone_selected,
                 unsigned zone)
{
    strela_machine_state s;
    uint64_t word;
    unsigned i;
    strela_machine_read_state(m, &s);
    puts("=== CONTROL PANEL ===");
    printf("status=%s  reason=%s (%d)  pc=%04o  omega=%u  check=%u\n",
           status_name(s.status), strela_machine_reason_name(m), s.legacy_reason,
           s.pc, s.omega, s.check_stops ? 1u : 0u);
    printf("instructions=%" PRIu64 "  intervals=%" PRIu64
           "  elapsed-us=%" PRIu64 "  main-tact-us=%u"
           "  nominal-instruction-us=%u  estimate-flags=%u\n",
           s.instructions, s.cycles, s.elapsed_us, s.main_tact_us,
           s.instruction_us, s.timing_estimate_flags);
    printf("stop-operands=%015" PRIo64 "/%015" PRIo64
           "  group-left=%u  batch-left=%u  block-left=%u\n",
           s.display_a, s.display_b, s.group_remaining,
           s.batch_remaining, s.block_remaining);
    puts("=== ADDRESS GENERATOR / INSTRUCTION ===");
    word_line("last   ", s.last_location, s.last_instruction);
    if (strela_machine_read_word(m, s.pc, &word))
        word_line("next   ", s.pc, word);
    else printf("next    %04o: unknown or invalid address\n", s.pc);
    puts("=== MEMORY INSPECTION ===");
    for (i = ram; i < ram + 8 && i < 04000; ++i) {
        if (strela_machine_read_word(m, (uint16_t)i, &word))
            word_line("ram    ", i, word);
        else printf("ram     %04o: unreadable\n", i);
    }
    puts("=== MEDIA / EVENTS ===");
    printf("cards-read=%zu  cards-punched=%zu  tape-supplied=%04o/%04o"
           "  pending-events=%u  delivered=%" PRIu64 "\n",
           strela_machine_card_position(m), strela_machine_punched_words(m),
           s.tape_supplied[0], s.tape_supplied[1],
           s.pending_events, s.events_delivered);
    if (zone_selected) {
        unsigned n = strela_machine_zone_length(m, (uint16_t)zone);
        printf("zone=%04o  words=%u\n", zone, n);
        for (i = 0; i < n && i < 8; ++i)
            if (strela_machine_read_zone_word(m, (uint16_t)zone, i, &word))
                word_line("zone   ", i, word);
        if (n > 8) printf("zone: %u further words\n", n - 8);
    }
}

static int step_count(strela_machine *m, uint64_t count)
{
    uint64_t i;
    for (i = 0; i < count; ++i) {
        strela_system_status result = strela_machine_step(m);
        if (result != STRELA_SYS_PAUSED) break;
    }
    return 1;
}

static void help(void)
{
    puts("commands: show | ram ADDRESS | zone ADDRESS | step [COUNT] | run US");
    puts("          pause | resume | reset ADDRESS | set ADDRESS WORD");
    puts("          check 0|1 | save PATH | help | quit");
    puts("addresses are four octal digits; words are fifteen octal digits.");
}

static int action(strela_machine *m, char *line, unsigned *ram,
                  int *zone_selected, unsigned *zone)
{
    char *arg[5];
    int n = 0;
    uint64_t a, b;
    char *p = strtok(line, " \t\r\n");
    while (p && n < 5) {
        arg[n++] = p;
        p = strtok(NULL, " \t\r\n");
    }
    if (p || n > 3) return 0;
    if (!n) return 1;
    if (!strcmp(arg[0], "quit") && n == 1) return -1;
    if (!strcmp(arg[0], "help") && n == 1) { help(); return 1; }
    if (!strcmp(arg[0], "show") && n == 1) return 1;
    if (!strcmp(arg[0], "ram") && n == 2 &&
        octal(arg[1], 4, 03777, &a)) {
        *ram = (unsigned)a; return 1;
    }
    if (!strcmp(arg[0], "zone") && n == 2 &&
        octal(arg[1], 4, 05777, &a) &&
        (((a & 07000) == 04000) || ((a & 07000) == 05000)) &&
        (a & 0777)) {
        *zone = (unsigned)a; *zone_selected = 1; return 1;
    }
    if (!strcmp(arg[0], "step") && n <= 2 &&
        (n == 1 || decimal(arg[1], &a))) {
        if (n == 1) a = 1;
        return a && a <= 1000000 ? step_count(m, a) : 0;
    }
    if (!strcmp(arg[0], "run") && n == 2 &&
        decimal(arg[1], &a) && a) {
        strela_machine_resume(m);
        strela_machine_run_for_us(m, a);
        strela_machine_pause(m);
        return 1;
    }
    if (!strcmp(arg[0], "pause") && n == 1) {
        strela_machine_pause(m); return 1;
    }
    if (!strcmp(arg[0], "resume") && n == 1) {
        strela_machine_resume(m); return 1;
    }
    if (!strcmp(arg[0], "reset") && n == 2 &&
        octal(arg[1], 4, 03777, &a))
        return strela_machine_reset(m, (uint16_t)a) ? 1 : 0;
    if (!strcmp(arg[0], "set") && n == 3 &&
        octal(arg[1], 4, 07777, &a) &&
        octal(arg[2], 15, UINT64_C(0x7ffffffffff), &b))
        return (a < 04000 ?
                strela_machine_write_word(m, (uint16_t)a, b) :
                strela_machine_set_constant(m, (uint16_t)a, b)) ? 1 : 0;
    if (!strcmp(arg[0], "check") && n == 2 &&
        (!strcmp(arg[1], "0") || !strcmp(arg[1], "1"))) {
        strela_machine_state s;
        strela_machine_read_state(m, &s);
        if (s.status == STRELA_SYS_RUNNING) return 0;
        strela_machine_set_check_stops(m, arg[1][0] == '1');
        return 1;
    }
    if (!strcmp(arg[0], "save") && n == 2)
        return strela_machine_save_snapshot(m, arg[1]) ? 1 : 0;
    return 0;
}

int main(int argc, char **argv)
{
    strela_machine *m;
    const char *save = NULL;
    unsigned ram = 0, zone = 0;
    int zone_selected = 0, batch = 0, i;
    uint64_t value;
    char line[512];
    if (argc < 2 || !strcmp(argv[1], "--help")) {
        puts("usage: strela_mon session.sts [--state] [--ram 0052]");
        puts("       [--zone 4001] [--step N] [--run-us N]");
        puts("       [--save-snapshot next.sts]");
        puts("Without options, opens the interactive operator console.");
        return argc < 2 ? 2 : 0;
    }
    m = strela_machine_create(NULL);
    if (!m || !strela_machine_load_snapshot(m, argv[1])) {
        fprintf(stderr, "cannot load snapshot: %s\n", argv[1]);
        strela_machine_destroy(m);
        return 2;
    }
    for (i = 2; i < argc; ++i) {
        const char *key = argv[i];
        batch = 1;
        if (!strcmp(key, "--state")) continue;
        if (++i >= argc) goto invalid;
        if (!strcmp(key, "--ram") &&
            octal(argv[i], 4, 03777, &value)) ram = (unsigned)value;
        else if (!strcmp(key, "--zone") &&
                 octal(argv[i], 4, 05777, &value) &&
                 (((value & 07000) == 04000) ||
                  ((value & 07000) == 05000)) && (value & 0777)) {
            zone = (unsigned)value; zone_selected = 1;
        } else if (!strcmp(key, "--step") &&
                   decimal(argv[i], &value) && value && value <= 1000000)
            step_count(m, value);
        else if (!strcmp(key, "--run-us") &&
                 decimal(argv[i], &value) && value) {
            strela_machine_resume(m);
            strela_machine_run_for_us(m, value);
            strela_machine_pause(m);
        } else if (!strcmp(key, "--save-snapshot")) save = argv[i];
        else goto invalid;
    }
    show(m, ram, zone_selected, zone);
    if (save && !strela_machine_save_snapshot(m, save)) goto invalid;
    if (!batch) {
        help();
        while (fputs("strela> ", stdout), fflush(stdout),
               fgets(line, sizeof(line), stdin)) {
            int result;
            if (!strchr(line, '\n') && !feof(stdin)) {
                int c;
                while ((c = getchar()) != '\n' && c != EOF) {}
                fputs("command too long\n", stderr);
                continue;
            }
            result = action(m, line, &ram, &zone_selected, &zone);
            if (result < 0) break;
            if (!result) { fputs("invalid action or machine state\n", stderr); continue; }
            show(m, ram, zone_selected, zone);
        }
    }
    strela_machine_destroy(m);
    return 0;
invalid:
    fputs("invalid option, operator action, or output path\n", stderr);
    strela_machine_destroy(m);
    return 2;
}
