#include <stdint.h>
#include <stdio.h>

#include "project_dbc.h"

static int failures = 0;

#define CHECK(condition)                                                          \
  do                                                                              \
  {                                                                               \
    if (!(condition))                                                             \
    {                                                                             \
      (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
      failures++;                                                                 \
    }                                                                             \
  } while (0)

static void expect_bytes(const uint8_t *actual,
                         const uint8_t *expected,
                         uint32_t length)
{
  uint32_t index;

  for (index = 0U; index < length; index++)
  {
    CHECK(actual[index] == expected[index]);
  }
}

static int nearly_equal(double left, double right)
{
  double difference = left - right;
  if (difference < 0.0)
  {
    difference = -difference;
  }
  return difference < 0.000001;
}

int main(void)
{
  uint8_t data[8] = {0};
  const uint8_t expected_engine[8] = {0x13U, 0x4AU, 0x7DU, 0U, 0U, 0U, 0U, 0U};
  const uint8_t expected_vehicle[8] = {0x9AU, 0x10U, 0x01U, 0U, 0U, 0U, 0U, 0U};
  const uint8_t expected_diagnostic[8] = {0x10U, 0x03U, 0U, 0U, 0U, 0U, 0U, 0U};

  EngineData_t engine = {1234.5, 85.0};
  EngineData_t decoded_engine = {0.0, 0.0};
  EngineData_pack(&engine, data);
  expect_bytes(data, expected_engine, 8U);
  EngineData_unpack(data, &decoded_engine);
  CHECK(nearly_equal(decoded_engine.EngineSpeed, 1234.5));
  CHECK(nearly_equal(decoded_engine.CoolantTemp, 85.0));

  VehicleStatus_t vehicle = {42.5, 1U};
  VehicleStatus_t decoded_vehicle = {0.0, 0U};
  VehicleStatus_pack(&vehicle, data);
  expect_bytes(data, expected_vehicle, 8U);
  VehicleStatus_unpack(data, &decoded_vehicle);
  CHECK(nearly_equal(decoded_vehicle.VehicleSpeed, 42.5));
  CHECK(decoded_vehicle.BrakeActive == 1U);

  DiagnosticRequest_ECU1_t request = {
      0x10U, 0x03U, 0U, 0U, 0U, 0U, 0U, 0U};
  DiagnosticRequest_ECU1_t decoded_request = {0};
  DiagnosticRequest_ECU1_pack(&request, data);
  expect_bytes(data, expected_diagnostic, 8U);
  DiagnosticRequest_ECU1_unpack(data, &decoded_request);
  CHECK(decoded_request.PciByte0 == 0x10U);
  CHECK(decoded_request.PciByte1 == 0x03U);
  CHECK(decoded_request.PciByte7 == 0U);

  if (failures != 0)
  {
    (void)fprintf(stderr, "project DBC host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("project DBC host tests: PASS\n");
  return 0;
}
