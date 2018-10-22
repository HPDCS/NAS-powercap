#ifndef STATS_T_STM_HOPE
#define STATS_T_STM_HOPE

typedef struct stats{   
    char reset_bit;                    // If set to 1, local thread should set commits to 0 and reset it to 0
    int total_commits;                 // Defined as number of commits for the current round
    int commits;                       // Number of commits in the current round
    long start_energy;                 // Value of energy consumption taken at the start of the round, expressed in micro joule
    long start_time;        		   // Start time of the current round
  } stats_t;

  #endif