#include "PFCSCommon.h"
#include "PFCSMessage.h"

PFCSMessage::PFCSMessage(){
    PFCSMessage("");
}
PFCSMessage::PFCSMessage(const char* initialMsg){
    strlcpy(_s, initialMsg, SOL_CHAR_BUFFER_LEN);
}
void PFCSMessage::makeMsg(const char* machineID, int seq_num, const char* cs_reqtype, const char* pdata){
    char seq[7];
    intToStr(seq, seq_num, 6);

    char dataLength[5];
    intToStr(dataLength, strlen(pdata), 4);

    sprintf(_s, "%s   %s%s%s%s\r", machineID, seq, cs_reqtype, dataLength, pdata);
}
void PFCSMessage::makekeepalive(const char* machineID, int seq_num, const char* pdata){
    char seq[7];
    intToStr(seq, seq_num, 6);

    char dataLength[5];
    intToStr(dataLength, strlen(pdata), 4);

    sprintf(_s, "%s   %s%s%s%s\r", machineID, seq, "9999", dataLength, pdata);
}
void PFCSMessage::makeack(const char* machineID, const char* ack, int seq_num, const char* req_type, char error){
    char seq[7];
    intToStr(seq, seq_num, 6);

    sprintf(_s, "%s%s%s%s%c\r", machineID, ack, seq, req_type, error);
}
int PFCSMessage::getLength(){ return strlen(_s); };
char* PFCSMessage::getBuffer(){
    return _s;
};
void PFCSMessage::getMachName(char* dest){ 
    char auxbuf[5];
    for(int i = 0; i < 4; i++){
        auxbuf[i] = _s[i];
    }
    auxbuf[4] = 0;

    strcpy(dest, auxbuf);
};
void PFCSMessage::getReqType(char* dest){ 
    char auxbuf[5];
    for(int i = 0; i < 4; i++){
        auxbuf[i] = _s[i + 13];
    }
    auxbuf[4] = 0;

    strcpy(dest, auxbuf);
};
void PFCSMessage::getMsgType(char* dest){ 
    char auxbuf[4];
    for(int i = 0; i < 4; i++){
        auxbuf[i] = _s[i + 3];
    }
    auxbuf[3] = 0;

    strcpy(dest, auxbuf);
};
int PFCSMessage::getReqVal(){
    if(strlen(_s) < 17) return false;
    uint32_t iReq = 0;
    for(byte i = 13; i < 17; i++){
        char c = _s[i];
        if(!isDigit(c)) return 0;
        iReq = (c - '0') + iReq * 10;
    }
    return iReq;
}
uint32_t PFCSMessage::getSeqNum(){
    uint32_t iSeq = 0;
    if(strlen(_s) < 13) return false;
    for(byte i = 7; i < 13; i++){
        char c = _s[i];
        if(!isDigit(c)) return 0;
        iSeq = (c - '0') + iSeq * 10;
    }
    return iSeq;
}
int PFCSMessage::getError() { return _error; };
void PFCSMessage::setBuffer(char* newBuffer, size_t bufLength){
    strlcpy(_s, newBuffer, bufLength);
}
void PFCSMessage::setSeqNum(int seq_num){
    if(getLength() < 13) return;
    for(int i = 12; i > 6; i--){
        _s[i] = (48 + seq_num % 10);
        seq_num /= 10;
    }
}
void PFCSMessage::setReqType(int req_type){
    if(getLength() < 17) return;
    for(int i = 16; i > 12; i--){
        _s[i] = (48 + req_type % 10);
        req_type /= 10;
    }
}
void PFCSMessage::setMachName(const char* machineID){
    if(strlen(machineID) != 4) return;
    for(byte i = 0; i < 4; i++){ _s[i] = machineID[i]; }
}
void PFCSMessage::checkACKMsg(const char* machineID, int seq_num, int req_type){
    int msgLength = getLength();

    _error = ERR_NONE;

    int i;
    for(i = 0; i < msgLength; i++){if(_s[i] == '\r') break;}

    if(msgLength < 18) { _error = ERR_SHORT; return; }
    if(i >= 1024) { _error = ERR_INVALID_LEN; return; }

    int dataend = i;
    
    for(i = 0; i < dataend; i++) if(!isprint(_s[i])) { _error = ERR_UNRECOG_CHAR; return; }

    int iSeqNum = 0;
    for (i = 7; i < 13; i++) {
        if (!isdigit(_s[i])) { _error = ERR_BAD_SEQ_NUM; return; }
        iSeqNum = iSeqNum * 10 + _s[i] - 48;
    }
    //if (iSeqNum != pi_num) { Error = ERR_WRONG_SEQ_NUM; return; }

    char reqtype_buf[5];
    for(int j = 0; j < 4; j++){
        reqtype_buf[j] = _s[j + 13];
    }
    reqtype_buf[4] = 0;

    int ireq = atoi(reqtype_buf);
    if(ireq != req_type) {_error = ERR_BAD_REQ_TYPE; return; }

    char ack_buf[4];
    for(int j = 0; j < 3; j++){
        ack_buf[j] = _s[j + 4];
    }
    ack_buf[3] = 0;

    if(strncmp(ack_buf, "NAK", 3) == 0){
        if(_s[17] == 'A') { _error = ERR_NAK_A; return; }
        if(_s[17] == 'B') { _error = ERR_NAK_B; return; }
        if(_s[17] == 'E') { _error = ERR_NAK_E; return; }
        if(_s[17] == 'H') { _error = ERR_NAK_H; return; }
        if(_s[17] == 'I') { _error = ERR_NAK_I; return; }
        if(_s[17] == 'J') { _error = ERR_NAK_J;         }
    } else {
        if(strncmp(ack_buf, "ACK", 3) == 0){
            if(_s[17] == 'D') _error = ERR_ACK_D;
        } else { _error = ERR_BAD_MSG_RESULT; return; }
    }

    char mid_buf[5];
    for(int j = 0; j < 4; j++){
        mid_buf[j] = _s[j];
    }
    mid_buf[4] = 0;

    if((strncmp(mid_buf, machineID, 4) != 0) && (strncmp(machineID, "----", 4) != 0)){
        _error = ERR_WRONG_MACH_ID; return;
    }
}
void PFCSMessage::checkDataMsg(const char* machineID){
    int msgLength = getLength();
    _error = ERR_NONE;

    if(msgLength < 18) { _error = ERR_SHORT; return; }
    if(msgLength > 1046) { _error = ERR_INVALID_LEN; return; }

    int dataend = msgLength - 1;
    int temp = dataend - 1;

    while(temp != 0 && !isprint(_s[temp])){
        _s[temp] = ' ';
        temp--;
    }

    if(dataend < 21) { _error = ERR_SHORT; return; }
    for(int j = 0; j < dataend; j++){
        if(isprint(_s[j]) == 0) { _error = ERR_UNRECOG_CHAR; return; }
    }

    char mid_buf[5];
    for(int j = 0; j < 4; j++){
        mid_buf[j] = _s[j];
    }
    mid_buf[4] = 0;
    if(strncmp(mid_buf, machineID, 4) != 0) { _error = ERR_WRONG_MACH_ID; return; }
    
    char ack_buf[4];
    for(int j = 0; j < 3; j++){
        ack_buf[j] = _s[j + 4];
    }
    ack_buf[3] = 0;
    if(strncmp(ack_buf, "   ", 3) != 0) { _error = ERR_BAD_MSG_RESULT; return; }

    int iSeqNum = 0;
    for (int i = 7; i < 13; i++) {
        if (!isdigit(_s[i])) { _error = ERR_BAD_SEQ_NUM; return; }
        iSeqNum = iSeqNum * 10 + _s[i] - 48;
    }

    char dataLength_buf[5];
    for(int j = 0; j < 4; j++){
        dataLength_buf[j] = _s[17 + j];
    }
    dataLength_buf[4] = 0;
    int dataLength = atoi(dataLength_buf);

    if(dataLength < dataend - 21) { 
        _error = ERR_BYTE_COUNT_HIGH;
        char buf[22 + dataLength + 1];
        for(int i = 0; i < 22 + dataLength; i++){
            buf[i] = _s[i];
        }
        buf[22 + dataLength] = 0;
        strncpy(_s, buf, 22+dataLength);
    }
    if(dataLength > dataend - 21) { _error = ERR_BYTE_COUNT_HIGH; return; }
}
