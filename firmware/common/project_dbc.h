/* Auto-generated from a DBC file. Do not edit by hand. */
#ifndef PROJECT_DBC_H
#define PROJECT_DBC_H

#include <stdint.h>

typedef struct
{
  uint8_t PciByte0;
  uint8_t PciByte1;
  uint8_t PciByte2;
  uint8_t PciByte3;
  uint8_t PciByte4;
  uint8_t PciByte5;
  uint8_t PciByte6;
  uint8_t PciByte7;
} DiagnosticRequest_ECU1_t;

void DiagnosticRequest_ECU1_pack(const DiagnosticRequest_ECU1_t *message, uint8_t data[8]);
void DiagnosticRequest_ECU1_unpack(const uint8_t data[8], DiagnosticRequest_ECU1_t *message);

typedef struct
{
  uint8_t PciByte0;
  uint8_t PciByte1;
  uint8_t PciByte2;
  uint8_t PciByte3;
  uint8_t PciByte4;
  uint8_t PciByte5;
  uint8_t PciByte6;
  uint8_t PciByte7;
} DiagnosticResponse_ECU1_t;

void DiagnosticResponse_ECU1_pack(const DiagnosticResponse_ECU1_t *message, uint8_t data[8]);
void DiagnosticResponse_ECU1_unpack(const uint8_t data[8], DiagnosticResponse_ECU1_t *message);

typedef struct
{
  uint8_t PciByte0;
  uint8_t PciByte1;
  uint8_t PciByte2;
  uint8_t PciByte3;
  uint8_t PciByte4;
  uint8_t PciByte5;
  uint8_t PciByte6;
  uint8_t PciByte7;
} DiagnosticRequest_ECU2_t;

void DiagnosticRequest_ECU2_pack(const DiagnosticRequest_ECU2_t *message, uint8_t data[8]);
void DiagnosticRequest_ECU2_unpack(const uint8_t data[8], DiagnosticRequest_ECU2_t *message);

typedef struct
{
  uint8_t PciByte0;
  uint8_t PciByte1;
  uint8_t PciByte2;
  uint8_t PciByte3;
  uint8_t PciByte4;
  uint8_t PciByte5;
  uint8_t PciByte6;
  uint8_t PciByte7;
} DiagnosticResponse_ECU2_t;

void DiagnosticResponse_ECU2_pack(const DiagnosticResponse_ECU2_t *message, uint8_t data[8]);
void DiagnosticResponse_ECU2_unpack(const uint8_t data[8], DiagnosticResponse_ECU2_t *message);

typedef struct
{
  double EngineSpeed;
  double CoolantTemp;
} EngineData_t;

void EngineData_pack(const EngineData_t *message, uint8_t data[8]);
void EngineData_unpack(const uint8_t data[8], EngineData_t *message);

typedef struct
{
  double VehicleSpeed;
  uint8_t BrakeActive;
} VehicleStatus_t;

void VehicleStatus_pack(const VehicleStatus_t *message, uint8_t data[8]);
void VehicleStatus_unpack(const uint8_t data[8], VehicleStatus_t *message);

#endif /* PROJECT_DBC_H */
