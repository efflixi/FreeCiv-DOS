#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef __DJGPP__
#error This diagnostic requires DJGPP and a real DOS/DPMI runtime
#endif

#include <conio.h>
#include <dir.h>
#include <dpmi.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/movedata.h>
#include <sys/stat.h>
#include <unistd.h>

#include "capability.h"
#include "capstr.h"
#include "connection.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "packets.h"
#include "support.h"

#include "engine.h"
#include "session.h"

/* This marker is test data, not a ruleset or any other game asset. */
#define FIXTURE_MARKER "FREECIV DOS RUNTIME FIXTURE 1"
#define HEAP_BYTES (1024UL * 1024UL)
#define TRANSFER_BYTES 512

bool is_server = FALSE;
static FILE *runtime_log;

static void report(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  putchar('\n');
  fflush(stdout);
  if (runtime_log) {
    va_start(args, format);
    vfprintf(runtime_log, format, args);
    va_end(args);
    fputc('\n', runtime_log);
    fflush(runtime_log);
    if (ferror(runtime_log)) {
      fputs("FAIL LOG: cannot write runtime log\n", stderr);
      exit(EXIT_FAILURE);
    }
  }
}

static int require(int condition, const char *message)
{
  report("%s %s", condition ? "PASS" : "FAIL", message);
  return condition;
}

/* The client common instance needs these test-only linkage helpers. */
void dealloc_id(int id)
{
  (void)id;
  if (is_server) {
    report("FAIL ENGINE: server ID release reached client common state");
    exit(EXIT_FAILURE);
  }
}

void send_unit_info(struct player *owner, struct unit *unit)
{
  (void)owner;
  (void)unit;
  report("FAIL ENGINE: unexpected server-only client unit notification");
  exit(EXIT_FAILURE);
}

static int check_dos_dpmi(void)
{
  __dpmi_regs regs;
  __dpmi_version_ret version;

  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x3000;             /* Read-only DOS get-version call. */
  if (!require(__dpmi_int(0x21, &regs) == 0,
               "DOS: real-mode INT 21h AX=3000 query")) {
    return 0;
  }
  report("INFO DOS reported version %u.%u", (unsigned)regs.h.al,
         (unsigned)regs.h.ah);
  if (!require(regs.h.al == 6 && regs.h.ah == 22,
               "DOS: required reported version is 6.22")) {
    return 0;
  }
  memset(&version, 0, sizeof(version));
  if (!require(__dpmi_get_version(&version) == 0,
               "DPMI: INT 31h AX=0400 version query")) {
    return 0;
  }
  report("INFO DPMI version %u.%u flags=0x%04x CPU=%u PIC=%u/%u",
         (unsigned)version.major, (unsigned)version.minor,
         (unsigned)version.flags, (unsigned)version.cpu,
         (unsigned)version.master_pic, (unsigned)version.slave_pic);
  return require((version.flags & 1) != 0 && version.cpu >= 3,
                 "DPMI: host supports 32-bit applications on 386+");
}

static int check_memory(void)
{
  volatile unsigned long *heap;
  unsigned long i, words = HEAP_BYTES / sizeof(unsigned long);
  unsigned char source[TRANSFER_BYTES], copy[TRANSFER_BYTES];
  int segment, selector, matched, released;

  if (!require(sizeof(void *) == 4 && sizeof(unsigned long) == 4,
               "MEMORY: 32-bit pointer and word ABI")) {
    return 0;
  }
  heap = malloc(HEAP_BYTES);
  if (!require(heap != NULL, "MEMORY: allocate 1 MiB protected-mode heap")) {
    return 0;
  }
  for (i = 0; i < words; i++) {
    heap[i] = i ^ 0xa55a1234UL;
  }
  for (i = 0; i < words; i++) {
    if (heap[i] != (i ^ 0xa55a1234UL)) {
      report("FAIL MEMORY: heap read/write mismatch at word %lu", i);
      free((void *)heap);
      return 0;
    }
  }
  free((void *)heap);
  report("PASS MEMORY: all 1 MiB read/written and heap released");

  segment = __dpmi_allocate_dos_memory((TRANSFER_BYTES + 15) / 16,
                                       &selector);
  if (!require(segment >= 0,
               "TRANSFER: DPMI allocate conventional DOS buffer")) {
    return 0;
  }
  report("INFO TRANSFER segment=0x%04x selector=0x%04x bytes=%u",
         segment, selector, (unsigned)TRANSFER_BYTES);
  for (i = 0; i < TRANSFER_BYTES; i++) {
    source[i] = (unsigned char)(i ^ (i >> 8) ^ 0x5a);
  }
  memset(copy, 0, sizeof(copy));
  dosmemput(source, sizeof(source), (unsigned long)segment << 4);
  dosmemget((unsigned long)segment << 4, sizeof(copy), copy);
  matched = require(memcmp(source, copy, sizeof(copy)) == 0,
                    "TRANSFER: dosmemput/dosmemget roundtrip all 512 bytes");
  released = require(__dpmi_free_dos_memory(selector) == 0,
                     "TRANSFER: DPMI conventional buffer released");
  return matched && released;
}

static int fixture_path(const char *program, const char *fixture, char *path)
{
  char executable[MAXPATH], drive[MAXDRIVE], directory[MAXDIR];
  char name[MAXFILE], extension[MAXEXT];
  char *located;
  size_t drive_len, directory_len, fixture_len;

  if (strlen(fixture) >= MAXPATH) {
    report("FAIL DATA: fixture path is too long");
    return 0;
  }
  if (fixture[0] == '/' || fixture[0] == '\\'
      || (strlen(fixture) >= 3 && fixture[1] == ':'
          && (fixture[2] == '/' || fixture[2] == '\\'))) {
    strcpy(path, fixture);
    return 1;
  }
  if (strchr(fixture, ':')) {
    report("FAIL DATA: drive-relative paths are ambiguous; use C:\\path");
    return 0;
  }
  located = searchpath(program);
  if (!located || strlen(located) >= MAXPATH) {
    report("FAIL DATA: DJGPP searchpath cannot locate executable");
    return 0;
  }
  _fixpath(located, executable);
  fnsplit(executable, drive, directory, name, extension);
  drive_len = strlen(drive);
  directory_len = strlen(directory);
  fixture_len = strlen(fixture);
  if (drive_len + directory_len + fixture_len >= MAXPATH) {
    report("FAIL DATA: executable-relative fixture path is too long");
    return 0;
  }
  memcpy(path, drive, drive_len);
  memcpy(path + drive_len, directory, directory_len);
  memcpy(path + drive_len + directory_len, fixture, fixture_len + 1);
  return 1;
}

static int check_fixture(const char *path)
{
  FILE *file;
  char line[128];
  int valid, closed;

  report("INFO DATA runtime-test fixture lookup: %s", path);
  file = fopen(path, "rb");
  if (!file) {
    report("FAIL DATA: cannot read runtime-test fixture: %s", strerror(errno));
    return 0;
  }
  valid = fgets(line, sizeof(line), file) != NULL;
  if (valid) {
    line[strcspn(line, "\r\n")] = '\0';
    valid = strcmp(line, FIXTURE_MARKER) == 0 && fgetc(file) == EOF
            && !ferror(file);
  }
  closed = fclose(file) == 0;
  return require(valid && closed,
                 "DATA: exact runtime-test marker read and fixture closed");
}

static int check_engine(void)
{
  struct offline_session session = {0};
  struct connection client;
  struct fc_offline_snapshot snapshot;
  struct tile *client_tiles;
  int starts = 0, finishes = 0, replies = 0, iterations, received;
  int ok = 0;

  memset(&client, 0, sizeof(client));
  game_init();
  game.year = 1234;
  client_tiles = map.tiles;
  if (!require(offline_session_open(&session, &client, "DOSRuntime") == 0,
               "ENGINE: open actual isolated authoritative local session")) {
    goto done;
  }
  if (!require(send_packet_generic_empty(&client, PACKET_CONN_PONG) == 2,
               "ENGINE: join plus pong submitted as requests 1 and 2")) {
    goto done;
  }
  for (iterations = 0; iterations < 128; iterations++) {
    enum packet_type type;
    bool available;
    void *packet;

    received = offline_session_pump(&session, 1);
    if (received < 0) {
      report("FAIL ENGINE: local session pump/transfer failed");
      goto done;
    }
    for (;;) {
      packet = get_packet_from_connection(&client, &type, &available);
      if (!available) {
        break;
      }
      if (!packet) {
        report("FAIL ENGINE: available packet failed to decode");
        goto done;
      }
      if (type == PACKET_PROCESSING_STARTED) {
        if (starts != finishes) {
          free(packet);
          report("FAIL ENGINE: overlapping request boundaries");
          goto done;
        }
        starts++;
      } else if (type == PACKET_JOIN_GAME_REPLY) {
        struct packet_join_game_reply *reply = packet;
        if (!require(reply->you_can_join && starts == 1 && finishes == 0,
                     "ENGINE: authoritative join accepted inside request 1")
            || !require(has_capabilities(our_capability, reply->capability)
                        && has_capabilities(reply->capability, our_capability),
                        "ENGINE: real join capability compatibility")) {
          free(packet);
          goto done;
        }
        report("INFO ENGINE accepted capability: %s", reply->capability);
        sz_strlcpy(client.capability, reply->capability);
        client.established = TRUE;
        replies++;
      } else if (type == PACKET_PROCESSING_FINISHED) {
        if (starts != finishes + 1 || replies != 1) {
          free(packet);
          report("FAIL ENGINE: unordered request finish/join reply");
          goto done;
        }
        finishes++;
      }
      free(packet);
    }
    if (finishes == 2 && received == 0) {
      break;
    }
  }
  report("INFO ENGINE counters starts=%d finishes=%d replies=%d client=%d",
         starts, finishes, replies, client.client.last_request_id_used);
  if (!require(starts == 2 && finishes == 2 && replies == 1
               && client.client.last_request_id_used == 2
               && client.buffer->ndata == 0 && client.send_buffer->ndata == 0
               && session.requests.count == 0 && session.responses.count == 0,
               "ENGINE: 2 starts/2 finishes/1 join; request counters and queues")
      || !require(fc_offline_engine_snapshot(&snapshot) == 0
                  && snapshot.established && snapshot.players == 1
                  && snapshot.last_request == 2 && !snapshot.map_allocated,
                  "ENGINE: authoritative established player/request=2; no map")
      || !require(!is_server && game.nplayers == 0 && game.year == 1234
                  && snapshot.year != game.year && map.tiles == client_tiles,
                  "ENGINE: client/common game state remains separate")) {
    goto done;
  }
  report("INFO ENGINE authoritative last_request=%d players=%d",
         snapshot.last_request, snapshot.players);
  ok = 1;

done:
  offline_session_close(&session);
  offline_session_close(&session);
  if (!require(!session.client && !client.used && !client.buffer
               && !client.send_buffer && !client.local_write
               && !session.requests.data && !session.responses.data
               && game.year == 1234 && game.nplayers == 0
               && map.tiles == client_tiles,
               "ENGINE: clean idempotent teardown; client state preserved")) {
    ok = 0;
  }
  if (ok) {
    if (!require(offline_session_open(&session, &client, "DOSRuntime") == 0
                 && fc_offline_engine_snapshot(&snapshot) == 0
                 && snapshot.players == 0 && snapshot.last_request == 0
                 && !snapshot.established && !snapshot.map_allocated,
                 "ENGINE: reopen confirms clean authoritative reset")) {
      ok = 0;
    }
    offline_session_close(&session);
  }
  game_free();
  report("INFO ENGINE no rulesets/assets loaded; gameplay/map/AI untested");
  return ok;
}

int main(int argc, char **argv)
{
  const char *log_path = NULL, *fixture = "DATA\\RUNTIME.DAT";
  char path[MAXPATH];
  int batch = 0, i, ok;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--batch") == 0) {
      batch = 1;
    } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
      log_path = argv[++i];
    } else if (strcmp(argv[i], "--fixture") == 0 && i + 1 < argc) {
      fixture = argv[++i];
    } else {
      fprintf(stderr, "Usage: RTCHECK --log PATH [--fixture PATH] [--batch]\n");
      return EXIT_FAILURE;
    }
  }
  if (!log_path || !*log_path) {
    fputs("FAIL LOG: explicit --log PATH is required\n", stderr);
    return EXIT_FAILURE;
  }
  /* Append preserves prior boot evidence instead of silently erasing it. */
  runtime_log = fopen(log_path, "a");
  if (!runtime_log) {
    fprintf(stderr, "FAIL LOG: cannot open %s: %s\n", log_path, strerror(errno));
    return EXIT_FAILURE;
  }
  report("INIT RTCHECK separate DOS runtime diagnostic started");
  report("INFO GUI production fatal readiness gate unchanged");
  report("INFO UNTESTED: production GUI, graphics/VBE/video mapping, devices");
  log_init(NULL, LOG_ERROR, NULL);
  ok = check_dos_dpmi() && check_memory()
       && fixture_path(argv[0], fixture, path) && check_fixture(path)
       && check_engine();
  if (ok) {
    report("INIT SUCCESS: diagnostic DOS/DPMI/memory/data/local-engine only");
    report("READY RTCHECK (not production GUI readiness)");
    if (!batch) {
      report("WAIT press Q to quit; diagnostic remains running");
      for (;;) {
        if (kbhit()) {
          int key = getch();
          if (key == 'q' || key == 'Q') {
            break;
          }
        }
        usleep(20000);
      }
      report("PASS QUIT: Q received; test resources already released");
    } else {
      report("PASS QUIT: --batch skips keyboard wait; resources released");
    }
  } else {
    report("FAIL RTCHECK initialization; no READY and no keyboard wait");
  }
  report("EXIT %s", ok ? "SUCCESS" : "FAILURE");
  if (fclose(runtime_log) != 0) {
    fputs("FAIL LOG: close failed\n", stderr);
    return EXIT_FAILURE;
  }
  runtime_log = NULL;
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
