#include "powercap.h"
#include "math.h"

///////////////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////////////

// Checks if the current config is better than the currently best config and if that's the case update it 
void update_best_config(double throughput, double power){
	
	if(throughput > best_throughput || (best_threads == active_threads && current_pstate <= best_pstate)){
		best_throughput = throughput;
		best_pstate = current_pstate;
		best_threads = active_threads;
		best_power = power;
	}
}

int update_level_best_config(double throughput){
	if(throughput > level_best_throughput){
		level_best_throughput = throughput;
		level_best_threads = active_threads;
		level_best_pstate = current_pstate;
		return 1;
	}
	else return 0;
}

void compare_best_level_config(){
	if(level_best_throughput > best_throughput){
		best_throughput = level_best_throughput;
		best_pstate = level_best_pstate;
		best_threads = level_best_threads;
	}
}

// Stop searching and set the best configuration 
void stop_searching(){

	decreasing = 0;
	new_pstate = 1;
	stopped_searching = 1;

	level_best_throughput = 0;
	level_best_threads = 0;
	level_best_pstate = 0;

	phase0_threads = -1;
	phase0_pstate = -1;
	phase = 0;

	current_exploit_steps = 0;

	if(best_throughput == -1){
		best_threads = 1;
		best_pstate = max_pstate;
	}

	if(heuristic_mode == 15 && detection_mode == 3){
		set_pstate(validation_pstate);
		set_threads(1);
	}else{
		set_pstate(best_pstate);
		set_threads(best_threads);
    }

	#ifdef DEBUG_HEURISTICS
		printf("EXPLORATION COMPLETED IN %d STEPS. OPTIMAL: %d THREADS P-STATE %d\n", steps, best_threads, best_pstate);
	#endif

	steps = 0; 
}




// Helper function for dynamic heuristic0, called in phase 0
 static void from_phase0_to_next(){
	
	if(best_throughput > 0){
		phase0_threads = best_threads;
		phase0_pstate = current_pstate;
	}
	else{
		phase0_threads = 1;
		phase0_pstate = current_pstate;
	}

	if(current_pstate == 0){
		if(best_threads == total_threads){
			#ifdef DEBUG_HEURISTICS
				printf("PHASE 0 -> END\n");
			#endif
			stop_searching();
		}
		else{
			phase = 2; 

			if(best_throughput > 0){
				set_threads(best_threads);
				set_pstate(current_pstate+1);
			}

			level_best_throughput = 0;
			level_best_threads = 0;
			level_best_pstate = 0;
			#ifdef DEBUG_HEURISTICS
				printf("PHASE 0 -> PHASE 2\n");
			#endif
		}
	}else{
		phase = 1;
		if(best_throughput > 0){
			set_threads(best_threads);
			set_pstate(current_pstate-1);
		}
		#ifdef DEBUG_HEURISTICS
				printf("PHASE 0 -> PHASE 1\n");
		#endif
	}
}

// Helper function for dynamic heuristic0, called in phase 1
static void from_phase1_to_next(){
	// Check if should move to phase 0 or should stop searching 
	if(phase0_pstate == max_pstate || best_threads == total_threads || phase0_threads == total_threads){
		#ifdef DEBUG_HEURISTICS
				printf("PHASE 1 -> END\n");
		#endif
		stop_searching();
	}
	else{
		phase = 2;
		if(phase0_threads > 0){
			set_threads(phase0_threads);
			set_pstate(phase0_pstate+1);
		}
		level_best_threads = 0;
		level_best_throughput = 0;
		level_best_pstate = 0;
		#ifdef DEBUG_HEURISTICS
				printf("PHASE 1 -> PHASE 2\n");
		#endif
	}
}


// Baseline dynamic exploration. In phase 0 the explorations looks for the number of threads that provides the best performance. 
// In phase 1 the algorithm iteratively decreases the p-state and seeks for the first config within the powercap at each frequency/voltage.
// The exploration at the next p_state always starts from the number of threads found optimal from the previous performance state and only decreases it if needed 
void dynamic_heuristic0(double throughput, double power){
	
	if(phase == 0){	// Thread scheduling at lowest frequency
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if(steps == 0){ // First exploration step
			if(active_threads != total_threads && power < power_limit){
				set_threads(active_threads+1);
			}
			else if(active_threads > 1){
				decreasing = 1;
				set_threads(active_threads-1);
				
				#ifdef DEBUG_HEURISTICS
					printf("PHASE 0 - DECREASING\n");
				#endif
			}
			else{ //Starting from 1 thread and cannote increase 
				from_phase0_to_next();
			}
		}
		else if(steps == 1 && !decreasing){ //Second exploration step, define if should set decreasing 
			if(throughput >= best_throughput*0.9 && power < power_limit && active_threads != total_threads){
				set_threads(active_threads+1);
			} else{ // Should set decreasing to 0 
				if(starting_threads > 1){
					decreasing = 1; 
					set_threads(starting_threads-1);	

					#ifdef DEBUG_HEURISTICS
						printf("PHASE 0 - DECREASING\n");
					#endif
				}
				else{ // Cannot reduce number of thread more as starting_thread is already set to 1 
					from_phase0_to_next();
				}
			}
		} 
		else if(decreasing){ // Decreasing threads  
			if(throughput < best_throughput*0.9 || active_threads == 1)
				from_phase0_to_next();
			else
				set_threads(active_threads-1);
			
		} else{ // Increasing threads
			if( power > power_limit || active_threads == total_threads || throughput < best_throughput*0.9){
				if(starting_threads > 1){
					decreasing = 1; 
					set_threads(starting_threads-1);	

					#ifdef DEBUG_HEURISTICS
						printf("PHASE 0 - DECREASING\n");
					#endif
				}
				else{ // Cannot reduce number of thread more as starting_thread is already set to 1 
					from_phase0_to_next();
				}
			}
			else set_threads(active_threads+1);
		}
	}
	else if(phase == 1){ //Increase in performance states while being within the power cap
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if( (power < power_limit && current_pstate == 0) || (power > power_limit && active_threads == 1) )
			from_phase1_to_next();
		
		else{ // Not yet completed to explore in phase 1 
			if(power < power_limit) //Should decrease number of active threads
				set_pstate(current_pstate-1);
			else // Power beyond power limit
				set_threads(active_threads-1);
		}	
	}
	else{ // Phase == 2. Decreasing the CPU frequency and increasing the number of threads. Not called in the first exploration phase 
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if( (current_pstate == max_pstate && power>power_limit) || (current_pstate == max_pstate && active_threads == total_threads) || throughput < level_best_throughput || (active_threads == total_threads && power<power_limit ))
			stop_searching();
		else{ // Should still explore in phase 2 
			if(power < power_limit){
				update_level_best_config(throughput);
				set_threads(active_threads+1);
			}
			else{ // Outside power_limit, should decrease P-state and reset level related data
				set_pstate(current_pstate+1);
				level_best_threads = 0;
				level_best_throughput = 0;
				level_best_pstate = 0;
			}
		}
	}
}

void update_highest_threads(double throughput, double power){
	
	if( power < power_limit){
		if(active_threads == best_threads){
			if(best_pstate == -1 || current_pstate < best_pstate){
				best_throughput = throughput;
				best_threads = active_threads;
				best_pstate = current_pstate;
			}
		}
		else if( active_threads > best_threads){
			best_throughput = throughput;
			best_threads = active_threads;
			best_pstate = current_pstate;
		}					
	}
}

// Heuristic similar to to Cap and Pack. Used as a comparison. Consider the best config the one with the most active threads. Within the same number of threads the one within the cap with the highest frequency is selected 
void heuristic_highest_threads(double throughput, double power){
	
	update_highest_threads(throughput, power);

	if(phase == 0){	// Find the highest number of threads at the lowest P-state
		if(steps == 0){
			if(active_threads == total_threads || power > power_limit){
				decreasing = 1; 
				set_threads(active_threads-1);
			}
			else 
				set_threads(active_threads+1);
		}else{
			if(decreasing){
				if(power < power_limit){
					if(best_pstate != 0){
						phase = 1; 
						set_threads(best_threads);
						set_pstate(best_pstate-1);
					}else stop_searching();
				} else set_threads(active_threads-1);
			}else{	//Increasing
				if( power > power_limit || active_threads == total_threads){
					phase = 1;
					set_threads(best_threads);
					set_pstate(best_pstate-1);
				} else set_threads(active_threads+1);
			}
		}
	}
	else if (phase == 1){
		if(power > power_limit || current_pstate == 0 )
			stop_searching();
		else set_pstate(current_pstate-1);
	}
}
	

// Explore the number active threads and DVFS settings indepedently. Implemented as a comparison to a state-of-the-art solution that considers the different power management knobs independently which might be sub-optimal. 
// When this policy is set, the exploration starts with 1 thread at the maximum p-state
void heuristic_binary_search(double throughput, double power){

	if(phase == 0){ // Thread tuning

		// First two steps should check performance results with lowest number of active threads (1) and highest. If the latter performs worse than then former should directly move to DVFS tuning
		if(steps == 0){
			min_thread_search_throughput = throughput;
			set_threads(total_threads);
			steps++;
		}else if(steps == 1){
			max_thread_search_throughput = throughput;
			if(max_thread_search_throughput < min_thread_search_throughput){
				phase = 1;
				set_threads(1);
				set_pstate((int) max_pstate/2);

				#ifdef DEBUG_HEURISTICS
					printf("PHASE 0 --> PHASE 1\n");
				#endif

			}else set_threads(min_thread_search+((int) (max_thread_search - min_thread_search) /2));
		}else{ 	
			if(min_thread_search >= max_thread_search){ // Stop the binary search on threads and move on dvfs
				phase = 1; 
				set_pstate((int) max_pstate/2);

				#ifdef DEBUG_HEURISTICS
					printf("PHASE 0 --> PHASE 1\n");
				#endif

			}else{ // Keep searching
				if(power > power_limit || throughput > max_thread_search_throughput){ // Should set current to high
					max_thread_search = active_threads;
					max_thread_search_throughput = throughput; 
				}else{ // Should set current to low 
					min_thread_search = active_threads;
					min_thread_search_throughput = throughput; 
				}
				set_threads(min_thread_search+((int) (max_thread_search - min_thread_search) /2));
			}
		}
		
		#ifdef DEBUG_HEURISTICS
			printf("SEARCH RANGE - THREADS: %d - %d \n", min_thread_search, max_thread_search);
		#endif

	}else{ // DVFS tuning, phase == 1 

		if(min_pstate_search >= max_pstate_search){
			
			#ifdef DEBUG_HEURISTICS
					printf("PHASE 1 --> END\n");
			#endif


			update_best_config(throughput, power);
			stop_searching();
		}else{ 	// Decreasing the p-state always improves performance
			if(power < power_limit) 
				max_pstate_search = current_pstate;
			else min_pstate_search = current_pstate;

			set_pstate(min_pstate_search+( (int) ceil(((double) max_pstate_search - (double) min_pstate_search)/2)));
		}

		#ifdef DEBUG_HEURISTICS
			printf("SEARCH RANGE - P-STATE: %d - %d \n", min_pstate_search, max_pstate_search);
		#endif
	}
}

// In phase 1 this heuristic first explores in the direction of increased active threads then,
// if either performance decreases or the power consumption reaches the power limit, it to phase 2 where the DVFS setting, for that given amount of active threads, is tuned
void heuristic_two_step_search(double throughput, double power){

	if(power<power_limit){
		update_best_config(throughput,power);
	}

	if(phase == 0){ // Searching threads
		if(power<power_limit && active_threads < total_threads && throughput > best_throughput*0.9){
			set_threads(active_threads+1);
		}else{
			if(best_throughput != -1){
				set_threads(best_threads);
			}
			set_pstate(current_pstate-1);
			phase = 1; 
		}
	}else{ // Phase == 1. Optimizing DVFS
		if(power<power_limit && current_pstate > 0){
			set_pstate(current_pstate-1);
		}
		else{
			stop_searching();
		}
	}
}


// Helper function for dynamic heuristic0, called in phase 0
static void from_phase0_to_next_stateful(){

	phase == 1;

	if(best_throughput == -1){
		set_pstate(current_pstate+1);
	}else{

		if(current_pstate == 0)
			stop_searching();
		else{
			set_threads(best_threads);
			set_pstate(current_pstate-1);
		}
	} 
}

// Equivalent to heuristic_two_step_search but when the exploration is restarted it starts from the previous best configuration instead of 1 Thread at lowest DVFS setting. 
void heuristic_two_step_stateful(double throughput, double power){
	
	if(phase == 0){	// Thread scheduling at lowest frequency
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if(steps == 0){ // First exploration step
			if(active_threads != total_threads && power < power_limit){
				set_threads(active_threads+1);
			}
			else if(active_threads > 1){
				decreasing = 1;
				set_threads(active_threads-1);
				
				#ifdef DEBUG_HEURISTICS
					printf("PHASE 0 - DECREASING\n");
				#endif
			}
			else{ //Starting from 1 thread and cannote increase 
				from_phase0_to_next_stateful();
			}
		}
		else if(steps == 1 && !decreasing){ //Second exploration step, define if should set decreasing 
			if(throughput >= best_throughput*0.9 && power < power_limit && active_threads != total_threads){
				set_threads(active_threads+1);
			} else{ // Should set decreasing to 0 
				if(starting_threads > 1){
					decreasing = 1; 
					set_threads(starting_threads-1);	

					#ifdef DEBUG_HEURISTICS
						printf("PHASE 0 - DECREASING\n");
					#endif
				}
				else{ // Cannot reduce number of thread more as starting_thread is already set to 1 
					from_phase0_to_next_stateful();
				}
			}
		} 
		else if(decreasing){ // Decreasing threads  
			if(throughput < best_throughput*0.9 || active_threads == 1)
				from_phase0_to_next_stateful();
			else
				set_threads(active_threads-1);
			
		} else{ // Increasing threads
			if( power > power_limit || active_threads == total_threads || throughput < best_throughput*0.9){
				if(starting_threads > 1){
					decreasing = 1; 
					set_threads(starting_threads-1);	

					#ifdef DEBUG_HEURISTICS
						printf("PHASE 0 - DECREASING\n");
					#endif
				}
				else{ // Cannot reduce number of thread more as starting_thread is already set to 1 
					from_phase0_to_next_stateful();
				}
			}
			else set_threads(active_threads+1);
		}
	}
	else{ // Phase == 1 -> Increase DVFS until reaching the powercap
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if( (power < power_limit && current_pstate == 0) || (power > power_limit && active_threads == 1) ){
		
			#ifdef DEBUG_HEURISTICS
				printf("PHASE 1 - END\n");
			#endif
			
			stop_searching();;
		} else{ // Not yet completed to explore in phase 1 
			if(power < power_limit)
				set_pstate(current_pstate-1);
			else{

				if(best_throughput == -1)
					set_pstate(current_pstate+1);
				else{
					#ifdef DEBUG_HEURISTICS
						printf("PHASE 1 - END\n");
					#endif

					stop_searching();
				}
				
			} 
		}	
	}
}

void compute_power_model(){

	double alfa, beta, pwr_h, pwr_l, freq_h, freq_l, freq_i, freq3_h, freq3_l;
	int i,j; 

	freq_h = ((double) pstate[1])/1000;
	freq_l = ((double) pstate[max_pstate])/1000;

	freq3_h = freq_h*freq_h*freq_h;
	freq3_l = freq_l*freq_l*freq_l;

	// Must compute specific model instance for each number of active threads
	for(j = 1; j <= total_threads; j++){
		
		pwr_h = power_model[1][j] - power_uncore;
		pwr_l = power_model[max_pstate][j] - power_uncore;
		
		alfa = (pwr_l*freq_h - pwr_h*freq_l)/(freq_h*freq3_l - freq3_h*freq_l);
		beta = (pwr_l*freq_h*freq_h*freq_h - pwr_h*freq_l*freq_l*freq_l)/(freq_h*freq_h*freq_h*freq_l - freq_h*freq_l*freq_l*freq_l);

		#ifdef DEBUG_HEURISTICS
			printf("Setting up the power model ...\n");
			printf("Threads = %d - alfa = %lf - beta = %lf - power uncore = %lf - pwr_h = %lf - pwr_l = %lf - freq_h %lf - freq3_h %lf\n", 
				j, alfa, beta, power_uncore, pwr_h, pwr_l, freq_h, freq3_h);
		#endif
		
		for(i = 2; i < max_pstate; i++){
			freq_i = ((double) pstate[i])/1000;
			power_model[i][j] = alfa*freq_i*freq_i*freq_i+beta*freq_i+power_uncore; 
		}
		
	}

	#ifdef DEBUG_HEURISTICS
		for(i = 1; i <= max_pstate; i++){
			for (j = 1; j <= total_threads; j++){
				printf("%lf\t", power_model[i][j]);
			}
			printf("\n");
		}
		printf("Setup of the power model completed\n");
	#endif
}

void compute_throughput_model(){

	double c,m,speedup; 
	int i,j; 

	// Must compute specific model instance for each number of active threads
	for(j = 1; j <= total_threads; j++){
		speedup = throughput_model[1][j]/throughput_model[max_pstate][j];
		c = (pstate[1]*(1-speedup))/(speedup*(pstate[max_pstate]-pstate[1]));
		m = 1-c;

		#ifdef DEBUG_HEURISTICS
			printf("Setting up the throughput model ...\n");
			printf("Threads = %d - C = %lf - M = %lf - speedup = %lf\n", j, c, m, speedup);
		#endif
		
		for(i = 2; i < max_pstate; i++)
			throughput_model[i][j] = (1/(((double) pstate[max_pstate] * 1000)/((double) pstate[i] * 1000)*c+m))*throughput_model[max_pstate][j];
		
	}

	#ifdef DEBUG_HEURISTICS
		for(i = 1; i <= max_pstate; i++){
			for (j = 1; j <= total_threads; j++){
				printf("%lf\t", throughput_model[i][j]);
			}
			printf("\n");
		}
		printf("Setup of the throughput model completed\n");
	#endif
}


// Relies on power and performance models to predict power and performance.The setup of the models
// require to sample power and performance of all configurations with P-state = 1 and P-state = max_pstate.
// In the initial phase, the setup is performed. Consequently, the models are used to selects the best configuration
// under the power cap based on their predictions. 
void model_power_throughput(double throughput, double power){

	int i, j;

	power_model[current_pstate][active_threads] = power;
	throughput_model[current_pstate][active_threads] = throughput;

	if(active_threads == total_threads && current_pstate == 1){
		
		compute_power_model();
		compute_throughput_model();

		// Init best_threads and best_pstate
		best_threads = 1;
		best_pstate = max_pstate;
		best_throughput = throughput_model[max_pstate][1];

		for(i = 1; i <= max_pstate; i++){
			for(j = 1; j <= total_threads; j++){
				if(power_model[i][j] < power_limit && throughput_model[i][j] > best_throughput){
					best_pstate = i;
					best_threads = j;
					best_throughput = throughput_model[i][j];
				}
			}
		}
		
		stop_searching();

	}else{ 

		if(active_threads < total_threads)
			set_threads(active_threads+1);
		else{
			set_pstate(1);
			set_threads(1);
		}
	} 
}

///////////////////////////////////////////////////////////////
// Main heuristic function
///////////////////////////////////////////////////////////////




// Takes decision on frequency and number of active threads based on statistics of current round 
void heuristic(double throughput, double power, long time){
	
	#ifdef DEBUG_HEURISTICS
		printf("Heuristic called - throughput: %lf - power: %lf Watt - time_interval %lf ms\n", throughput, power, ((double) time)/1000000);
	#endif

	if(!stopped_searching){
		switch(heuristic_mode){
			case 8: // Fixed number of threads at p_state static_pstate set in hope_config.txt
				stopped_searching = 1;
				break;
			case 9:	// Dynamic heuristic0
				dynamic_heuristic0(throughput, power);
				break;
			case 11:
				heuristic_highest_threads(throughput, power);
				break;
			case 12:
				heuristic_binary_search(throughput,power);
				break;
			case 13:
				heuristic_two_step_search(throughput, power);
				break;
			case 14:
				heuristic_two_step_stateful(throughput, power);
				break;
			case 15:
				model_power_throughput(throughput, power);
				break;
			default:
				printf("Heuristic mode invalid\n");
				exit(1);
				break;
		}

		if(!stopped_searching)
			steps++;

		#ifdef DEBUG_HEURISTICS
			printf("Switched to %d threads/cores - pstate %d\n", active_threads, current_pstate);
		#endif 
	}
	else{	// Workload change detection
		if(detection_mode == 3){
			if(heuristic_mode == 15){
				
				// Copy sample data to compare models with real data
				power_real[current_pstate][active_threads] = power;
				throughput_real[current_pstate][active_threads] = throughput;

				// Copy to the validation array predictions from the model. Necessary as we perform multiple runs of the model
				// to account for workload variability
				power_validation[current_pstate][active_threads] = power_model[current_pstate][active_threads];
				throughput_validation[current_pstate][active_threads] = throughput_model[current_pstate][active_threads];
				
				if(current_pstate == 2 && active_threads == total_threads){
					
					// Do not restart the exploration/model setup
					detection_mode = 0;

					// Print validation results to file
					FILE* model_validation_file = fopen("model_validation.txt","w+");
					int i,j = 0;

					// Write real, predicted and error for throughput to file
					fprintf(model_validation_file, "Real throughput\n");
					for(i = 2; i < max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", throughput_real[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fprintf(model_validation_file, "Predicted throughput\n");
					for(i = 2; i < max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", throughput_validation[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fprintf(model_validation_file, "Throughput error percentage\n");
					for(i = 2; i < max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", (100*(throughput_validation[i][j]-throughput_real[i][j]))/throughput_real[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					// Write real, predicted and error for power to file
					fprintf(model_validation_file, "Real power\n");
					for(i = 2; i < max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", power_real[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fprintf(model_validation_file, "Predicted power\n");
					for(i = 2; i < max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", power_validation[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fprintf(model_validation_file, "power error percentage\n");
					for(i = 2; i < max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", (100*(power_validation[i][j]-power_real[i][j]))/power_real[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fclose(model_validation_file);

					#ifdef DEBUG_HEURISTICS
						printf("Model validation completed\n");
						exit(0);
					#endif
				}
				else if(active_threads == total_threads){ // Should restart the model 
					validation_pstate--;
					stopped_searching = 0;
					set_pstate(max_pstate);
  					set_threads(1);
  					best_throughput = -1;
					best_threads = 1;
					best_pstate = max_pstate;

					#ifdef DEBUG_HEURISTICS
						printf("Switched to: #threads %d - pstate %d\n", active_threads, current_pstate);
					#endif 
				}
				else{ // Not yet finished current P-state, should increase threads
					set_threads(active_threads+1);

					#ifdef DEBUG_HEURISTICS
						printf("Switched to: #threads %d - pstate %d\n", active_threads, current_pstate);
					#endif 
				}
			}
		}
		else if(detection_mode == 2){

			if(current_pstate == 0 && heuristic_mode != 10 && power > (power_limit*(1+(1/100))) ){
				#ifdef DEBUG_HEURISTICS
					printf("Disabling power boost\n");
				#endif

				set_pstate(1);
			}

			if(current_exploit_steps++ == exploit_steps){
				
				#ifdef DEBUG_HEURISTICS
					printf("EXPLORATION RESTARTED. PHASE 0 - INITIAL CONFIGURATION: #threads %d - p_state %d\n", best_threads, best_pstate);
				#endif
				
				if(heuristic_mode == 11){
					set_pstate(max_pstate);
					set_threads(starting_threads);
				}else if(heuristic_mode == 12 || heuristic_mode == 13 || heuristic_mode == 15){
					set_pstate(max_pstate);
					set_threads(1);
				}else{
					set_pstate(best_pstate);
					set_threads(best_threads);
					starting_threads = best_threads;
				}
				
				best_throughput = -1;
				best_pstate = -1; 
				best_threads = -1;
				best_power = -1;

				stopped_searching = 0;

				min_thread_search = 1;
				max_thread_search = total_threads;

				min_pstate_search = 0; 
				max_pstate_search = max_pstate;

				min_thread_search_throughput = -1;
				max_thread_search_throughput = -1;
			}
		}
	}

	#ifdef DEBUG_OVERHEAD
		time_heuristic_end = get_time();
		time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
		printf("DEBUG OVERHEAD -  Inside heuristic(): %lf microseconds\n", time_heuristic_microseconds);
	#endif 
}
