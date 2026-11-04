// SPDX-License-Identifier: MIT
//
// HIL test sketch (.claude/skills/hil-contract/SKILL.md): erases, writes and
// verifies only the region the host names, after checking it lies inside the
// transport's window. One command per line on Serial (the DUT's USB CDC), one
// reply line each, carrying the command's nonce. A malformed, misaligned, empty
// or out-of-window request is answered REJECT why=<reason> before any flash
// operation.
//
// clang-format off
//   INFO <nonce>               HIL_INFO <nonce> ok=<0|1> transport=<t>
//                                jedec=<hex6> size=<window> sector=4096
//   CHECK <nonce> <off> <len>  HIL_RESULT <nonce> check PASS|FAIL off= len=
//                                crc=<crc32> erased=<0|1>
//   RUN <nonce> <off> <len>    erase, verify erased, write the nonce's pattern,
//                                read back: HIL_RESULT <nonce> run ... [step=]
//   ERASE <nonce> <off> <len>  erase, verify erased: HIL_RESULT <nonce> erase ...
// clang-format on

#include <Adafruit_SPIFlash.h>
#include <SPI.h>

#if defined(ARDUINO_ARCH_RP2040)
// the core's FS region (Tools > Flash Size): the window never holds the sketch
Adafruit_FlashTransport_RP2040 flashTransport;
#define TRANSPORT "rp2040"
// flash_range_program takes whole pages only (pico-sdk hardware_flash)
#define UNALIGNED_SPLIT 0
#elif defined(EXTERNAL_FLASH_USE_QSPI)
Adafruit_FlashTransport_QSPI flashTransport;
#define TRANSPORT "qspi"
// the first sector's write is split here: an unaligned, page-crossing write
#define UNALIGNED_SPLIT 300
#else
#error the HIL sketches build only for the rig boards (RP2040 internal flash, SAMD51 QSPI)
#endif

#define SECTOR 4096u

Adafruit_SPIFlash flash(&flashTransport);
bool flash_ok;
uint8_t buf[SECTOR];

static uint32_t crc32_update(uint32_t crc, uint8_t const *data, uint32_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (int k = 0; k < 8; k++) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

// byte i of the region's pattern: seeded by the nonce, so a stale image or
// another run's data cannot match
static uint8_t pattern(uint32_t seed, uint32_t i) {
  return (uint8_t)((seed >> ((i & 3) * 8)) ^ (i * 7u) ^ (i >> 8));
}

static uint32_t seed_of(char const *nonce) {
  return crc32_update(0, (uint8_t const *)nonce, strlen(nonce));
}

static bool parse_u32(char const *s, uint32_t *out) {
  if (!s || !*s) {
    return false;
  }
  char *end;
  unsigned long v = strtoul(s, &end, 10);
  if (*end || s[0] == '-' || v > 0xFFFFFFFFul) {
    return false;
  }
  *out = (uint32_t)v;
  return true;
}

// why the region is refused, or NULL: checked with subtraction so off + len
// cannot wrap
static char const *region_problem(char const *off_s, char const *len_s,
                                  uint32_t *off, uint32_t *len) {
  if (!parse_u32(off_s, off) || !parse_u32(len_s, len)) {
    return "malformed";
  }
  if (!flash_ok) {
    return "flash-not-begun";
  }
  uint32_t size = flash.size();
  if (*len == 0) {
    return "empty";
  }
  if (*off % SECTOR || *len % SECTOR) {
    return "misaligned";
  }
  if (*off >= size || *len > size - *off) {
    return "outside-window";
  }
  return NULL;
}

// crc32 of the region read back, and whether every byte is 0xFF; false on a
// short read
static bool scan(uint32_t off, uint32_t len, uint32_t *crc, bool *erased) {
  *crc = 0;
  *erased = true;
  for (uint32_t done = 0; done < len; done += SECTOR) {
    if (flash.readBuffer(off + done, buf, SECTOR) != SECTOR) {
      return false;
    }
    *crc = crc32_update(*crc, buf, SECTOR);
    for (uint32_t i = 0; i < SECTOR; i++) {
      *erased = *erased && buf[i] == 0xFF;
    }
  }
  return true;
}

static char const *erase_region(uint32_t off, uint32_t len) {
  for (uint32_t s = off / SECTOR; s < (off + len) / SECTOR; s++) {
    if (!flash.eraseSector(s)) {
      return "erase";
    }
  }
  uint32_t crc;
  bool erased;
  if (!scan(off, len, &crc, &erased)) {
    return "read-erased";
  }
  return erased ? NULL : "not-erased";
}

static char const *write_region(uint32_t off, uint32_t len, uint32_t seed) {
  for (uint32_t done = 0; done < len; done += SECTOR) {
    for (uint32_t i = 0; i < SECTOR; i++) {
      buf[i] = pattern(seed, done + i);
    }
    uint32_t split = (done == 0) ? UNALIGNED_SPLIT : 0;
    if (split && (flash.writeBuffer(off, buf, split) != split ||
                  flash.writeBuffer(off + split, buf + split, SECTOR - split) !=
                      SECTOR - split)) {
      return "write";
    }
    if (!split && flash.writeBuffer(off + done, buf, SECTOR) != SECTOR) {
      return "write";
    }
  }
  for (uint32_t done = 0; done < len; done += SECTOR) {
    if (flash.readBuffer(off + done, buf, SECTOR) != SECTOR) {
      return "read-back";
    }
    for (uint32_t i = 0; i < SECTOR; i++) {
      if (buf[i] != pattern(seed, done + i)) {
        return "compare";
      }
    }
  }
  return NULL;
}

static void handle(char *line) {
  char *cmd = strtok(line, " ");
  char *nonce = strtok(NULL, " ");
  char *a = strtok(NULL, " ");
  char *b = strtok(NULL, " ");
  char *extra = strtok(NULL, " ");
  if (!cmd || !nonce) {
    Serial.println("HIL_RESULT - ? REJECT why=malformed");
    return;
  }
  if (!strcmp(cmd, "INFO") && !a) {
    Serial.printf(
        "HIL_INFO %s ok=%d transport=%s jedec=%06lx size=%lu sector=%u\r\n",
        nonce, flash_ok, TRANSPORT,
        flash_ok ? (unsigned long)flash.getJEDECID() : 0ul,
        flash_ok ? (unsigned long)flash.size() : 0ul, SECTOR);
    return;
  }
  char const *op = !strcmp(cmd, "CHECK")   ? "check"
                   : !strcmp(cmd, "RUN")   ? "run"
                   : !strcmp(cmd, "ERASE") ? "erase"
                                           : NULL;
  if (!op) {
    Serial.printf("HIL_RESULT %s ? REJECT why=unknown-command\r\n", nonce);
    return;
  }
  uint32_t off, len;
  char const *why = extra ? "malformed" : region_problem(a, b, &off, &len);
  if (why) {
    Serial.printf("HIL_RESULT %s %s REJECT why=%s\r\n", nonce, op, why);
    return;
  }
  uint32_t crc = 0;
  bool erased = false;
  char const *step = NULL;
  if (!strcmp(op, "run")) {
    step = erase_region(off, len);
    step = step ? step : write_region(off, len, seed_of(nonce));
  } else if (!strcmp(op, "erase")) {
    step = erase_region(off, len);
  }
  if (!step && !scan(off, len, &crc, &erased)) {
    step = "read";
  }
  Serial.printf(
      "HIL_RESULT %s %s %s off=%lu len=%lu crc=%08lx erased=%d%s%s\r\n", nonce,
      op, step ? "FAIL" : "PASS", (unsigned long)off, (unsigned long)len,
      (unsigned long)crc, erased, step ? " step=" : "", step ? step : "");
}

void setup() {
  Serial.begin(115200);
  flash_ok = flash.begin();
}

void loop() {
  static char line[96];
  static size_t n;
  // discarding an overlong line through its newline: its tail is no command
  static bool overlong;
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      line[n] = 0;
      if (!overlong) {
        handle(line);
      }
      n = 0;
      overlong = false;
    } else if (n < sizeof(line) - 1) {
      line[n++] = (char)c;
    } else {
      overlong = true;
    }
  }
}
