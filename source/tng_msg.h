//
//tng_msg.c
//
void Write_MsgToLog(int logType, const char* msg, ...);

#define STAT_LOG    0
#define CHAT_LOG    1
#define SERVER_LOG  2
#define VOTE_LOG    3