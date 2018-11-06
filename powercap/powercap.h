#ifndef __POWERCAP_HEADER
#define __POWERCAP_HEADER

#include "stats_t.h"
#include "macros.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////////////////////////////////////

pthread_t* pthread_ids; // Array of pthread id's to be used with signals
int total_threads;				// Total number of threads that could be used by the transcational operation 
volatile int active_threads;	// Number of currently active threads, reflects the number of 1's in running_array
int nb_cores; 					// Number of cores. Detected at startup and used to set DVFS parameters for all cores
int nb_packages;				// Number of system package. Necessary to monitor energy consumption of all packages in the system
int cache_line_size;			// Size in byte of the cache line. Detected at startup and used to alloc memory cache aligned 
int* pstate;					// Array of p-states initialized at startup with available scaling frequencies 
int max_pstate;					// Maximum index of available pstate for the running machine 
int current_pstate;				// Value of current pstate, index of pstate array which contains frequencies
int steps;						// Number of steps required for the heuristic to converge 
stats_t** stats_array;			// Pointer to pointers of struct stats_s, one for each thread 	
volatile int round_completed;   // Defines if round completed and thread 0 should collect stats and call the heuristic function
volatile int thread_counter;	// Global variable used for assigning an increasing counter to threads
volatile int initialized_thread_counter;	// Global variable used for syncronizing threads during initialization
volatile int running_token;		// Token used to manage sleep after barrier. Necessary to manage situations where signals arrive before thread is actually back to pausing
volatile int cond_waiters;		// Used to manage phread conditional wait. It's incremented and decremented atomically
int first_commit;			// Used to filter out the first commit

// powercap_config.txt variables
int starting_threads;			// Number of threads running at the start of the exploration
int static_pstate;				// Static -state used for the execution with heuristic 8
double power_limit;				// Maximum power that should be used by the application expressed in Watt
int total_commits_round; 		// Number of total commits for each heuristics step 
int heuristic_mode;				// Used to switch between different heuristics mode. Check available values in heuristics.  
int detection_mode; 			// Defines the detection mode. Value 0 means detection is disabled. 1 restarts the exploration from the start. Detection mode 2 resets the execution after a given number of steps
int exploit_steps;				// Number of steps that should be waited until the next exploration is started
double power_uncore;			// System specific parameter that defines the amount of power consumption used by the uncore part of the system, which we consider to be constant
int min_cpu_freq;			// Minimum cpu frequency (in KHz)	
int max_cpu_freq;			// Maximum cpu frequency (in KHz)
int boost_disabled;			// Disable turbo-boost 
int core_packing;			// 0-> threads scheduling, 1 -> core packing
double extra_range_percentage;	// Defines the range in percentage over power_limit which is considered valid for the HIGH and LOW configurations. Used by dynamic_heuristic1. Defined in hope_config.txt
int window_size; 				// Defines the lenght of the window, defined in steps, that should achieve a power consumption within power_limit. Used by dynamic_heuristic1. Defined in hope_config.txt 
double hysteresis;				// Defines the amount in percentage of hysteresis that should be applied when deciding the next step in a window based on the current value of window_power. Used by dynamic_heuristic1. Defined in hope_config.txt

// Variable specific to NET_STATS
long net_time_sum;
long net_energy_sum;
long net_commits_sum;
long net_aborts_sum;

// Variables necessary to compute the error percentage from power_limit, computed once every seconds 
long net_time_slot_start;
long net_energy_slot_start;
long net_time_accumulator;
double net_error_accumulator; 
long net_discard_barrier;

// Variables necessary for the heuristics
double old_throughput;			
double old_power;			
double old_abort_rate; 		
double old_energy_per_tx;	
double best_throughput;
int current_exploit_steps;		// Current number of steps since the last completed exploration
int best_threads;				
int best_pstate;	
double best_power;			
double level_best_throughput; 
int level_best_threads;
int level_best_pstate;
int level_starting_threads;
int level_starting_energy_per_tx;
int phase0_pstate;
int phase0_threads;
int new_pstate;					// Used to check if just arrived to a new p_state in the heuristic search
int decreasing;					// If 0 heuristic should remove threads until it reaches the limit  
int stopped_searching;			// While 1 the algorithm searches for the best configuration, if 0 the algorithm moves to monitoring mode 
int phase;						// The value of phase has different semantics based on the running heuristic mode
int min_pstate_search;
int max_pstate_search;
int min_thread_search;
int max_thread_search;
double min_thread_search_throughput;
double max_thread_search_throughput;

// Variables specific to dynamic_heuristic1 
double high_throughput;
int high_pstate;
int high_threads; 
double high_power;
double low_throughput; 
int low_pstate;
int low_threads; 
double low_power;
int current_window_slot;		// Current slot within the window
double window_time;				// Expressed in nano seconds. Defines the current sum of time passed in the current window of configuration fluctuation
double window_power; 			// Expressed in Watt. Current average power consumption of the current fluctuation window
int fluctuation_state;			// Defines the configuration used during the last step, -1 for LOW, 0 for BEST, 1 for HIGH


// Model-based variables
// Matrices of predicted power consumption and throughput for different configurations. 
// Rows are p-states, columns are threads. It has total_threads+1 column as first column is filled with 0s 
// since it is not meaningful to run with 0 threads.
double** power_model; 
double** throughput_model;
double** power_validation; 
double** throughput_validation;
double** power_real; 
double** throughput_real;
int validation_pstate;	// Variable necessary to validate the effectiveness of the models

// Barrier detection variables
int barrier_detected; 			// If set to 1 should drop current statistics round, had to wake up all threads in order to overcome a barrier 
int pre_barrier_threads;	    // Number of threads before entering the barrier, should be restored afterwards

// Debug variables
long lock_counter; 

////////////////////////////////////////////////////////////////////////
// THREAD LOCAL VARIABLES
////////////////////////////////////////////////////////////////////////

static volatile __thread int thread_number;			// Number from 0 to Max_thread to identify threads inside the application
static volatile __thread int thread_number_init; 	// Used at each lock request to check if thread id of the current thread is already registered
static volatile __thread stats_t* stats_ptr;		// Pointer to stats struct for the current thread. This allows faster access

////////////////////////////////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////////////////////////////////

void powercap_lock_taken(void);
void powercap_lock_release(void);
void powercap_init(int);
void powercap_init_thread(void);
void powercap_before_barrier(void);
void powercap_after_barrier(void);
void powercap_before_cond_wait(void);
void powercap_after_cond_wait(void);
void powercap_print_stats(void);
void powercap_commit_work(void);

// Functions used by heuristics
void set_threads(int);
int set_pstate(int);


#endif
