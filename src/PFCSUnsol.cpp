#include "PFCSUnsol.h"

PFCSUnsolicited::PFCSUnsolicited(){
    PFCSUnsolicited("----", "", 0);
}
PFCSUnsolicited::PFCSUnsolicited(const char* machineID, 
                                const char* pfcsHost, 
                                int pfcsPort,
                                uint32_t waitForACK,
                                uint32_t numberOfRetries,
                                uint32_t waitForKeepAlive,
                                uint32_t waitForDataTime,
                                uint32_t timeToConnect, 
                                verifyDataCallback callback){
    bool bCheck = false;
    if(bCheck) bCheck = false;
    else bCheck = true;

    _waitForACK = waitForACK;
    _numberOfRetries = numberOfRetries;
    _waitForKeepAlive = waitForKeepAlive;
    _waitForDataTime = waitForDataTime;
    _timeToConnect = timeToConnect;
    _versionInfoKeepAlive = "GLM AutoRSIGV1V2.3.6:DLL3.2:UNSOL:01:AUTO GLM Automacao - RSIG";

    _messageCounter = 0;
    _messageErrorCount = 0;
    _messageStatus = STAT_NOCREATE;

    _callback = callback;

    strlcpy(_pfcsHost, pfcsHost, 17);
    _pfcsPort = pfcsPort;
    strlcpy(_machineID, machineID, 5);

    this->setupClient();

    _pfcsTimer = TimeoutCallback(_timeToConnect * 1000, [this](){
        switch(_messageStatus) {
            case STAT_RETRYNAK:
                _messageErrorCount++;
                if(_messageErrorCount > _numberOfRetries) {
                    log_w("[PFCS Unsol]Errors sending data. Tried a few times.");
                    _messageStatus = STAT_IDLE;
                    setTimer(_waitForKeepAlive);
                    _messageErrorCount = 0;
                } else {
                    repeatSendMessage();
                }
                break;
            
            case STAT_SENDINGACK:
            case STAT_IDLE:
                sendKeepAlive();
                break;
            
            case STAT_SENDINGDATA:
            case STAT_SENDING:
                _messageErrorCount++;
                if(_messageErrorCount > _numberOfRetries) {
                    log_w("[PFCS Unsol]Errors sending data. Tried a few times.");
                    _messageErrorCount = 0;
                    _messageStatus = STAT_NOCREATE;
                    _client->close();
                } else {
                    log_w("[PFCS Unsol]Errors sending data. Tried a few times.");
                    _messageStatus = STAT_NOCREATESENDERROR;
                    
                    _client->close();
                    log_w("[PFCS Unsol]Reconnecting...");
                    setTimer(_waitForACK);
                    connect();
                }
                break;
            case STAT_WAITACKDATA:
            case STAT_WAITACK:
                _messageErrorCount++;
                if(_messageErrorCount > _numberOfRetries) {
                    _received.setBuffer("", 0);
                    log_w("[PFCS Unsol]ACKs are not coming. Tried a few times.");
                    _messageStatus = STAT_NOCREATE;
                    
                    _client->close();
                    log_w("[PFCS Unsol]Reconnecting...");
                    setTimer(_timeToConnect);
                    connect();
                } else {
                    log_w("[PFCS Unsol]ACK did not come. Repost.");
                    repeatSendMessage();
                }
                break;
            case STAT_WAITVERIFYDATA0003:
                if(_callback(_received.getBuffer())){
                    sendACKNAK(true, (_messageCounter - 1), "0003", ' ');
                    log_w("[PFCS Unsol]Build data valid.");
                } else {
                    sendACKNAK(true, (_messageCounter - 1), "0003", 'K');
                    log_w("[PFCS Unsol]Build data was invalid.");
                }
                _messageStatus = STAT_IDLE;
                return;

            case STAT_NOCREATESENDERROR:
            case STAT_NOCONNECTSENDERROR:
                _messageErrorCount++;
                if(_messageErrorCount > _numberOfRetries) {
                    _messageErrorCount = 0;
                    _messageStatus = STAT_NOCREATE;
                    log_w("[PFCS Unsol]Reconnecting...");
                    setTimer(_timeToConnect);
                } else {
                    log_w("[PFCS Unsol]Reconnecting...");
                    setTimer(_waitForACK);
                    connect();
                }
                break;
            case STAT_NOCREATE:
            case STAT_NOCONNECT:
                log_w("[PFCS Unsol]Reconnecting from noConnect...");
                setTimer(_timeToConnect);
                connect();
                break;
            default: break;
        }
    });
}
PFCSUnsolicited::~PFCSUnsolicited(){
}

void PFCSUnsolicited::PFCSConfig(int iInterval, int iRepeatCount, int iSleepTime, int iWaitTime, int iReconnect, char* sVersion){
    if(iInterval > 0) {_waitForACK = iInterval;};
    if(iRepeatCount > 0) {_numberOfRetries = iRepeatCount;};
    if(iSleepTime > 0) {_waitForKeepAlive = iSleepTime;};
    if(iWaitTime > 0) {_waitForDataTime = iWaitTime;};
    if(iReconnect > 0) {_timeToConnect = iReconnect;};
    if(strlen(sVersion) == 0) return;
    _versionInfoKeepAlive = sVersion;
}
void PFCSUnsolicited::setupClient(){
    _client = new newAsyncClient();
    _client->setNoDelay(true);

    auto onConnect = [this](void* arg, newAsyncClient* client) {
        _messageCounter = 0;
        if(_messageStatus == STAT_NOCONNECTSENDERROR){
            repeatSendMessage();
        } else {
            _messageErrorCount = 0;
            if(strncmp(_machineID, "----", 4) == 0){
                log_w("[PFCS Unsol]Detected machine ID is ----");
                sendWhoAmI();
            } else {
                log_w("[PFCS Unsol]Has machineID configured.");
                sendKeepAlive();
            }
        }
        log_w("[PFCS Unsol]Connected!");
    };
    auto onData = [this](void* arg, newAsyncClient* client, void *data, size_t dataLength) {
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
        log_w("[PFCS Unsol Receive]Msg: (%s)", _received.getBuffer());

        char str1[1050];
        char str2[1050];

        int CRcount = 0;
        for(int i = 0; i < dataLength; i++){
            switch(CRcount) {
                case 0:
                    str1[i] = ((char*)data)[i];
                    if(str1[i] == 13) {
                        str1[i + 1] = 0;
                        CRcount++;
                    }
                    break;
                case 1:
                    str2[i] = ((char*)data)[i];
                    if(str2[i] == 13) {
                        str2[i + 1] = 0;
                        CRcount++;
                    }
                default:
                    break;
            }
        }

        //log_w("[PFCS Unsol]str1: (%s)", str1);
        //log_w("[PFCS Unsol]str2: (%s)", str2);
        int str1_len = strlen(str1) + 1;
        int str2_len = strlen(str2) + 1;
        switch(_messageStatus) {
            case STAT_IDLE:
                if(strlen(str1) >= 4) {
                    if(str1[4] == ' ') {
                        _received.setBuffer(str1, str1_len);
                        receiveData();
                    }
                }
                break;
            case STAT_WAITACK:
            case STAT_WAITACKDATA:
                if(str1_len >= 16) {
                    if(str1[16] == '3') {
                        _messageStatus = STAT_IDLE;
                        _received.setBuffer(str1, str1_len);
                        receiveData();
                        log_w("[PFCS Unsol]Type 0003 message received instead of ACK");
                    } else if (str1[4] == 'A' || str1[4] == 'N'){
                        _received.setBuffer(str1, str1_len);
                        receiveACK();
                    }
                }
                break;
            case STAT_SENDING:
            case STAT_SENDINGDATA:
                log_w("[PFCS Unsol]Unknown DATA received 'noCR' (%s%s)", str1, str2);
                break;
        }

        if(CRcount > 1){
            switch(_messageStatus) {
                case STAT_IDLE:
                    if(str2_len >= 4) {
                        if(str2[4] == ' ') {
                            _received.setBuffer(str2, str2_len);
                            receiveData();
                        }
                    }
                    break;
                case STAT_WAITACK:
                case STAT_WAITACKDATA:
                    if(str1_len >= 16) {
                        if(str2[4] == 'A' || str2[4] == 'N') {
                            _received.setBuffer(str2, str2_len);
                            receiveACK();
                        //Waiting for ACK, but received type 3 msg
                        } else if (str2[16] == '3') {
                            _messageStatus = STAT_IDLE;
                            _received.setBuffer(str2, str2_len);
                            receiveData();
                            log_w("[PFCS Unsol]Type 0003 message received instead of ACK");
                        }
                    }
                    break;
                case STAT_SENDING:
                case STAT_SENDINGDATA:
                    log_w("[PFCS Unsol]Unknown data received (%s%s)", str1, str2);
                    break;
            }
        }
    };
    auto onDisconnect = [this](void* arg, newAsyncClient* client) {
        _messageStatus = STAT_NOCREATE;
        log_w("[PFCS Unsol]Server Closed the Connection.");
        setTimer(_timeToConnect);
    };
    auto onPoll = [this](void* arg, newAsyncClient* client) {
        if(client->pcb() == nullptr) {
            log_d("OnPoll pcb is null");
        }
    };
    auto onError = [this](void* arg, newAsyncClient* client, err_t err) {
        log_w("[PFCS Unsol TCP Error]We have a problem: (%s)", _client->errorToString(err));
        //client->abort();
        //client->close(true);
        //while(!client->freeable());
        //client->free();
        //delete client;
    };

    _client->onData(onData, _client);
    _client->onConnect(onConnect, _client);
    _client->onDisconnect(onDisconnect, _client);
    _client->onError(onError, _client);
}
bool PFCSUnsolicited::connect(){
    if(!_client) {
        setupClient();
    }
    if(!_client->connect(_pfcsHost, _pfcsPort)){
        log_w("[PFCS Unsol]Connection failed.");
        return false;
    }    
    return true;
}
bool PFCSUnsolicited::connected(){
    return _client->connected();
}
int PFCSUnsolicited::repeatSendMessage(){
    if(_message.getReqVal() == 1) {
        _messageStatus = STAT_SENDINGDATA;
    } else {
        _messageStatus = STAT_SENDING;
    }
    setTimer(_waitForACK);
    
    /*while(!_client->canSend() || _client->space() <= _message.getLength()) { }
    if(_client->write(_message.getBuffer(), _message.getLength())) {
        dataSent();
        return true;    
    }*/

    if(writeToClient()){
        return true;
    }

    log_w("[PFCS Unsol]Error re-sending msg (%s)", _message.getBuffer());
    _messageErrorCount++;
    return false;
}
int PFCSUnsolicited::sendKeepAlive(){
    int count = _messageCounter;
    if(count != 0) count--;

    _message.makekeepalive(_machineID, count, _versionInfoKeepAlive);
    setTimer(_waitForACK);
    _messageStatus = STAT_SENDING;

    if(writeToClient()){
        return true;
    }

    log_w("[PFCS Unsol]Error sending Keep-Alive (%s)", _message.getBuffer());
    return false;
}
int PFCSUnsolicited::sendKeepAliveText(const char* text){
    if(_messageStatus != STAT_IDLE) {
        log_w("System is busy. Try later.");
        return false;
    }
    int count = _messageCounter;
    if(count != 0) count--;

    _message.makekeepalive(_machineID, count, text);
    setTimer(_waitForACK);
    _messageStatus = STAT_SENDING;

    if(writeToClient()){
        return true;
    }
    
    log_w("[PFCS Unsol]Error sending Keep-Alive (%s)", _message.getBuffer());
    return false;
}
bool PFCSUnsolicited::sendWhoAmI(){
    _message.makekeepalive("----", 0, _versionInfoKeepAlive);
    setTimer(_waitForACK);
    _messageStatus = STAT_SENDING;

    
    if(writeToClient()){
        return true;
    }
    
    log_w("[PFCS Unsol]Error sending msg (%s)", _message.getBuffer());
    return false;
}
int PFCSUnsolicited::sendACKNAK(bool ack, int seq_num, const char* req_type, char err){
    if(ack) {
        char id_buf[5];
        _received.getMachName(id_buf);
        _message.makeack(id_buf, "ACK", seq_num, req_type, err);
    } else {
        char id_buf[5];
        _received.getMachName(id_buf);
        _message.makeack(id_buf, "NAK", seq_num, req_type, err);
    }

    _messageStatus = STAT_SENDINGACK;
    setTimer(_waitForKeepAlive);

    
    if(writeToClient()){
        return true;
    }
    

    log_w("[PFCS Unsol]Error sending ACK/NAK (%s)", _message.getBuffer());
    return false;
}
void PFCSUnsolicited::loop(){
    _pfcsTimer.loop();
    //log_v("Unsol client state: %s", this->_client->stateToString());
}
void PFCSUnsolicited::dataSent(){
    if(strlen(_message.getBuffer()) != 0){
        log_w("[PFCS Unsol]Sent out: (%s)", _message.getBuffer());
    }
    switch(_messageStatus) {
        case STAT_SENDINGDATA:
            _messageStatus = STAT_WAITACKDATA;
            break;
        case STAT_SENDING:
            _messageStatus = STAT_WAITACK;
            break;
        case STAT_SENDINGACK:
            _messageStatus = STAT_IDLE;
            break;
    }
}
void PFCSUnsolicited::receiveACK(){
    char mid_buf[5];
    _message.getMachName(mid_buf);
    _received.checkACKMsg(mid_buf, _messageCounter, _message.getReqVal());
    log_w("[PFCS Unsol Receive]ACK/NAK");
    switch(_received.getError()) {
        case ERR_ACK_D:
        case ERR_NONE:
            _messageErrorCount = 0;
            if(_messageStatus == STAT_WAITACKDATA) {
                _messageStatus = STAT_WAITDATA;
            } else {
                setTimer(_waitForKeepAlive);
                _messageStatus = STAT_IDLE;
            }
            break;
        
        case ERR_NAK_A:
            log_w("[PFCS Unsol]ERROR: Received NAK-A in Response to Message");
            _messageStatus = STAT_RETRYNAK;
            break;
        case ERR_NAK_B:
            log_w("[PFCS Unsol]ERROR: Received NAK-B in Response to Message");
            char mid_buf[5];
            _message.getMachName(mid_buf);
            if (strncmp(mid_buf, "----", 4) == 0) {
                _received.getMachName(mid_buf);
                strlcpy(_machineID, mid_buf, 5);
                _message.setMachName(mid_buf);			  
            }
            retryNAK();		            
            break;
        case ERR_NAK_E:
            log_w("[PFCS Unsol]ERROR: Received NAK-E in Response to Message");
            _messageCounter = _message.getSeqNum();
            retryNAK();
            break;
        case ERR_NAK_H:
            log_w("[PFCS Unsol]ERROR: Received NAK-H in Response to Message");
            retryNAK();
            break;
        case ERR_NAK_I:   // should never happen
            log_w("[PFCS Unsol]ERROR: Received NAK-I in Response to Message");
            retryNAK();
            break;       
        case ERR_NAK_J: // should never happen
            log_w("[PFCS Unsol]ERROR: Received NAK-J in Response to Message");		  
            retryNAK();
            break;
        // NO MESSAGE RECEIVED
        case ERR_SHORT:  // less than 18 bytes received
            break;

        case ERR_INVALID_LEN:
		    log_w("[PFCS Unsol]ERROR: ACK message is larger than 1024 bytes"); break;	   
        case ERR_UNRECOG_CHAR:
		    log_w("[PFCS Unsol]ERROR: ACK Message contains non-printable characters"); break;
	    case ERR_WRONG_MACH_ID:
		    log_w("[PFCS Unsol]ERROR: ACK Message contains wrong machine ID"); break;
        case ERR_BAD_MSG_RESULT:
            log_w("[PFCS Unsol]ERROR: ACK Message result is unknown"); break;
        case ERR_BAD_SEQ_NUM:
            log_w("[PFCS Unsol]ERROR: ACK Message contains non-numeric sequence number"); break;
        case ERR_WRONG_SEQ_NUM:
            log_w("[PFCS Unsol]ERROR: ACK Message Sequence Number does not match"); 
            break;
        case ERR_BAD_REQ_TYPE:
            log_w("[PFCS Unsol]ERROR: ACK Message Request Type does not match");
            break;
        default:
            log_w("[PFCS Unsol]ERROR: ACK Message Contains Unknown Error");
            break;
    }
}
void PFCSUnsolicited::receiveData(){
    _received.checkDataMsg(_machineID);
    char req_buf[5];
    log_w("[PFCS Unsol Receive]Data");
    switch(_received.getError()) {
        case ERR_BYTE_COUNT_LOW:
            log_w("[PFCS Unsol]Data Message Contains Bad Byte Count");
        case ERR_NONE:
            if(_received.getReqVal() != 3) {
                _received.getReqType(req_buf);
                sendACKNAK(false, _received.getSeqNum(), req_buf, 'H');
                log_w("[PFCS Unsol]Format error: Request Type Invalid");
                return;
            }
            if(_messageCounter == _received.getSeqNum()) {
                log_w("[PFCS Unsol]Received Type 0003 message");
                _messageCounter++;
                _messageStatus = STAT_WAITVERIFYDATA0003;
                setTimer(0.01);
                return;
            }
            if(_received.getSeqNum() == 0){
                log_w("[PFCS Unsol]Received Type 0003 message");
                _messageCounter = 1;
                _messageStatus = STAT_WAITVERIFYDATA0003;
                setTimer(0.01);
                return;
            }
            if((_messageCounter - 1) == _received.getSeqNum()) {
                log_w("[PFCS Unsol]Duplicate Type 0003 message");    
                sendACKNAK(true, _messageCounter - 1, "0003", 'D');
                return;
            }
            sendACKNAK(false, _messageCounter, "0003", 'F');
            char seq_buf[15];
            intToStr(seq_buf, _received.getSeqNum(), 6);
            log_w("[PFCS Unsol]Type 0003 message not in sequence: Exp(%d) Got(%s)", _messageCounter, seq_buf);
            return;
        case ERR_BYTE_COUNT_HIGH:
            _received.getReqType(req_buf);
            sendACKNAK(false, _received.getSeqNum(), req_buf, 'J');
            log_w("[PFCS Unsol]Format error : Data Message too long");
            return;
        case ERR_SHORT:
            return;
        case ERR_INVALID_LEN:
            _received.getReqType(req_buf);
            sendACKNAK(false, _received.getSeqNum(), req_buf, 'I');
            log_w("[PFCS Unsol]Format error : Data Message too long");
            return;
        case ERR_UNRECOG_CHAR:
            _received.getReqType(req_buf);
            sendACKNAK(false, _received.getSeqNum(), req_buf, 'A');
            log_w("[PFCS Unsol]Format error : Data Message contains unprintable characters");
            return;
        case ERR_WRONG_MACH_ID:
            _received.getReqType(req_buf);
            sendACKNAK(false, _received.getSeqNum(), req_buf, 'B');
            log_w("[PFCS Unsol]Format error : Data Message Machine ID doesn't match");
            return;
        case ERR_BAD_MSG_RESULT:
            // assume it is an extra ACK or NAK -- ignore
            return;
        case ERR_BAD_SEQ_NUM:
            _received.getReqType(req_buf);
            sendACKNAK(false, _received.getSeqNum(), req_buf, 'E');
            log_w("[PFCS Unsol]Format error : Data Message has non-numeric sequence number");
            return;
    }
}
void PFCSUnsolicited::retryNAK(){
    _messageErrorCount++;
    if(_messageErrorCount > _numberOfRetries) {
        log_w("[PFCS UnsolUnsol]Errors sending data. Tried a few times.");
        _messageStatus = STAT_IDLE;
        setTimer(_waitForKeepAlive);
        _messageErrorCount = 0;
    } else {
        repeatSendMessage();
    }
}
void PFCSUnsolicited::setTimer(double seconds){
    log_d("Setting PFCSUnsol Timer");

    _pfcsTimer.stop();
    if(seconds == 0) return;
    _pfcsTimer.setInterval(seconds * 1000);
    _pfcsTimer.reset();
    _pfcsTimer.start();
}
int PFCSUnsolicited::writeToClient() {
    do {} while (xSemaphoreTake(pfcsSemaphore, portMAX_DELAY) != pdPASS);
    log_d("PFCSUnsol is taking the semaphore");
    while(!_client->canSend() || _client->space() <= _message.getLength()) { }
    if(_client->write(_message.getBuffer(), _message.getLength())) {
        dataSent();
        xSemaphoreGive(pfcsSemaphore);
        log_d("PFCSUnsol is giving the semaphore");
        return true;    
    }
    xSemaphoreGive(pfcsSemaphore);
    log_d("PFCSUnsol is giving the semaphore");
    return false;
}
void PFCSUnsolicited::stop(){
    //_client->abort();
    _client->close(false);
    /*while(!_client->disconnected());
    while(!_client->freeable());
    _client->free();
    delete _client;*/
}
uint8_t PFCSUnsolicited::getMsgStatus(){
    return this->_messageStatus;
}
void PFCSUnsolicited::setDataVerifyCallback(verifyDataCallback callback){
    _callback = callback;
}
uint32_t PFCSUnsolicited::printClientPCB() {
    return (uint32_t)_client->pcb();
}
