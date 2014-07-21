
void quality_display_on_stream_destructor(struct stream *s);

int check_time_rx(void);
int check_time_tx(void);

void rtcp_store_stat_rx(void);
void rtcp_store_stat_tx(void);

extern uint32_t tick_gap_rx;
extern uint32_t tick_gap_tx;

void rtcp_info(struct rtcp_stats *rs);
void stream_rtcp_info(struct stream *s);

/* in milliseconds */
#define msg_queue_time 10240
#define msg_q_l  256
