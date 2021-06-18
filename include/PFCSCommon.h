#ifndef PFCS_COMMON
#define PFCS_COMMON

#include <Arduino.h>

#include <string.h>
#include <TimeoutCallback.h>

#define ERR_NONE             0
#define ERR_SHORT            1
#define ERR_INVALID_LEN      2
#define ERR_UNRECOG_CHAR     3
#define ERR_WRONG_MACH_ID    4
#define ERR_BAD_MSG_RESULT   5
#define ERR_BAD_SEQ_NUM      6	// Sequence # is non-numeric
#define ERR_WRONG_SEQ_NUM    7	// Sequence # does not match
#define ERR_BAD_REQ_TYPE     8 
#define ERR_BYTE_COUNT_LOW   9 
#define ERR_BYTE_COUNT_HIGH	10  
#define ERR_NAK_A           11
#define ERR_NAK_B           12
#define ERR_ACK_D           13
#define ERR_NAK_E           14
#define ERR_NAK_H           15
#define ERR_NAK_I           16
#define ERR_NAK_J           17

#define STAT_NOCREATE         0
#define STAT_NOCONNECT        1
#define STAT_IDLE             2
#define STAT_SENDING          3
#define STAT_SENDINGDATA      4
#define STAT_SENDINGACK       5
#define STAT_WAITACK          6
#define STAT_WAITACKDATA      7
#define STAT_WAITDATA         8
#define STAT_RETRYNAK         9
#define STAT_WAITVERIFYDATA0001   11
#define STAT_WAITVERIFYDATA0003   12
#define STAT_NOCREATESENDERROR    13
#define STAT_NOCONNECTSENDERROR   14
#define SOL_CHAR_BUFFER_LEN		  1050 //Added on 1-08-2008

#define REQUESTDATA_FROM_HOST_TO_PFD 0001
#define TESTRESULTS_TO_PFS_FROM_PFD 0002
#define TESTRESULTS_TO_BROADCAST_FROM_PFD 0004
#define MESSAGES_FROM_ALSVS_TO_PFCS 0006
#define UNSOLICITED_FROM_HOST 0003
#define KEEP_ALIVE_FROM_PFD 9999

typedef std::function<bool(std::string)> verifyDataCallback;

void intToStr(char* dest, uint32_t value, uint32_t n);

static SemaphoreHandle_t pfcsSemaphore = xSemaphoreCreateMutex();

#endif