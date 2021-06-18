#include "PFCS.h"

PFCS::PFCS(){
    PFCS("----", "", 0);
}
PFCS::PFCS(const char* machineID, 
            const char* pfcsHost, 
            int pfcsPort, 
            uint32_t waitForACK,
            uint32_t numberOfRetries,
            uint32_t waitForKeepAlive,
            uint32_t waitForDataTime,
            uint32_t timeToConnect, 
            verifyDataCallback callback) {
    bool bCheck = false;
    if(bCheck) bCheck = false;
    else bCheck = true;

    _waitForACK = waitForACK;
    _numberOfRetries = numberOfRetries;
    _waitForKeepAlive = waitForKeepAlive;
    _waitForDataTime = waitForDataTime;
    _timeToConnect = timeToConnect;
    _versionInfoKeepAlive = "GLM AutoRSIGV2V2.3.6:DLL3.2:SOL:01:AUTO GLM Automacao - RSIG";

    _messageCounter = 0;
    _messageErrorCount = 0;
    _messageStatus = STAT_NOCREATE;

    _callback = callback;

    strlcpy(_pfcsHost, pfcsHost, 17);
    _pfcsPort = pfcsPort;
    strlcpy(_machineID, machineID, 5);

    this->setupClient();
    
    _pfcsTimer = TimeoutCallback(_timeToConnect * 1000, [this](){
        switch(_messageStatus){
            case STAT_RETRYNAK:
                retryNAK();
                break;
            case STAT_SENDINGACK:                
            case STAT_IDLE:
                sendKeepAlive();
                break;
            case STAT_WAITDATA:
                _messageStatus = STAT_IDLE;
                setTimer(_waitForKeepAlive - _waitForDataTime);
                log_w("[PFCS Sol]Data expected after request not received.");
                _dataMessageReceived = false;
                break;
            case STAT_SENDINGDATA:
            case STAT_SENDING:
                _messageErrorCount++;
                if(_messageErrorCount > _numberOfRetries){
                    log_w("[PFCS Sol]Errors sending data. Tried a few times.");
                    _messageErrorCount = 0;
                    _messageStatus = STAT_NOCREATE;

                    _client->close();

                    log_w("[PFCS Sol]Reconnecting...");
                    setTimer(_timeToConnect);
                    connect();
                } else {
                    log_w("[PFCS Sol]Errors sending data. Tried a few times.");
                    _messageStatus = STAT_NOCREATESENDERROR;

                    _client->close();

                    log_w("[PFCS Sol]Reconnecting...");
                    setTimer(_waitForACK);
                    connect();
                }
                break;
            case STAT_WAITACKDATA:
            case STAT_WAITACK:
                _messageErrorCount++;
                if(_messageErrorCount > _numberOfRetries){
                    _received.setBuffer("", 3);
                    log_w("[PFCS Sol]ACKs are not coming. Tried a few times.");
                    _messageStatus = STAT_NOCREATE;

                    _client->close();

                    log_w("[PFCS Sol]Reconnecting...");
                    setTimer(_timeToConnect);
                    connect();
                } else {
                    log_w("[PFCS Sol]ACK did not come. Repost.");
                    repeatSendMessage();
                }
                break;
            case STAT_WAITVERIFYDATA0001:
                //sendACKNAK(true, _messageCounter, "0001", 'K');
                log_w("[PFCS Sol]Build data was not verified.");
                _messageStatus = STAT_IDLE;
                setTimer(_waitForKeepAlive - 2);
                break;
            case STAT_NOCREATESENDERROR:
            case STAT_NOCONNECTSENDERROR:
                _messageErrorCount++;
                if(_messageErrorCount > _numberOfRetries){
                    _messageErrorCount = 0;
                    _messageStatus = STAT_NOCREATE;
                    log_w("[PFCS Sol]Reconnecting...");
                    setTimer(_timeToConnect);
                    //connect();
                } else {
                    log_w("[PFCS Sol]Reconnecting");
                    setTimer(_waitForACK);
                    connect();
                }
                break;
            case STAT_NOCREATE:
            case STAT_NOCONNECT:
                log_w("[PFCS Sol]Reconnecting from noConnect...");
                setTimer(_timeToConnect);
                connect();
                break;
            default: break;
        }
    });
}
PFCS::~PFCS(){
}

void PFCS::PFCSConfig(int iInterval, int iRepeatCount, int iSleepTime, int iWaitTime, int iReconnect, char* sVersion) {
    if(iInterval > 0) {_waitForACK = iInterval;};
    if(iRepeatCount > 0) {_numberOfRetries = iRepeatCount;};
    if(iSleepTime > 0) {_waitForKeepAlive = iSleepTime;};
    if(iWaitTime > 0) {_waitForDataTime = iWaitTime;};
    if(iReconnect > 0) {_timeToConnect = iReconnect;};
    if(strlen(sVersion) == 0) return;
    _versionInfoKeepAlive = sVersion;
}
void PFCS::setupClient(){
    _client = new newAsyncClient();
    _client->setNoDelay(true);

    auto onConnect = [this](void* arg, newAsyncClient* client){
        _messageCounter = 0;
        if(_messageStatus == STAT_NOCONNECTSENDERROR){
            repeatSendMessage();
        } else {
            _messageErrorCount = 0;
            if(strncmp(_machineID, "----", 4) == 0){
                log_w("[PFCS Sol]Detected machine ID is ----");
                sendWhoAmI();
            } else {
                log_w("[PFCS Sol]Has machineID configured.");
                sendKeepAlive();
            }
        }
        log_w("[PFCS Sol]Connected!");
    };
    auto onData = [this](void* arg, newAsyncClient* client, void *data, size_t dataLength){
        /*switch(_messageStatus){
            case STAT_SENDINGDATA:
            case STAT_SENDING:
                break;
            case STAT_WAITACK:
                _messageStatus = STAT_SENDING;
                break;
            case STAT_WAITACKDATA:
                _messageStatus = STAT_SENDINGDATA;
                break;
        }*/

        _received.setBuffer((char*)data, dataLength);
        log_w("[PFCS Sol Receive]Msg: (%s)", _received.getBuffer());

        switch(_messageStatus){
            case STAT_IDLE:
                char msg_buf[4];
                _received.getMsgType(msg_buf);
                if(strncmp(msg_buf, "   ", 3) != 0){
                    break;
                }
                if(_received.getReqVal() == 1){
                    if(_dataMessageReceived && _messageCounter == _received.getSeqNum()){
                        sendACKNAK(true, _messageCounter, "0001", 'D');
                    } else {
                        sendACKNAK(true, _messageCounter, "0001", 'G');
                    }
                    _dataMessageReceived = false;
                    setTimer(_waitForKeepAlive);
                } else {
                    if(_received.getReqVal() != -1){
                        char type_buf[5];
                        _received.getReqType(type_buf);
                        sendACKNAK(true, _received.getSeqNum(), type_buf, 'U');
                    }
                }
                break;

            case STAT_WAITACK:           
            case STAT_WAITACKDATA:       receiveACK();   break;
            case STAT_WAITDATA:          receiveData();  break;

            case STAT_SENDING:
            case STAT_SENDINGDATA:		    
                log_w("[PFCS Sol]Unknown data received.");
                break;
        }
    };
    
    auto onDisconnect = [this](void* arg, newAsyncClient* client){
        _messageStatus = STAT_NOCREATE;
        log_w("[PFCS Sol]Server Closed the Connection");
        setTimer(_timeToConnect);
    };
    auto onPoll = [this](void* arg, newAsyncClient* client){
        if(client->pcb() == nullptr) {
            log_d("OnPoll pcb is null");
        }
    };
    auto onError = [this](void* arg, newAsyncClient* client, err_t err){
        log_w("[PFCS Sol TCP Error]We have a problem: (%s)", _client->errorToString(err));
        //client->abort();
        //client->close(true);
        //while(!client->freeable());
        //client->free();
        //delete client;
    };

    _client->onData(onData, _client);
    _client->onConnect(onConnect, _client);
    _client->onDisconnect(onDisconnect, _client);
    //_client->onError(onError, _client);
}
bool PFCS::connect() {
    if(!_client){
        setupClient();
    }
    if(!_client->connect(_pfcsHost, _pfcsPort)){
        log_w("[PFCS Sol]Connection failed.");
        return false;
    }
    _dataMessageReceived = false;
    return true;
}
bool PFCS::connected(){
    return _client->connected();
}
int PFCS::repeatSendMessage() {
    if(_message.getReqVal() == 1){
        _messageStatus = STAT_SENDINGDATA;
    } else {
        _messageStatus = STAT_SENDING;
    }
    setTimer(_waitForACK);

    if(writeToClient()){
        return true;
    }

    // _client->add(_message.getBuffer(), _message.getLength());
    // if(_client->send()){
    //     dataSent();
    //     return true;
    // }

    log_w("[PFCS Sol]Error re-sending msg (%s)", _message.getBuffer());
    _messageErrorCount++;
    return false;
}
int PFCS::sendKeepAlive() {
    _message.makekeepalive(_machineID, _messageCounter, _versionInfoKeepAlive);
    setTimer(_waitForACK);
    _messageStatus = STAT_SENDING;

    if(writeToClient()){
        return true;
    }

    // _client->add(_message.getBuffer(), _message.getLength());
    // if(_client->send()){
    //     dataSent();
    //     return true;
    // }

    log_w("[PFCS Sol]Error sending Keep-Alive (%s)", _message.getBuffer());
    return false;
}
int PFCS::sendKeepAliveText(const char* text) {
    if(_messageStatus != STAT_IDLE){
        log_w("[PFCS Sol]System is busy. Try later.");
        return false;
    }

    _message.makekeepalive(_machineID, _messageCounter, text);
    setTimer(_waitForACK);
    _messageStatus = STAT_SENDING;

    if(writeToClient()){
        return true;
    }

    // _client->add(_message.getBuffer(), _message.getLength());
    // if(_client->send()){
    //     dataSent();
    //     return true;
    // }

    log_w("[PFCS Sol]Error sending Keep-Alive (%s)", _message.getBuffer());
    return false;
}
bool PFCS::sendWhoAmI() {
    _message.makekeepalive("----", 0, _versionInfoKeepAlive);

    setTimer(_waitForKeepAlive);
    _messageStatus = STAT_SENDING;

    if(writeToClient()){
        return true;
    }

    return false;
}
int PFCS::sendACKNAK(bool ack, int seq_num, const char* req_type, char err) {
    if(ack){
        char id_buf[5];
        _received.getMachName(id_buf);
        _message.makeack(id_buf, "ACK", seq_num, req_type, err);
        if(strncmp(req_type, "0001", 4) == 0 && err == 'D' && _messageStatus != STAT_IDLE){
            _messageStatus = STAT_WAITDATA;
        } else {
            _messageStatus = STAT_IDLE;
            setTimer(_waitForKeepAlive);
        }
    } else {
        char id_buf[5];
        _received.getMachName(id_buf);
        _message.makeack(id_buf, "NAK", seq_num, req_type, err);
        if(strncmp(req_type, "0001", 4) && _messageErrorCount <= 2){
            _messageErrorCount++;
            _messageStatus = STAT_WAITDATA;
            setTimer(_waitForDataTime);
        } else {
            _messageErrorCount = 0;
            _messageStatus = STAT_IDLE;
            setTimer(_waitForKeepAlive);
        }
    }

    if(writeToClient()) {
        return true;
    }
    // _client->add(_message.getBuffer(), _message.getLength());
    // if(_client->send()){
    //     dataSent();
    //     return true;
    // }

    log_w("[PFCS Sol]Error sending ACK/NAK (%s)", _message.getBuffer());
    return false;
}
void PFCS::loop() {
    _pfcsTimer.loop();
    //log_v("SOL client state: %s", this->_client->stateToString());
}
void PFCS::dataSent() {
    if(_message.getLength() != 0){
        log_w("[PFCS Sol] Sent out: (%s)", _message.getBuffer());
    }
    switch(_messageStatus){
        case STAT_SENDINGDATA:
            _messageStatus = STAT_WAITACKDATA;
            break;
        case STAT_SENDING:
            _messageStatus = STAT_WAITACK;
            break;
        case STAT_IDLE:
            break;
        case STAT_WAITDATA:
            break;
    }
}
void PFCS::receiveACK() {
    //int reqType = _message.getReqVal();
    char id_buf[5];
    _message.getMachName(id_buf);
    _received.checkACKMsg(id_buf, _messageCounter, _message.getReqVal());
    log_w("[PFCS Sol Receive]ACK/NAK");
    switch(_received.getError()){
        case ERR_ACK_D:
            _messageErrorCount = 0;
            setTimer(_waitForKeepAlive);
            _messageStatus = STAT_IDLE;
            break;
        case ERR_NONE:
            _messageErrorCount = 0;
            if(_messageStatus == STAT_WAITACKDATA){
                setTimer(_waitForACK);
                _messageStatus = STAT_WAITDATA;
            } else {
                setTimer(_waitForKeepAlive);
                _messageStatus = STAT_IDLE;
            }
            //TODO test for piggyback message, ack followed by data messages.
            break;
        case ERR_NAK_A:
            log_w("[PFCS Sol Error]Received NAK-A in Response to Message.");
            _messageStatus = STAT_RETRYNAK;
            break;
        case ERR_NAK_B:
            log_w("[PFCS Sol Error]Received NAK-B in Response to Message.");
            char id_buf[5];
            _message.getMachName(id_buf);
            if(strncmp(id_buf, "----", 4)){
                _received.getMachName(id_buf);
                _message.setMachName(id_buf);
            }
            retryNAK();
            break;
        case ERR_NAK_E:
            log_w("[PFCS Sol Error]Received NAK-E in Response to Message.");
            _message.setSeqNum(1);
            _messageCounter = 1;
            retryNAK();
            break;
        case ERR_NAK_H:
            log_w("[PFCS Sol Error]Received NAK-H in Response to Message.");
            retryNAK();
            break;
        case ERR_NAK_I:
            log_w("[PFCS Sol Error]Received NAK-I in Response to Message.");
            retryNAK();
            break;
        case ERR_NAK_J:
            log_w("[PFCS Sol Error]Received NAK-J in Response to Message.");
            retryNAK();
            break;
        
        //NO MESSAGE RECEIVED
        case ERR_SHORT:
            break;
        
        case ERR_INVALID_LEN:
            log_w("[PFCS Sol Error]ACK message is larger than 1024");
            break;
        case ERR_UNRECOG_CHAR:
            log_w("[PFCS Sol Error]ACK message contains non-printable chars");
            break;
        case ERR_WRONG_MACH_ID:
            log_w("[PFCS Sol Error]ACK message contains wrong Machine ID");
            break;
        case ERR_BAD_MSG_RESULT:
            log_w("[PFCS Sol Error]ACK message result is unknown");
            break;
        case ERR_BAD_SEQ_NUM:
            log_w("[PFCS Sol Error]ACK message contains non-numeric sequence number");
            break;
        case ERR_WRONG_SEQ_NUM:
            log_w("[PFCS Sol Error]ACK message sequence number does not match");
            break;
        case ERR_BAD_REQ_TYPE:
            log_w("[PFCS Sol Error]ACK message request type does not match");
            break;
        default:
            log_w("[PFCS Sol Error]ACK message contains unknown error");
            break;
    }
}
void PFCS::receiveData() {
    _received.checkDataMsg(_machineID);
    char reqtype_buf[5];
    switch(_received.getError()){
        case ERR_BYTE_COUNT_LOW:
            log_w("[PFCS Sol]Data Message Contains Bad Byte Count");
        case ERR_NONE:
            _dataMessageReceived = true;
            if(_received.getReqVal() != 1){
                log_w("[PFCS Sol]Received %d message while waiting for data", _received.getReqVal());
                return;
            }
            if(_messageCounter == _received.getSeqNum()){
                setTimer(2);
                _messageStatus = STAT_WAITVERIFYDATA0001;
            } else {
                if((_messageCounter - 1) == _received.getSeqNum()){
                    sendACKNAK(true, _messageCounter - 1, "0001", 'D');
                    log_w("[PFCS Sol]Received Duplicate Data Message");
                } else {
                    sendACKNAK(false, _messageCounter, "0001", 'F');
                    log_w("[PFCS Sol]Received Out of Sequence Data Message");
                }
            }
            return;

        case ERR_BYTE_COUNT_HIGH:
            _received.getReqType(reqtype_buf);
            sendACKNAK(false, _received.getSeqNum(), reqtype_buf, 'J');
            log_w("[PFCS Sol]Byte Count is Greater Than Data");
            return;
        case ERR_SHORT:
            return;
        case ERR_INVALID_LEN:
            _received.getReqType(reqtype_buf);
            sendACKNAK(false, _received.getSeqNum(), reqtype_buf, 'I');
            log_w("[PFCS Sol]Format Error: Data Message too long");
            return;
        case ERR_UNRECOG_CHAR:
            _received.getReqType(reqtype_buf);
            sendACKNAK(false, _received.getSeqNum(), reqtype_buf, 'A');
            log_w("[PFCS Sol]Format Error: Data Message contains unprintable chars");
            return;
        case ERR_WRONG_MACH_ID:
            _received.getReqType(reqtype_buf);
            sendACKNAK(false, _received.getSeqNum(), reqtype_buf, 'B');
            log_w("[PFCS Sol]Format Error: Data Message Machine ID doesn't match");
            return;
        case ERR_BAD_SEQ_NUM:
            _received.getReqType(reqtype_buf);
            sendACKNAK(false, _received.getSeqNum(), reqtype_buf, 'E');
            log_w("[PFCS Sol]Result Error: Data Message Results is not '   '");
            return;
        case ERR_BAD_MSG_RESULT:
            log_w("[PFCS Sol]Format Error: Data Message has non-numeric sequence number");
            return;
    }
}
void PFCS::retryNAK() {
    _messageErrorCount++;
    if(_messageErrorCount > _numberOfRetries){
        log_w("[PFCS Sol]Errors sending data. Tried a few times.");
        _messageStatus = STAT_IDLE;
        setTimer(_waitForKeepAlive);
        _messageErrorCount = 0;
    } else {
        repeatSendMessage();
    }
}
void PFCS::setTimer(double seconds) {
    log_d("Setting PFCS Timer");

    _pfcsTimer.stop();
    if(seconds == 0) return;
    _pfcsTimer.setInterval(seconds * 1000);
    _pfcsTimer.reset();
    _pfcsTimer.start();
}
int PFCS::writeToClient() {
    do {} while (xSemaphoreTake(pfcsSemaphore, portMAX_DELAY) != pdPASS);
    log_d("PFCSSol is taking the semaphore");
    while(!_client->canSend() || _client->space() <= _message.getLength()) { }
    if(_client->write(_message.getBuffer(), _message.getLength())) {
        _client->state();
        dataSent();
        xSemaphoreGive(pfcsSemaphore);
        log_d("PFCSSol is giving the semaphore");
        return true;    
    }
    xSemaphoreGive(pfcsSemaphore);
    log_d("PFCSSol is giving the semaphore");
    return false;
}
void PFCS::stop(){
    //_client->abort();
    _client->close();
    /*while(!_client->disconnected());
    while(!_client->freeable());
    _client->free();
    delete _client;*/
}
uint8_t PFCS::getMsgStatus(){
    return this->_messageStatus;
}
int PFCS::PFCSSendData(const char* machineID, int req_type, const char* data) {
    if(_messageStatus != STAT_IDLE){
        log_w("[PFCS Sol]System is busy. Try later.");
        return false;
    }

    if(req_type != 9999){
        _messageCounter++;
        if(_messageCounter > 999999) _messageCounter = 0;
    }

    char cnt_buf[5];
    intToStr(cnt_buf, req_type, 4);
    _message.makeMsg(machineID, _messageCounter, cnt_buf, data);
    setTimer(_waitForACK);
    switch(req_type){
        case 1  : _messageStatus = STAT_SENDINGDATA; break;
        default : _messageStatus = STAT_SENDING; break;
    }


    if(writeToClient()){
        return true;
    }

    // _client->add(_message.getBuffer(), _message.getLength());
    // if(_client->send()){
    //     dataSent();
    //     return true;
    // }

    log_w("[PFCS Sol]Error sending data (%s)", _message.getBuffer());
    return false;
}
int PFCS::PFCSSendData(int req_type, const char* data) {
    return PFCSSendData(_machineID, req_type, data);
}
uint32_t PFCS::printClientPCB() {
    return (uint32_t)_client->pcb();
}
