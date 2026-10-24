/* Auto-generated from a DBC file. Do not edit by hand. */
#include "project_dbc.h"

static int64_t project_dbc_round(double value)
{
  return (int64_t)((value >= 0.0) ? (value + 0.5) : (value - 0.5));
}

void DiagnosticRequest_ECU1_pack(const DiagnosticRequest_ECU1_t *message, uint8_t data[8])
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;

  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte0) + (0.0)) / (1.0)))) & 0xFFULL) << 0U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte1) + (0.0)) / (1.0)))) & 0xFFULL) << 8U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte2) + (0.0)) / (1.0)))) & 0xFFULL) << 16U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte3) + (0.0)) / (1.0)))) & 0xFFULL) << 24U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte4) + (0.0)) / (1.0)))) & 0xFFULL) << 32U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte5) + (0.0)) / (1.0)))) & 0xFFULL) << 40U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte6) + (0.0)) / (1.0)))) & 0xFFULL) << 48U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte7) + (0.0)) / (1.0)))) & 0xFFULL) << 56U;

  for (uint32_t index = 0U; index < 8U; index++)
  {
    const uint8_t be_byte =
        (uint8_t)((be_word >> (8U * (7U - index))) & 0xFFU);
    const uint8_t le_byte =
        (uint8_t)((le_word >> (8U * index)) & 0xFFU);
    data[index] = (uint8_t)(be_byte | le_byte);
  }
}

void DiagnosticRequest_ECU1_unpack(const uint8_t data[8], DiagnosticRequest_ECU1_t *message)
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;
  for (uint32_t index = 0U; index < 8U; index++)
  {
    be_word = (be_word << 8U) | data[index];
    le_word |= ((uint64_t)data[index]) << (8U * index);
  }

  {
    uint64_t raw = (le_word >> 0U) & 0xFFULL;
    message->PciByte0 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 8U) & 0xFFULL;
    message->PciByte1 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 16U) & 0xFFULL;
    message->PciByte2 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 24U) & 0xFFULL;
    message->PciByte3 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 32U) & 0xFFULL;
    message->PciByte4 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 40U) & 0xFFULL;
    message->PciByte5 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 48U) & 0xFFULL;
    message->PciByte6 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 56U) & 0xFFULL;
    message->PciByte7 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
}

void DiagnosticResponse_ECU1_pack(const DiagnosticResponse_ECU1_t *message, uint8_t data[8])
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;

  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte0) + (0.0)) / (1.0)))) & 0xFFULL) << 0U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte1) + (0.0)) / (1.0)))) & 0xFFULL) << 8U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte2) + (0.0)) / (1.0)))) & 0xFFULL) << 16U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte3) + (0.0)) / (1.0)))) & 0xFFULL) << 24U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte4) + (0.0)) / (1.0)))) & 0xFFULL) << 32U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte5) + (0.0)) / (1.0)))) & 0xFFULL) << 40U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte6) + (0.0)) / (1.0)))) & 0xFFULL) << 48U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte7) + (0.0)) / (1.0)))) & 0xFFULL) << 56U;

  for (uint32_t index = 0U; index < 8U; index++)
  {
    const uint8_t be_byte =
        (uint8_t)((be_word >> (8U * (7U - index))) & 0xFFU);
    const uint8_t le_byte =
        (uint8_t)((le_word >> (8U * index)) & 0xFFU);
    data[index] = (uint8_t)(be_byte | le_byte);
  }
}

void DiagnosticResponse_ECU1_unpack(const uint8_t data[8], DiagnosticResponse_ECU1_t *message)
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;
  for (uint32_t index = 0U; index < 8U; index++)
  {
    be_word = (be_word << 8U) | data[index];
    le_word |= ((uint64_t)data[index]) << (8U * index);
  }

  {
    uint64_t raw = (le_word >> 0U) & 0xFFULL;
    message->PciByte0 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 8U) & 0xFFULL;
    message->PciByte1 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 16U) & 0xFFULL;
    message->PciByte2 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 24U) & 0xFFULL;
    message->PciByte3 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 32U) & 0xFFULL;
    message->PciByte4 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 40U) & 0xFFULL;
    message->PciByte5 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 48U) & 0xFFULL;
    message->PciByte6 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 56U) & 0xFFULL;
    message->PciByte7 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
}

void DiagnosticRequest_ECU2_pack(const DiagnosticRequest_ECU2_t *message, uint8_t data[8])
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;

  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte0) + (0.0)) / (1.0)))) & 0xFFULL) << 0U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte1) + (0.0)) / (1.0)))) & 0xFFULL) << 8U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte2) + (0.0)) / (1.0)))) & 0xFFULL) << 16U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte3) + (0.0)) / (1.0)))) & 0xFFULL) << 24U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte4) + (0.0)) / (1.0)))) & 0xFFULL) << 32U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte5) + (0.0)) / (1.0)))) & 0xFFULL) << 40U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte6) + (0.0)) / (1.0)))) & 0xFFULL) << 48U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte7) + (0.0)) / (1.0)))) & 0xFFULL) << 56U;

  for (uint32_t index = 0U; index < 8U; index++)
  {
    const uint8_t be_byte =
        (uint8_t)((be_word >> (8U * (7U - index))) & 0xFFU);
    const uint8_t le_byte =
        (uint8_t)((le_word >> (8U * index)) & 0xFFU);
    data[index] = (uint8_t)(be_byte | le_byte);
  }
}

void DiagnosticRequest_ECU2_unpack(const uint8_t data[8], DiagnosticRequest_ECU2_t *message)
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;
  for (uint32_t index = 0U; index < 8U; index++)
  {
    be_word = (be_word << 8U) | data[index];
    le_word |= ((uint64_t)data[index]) << (8U * index);
  }

  {
    uint64_t raw = (le_word >> 0U) & 0xFFULL;
    message->PciByte0 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 8U) & 0xFFULL;
    message->PciByte1 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 16U) & 0xFFULL;
    message->PciByte2 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 24U) & 0xFFULL;
    message->PciByte3 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 32U) & 0xFFULL;
    message->PciByte4 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 40U) & 0xFFULL;
    message->PciByte5 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 48U) & 0xFFULL;
    message->PciByte6 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 56U) & 0xFFULL;
    message->PciByte7 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
}

void DiagnosticResponse_ECU2_pack(const DiagnosticResponse_ECU2_t *message, uint8_t data[8])
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;

  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte0) + (0.0)) / (1.0)))) & 0xFFULL) << 0U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte1) + (0.0)) / (1.0)))) & 0xFFULL) << 8U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte2) + (0.0)) / (1.0)))) & 0xFFULL) << 16U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte3) + (0.0)) / (1.0)))) & 0xFFULL) << 24U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte4) + (0.0)) / (1.0)))) & 0xFFULL) << 32U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte5) + (0.0)) / (1.0)))) & 0xFFULL) << 40U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte6) + (0.0)) / (1.0)))) & 0xFFULL) << 48U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->PciByte7) + (0.0)) / (1.0)))) & 0xFFULL) << 56U;

  for (uint32_t index = 0U; index < 8U; index++)
  {
    const uint8_t be_byte =
        (uint8_t)((be_word >> (8U * (7U - index))) & 0xFFU);
    const uint8_t le_byte =
        (uint8_t)((le_word >> (8U * index)) & 0xFFU);
    data[index] = (uint8_t)(be_byte | le_byte);
  }
}

void DiagnosticResponse_ECU2_unpack(const uint8_t data[8], DiagnosticResponse_ECU2_t *message)
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;
  for (uint32_t index = 0U; index < 8U; index++)
  {
    be_word = (be_word << 8U) | data[index];
    le_word |= ((uint64_t)data[index]) << (8U * index);
  }

  {
    uint64_t raw = (le_word >> 0U) & 0xFFULL;
    message->PciByte0 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 8U) & 0xFFULL;
    message->PciByte1 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 16U) & 0xFFULL;
    message->PciByte2 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 24U) & 0xFFULL;
    message->PciByte3 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 32U) & 0xFFULL;
    message->PciByte4 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 40U) & 0xFFULL;
    message->PciByte5 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 48U) & 0xFFULL;
    message->PciByte6 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 56U) & 0xFFULL;
    message->PciByte7 = (uint8_t)((double)raw * 1.0 + 0.0);
  }
}

void EngineData_pack(const EngineData_t *message, uint8_t data[8])
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;

  be_word |= (((uint64_t)(project_dbc_round((((double)message->EngineSpeed) + (0.0)) / (0.25)))) & 0xFFFFULL) << 48U;
  be_word |= (((uint64_t)(project_dbc_round((((double)message->CoolantTemp) + (40.0)) / (1.0)))) & 0xFFULL) << 40U;

  for (uint32_t index = 0U; index < 8U; index++)
  {
    const uint8_t be_byte =
        (uint8_t)((be_word >> (8U * (7U - index))) & 0xFFU);
    const uint8_t le_byte =
        (uint8_t)((le_word >> (8U * index)) & 0xFFU);
    data[index] = (uint8_t)(be_byte | le_byte);
  }
}

void EngineData_unpack(const uint8_t data[8], EngineData_t *message)
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;
  for (uint32_t index = 0U; index < 8U; index++)
  {
    be_word = (be_word << 8U) | data[index];
    le_word |= ((uint64_t)data[index]) << (8U * index);
  }

  {
    uint64_t raw = (be_word >> 48U) & 0xFFFFULL;
    message->EngineSpeed = (double)((double)raw * 0.25 + 0.0);
  }
  {
    uint64_t raw = (be_word >> 40U) & 0xFFULL;
    message->CoolantTemp = (double)((double)raw * 1.0 + -40.0);
  }
}

void VehicleStatus_pack(const VehicleStatus_t *message, uint8_t data[8])
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;

  le_word |= (((uint64_t)(project_dbc_round((((double)message->VehicleSpeed) + (0.0)) / (0.01)))) & 0xFFFFULL) << 0U;
  le_word |= (((uint64_t)(project_dbc_round((((double)message->BrakeActive) + (0.0)) / (1.0)))) & 0x1ULL) << 16U;

  for (uint32_t index = 0U; index < 8U; index++)
  {
    const uint8_t be_byte =
        (uint8_t)((be_word >> (8U * (7U - index))) & 0xFFU);
    const uint8_t le_byte =
        (uint8_t)((le_word >> (8U * index)) & 0xFFU);
    data[index] = (uint8_t)(be_byte | le_byte);
  }
}

void VehicleStatus_unpack(const uint8_t data[8], VehicleStatus_t *message)
{
  uint64_t be_word = 0U;
  uint64_t le_word = 0U;
  for (uint32_t index = 0U; index < 8U; index++)
  {
    be_word = (be_word << 8U) | data[index];
    le_word |= ((uint64_t)data[index]) << (8U * index);
  }

  {
    uint64_t raw = (le_word >> 0U) & 0xFFFFULL;
    message->VehicleSpeed = (double)((double)raw * 0.01 + 0.0);
  }
  {
    uint64_t raw = (le_word >> 16U) & 0x1ULL;
    message->BrakeActive = (uint8_t)((double)raw * 1.0 + 0.0);
  }
}
