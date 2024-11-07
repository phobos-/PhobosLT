#include "config.h"

#include <EEPROM.h>

#include "debug.h"

void Config::init(void) {
    if (sizeof(laptimer_config_t) > EEPROM_RESERVED_SIZE) {
        DEBUG("Config size too big, adjust reserved EEPROM size\n");
        return;
    }

    EEPROM.begin(EEPROM_RESERVED_SIZE);  // Size of EEPROM
    load();                              // Override default settings from EEPROM

    checkTimeMs = millis();

    DEBUG("EEPROM Init Successful\n");
}

void Config::load(void) {
    modified = false;
    EEPROM.get(0, conf);

    uint32_t version = 0xFFFFFFFF;
    if ((conf.version & CONFIG_MAGIC_MASK) == CONFIG_MAGIC) {
        version = conf.version & ~CONFIG_MAGIC_MASK;
    }

    // If version is not current, reset to defaults
    if (version != CONFIG_VERSION) {
        setDefaults();
    }
}

void Config::write(void) {
    if (!modified) return;

    DEBUG("Writing to EEPROM\n");

    EEPROM.put(0, conf);
    EEPROM.commit();

    DEBUG("Writing to EEPROM done\n");

    modified = false;
}

void Config::toJson(AsyncResponseStream& destination) {
    // Use https://arduinojson.org/v6/assistant to estimate memory
    DynamicJsonDocument config(256);
    config["freq"] = conf.frequency;
    config["minLap"] = conf.minLap;
    config["alarm"] = conf.alarm;
    config["anType"] = conf.announcerType;
    config["anRate"] = conf.announcerRate;
    config["enterRssi"] = conf.enterRssi;
    config["exitRssi"] = conf.exitRssi;
    config["name"] = conf.pilotName;
    config["ssid"] = conf.ssid;
    config["pwd"] = conf.password;
    serializeJson(config, destination);
}

void Config::toJsonString(char* buf) {
    DynamicJsonDocument config(256);
    config["freq"] = conf.frequency;
    config["minLap"] = conf.minLap;
    config["alarm"] = conf.alarm;
    config["anType"] = conf.announcerType;
    config["anRate"] = conf.announcerRate;
    config["enterRssi"] = conf.enterRssi;
    config["exitRssi"] = conf.exitRssi;
    config["name"] = conf.pilotName;
    config["ssid"] = conf.ssid;
    config["pwd"] = conf.password;
    serializeJsonPretty(config, buf, 256);
}

void Config::fromJson(JsonObject source) {
    if (source["freq"] != conf.frequency) {
        conf.frequency = source["freq"];
        modified = true;
    }
    if (source["minLap"] != conf.minLap) {
        conf.minLap = source["minLap"];
        modified = true;
    }
    if (source["alarm"] != conf.alarm) {
        conf.alarm = source["alarm"];
        modified = true;
    }
    if (source["anType"] != conf.announcerType) {
        conf.announcerType = source["anType"];
        modified = true;
    }
    if (source["anRate"] != conf.announcerRate) {
        conf.announcerRate = source["anRate"];
        modified = true;
    }
    if (source["enterRssi"] != conf.enterRssi) {
        conf.enterRssi = source["enterRssi"];
        modified = true;
    }
    if (source["exitRssi"] != conf.exitRssi) {
        conf.exitRssi = source["exitRssi"];
        modified = true;
    }
    if (source["name"] != conf.pilotName) {
        strlcpy(conf.pilotName, source["name"] | "", sizeof(conf.pilotName));
        modified = true;
    }
    if (source["ssid"] != conf.ssid) {
        strlcpy(conf.ssid, source["ssid"] | "", sizeof(conf.ssid));
        modified = true;
    }
    if (source["pwd"] != conf.password) {
        strlcpy(conf.password, source["pwd"] | "", sizeof(conf.password));
        modified = true;
    }
}

uint16_t Config::getFrequency() {
    return conf.frequency;
}

uint32_t Config::getMinLapMs() {
    return conf.minLap * 100;
}

uint8_t Config::getAlarmThreshold() {
    return conf.alarm;
}

uint8_t Config::getEnterRssi() {
    return conf.enterRssi;
}

uint8_t Config::getExitRssi() {
    return conf.exitRssi;
}

char* Config::getSsid() {
    return conf.ssid;
}

char* Config::getPassword() {
    return conf.password;
}

char* Config::getPilotName() {
    return conf.pilotName;
}

const char* Config::getFrequencyName() {
    switch(conf.frequency) {
        case 5658: return "Race 1";
        case 5695: return "Race 2";
        case 5732: return "Race 3";
        case 5769: return "Race 4";
        case 5806: return "Race 5";
        case 5843: return "Race 6";
        case 5880: return "Race 7";
        case 5917: return "Race 8";
        case 5865: return "Boscam A 1";
        case 5845: return "Boscam A 2";
        case 5825: return "Boscam A 3";
        case 5805: return "Boscam A 4";
        case 5785: return "Boscam A 5";
        case 5765: return "Boscam A 6";
        case 5745: return "Boscam A 7";
        case 5725: return "Boscam A 8";
        case 5733: return "Boscam B 1";
        case 5752: return "Boscam B 2";
        case 5771: return "Boscam B 3";
        case 5790: return "Boscam B 4";
        case 5809: return "Boscam B 5";
        case 5828: return "Boscam B 6";
        case 5847: return "Boscam B 7";
        case 5866: return "Boscam B 8";
        case 5705: return "Boscam C 1";
        case 5685: return "Boscam C 2";
        case 5665: return "Boscam C 3";
        case 5645: return "Boscam C 4";
        case 5885: return "Boscam C 5";
        case 5905: return "Boscam C 6";
        case 5925: return "Boscam C 7";
        case 5945: return "Boscam C 8";
        case 5740: return "Fatshark 1";
        case 5760: return "Fatshark 2";
        case 5780: return "Fatshark 3";
        case 5800: return "Fatshark 4";
        case 5820: return "Fatshark 5";
        case 5840: return "Fatshark 6";
        case 5860: return "Fatshark 7";
        case 5333: return "LowBand 1";
        case 5373: return "LowBand 2";
        case 5413: return "LowBand 3";
        case 5453: return "LowBand 4";
        case 5493: return "LowBand 5";
        case 5533: return "LowBand 6";
        case 5573: return "LowBand 7";
        case 5613: return "LowBand 8";
        default: return "Unknown";
    }
}

void Config::setDefaults(void) {
    DEBUG("Setting EEPROM defaults\n");
    // Reset everything to 0/false and then just set anything that zero is not appropriate
    memset(&conf, 0, sizeof(conf));
    conf.version = CONFIG_VERSION | CONFIG_MAGIC;
    conf.frequency = 1111;
    conf.minLap = 100;
    conf.alarm = 36;
    conf.announcerType = 2;
    conf.announcerRate = 10;
    conf.enterRssi = 120;
    conf.exitRssi = 100;
    strlcpy(conf.ssid, "", sizeof(conf.ssid));
    strlcpy(conf.password, "", sizeof(conf.password));
    strlcpy(conf.pilotName, "", sizeof(conf.pilotName));
    modified = true;
    write();
}

void Config::handleEeprom(uint32_t currentTimeMs) {
    if (modified && ((currentTimeMs - checkTimeMs) > EEPROM_CHECK_TIME_MS)) {
        checkTimeMs = currentTimeMs;
        write();
    }
}
