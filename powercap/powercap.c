#define _GNU_SOURCE

#include "powercap.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sched.h>
#include "heuristics.c"
#include <omp.h>


int set_pstate(int input_pstate){
	
	int i;
	char fname[64];
	FILE* frequency_file;
	
	if(input_pstate > max_pstate)
		return -1;
		
	if(current_pstate != input_pstate){
		int frequency = pstate[input_pstate];

		for(i=0; i<nb_cores; i++){
			sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", i);
			frequency_file = fopen(fname,"w+");
			if(frequency_file == NULL){
				printf("Error opening cpu%d scaling_setspeed file. Must be superuser\n", i);
				exit(0);		
			}		
			fprintf(frequency_file, "%d", frequency);
			fflush(frequency_file);
			fclose(frequency_file);
		}
		current_pstate = input_pstate;
	}
	return 0;
}

// Sets the governor to userspace and sets the highest frequency
int init_DVFS_management(){
	
	char fname[64];
	char* freq_available;
	int frequency, i;
	FILE* governor_file;

	//Set governor to userspace
	nb_cores = sysconf(_SC_NPROCESSORS_ONLN);
	for(i=0; i<nb_cores;i++){
		sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		governor_file = fopen(fname,"w+");
		if(governor_file == NULL){
			printf("Error opening cpu%d scaling_governor file. Must be superuser\n", i);
			exit(0);		
		}		
		fprintf(governor_file, "userspace");
		fflush(governor_file);
		fclose(governor_file);
	}

	// Init array of available frequencies
	FILE* available_freq_file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","r");
	if(available_freq_file == NULL){
		printf("Cannot open scaling_available_frequencies file\n");
		exit(0);
	}
	freq_available = malloc(sizeof(char)*256);
	fgets(freq_available, 256, available_freq_file);
	
	pstate = malloc(sizeof(int)*32);
	i = 0; 
	char * end;

	for (frequency = strtol(freq_available, &end, 10); freq_available != end; frequency = strtol(freq_available, &end, 10)){
		pstate[i]=frequency;
		freq_available = end;
  		i++;
	}
  	max_pstate = --i;

	#ifdef DEBUG_HEURISTICS
  		printf("Found %d p-states in the range from %d MHz to %d MHz\n", max_pstate, pstate[max_pstate]/1000, pstate[0]/1000);
  	#endif
  	fclose(available_freq_file);

  	current_pstate = -1;
  	set_pstate(max_pstate);

	set_boost(boost_disabled);

	return 0;
}

// Sets the governor to userspace and sets the highest frequency
int init_DVFS_management_intel_pstate_passive_mode(){
	
	char fname[64];
	char* freq_available;
	int frequency, i;
	FILE* governor_file;

	//Set governor to userspace
	nb_cores = sysconf(_SC_NPROCESSORS_ONLN);
	printf("Number of available cores: %i\n ", nb_cores);	
	for(i=0; i<nb_cores;i++){
		sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		governor_file = fopen(fname,"w+");
		if(governor_file == NULL){
			printf("Error opening cpu%d scaling_governor file. Must be superuser\n", i);
			exit(0);		
		}		
		fprintf(governor_file, "userspace");
		fflush(governor_file);
		fclose(governor_file);
	}

	// Init array of available frequencies

	pstate = malloc(sizeof(int)*32);
	i = (max_cpu_freq-min_cpu_freq)/100000+1; 
	char * end;
	printf("\nCreating Cpu frequency list with %i p-states\n",i);
	max_pstate = --i;
	frequency=min_cpu_freq;
	while(frequency<max_cpu_freq) {
		pstate[i]=frequency;
		printf(" %i",pstate[i]);
		freq_available = end;
  		frequency+=100000;
		i--;
	}
	//set pstate 0
	if (boost_disabled) frequency-=100000;
	pstate[i]=frequency;
	printf(" %i",pstate[i]);
	freq_available = end;

	printf("\nCpu frequency list completed\n");

	#ifdef DEBUG_HEURISTICS
  		printf("Created %d p-states in the range from %d MHz to %d MHz\n", max_pstate+1, pstate[max_pstate]/1000, pstate[0]/1000);
  	#endif
 
  	current_pstate = -1;
  	set_pstate(max_pstate);

	return 0;
}

// Executed inside stm_init
void init_thread_management(int threads){

	char* filename;
	FILE* numafile;
	int package_last_core;
	int i;

	// Init total threads and active threads
	total_threads = threads;

	#ifdef DEBUG_HEURISTICS
		printf("Set total_threads to %d\n", threads);
	#endif

	active_threads = total_threads;
	pthread_ids = malloc(sizeof(pthread_t)*total_threads);

	//init number of packages
	filename = malloc(sizeof(char)*64); 
	sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id", nb_cores-1);
	numafile = fopen(filename,"r");
	if (numafile == NULL){
		printf("Cannot read number of packages\n");
		exit(1);
	} 
	fscanf(numafile ,"%d", &package_last_core);
	nb_packages = package_last_core+1;

	#ifdef DEBUG_HEURISTICS
		printf("Number of packages detected: %d\n", nb_packages);
	#endif
}


// Function used to set the number of running threads. Based on active_threads and threads might wake up or pause some threads 
void set_threads(int to_threads){

	if(to_threads < 1 || to_threads > total_threads){
		printf("Setting threads/cores to %d which is invalid for this system\n", to_threads);
		exit(1);
	}

	if (core_packing) {

		#ifdef DEBUG_HEURISTICS
		printf("Packing to %d cores\n", to_threads);
		#endif
		int i;

		cpu_set_t cpu_set;       
		CPU_ZERO(&cpu_set);
		for(i=0; i<to_threads; i++){
			CPU_SET(i, &cpu_set);
		}
	
		for(i = 0; i < total_threads;i++){
			pthread_setaffinity_np(pthread_ids[i], sizeof(cpu_set_t), &cpu_set); 
		}
		
	} else {
		#ifdef DEBUG_HEURISTICS
		printf("Scheduling %d threads\n", to_threads);
		#endif
		//omp_set_dynamic(0);     // Explicitly disable dynamic teams
		omp_set_num_threads(to_threads);
	}

	active_threads = to_threads;
}

// Executed inside stm_init
void init_stats_array_pointer(int threads){

	// Allocate memory for the pointers of stats_t
	stats_array = malloc(sizeof(stats_t*)*threads); 

	cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

	#ifdef DEBUG_HEURISTICS
		printf("D1 cache line size: %d bytes\n", cache_line_size);
	#endif
}

// Executed by each thread inside stm_pre_init_thread
stats_t* alloc_stats_buffer(int thread_number){
	
	stats_t* stats_ptr = stats_array[thread_number];

	int ret = posix_memalign(((void**) &stats_ptr), cache_line_size, sizeof(stats_t));
	if ( ret != 0 ){ printf("Error allocating stats_t for thread %d\n", thread_number);
		exit(0);
	}

	stats_ptr->total_commits = total_commits_round/active_threads;
	stats_ptr->commits = 0;
	stats_ptr->start_energy = 0;
	stats_ptr->start_time = 0;

	stats_array[thread_number] = stats_ptr;

	return stats_ptr;
}


void load_config_file(){
	
	// Load config file 
	FILE* config_file;
	if ((config_file = fopen("powercap_config.txt", "r")) == NULL) {
		printf("Error opening powercap_config configuration file.\n");
		exit(1);
	}
	if (fscanf(config_file, "STARTING_THREADS=%d STATIC_PSTATE=%d POWER_LIMIT=%lf COMMITS_ROUND=%d HEURISTIC_MODE=%d DETECTION_MODE=%d EXPLOIT_STEPS=%d POWER_UNCORE=%lf MIN_CPU_FREQ=%d MAX_CPU_FREQ=%d BOOST_DISABLED=%d CORE_PACKING=%d EXTRA_RANGE_PERCENTAGE=%lf WINDOW_SIZE=%d HYSTERESIS=%lf ", 
			 &starting_threads, &static_pstate, &power_limit, &total_commits_round, &heuristic_mode, &detection_mode, &exploit_steps, &power_uncore, &min_cpu_freq, &max_cpu_freq, &boost_disabled, &core_packing, &extra_range_percentage, &window_size, &hysteresis)!=15) {
		printf("The number of input parameters of the configuration file does not match the number of required parameters.\n");
		exit(1);
	}

	if(extra_range_percentage < 0 || extra_range_percentage > 100){
	  		printf("Extra_range_percentage value is not a percentage. Should be a floating point number in the range from 0 to 100\n");
	  		exit(1);
	  	}


	if(hysteresis < 0 || hysteresis > 100){
	  	printf("Hysteresis value is not a percentage. Should be a floating point number in the range from 0 to 100\n");
	  	exit(1);
	}

	fclose(config_file);
}


// Returns energy consumption of package 0 cores in micro Joule
long get_energy(){
	
	long energy;
	int i;
	FILE* energy_file;
	long total_energy = 0;
	char fname[64];

	for(i = 0; i<nb_packages; i++){

		// Package energy consumtion
		sprintf(fname, "/sys/class/powercap/intel-rapl/intel-rapl:%d/energy_uj", i);
		energy_file = fopen(fname, "r");
		
		// Cores energy consumption
		//FILE* energy_file = fopen("/sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:0/energy_uj", "r");	

		// DRAM module, considered inside the package
		//FILE* energy_file = fopen("/sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:1/energy_uj", "r");	

		if(energy_file == NULL){
			printf("Error opening energy file\n");		
		}
		fscanf(energy_file,"%ld",&energy);
		fclose(energy_file);
		total_energy+=energy;
	}

	return total_energy;
}


// Return time as a monotomically increasing long expressed as nanoseconds 
long get_time(){
	
	long time = 0;
	struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    time += (ts.tv_sec*1000000000);
    time += ts.tv_nsec;

	return time;
}

void init_model_matrices(){

	int i;

	// Allocate the matrices
	power_model = (double**) malloc(sizeof(double*) * (max_pstate+1));
	throughput_model = (double**) malloc(sizeof(double*) * (max_pstate+1)); 

	// Allocate the validation matrices
	power_validation = (double**) malloc(sizeof(double*) * (max_pstate+1));
	throughput_validation = (double**) malloc(sizeof(double*) * (max_pstate+1)); 

	// Allocate matrices to store real values during validation
	power_real = (double**) malloc(sizeof(double*) * (max_pstate+1));
	throughput_real = (double**) malloc(sizeof(double*) * (max_pstate+1)); 

	for (i = 0; i <= max_pstate; i++){
   		power_model[i] = (double *) malloc(sizeof(double) * (total_threads));
   		throughput_model[i] = (double *) malloc(sizeof(double) * (total_threads));

   		power_validation[i] = (double *) malloc(sizeof(double) * (total_threads));
   		throughput_validation[i] = (double *) malloc(sizeof(double) * (total_threads));

   		power_real[i] = (double *) malloc(sizeof(double) * (total_threads));
   		throughput_real[i] = (double *) malloc(sizeof(double) * (total_threads));
   	}

   	// Init first row with all zeros 
   	for(i = 0; i <= max_pstate; i++){
   		power_model[i][0] = 0;
   		throughput_model[i][0] = 0;

   		power_validation[i][0] = 0;
   		throughput_validation[i][0] = 0;

   		power_real[i][0] = 0;
   		throughput_real[i][0] = 0;
   	}
}

// Initialization of global variables 
void init_global_variables(){

	#ifdef DEBUG_HEURISTICS
		printf("Initializing global variables\n");
	#endif

	round_completed=0;
	old_throughput = -1;
	old_power = -1;
	old_energy_per_tx = -1;
	level_best_throughput = -1; 
	level_best_threads = 0;
	level_starting_threads = starting_threads;
	new_pstate = 1;
	decreasing = 0;
	stopped_searching = 0;
	steps=0;
	phase = 0; 
	current_exploit_steps = 0;
	barrier_detected = 0;
	pre_barrier_threads = 0;


	high_throughput = -1;
	high_threads = -1;
	high_pstate = -1;

	best_throughput = -1;
	best_pstate = -1;
	best_threads = -1;

	low_throughput = -1;
	low_threads = -1;
	low_pstate = -1;

	net_time_sum = 0;
    net_energy_sum = 0;
    net_commits_sum = 0;
	net_aborts_sum = 0;

	net_time_slot_start= 0;
	net_energy_slot_start= 0;
    net_time_accumulator= 0;
	net_error_accumulator= 0; 
	net_discard_barrier= 0;

	min_pstate_search = 0;
	max_pstate_search = max_pstate;

	min_thread_search = 1;
	max_thread_search = total_threads;
	min_thread_search_throughput = -1;
	max_thread_search_throughput = -1;

	validation_pstate = max_pstate-1;
	#ifdef DEBUG_HEURISTICS
		printf("Global variables initialized\n");
fflush(stdout);
	#endif
}


// Used to either enable or disable boosting facilities such as TurboBoost. Boost is disabled whenever the current config goes out of the powercap 
void set_boost(int value){

	int i;
	char fname[64];
	FILE* boost_file;

	if(value != 0 && value != 1){
		printf("Set_boost parameter invalid. Shutting down application\n");
		exit(1);
	}
	
	boost_file = fopen("/sys/devices/system/cpu/cpufreq/boost", "w+");
	if(boost_file == NULL){
		printf("Error opening boost_file\n", i);
		exit(0);		
	}
	fprintf(boost_file, "%d", value);
	fflush(boost_file);
	fclose(boost_file);

	return;
}


/////////////////////////////////////////////////////////////
// EXTERNAL API
/////////////////////////////////////////////////////////////

void powercap_init(int threads){
	
	#ifdef DEBUG_HEURISTICS	
		printf("CREATE called\n");
	#endif

	load_config_file();
	init_DVFS_management();
	init_thread_management(threads);
	init_stats_array_pointer(threads);
	init_global_variables();	

  	// Necessary for the static execution in order to avoid running for the first step with a different frequency than manually set in hope_config.txt
  	if(heuristic_mode == 8){
  		if(static_pstate >= 0 && static_pstate <= max_pstate)
  			set_pstate(static_pstate);
  		else 
  			printf("The parameter manual_pstate is set outside of the valid range for this CPU. Setting the CPU to the slowest frequency/voltage\n");
  	}else if(heuristic_mode == 12 || heuristic_mode == 13 || heuristic_mode == 15){
  		set_pstate(max_pstate);
  		starting_threads = 1;
  	}

	if(heuristic_mode == 15)
		init_model_matrices();

	#ifdef DEBUG_HEURISTICS
		printf("Heuristic mode: %d\n", heuristic_mode);
	#endif

	if(starting_threads > total_threads){
		printf("Starting threads set higher than total threads. Please modify this value in hope_config.txt\n");
		exit(1);
	}
} 


void powercap_init_thread(){


	thread_number_init = 1;
	int id = __atomic_fetch_add(&thread_counter, 1, __ATOMIC_SEQ_CST);
	stats_ptr = alloc_stats_buffer(id);
	
	// Initialization of stats struct
	stats_ptr->reset_bit = 0;
	stats_ptr->total_commits = total_commits_round;

	thread_number = id;

	pthread_ids[id]=pthread_self();

	__atomic_fetch_add(&initialized_thread_counter, 1, __ATOMIC_SEQ_CST);

	// Wait for all threads to get initialized
	while(initialized_thread_counter < total_threads){}
	
	#ifdef DEBUG_HEURISTICS
		if(id == 0){
			printf("Initialized all thread ids\n");
			fflush(stdout);
		}
	#endif



	// Thread 0 sets itself as a collector and inits global variables or init global variables if lock based
	if(id == 0){

		// Set active_threads to starting_threads
		set_threads(starting_threads);
		net_time_slot_start = get_time();

		net_energy_slot_start = get_energy();
		stats_ptr->start_time = net_time_slot_start;
		stats_ptr->start_energy = net_energy_slot_start;
	}


} 


// Function called before taking a lock
void powercap_lock_taken(){
	
	/*
	// At first run should initialize thread and get thread number
	if(thread_number_init == 0){
		powercap_init_thread();
	}

	if(thread_number == 0 && stats_ptr->commits >= stats_ptr->total_commits){

		//Aggregate data and set reset_bits to 1 for all threads
		double throughput, power;	// Expressed as critical sections per second and Watts respectively
		long end_time_slot, end_energy_slot, time_interval, energy_interval;

		double commits_sum = 0;
		end_time_slot = get_time();
		end_energy_slot = get_energy();


		for(int i=0; i<total_threads; i++){
				if(stats_array[i]->reset_bit == 0){
					commits_sum += stats_array[i]->commits;
					stats_array[i]->reset_bit = 1;		
				}
		}

		time_interval = end_time_slot - stats_ptr->start_time; //Expressed in nano seconds 
		energy_interval = end_energy_slot - stats_ptr->start_energy; // Expressed in micro Joule
		throughput = ((double) commits_sum) / (((double) time_interval)/ 1000000000);
		power = ((double) energy_interval) / (((double) time_interval)/ 1000);

		//Update counters for computing the powercap error with 1 second granularity
		long slot_time_passed = end_time_slot - net_time_slot_start;

		if(slot_time_passed > 1000000000){ //If higher than 1 second update the accumulator with the value of error compared to power_limit
			if(net_discard_barrier == 0){
				long slot_energy_consumed = end_energy_slot - net_energy_slot_start;
				double slot_power = (((double) slot_energy_consumed)/ (((double) slot_time_passed)/1000));

				double error_signed = slot_power - power_limit;
				double error = 0;
				if(error_signed > 0)
					error = error_signed/power_limit*100;

				// Add the error to the accumulator
				net_error_accumulator = (net_error_accumulator*((double)net_time_accumulator)+error*((double)slot_time_passed))/( ((double)net_time_accumulator)+( (double) slot_time_passed));
				net_time_accumulator+=slot_time_passed;
			}else{
				net_discard_barrier = 0;
			}
						
			//Reset start counters
			net_time_slot_start = end_time_slot;
			net_energy_slot_start = end_energy_slot;
		}

		// Call heuristics if should not discard sampling
		if(barrier_detected == 1){
			barrier_detected = 0;
		}
		else{
			// We don't call the heuristic if the energy results are out or range due to an overflow 
			if(power > 0){
				net_time_sum += time_interval;
				net_energy_sum += energy_interval;
				net_commits_sum += commits_sum;

				heuristic(throughput, power, time_interval);
			}
		}

		//Setup next round
		stats_ptr->start_energy = get_energy();
		stats_ptr->start_time = get_time();
	}*/
}

// Function called after releasing a lock
void powercap_lock_release(){

	/*
	if(stats_ptr->reset_bit == 1){
		stats_ptr->commits = 1;
		stats_ptr-> reset_bit = 0;
	} else{
		stats_ptr->commits++;
	}*/
}

// Called before a barrier, must wake-up all threads to avoid a deadlock
void powercap_before_barrier(){

	if(thread_number == 0 && thread_number_init == 1) {
		
		/*
		#ifdef DEBUG_HEURISTICS
			printf("Powercap_before_barrier - active_thread %d\n", active_threads);
		#endif*/
			
		// Next decision phase should be dropped
		//barrier_detected = 1;

		// Dont consider next slot for power_limit error measurements
		//net_discard_barrier = 1;
	}
}

void powercap_after_barrier(){

	/*#ifdef DEBUG_HEURISTICS
		printf("Powercap_after_barrier - active_thread %d\n", active_threads);
	#endif
	if(thread_number == 0){
		if(stats_ptr->reset_bit == 1){
                	stats_ptr->commits = 1;
                	stats_ptr-> reset_bit = 0;
       		} else{
                	stats_ptr->commits++;
		}
	}
	*/

}

void powercap_before_cond_wait(){

	if(thread_number == 0 && thread_number_init == 1) {
			
		// Next decision phase should be dropped
		//barrier_detected = 1;


		// Dont consider next slot for power_limit error measurements
		//net_discard_barrier = 1;
	}

	/*
	#ifdef DEBUG_HEURISTICS
		printf("powercap_before_cond_wait() called\n");
	#endif
	*/
}

void powercap_after_cond_wait(){
	
	/*#ifdef DEBUG_HEURISTICS
		printf("powercap_after_cond_wait() called\n");
	#endif*/
}

void powercap_commit_work(){

	stats_ptr->commits++;

	if(stats_ptr->commits >= stats_ptr->total_commits){

		//Aggregate data and set reset_bits to 1 for all threads
		double throughput, power;	// Expressed as critical sections per second and Watts respectively
		long end_time_slot, end_energy_slot, time_interval, energy_interval;

		double commits_sum = (double) stats_ptr->commits;
		end_time_slot = get_time();
		end_energy_slot = get_energy();


		time_interval = end_time_slot - stats_ptr->start_time; //Expressed in nano seconds 
		energy_interval = end_energy_slot - stats_ptr->start_energy; // Expressed in micro Joule
		throughput = ((double) commits_sum) / (((double) time_interval)/ 1000000000);
		power = ((double) energy_interval) / (((double) time_interval)/ 1000);

		//Update counters for computing the powercap error with 1 second granularity
		long slot_time_passed = end_time_slot - net_time_slot_start;

		if(slot_time_passed > 1000000000){ //If higher than 1 second update the accumulator with the value of error compared to power_limit
			if(net_discard_barrier == 0){
				long slot_energy_consumed = end_energy_slot - net_energy_slot_start;
				double slot_power = (((double) slot_energy_consumed)/ (((double) slot_time_passed)/1000));

				double error_signed = slot_power - power_limit;
				double error = 0;
				if(error_signed > 0)
					error = error_signed/power_limit*100;

				// Add the error to the accumulator
				net_error_accumulator = (net_error_accumulator*((double)net_time_accumulator)+error*((double)slot_time_passed))/( ((double)net_time_accumulator)+( (double) slot_time_passed));
				net_time_accumulator+=slot_time_passed;
			}else{
				net_discard_barrier = 0;
			}
						
			//Reset start counters
			net_time_slot_start = end_time_slot;
			net_energy_slot_start = end_energy_slot;
		}

		// Call heuristics if should not discard sampling
		if(barrier_detected == 1 || first_commit == 0){
			barrier_detected = 0;
			first_commit = 1;
		}
		else{
			// We don't call the heuristic if the energy results are out or range due to an overflow 
			if(power > 0){
				net_time_sum += time_interval;
				net_energy_sum += energy_interval;
				net_commits_sum += commits_sum;

				heuristic(throughput, power, time_interval);
			}
		}

		//Setup next round
		stats_ptr->start_energy = get_energy();
		stats_ptr->start_time = get_time();
		stats_ptr->commits = 0;
	}
}




void powercap_print_stats(){

extern char *__progname;

char fileName[32];

	if (heuristic_mode==8)
		sprintf(fileName, "%s-%i-%i.txt", __progname, current_pstate, active_threads);
	else 
		sprintf(fileName, "%s-%i-%i.txt", __progname, heuristic_mode, (int)power_limit);
	
	printf ("\nWrinting stats to file: %s\n", fileName);
	fflush(stdout);
	
	int fd = fopen(fileName, "a");
	if(fd==NULL) {
		printf("\nError opening output file. Exiting...\n");
    		exit(1);
  	}


	#ifdef PRINT_STATS
	  	double time_in_seconds = ( (double) net_time_sum) / 1000000000;
	  	double net_throughput =  ( (double) net_commits_sum) / time_in_seconds;
	  	double net_avg_power = ( (double) net_energy_sum) / (( (double) net_time_sum) / 1000);

	  	fprintf(fd,"Net_runtime: %lf\tNet_throughput: %lf\tNet_power: %lf\tNet_commits: %ld\tNet_error: %lf\n",time_in_seconds, net_throughput, net_avg_power, net_commits_sum, net_error_accumulator);

  	#endif
	fclose(fd);
  }
