#pragma once

#include <Arduino.h>
#include "msptypes.h"

// TODO: MSP_PORT_INBUF_SIZE should be changed to
// dynamically allocate array length based on the payload size
// Hardcoding payload size to 64 bytes for now, to allow enough space
// for custom OSD text.
#define MSP_PORT_INBUF_SIZE 64

#define CHECK_PACKET_PARSING() \
  if (packet->readError) {\
    return;\
  }

typedef enum {
    MSP_IDLE,
    MSP_HEADER_START,
    MSP_HEADER_X,

    MSP_HEADER_V2_NATIVE,
    MSP_PAYLOAD_V2_NATIVE,
    MSP_CHECKSUM_V2_NATIVE,

    MSP_COMMAND_RECEIVED
} mspState_e;

typedef enum {
    MSP_PACKET_UNKNOWN,
    MSP_PACKET_COMMAND,
    MSP_PACKET_RESPONSE
} mspPacketType_e;

typedef struct __attribute__((packed)) {
    uint8_t  flags;
    uint16_t function;
    uint16_t payloadSize;
} mspHeaderV2_t;

typedef struct {
    mspPacketType_e type;
    uint8_t         flags;
    uint16_t        function;
    uint16_t        payloadSize;
    uint8_t         payload[MSP_PORT_INBUF_SIZE];
    uint16_t        payloadReadIterator;
    bool            readError;
    // Function that replace function enum with string
    const char *get_func_name()
    {
        switch (function)
        {
        case MSP_ELRS_FUNC:
            return "MSP_ELRS_FUNC";
        case MSP_SET_RX_CONFIG:
            return "MSP_SET_RX_CONFIG";
        case MSP_VTX_CONFIG:
            return "MSP_VTX_CONFIG";
        case MSP_SET_VTX_CONFIG:
            return "MSP_SET_VTX_CONFIG";
        case MSP_EEPROM_WRITE:
            return "MSP_EEPROM_WRITE";
        case MSP_ELRS_RF_MODE:
            return "MSP_ELRS_RF_MODE";
        case MSP_ELRS_TX_PWR:
            return "MSP_ELRS_TX_PWR";
        case MSP_ELRS_TLM_RATE:
            return "MSP_ELRS_TLM_RATE";
        case MSP_ELRS_BIND:
            return "MSP_ELRS_BIND";
        case MSP_ELRS_MODEL_ID:
            return "MSP_ELRS_MODEL_ID";
        case MSP_ELRS_REQU_VTX_PKT:
            return "MSP_ELRS_REQU_VTX_PKT";
        case MSP_ELRS_SET_TX_BACKPACK_WIFI_MODE:
            return "MSP_ELRS_SET_TX_BACKPACK_WIFI_MODE";
        case MSP_ELRS_SET_VRX_BACKPACK_WIFI_MODE:
            return "MSP_ELRS_SET_VRX_BACKPACK_WIFI_MODE";
        case MSP_ELRS_SET_RX_WIFI_MODE:
            return "MSP_ELRS_SET_RX_WIFI_MODE";
        case MSP_ELRS_SET_RX_LOAN_MODE:
            return "MSP_ELRS_SET_RX_LOAN_MODE";
        case MSP_ELRS_GET_BACKPACK_VERSION:
            return "MSP_ELRS_GET_BACKPACK_VERSION";
        case MSP_ELRS_BACKPACK_CRSF_TLM:
            return "MSP_ELRS_BACKPACK_CRSF_TLM";
        case MSP_ELRS_SET_SEND_UID:
            return "MSP_ELRS_SET_SEND_UID";
        case MSP_ELRS_SET_OSD:
            return "MSP_ELRS_SET_OSD";
        case MSP_ELRS_BACKPACK_CONFIG:
            return "MSP_ELRS_BACKPACK_CONFIG";
        case MSP_ELRS_BACKPACK_CONFIG_TLM_MODE:
            return "MSP_ELRS_BACKPACK_CONFIG_TLM_MODE";
        case MSP_ELRS_BACKPACK_GET_CHANNEL_INDEX:
            return "MSP_ELRS_BACKPACK_GET_CHANNEL_INDEX";
        case MSP_ELRS_BACKPACK_SET_CHANNEL_INDEX:
            return "MSP_ELRS_BACKPACK_SET_CHANNEL_INDEX";
        case MSP_ELRS_BACKPACK_GET_FREQUENCY:
            return "MSP_ELRS_BACKPACK_GET_FREQUENCY";
        case MSP_ELRS_BACKPACK_GET_RECORDING_STATE:
            return "MSP_ELRS_BACKPACK_GET_RECORDING_STATE";
        default:
            static char buffer[32];
            snprintf(buffer, sizeof(buffer), "Unknown function: %u", function);
            return buffer;
        }
    }

    void reset()
    {
        type = MSP_PACKET_UNKNOWN;
        flags = 0;
        function = 0;
        payloadSize = 0;
        payloadReadIterator = 0;
        readError = false;
    }

    void addByte(uint8_t b)
    {
        payload[payloadSize++] = b;
    }

    void makeResponse()
    {
        type = MSP_PACKET_RESPONSE;
    }

    void makeCommand()
    {
        type = MSP_PACKET_COMMAND;
    }

    uint8_t readByte()
    {
        if (payloadReadIterator >= payloadSize) {
            // We are trying to read beyond the length of the payload
            readError = true;
            return 0;
        }

        return payload[payloadReadIterator++];
    }
} mspPacket_t;

/////////////////////////////////////////////////

class MSP
{
public:
    MSP();
    bool            processReceivedByte(uint8_t c);
    mspPacket_t*    getReceivedPacket();
    void            markPacketReceived();
    bool            sendPacket(mspPacket_t* packet, Stream* port);
    uint8_t         convertToByteArray(mspPacket_t* packet, uint8_t* byteArray);
    uint8_t         getTotalPacketSize(mspPacket_t* packet);
    bool            awaitPacket(mspPacket_t* packet, Stream* port, uint32_t timeoutMillis);

private:
    mspState_e  m_inputState;
    uint16_t    m_offset;
    uint8_t     m_inputBuffer[MSP_PORT_INBUF_SIZE];
    mspPacket_t m_packet;
    uint8_t     m_crc;
};