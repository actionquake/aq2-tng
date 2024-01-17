#include "g_local.h"
#include <jansson.h>


qboolean ready_to_announce()
{
    if (level.framenum % 100 == 0 && level.lc_recently_sent)
        return true;
}

// qboolean announce_to_discord()
// {
//     if (ready_to_announce()) {
//         announce_server_populating();
//         level.lc_recently_sent = true;
//         return true;
//     }
// }

// qboolean evaluate_discord_announcement()
// {
//     int i;

//     for (i = NOTIFY_NONE; i < NOTIFY_MAX; i++) {
//         if (level.lc_recently_sent[i] 
//         }
        
//     }
// }