#ifndef PFCS_MESSAGE
#define PFCS_MESSAGE

class PFCSMessage{
private:
    byte _error;
    char _s[SOL_CHAR_BUFFER_LEN];
public:
    PFCSMessage();
    PFCSMessage(const char* initialMsg);

    void makeMsg(const char* machineID, int seq_num, const char* cs_reqtype, const char* pdata);
    void makekeepalive(const char* machineID, int seq_num, const char* pdata);
    void makeack(const char* machineID, const char* ack, int seq_num, const char* req_type, char error);
    int getLength();
    char* getBuffer();
    void getMachName(char* dest);
    void getReqType(char* dest);
    void getMsgType(char* dest);
    int getReqVal();
    uint32_t getSeqNum();
    int getError();
    void setBuffer(char* newBuffer, size_t bufLength);
    void setSeqNum(int seq_num);
    void setReqType(int req_type);
    void setMachName(const char* machineID);
    void checkACKMsg(const char* machineID, int seq_num, int req_type);
    void checkDataMsg(const char* machineID);
    void pureData(char* buf);
    void add(PFCSMessage msgin);
};

#endif