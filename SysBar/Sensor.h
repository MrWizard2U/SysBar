#pragma once

#include "State.h"
#include <map>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// HTTP
// ─────────────────────────────────────────────────────────────────────────────
std::string FetchJsonRaw(int port);

// ─────────────────────────────────────────────────────────────────────────────
// Per-sensor metadata collected during the DFS pass
// ─────────────────────────────────────────────────────────────────────────────
struct SensorMeta
{
    std::string sensorId;
    std::string type;    // "Load", "Temperature", "Clock", "Power", "Throughput", "Data"
    std::string text;    // human-readable label from LHM
    double      value;
};

// ─────────────────────────────────────────────────────────────────────────────
// Hardware node collected during the DFS pass
// ─────────────────────────────────────────────────────────────────────────────
struct HardwareNode
{
    std::string              hwId;
    std::string              name;
    std::string              hwType; // "cpu","gpu","ram","nic","disk","dimm"
    std::vector<SensorMeta>  sensors;
};

// ─────────────────────────────────────────────────────────────────────────────
// Full parse result
// ─────────────────────────────────────────────────────────────────────────────
struct ParseResult
{
    std::map<std::string, double> sensorValues; // sensorId -> numeric value
    std::vector<HardwareNode>     hardware;
    std::vector<DeviceInfo>       gpus;
    std::vector<DeviceInfo>       disks;
    std::vector<DeviceInfo>       nics;
};

// ─────────────────────────────────────────────────────────────────────────────
// JSON parse — single DFS pass
// ─────────────────────────────────────────────────────────────────────────────
ParseResult ParseLhmJson(const std::string& rawJson);

// ─────────────────────────────────────────────────────────────────────────────
// Sensor discovery — populates SensorMap by matching Type+Text labels
// ─────────────────────────────────────────────────────────────────────────────
SensorMap DiscoverSensors(const ParseResult& pr, const Config& cfg);

// ─────────────────────────────────────────────────────────────────────────────
// Value extraction — uses s->sensorMap to read from pr.sensorValues
// ─────────────────────────────────────────────────────────────────────────────
void ExtractValues(State* s, const ParseResult& pr);

// ─────────────────────────────────────────────────────────────────────────────
// Formatting
// sensorPresent = s->sensorFound[id]; false -> "N/A"
// ─────────────────────────────────────────────────────────────────────────────
std::wstring FormatValue(ParamId id, double v, bool sensorPresent);

// ─────────────────────────────────────────────────────────────────────────────
// Sensor thread
// ─────────────────────────────────────────────────────────────────────────────
void SensorThreadProc(State* s, HANDLE shutdownEvent);
