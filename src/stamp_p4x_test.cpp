#include <M5Unified.h>
#include <esp_chip_info.h>

namespace {

constexpr int kExpectedRevision = 300;
constexpr int kExpectedBoardId = 156;

void printResult()
{
  esp_chip_info_t chip_info = {};
  esp_chip_info(&chip_info);

  const int board_id = static_cast<int>(M5.getBoard());
  const bool revision_ok = chip_info.revision >= kExpectedRevision;
  const bool board_ok = board_id == kExpectedBoardId;

  Serial.println();
  Serial.println("StampP4X detection test");
  Serial.printf("Chip revision: %d (expected >= %d)\n", chip_info.revision, kExpectedRevision);
  Serial.printf("M5 board ID: %d (expected %d)\n", board_id, kExpectedBoardId);
  Serial.printf("RESULT: %s\n", revision_ok && board_ok ? "PASS" : "FAIL");
}

}  // namespace

void setup()
{
  Serial.begin(115200);
  delay(1500);

  auto cfg = M5.config();
  M5.begin(cfg);
  printResult();
}

void loop()
{
  delay(5000);
  printResult();
}
