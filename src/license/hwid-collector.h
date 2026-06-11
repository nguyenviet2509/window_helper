#pragma once
// HWID Collector — generates a stable machine fingerprint from:
//   VolumeSerial(C:) + cpuid(EAX=1) EAX+EBX + MachineGuid registry + primary MAC
// SHA-256 via Windows CNG (bcrypt). Result cached after first call (mutex-protected).
// DO NOT log HwidFull() output to log files.

#include <string>

namespace License {

// Returns 64-hex SHA-256 fingerprint. Cached after first call.
std::string HwidFull();

// Returns first 8 hex chars of HwidFull().
std::string HwidShort();

} // namespace License
