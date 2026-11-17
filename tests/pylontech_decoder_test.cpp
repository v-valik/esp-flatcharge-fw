// Host-side C++17 test: g++ -std=c++17 -Iinclude tests/pylontech_decoder_test.cpp -o /tmp/test_pylontech
#include "PylontechBms.h"
#include <cassert>
#include <cstdio>

int main() {
    PylontechBms bms;
    // 56.0 V; charge limit 50.0 A; discharge limit 100.0 A
    const uint8_t limits[] = {0x30,0x02,0xF4,0x01,0xE8,0x03};
    const uint8_t soc[] = {50,0,99,0};
    // 51.20 V; 0.0 A; 25.0 C
    const uint8_t telemetry[] = {0x00,0x14,0,0,0xFA,0};
    const uint8_t alarm[] = {0,0,0,0,0,0,0,0};
    const uint8_t allow[] = {0xC0};
    const uint8_t deny[] = {0x40};
    const uint8_t fault[] = {1,0,0,0};

    assert(!bms.canCharge(100));
    assert(bms.ingest(0x351,false,false,limits,6,100));
    assert(bms.ingest(0x355,false,false,soc,4,100));
    assert(bms.ingest(0x356,false,false,telemetry,6,100));
    assert(bms.ingest(0x359,false,false,alarm,8,100));
    assert(!bms.canCharge(100));  // missing 0x35C
    assert(bms.ingest(0x35C,false,false,allow,1,100));
    assert(bms.valid(100));
    assert(bms.canCharge(100));
    assert(bms.data().soc == 50);
    assert(bms.data().chargeVoltageV > 55.9f && bms.data().chargeVoltageV < 56.1f);
    assert(bms.data().chargeLimitA > 49.9f && bms.data().chargeLimitA < 50.1f);
    assert(bms.data().packVoltageV > 51.1f && bms.data().packVoltageV < 51.3f);
    assert(!bms.canCharge(4000)); // stale
    assert(bms.ingest(0x35C,false,false,deny,1,4100));
    assert(!bms.canCharge(4100)); // no enable
    assert(bms.ingest(0x35C,false,false,allow,1,4100));
    assert(bms.ingest(0x351,false,false,limits,6,4100));
    assert(bms.ingest(0x355,false,false,soc,4,4100));
    assert(bms.ingest(0x356,false,false,telemetry,6,4100));
    assert(bms.ingest(0x359,false,false,fault,4,4100));
    assert(!bms.canCharge(4100)); // alarm
    assert(bms.ingest(0x359,false,false,alarm,8,4100));
    assert(bms.canCharge(4100));
    // Malformed / incompatible frames MUST NOT refresh validity.
    assert(!bms.ingest(0x351,true,false,limits,6,8000));
    assert(!bms.ingest(0x351,false,true,limits,6,8000));
    assert(!bms.ingest(0x351,false,false,limits,3,8000));
    assert(!bms.canCharge(8000));
    std::puts("Pylontech parser tests passed");
}
