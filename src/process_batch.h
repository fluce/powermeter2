#ifndef PROCESS_BATCH_H
#define PROCESS_BATCH_H

#include "channel.h"

void prepare_new_batch(channel_batch_t* batch);
void process_batch(channel_info_t *channel, channel_batch_t *channel_batch, channel_index_t *channel_index, channel_global_t *channel_global);

#endif
