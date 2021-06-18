#ifndef PFCS_H
#define PFCS_H

#include "PFCSCommon.h"
#include "PFCSMessage.h"

class PFCS {
private:
    const char* _versionInfoKeepAlive;   //Version information inside keep alive message
    uint32_t _waitForACK;      //Time to wait for a response from PFCS
    uint32_t _numberOfRetries;          //Number of failed retries when trying to send a message to PFCS
    uint32_t _waitForKeepAlive;  //Time to wait during inactivity to send keep alive
    uint32_t _waitForDataTime;          //Time to wait for data from server after 0001 server ACK
    uint32_t _timeToConnect;         //Time in seconds between attemps of reconnection to PFCS

    char _machineID[5];
    char _pfcsHost[17];
    int _pfcsPort;
    bool _dataMessageReceived;

    //COMMON CLASS VARIABLES
    byte _messageStatus;
    uint32_t _messageCounter;
    int _messageErrorCount;
    PFCSMessage _message;
    PFCSMessage _received;

    newAsyncClient* _client;
    TimeoutCallback _pfcsTimer;

    verifyDataCallback _callback;

    void setupClient();
    void dataSent();
    void receiveACK();
    void receiveData();
    void retryNAK();
    void setTimer(double seconds);

    int writeToClient();

public:
    PFCS();
    PFCS(const char* machineID, 
        const char* pfcsHost, 
        int pfcsPort, 
        uint32_t waitForAck = 5,
        uint32_t numberOfRetries = 3,
        uint32_t waitForKeepAlive = 120,
        uint32_t waitForDataTime = 5,
        uint32_t timeToConnect = 10, 
        verifyDataCallback callback = nullptr);
    ~PFCS();

    void PFCSConfig(int iInterval, int iRepeatCount, int iSleepTime, int iWaitTime, int iReconnect, char* sVersion);
    bool connect();
    bool connected();
    int repeatSendMessage();
    int sendKeepAlive();
    int sendKeepAliveText(const char* text);
    bool sendWhoAmI();
    int PFCSSendData(int req_type, const char* data);
    int PFCSSendData(const char* machineID, int req_type, const char* data);
    int sendACKNAK(bool ack, int seq_num, const char* req_type, char err);
    void setDataVerifyCallback(verifyDataCallback callback);
    void loop();
    void stop();
    uint8_t getMsgStatus();
    uint32_t printClientPCB();
};

#endif