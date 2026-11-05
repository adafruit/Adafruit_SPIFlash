// SPDX-License-Identifier: MIT
//
// HIL restoration sketch (.claude/skills/hil-contract/SKILL.md): the board's
// resting firmware between runs. It has no erase or write command, so leaving
// it running keeps the region's data (flash.begin() may still set a QSPI chip's
// quad-enable status bit). One command per line on Serial (the DUT's USB CDC),
// one reply line each, carrying the command's nonce. Anything else, or a
// malformed, misaligned, empty or out-of-window region, is answered REJECT
// why=<reason>.
//
// clang-format off
//   INFO <nonce>               HIL_INFO <nonce> ok=<0|1> transport=<t>
//                                jedec=<hex6> size=<window> sector=4096
//   CHECK <nonce> <off> <len>  HIL_RESULT <nonce> check PASS|FAIL off= len=
//                                crc=<crc32> erased=<0|1>
// clang-format on

#include <Adafruit_SPIFlash.h>
#include <SPI.h>

#if defined(ARDUINO_ARCH_RP2040)
Adafruit_FlashTransport_RP2040 flashTransport;
#define TRANSPORT "rp2040"
#elif defined(EXTERNAL_FLASH_USE_QSPI)
Adafruit_FlashTransport_QSPI flashTransport;
#define TRANSPORT "qspi"
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
  if (strcmp(cmd, "CHECK")) {
    Serial.printf("HIL_RESULT %s ? REJECT why=read-only\r\n", nonce);
    return;
  }
  uint32_t off, len;
  char const *why = NULL;
  if (extra || !parse_u32(a, &off) || !parse_u32(b, &len)) {
    why = "malformed";
  } else if (!flash_ok) {
    why = "flash-not-begun";
  } else if (len == 0) {
    why = "empty";
  } else if (off % SECTOR || len % SECTOR) {
    why = "misaligned";
  } else if (off >= flash.size() || len > flash.size() - off) {
    why = "outside-window";
  }
  if (why) {
    Serial.printf("HIL_RESULT %s check REJECT why=%s\r\n", nonce, why);
    return;
  }
  uint32_t crc = 0;
  bool erased = true, read_ok = true;
  for (uint32_t done = 0; done < len && read_ok; done += SECTOR) {
    read_ok = flash.readBuffer(off + done, buf, SECTOR) == SECTOR;
    crc = crc32_update(crc, buf, SECTOR);
    for (uint32_t i = 0; i < SECTOR; i++) {
      erased = erased && buf[i] == 0xFF;
    }
  }
  Serial.printf(
      "HIL_RESULT %s check %s off=%lu len=%lu crc=%08lx erased=%d%s\r\n", nonce,
      read_ok ? "PASS" : "FAIL", (unsigned long)off, (unsigned long)len,
      (unsigned long)crc, erased, read_ok ? "" : " step=read");
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
