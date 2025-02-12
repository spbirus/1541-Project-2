/**************************************************************/
/* CS/COE 1541				 			
   just compile with gcc -o pipeline pipeline.c			
   and execute using							
   ./pipeline  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	0  
***************************************************************/

#include <stdio.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "CPU.h" 
#include "cache.h"
#include <time.h>

// to keep cache statistics
unsigned int I_accesses = 0;
unsigned int I_misses = 0;
unsigned int I_hits = 0;
unsigned int D_read_accesses = 0;
unsigned int D_read_misses = 0;
unsigned int D_write_accesses = 0; 
unsigned int D_write_misses = 0;
unsigned int D_hits = 0;

#define BP_ENTRIES 64     // size of branch predictor table


int bp_table[BP_ENTRIES][4]; //1 bit branch predictor table 
//[bit 1][bit 0][PC address][branch address]

//**********************************************************************
//ONE BIT BRANCH PREDICTION STUFF
//get from the btb table
int get_value_from_bpt_one_bit(unsigned int address){
	
  int index = (address << 23) >> 26;  
  
  if(bp_table[2][index] == address){
    return bp_table[1][index];
  }else if(bp_table[2][index] == 0){
  	//predict not taken 
  	//nothing has been added here
  	return 0;
  }else{
  	//have to flush
  	//made wrong prediction because they don't match and not 0
  	return -1;
  }
	
}



//send to the btb table
void set_value_bpt_one_bit(unsigned int address, unsigned int dest_addr, int taken){
  int index = (address << 23) >> 26;

  //printf("index: %d, address: %d, dest_addr: %d", index, address, dest_addr);


  bp_table[index][1] = (taken ==1) ? 1:0; //do we need the ? : thing??
  bp_table[index][2] = address;
  bp_table[index][3] = dest_addr;

}
//**********************************************************************
//TWO BIT BRANCH PREDICTION STUFF
int get_value_from_bpt_two_bit(unsigned int address){
  
  int index = (address << 23) >> 26;
  
  if (bp_table[0][index] == 0 && bp_table[1][index] == 0) {
    //predict not taken
    return 0;
  } else if (bp_table[0][index] == 0 && bp_table[1][index] == 1) {
    //predict not taken, miss predicted once
    return 1;
  } else if (bp_table[0][index] == 1 && bp_table[1][index] == 1) {
    //predict taken
    return 3;
  } else if (bp_table[0][index] == 1 && bp_table[1][index] == 0) {
    //predict taken, miss predicted once
    return 2;
  } else {
    
  }
  
}

void set_value_bpt_two_bit(unsigned int address, unsigned int dest_addr, int taken1, int taken2){
	int index = (address << 23) >> 26;

	//printf("index: %d, address: %d, dest_addr: %d", index, address, dest_addr);

	bp_table[index][0] = (taken1 == 1) ? 1:0;
	bp_table[index][1] = (taken2 == 1) ? 1:0;
	bp_table[index][2] = address;
 	bp_table[index][3] = dest_addr;

}


//**********************************************************************


print_finished_instr(struct trace_item* instr, int cycle_number){
	switch(instr->type) {
        case ti_NOP:
          printf("[cycle %d] NOP:\n",cycle_number) ;
          break;
        case ti_RTYPE:
          printf("[cycle %d] RTYPE:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", instr->PC, instr->sReg_a, instr->sReg_b, instr->dReg);
          break;
        case ti_ITYPE:
          printf("[cycle %d] ITYPE:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", instr->PC, instr->sReg_a, instr->dReg, instr->Addr);
          break;
        case ti_LOAD:
          printf("[cycle %d] LOAD:",cycle_number) ;      
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", instr->PC, instr->sReg_a, instr->dReg, instr->Addr);
          break;
        case ti_STORE:
          printf("[cycle %d] STORE:",cycle_number) ;      
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", instr->PC, instr->sReg_a, instr->sReg_b, instr->Addr);
          break;
        case ti_BRANCH:
          printf("[cycle %d] BRANCH:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", instr->PC, instr->sReg_a, instr->sReg_b, instr->Addr);
          break;
        case ti_JTYPE:
          printf("[cycle %d] JTYPE:",cycle_number) ;
          printf(" (PC: %x)(addr: %x)\n", instr->PC,instr->Addr);
          break;
        case ti_SPECIAL:
          printf("[cycle %d] SPECIAL:\n",cycle_number) ;      	
          break;
        case ti_JRTYPE:
          printf("[cycle %d] JRTYPE:",cycle_number) ;
          printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", instr->PC, instr->dReg, instr->Addr);
          break;
      }
}

//**********************************************************************
//queue that holds the instructions that were removed from the pipeline due to a jump or a branch
typedef struct queue_entry{
	struct trace_item entry;
	struct queue_entry* next;
	struct queue_entry* prev;
} queue_entry;
queue_entry* queue_start = 0;
queue_entry* queue_end = 0;
int queue_size = 0;

//add instruction to the queue
void add_queue_instr(struct trace_item* instr){
	queue_entry* new_entry = (queue_entry*) malloc(sizeof(queue_entry));
	new_entry->entry = *instr;
	new_entry->next = queue_start;
	new_entry->prev = 0;
	queue_entry* old_start = queue_start;
	queue_start = new_entry;
	//sets the start and end to the same thing when there is no queue left
	if(queue_size == 0){
		queue_end = queue_start;
	}else{
		old_start->prev = queue_start;
	}
	queue_size += 1;
}

//remove from the queue
int remove_queue_instr(struct trace_item* instr){
	if(queue_end == NULL){
		return 0;
	}
	*instr = queue_end->entry;
	queue_entry* new_end = queue_end->prev;
	free(queue_end);
	queue_end = new_end;
	queue_size -= 1;
	return 1;
}

//**********************************************************************

//sets an instruction to a no-op
void set_instr_to_noop(struct trace_item* instruction){
	//instruction = malloc(sizeof(struct trace_item));
	//memset(instruction, 0, sizeof(struct trace_item));
	instruction->type = ti_NOP;
}

//**********************************************************************

int main(int argc, char **argv)
{
	
	clock_t start, end;
	double cpu_time_used;
	
	start = clock();
  
  int branch_prediction_method = 0;
  char *trace_file_name;
  int trace_view_on = 0;
  unsigned int cycle_number = 0;
  
  unsigned char t_type = 0;
  unsigned char t_sReg_a= 0;
  unsigned char t_sReg_b= 0;
  unsigned char t_dReg= 0;
  unsigned int t_PC = 0;
  unsigned int t_Addr = 0;

  //open the cache file
  FILE *cache_file;
  cache_file = fopen("cache_config.txt", "r");
  unsigned int I_size = 0; 
  unsigned int I_assoc = 0; 
  unsigned int D_size = 0;
  unsigned int D_assoc = 0;
  unsigned int L2_size = 0;
  unsigned int L2_assoc = 0;
  unsigned int bsize = 0;
  unsigned int L2_latency = 0;
  unsigned int mem_time = 0;

  //read in the parameters
  fscanf(cache_file, "%d", &I_size);
  fscanf(cache_file, "%d", &I_assoc);
  fscanf(cache_file, "%d", &D_size);
  fscanf(cache_file, "%d", &D_assoc);
  fscanf(cache_file, "%d", &L2_size);
  fscanf(cache_file, "%d", &L2_assoc);
  fscanf(cache_file, "%d", &bsize);
  fscanf(cache_file, "%d", &L2_latency);
  fscanf(cache_file, "%d", &mem_time);
  fclose(cache_file);

  //read the inputs
  if(argc == 2){
  	trace_file_name = argv[1];
  	trace_view_on = 0;
  	branch_prediction_method = 0;
  }else if(argc == 4){
  	trace_file_name = argv[1];
  	branch_prediction_method = atoi(argv[2]); //might need a ? : operator here. unsure though
  	trace_view_on = atoi(argv[3]);
  }else{
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character> <branch_prediction - 0|1|2>\n");
    fprintf(stdout, "\n(branch_prediction) 0 - no branch prediction, 1 - one bit branch prediction, 2 - two bit branch prediction \n\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n");
    exit(0);
  }
  

  //open the trace file designated
  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);
  trace_fd = fopen(trace_file_name, "rb");
  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();

  //do the trace stuff

  //pipeline instructions
  //fprintf(stdout, "define the stages\n");
  struct trace_item *new_instr; //reads what will be next
  new_instr = malloc(sizeof(struct trace_item));
  struct trace_item *if1_stage;
  if1_stage = malloc(sizeof(struct trace_item));
  struct trace_item *if2_stage;
  if2_stage = malloc(sizeof(struct trace_item));
  struct trace_item *id_stage;
  id_stage = malloc(sizeof(struct trace_item));
  struct trace_item *ex_stage;
  ex_stage = malloc(sizeof(struct trace_item));
  struct trace_item *mem1_stage;
  mem1_stage = malloc(sizeof(struct trace_item));
  struct trace_item *mem2_stage;
  mem2_stage = malloc(sizeof(struct trace_item));
  struct trace_item *wb_stage;
  wb_stage = malloc(sizeof(struct trace_item));
  //initialize the stages to no-ops
  //fprintf(stdout, "initialize the stages\n");
  set_instr_to_noop(new_instr);
  set_instr_to_noop(if1_stage);
  set_instr_to_noop(if2_stage);
  set_instr_to_noop(id_stage);
  set_instr_to_noop(ex_stage);
  set_instr_to_noop(mem1_stage);
  set_instr_to_noop(mem2_stage);
  set_instr_to_noop(wb_stage);

  
  //initialize the L1 caches
  struct cache_L1_create *instr_cache;
  struct cache_L1_create *data_cache;
  struct cache_L2_create *L2_cache;
  //create the instruction L1 cache
  instr_cache = cache_L1_create(I_size, bsize, I_assoc, mem_time); //don't know if mem_time is the right variable to throw in here
  //create the mem L1 cache
  data_cache = cache_L1_create(D_size, bsize, D_assoc, mem_time); //don't know if mem_time is the right variable to throw in here
  //create the unified L2 cache
  L2_cache = cache_L2_create(L2_size, bsize, L2_assoc, L2_latency);
 

  //loop while there are still instructions left
  int instr_left = 8;
  int pc = 0; //attempt to use PC to detect same instruction infinite loop
  int misses = 0;
  while(instr_left){
  	cycle_number++; //increase the cycle number
  	int hazard = 0; //initializes a no hazard state. will change based on branch detection and stuff


  	if(trace_view_on){
  		//sends the stage and cycle number of each wb stage that has finished
  		fprintf(stdout, "\n");
       //print_finished_instr(if1_stage, cycle_number);
      // print_finished_instr(if2_stage, cycle_number);
      // print_finished_instr(id_stage, cycle_number);
      // print_finished_instr(ex_stage, cycle_number);
      // print_finished_instr(mem1_stage, cycle_number);
      // print_finished_instr(mem2_stage, cycle_number);
  	  print_finished_instr(wb_stage, cycle_number);
  	}

    //**********************************************************************
 


    //detect structural hazards
    //dectects if wb is used
    //NOTE NOT SURE IF WE NEED TO CHECK MORE TYPES LIKE ITYPE AND SPECIAL
    if((wb_stage->type == ti_STORE || wb_stage->type == ti_RTYPE || wb_stage->type == ti_ITYPE) && (id_stage->type == ti_RTYPE || id_stage->type == ti_LOAD || id_stage->type == ti_ITYPE)){
            hazard = 1;
            //fprintf(stdout, "\nstructural hazard detected \n");
    }
    
    //detect data hazards
    if(ex_stage->type == ti_LOAD && (ex_stage->dReg == id_stage->sReg_a || ex_stage->dReg == id_stage->sReg_b)){
    	hazard = 2;
    	//fprintf(stdout, "\ndata hazard detected \n");

    }
    
    
    
    //detect jump control hazards
    if(ex_stage->type == ti_JTYPE || ex_stage->type == ti_JRTYPE){
    	hazard = 3;
    	//fprintf(stdout, "\njump control hazard detected \n");

    }

    //detect branch control hazards
    if(ex_stage->type == ti_BRANCH){
    	//fprintf(stdout, "\nbranch control hazard detected: ");

    	if(branch_prediction_method == 0){
    		if(ex_stage->PC + 4 != id_stage->PC){
    			hazard = 3; //incorrect no prediction
    			//fprintf(stdout, "incorrect default (not taken) prediction\n");
    		}
    	}else if(branch_prediction_method == 1){
    		if(ex_stage->PC + 4 == id_stage->PC){//not taken
    			//predicted taken
    				hazard = 3;
    				//fprintf(stdout, "predicted taken when not taken\n");
    				set_value_bpt_one_bit(ex_stage->PC, ex_stage->Addr, 0);
    				misses++;

    		}else{ //taken
    			//predicted not taken
    				hazard = 3;
    				//fprintf(stdout, "predicted not taken when taken\n");
    				set_value_bpt_one_bit(ex_stage->PC, ex_stage->Addr, 1);
    			
    		}
    	}else if(branch_prediction_method == 2){
    		if(ex_stage->PC + 4 != id_stage->PC){ //not taken
    			//predicted taken (1st miss prediction)
				if (get_value_from_bpt_two_bit(ex_stage->PC) == 3) {
					hazard = 3;
					//fprintf(stdout, "predicted taken when not taken (1st miss)");
					set_value_bpt_two_bit(ex_stage->PC, ex_stage->Addr, 1, 0);
					misses++;
				}
				//predicted taken (2nd time, change prediction)
				if (get_value_from_bpt_two_bit(ex_stage->PC) == 2) {
					hazard = 3;
					//fprintf(stdout, "predicted taken when not taken (2nd miss)");
					set_value_bpt_two_bit(ex_stage->PC, ex_stage->Addr, 0, 0);
					misses++;
				}
				//predicted not taken (correct prediction)
				if (get_value_from_bpt_two_bit(ex_stage->PC) == 0) {
					//do nothing, table doesnt change
					//set_value_bpt_two_bit(ex_stage->PC, ex_stage->Addr, 0, 0);
					//dont set a hazard
				}

    		}else{ //taken
    			//predicted not taken (1st miss prediction)
				if (get_value_from_bpt_two_bit(ex_stage->PC) == 0){					
					hazard = 3;
					//fprintf(stdout, "predicted not taken when taken (1st miss)");
					set_value_bpt_two_bit(ex_stage->PC, ex_stage->Addr, 0, 1);
					misses++;
				} 
				//predicted not taken (2nd time, change prediction)
				if (get_value_from_bpt_two_bit(ex_stage->PC) == 1){					
					hazard = 3;
					//fprintf(stdout, "predicted not taken when taken (2nd miss)");
					set_value_bpt_two_bit(ex_stage->PC, ex_stage->Addr, 1, 1);
					misses++;
				} 
				//predicted taken (correct prediction)
				if (get_value_from_bpt_two_bit(ex_stage->PC) == 3){
					//do nothing, table doesnt change
					//set_value_bpt_two_bit(ex_stage->PC, ex_stage->Addr, 1, 1);
					hazard = 3;
					//fprintf(stdout, "predicted taken when taken");
				} 
    		}
    	}
    }

    //**********************************************************************

    int latency;

    //-------------------------------------------------------
    //SHOULD THIS SECTION GO HERE?????? OR BELOW THE MOVING OF THE WB, MEM1, MEM2, AND EX STAGES A COUPLE LINES BELOW????? 
    //check the L1 data cache
    if(mem1_stage->type == ti_LOAD){
      //Loading == reading??
      latency = cache_access(data_cache, L2_cache, mem1_stage->Addr, 0); 
  	  D_read_accesses++; //increment the data read accesses
      //hit -> latency = 0
      //miss -> latency value varies based on memory (have to stall)
      if(latency > 0){
      	D_read_misses++; //increment the data read misses when the latency is anything greater than 0
      }else{
        D_hits++;
      }
    }else if(mem1_stage->type == ti_STORE){
      //Storing == writing??
      latency = cache_access(data_cache, L2_cache, mem1_stage->Addr, 1); 
      D_write_accesses++; //increment the data read accesses
      //hit -> latency = 0
      //miss -> latency value varies based on memory (have to stall)
      if(latency > 0){
        D_write_misses++; //increment the data read misses when the latency is anything greater than 0
      }else{
        D_hits++;
      }
    }
    //-------------------------------------------------------




    //moved these here since they are common across all hazards
    wb_stage = mem2_stage;
  	mem2_stage = mem1_stage;
  	mem1_stage = ex_stage;


  	
  	//hazard switching
  	switch(hazard){
  		case 0: //no hazard

			     //-------------------------------------------------------
			     //check the L1 instruction cache
			     latency = cache_access(instr_cache, L2_cache, new_instr->Addr, 0); 
						//TODO: when do we want to write vs. when do we want to read?? 
						//I think instr_cache is always read and data_cache will be read or write
			     I_accesses++; //increment the instruction accesses
	        //hit -> latency = 0
	        //miss -> latency value varies based on memory (have to stall)
	        if(latency > 0){
	        	I_misses++; //increment the insctruction misses when the latency is anything greater than 0
	        }else{
	          I_hits++;
	        }
	        //-------------------------------------------------------

            
  			ex_stage = id_stage;
  			id_stage = if2_stage;
  			if2_stage = if1_stage;
  			if1_stage = new_instr;

  			//get next instr, if none decrement the instr_left
  			if(!trace_get_item(&new_instr)){
          		//get next instr, if none decrement the instr_left
          		instr_left -= 1;
          		set_instr_to_noop(new_instr);
        	}
  			break;

  		case 1: //structural hazard
  			//move up ex, mem1, mem2, and wb
  			//dont change if1, if2, and id, and don't fetch a new instruction
  			set_instr_to_noop(ex_stage);
  			break;

  		case 2: //data hazard
  			set_instr_to_noop(ex_stage);
  			break;

  		case 3: //control hazard
  			//flush IF1, IF2, and ID
  			add_queue_instr(if1_stage);
  			add_queue_instr(if2_stage);
  			add_queue_instr(id_stage);

        

  			set_instr_to_noop(if1_stage);
  			set_instr_to_noop(if2_stage);
  			set_instr_to_noop(id_stage);


  			//-------------------------------------------------------
  			//check the L1 instruction cache
  			latency = cache_access(instr_cache, L2_cache, new_instr->Addr, 1); //TODO: when do we want to write vs. when do we want to read??
  			I_accesses++; //increment the instruction accesses
	        //hit -> latency = 0
	        //miss -> latency value varies based on memory (have to stall)
	            //maybe use a while loop with repeated print("waiting") statements
	        if(latency > 0){
	        	I_misses++; //increment the insctruction misses when the latency is anything greater than 0
	        }else{
            I_hits++;
          }
	        //-------------------------------------------------------

  			ex_stage = id_stage;
  			id_stage = if2_stage;
  			if2_stage = if1_stage;
  			if1_stage = new_instr;

  			
	        if(remove_queue_instr(new_instr)){
	          //need to add three "squashed" instructions afterward to get correct cycle time
	          //ADD CODE HERE
	        }else if(!trace_get_item(&new_instr)){
	          //get next instr, if none decrement the instr_left
	  				instr_left -= 1;
	  				set_instr_to_noop(new_instr);
	          //fprintf(stdout, "instructinos left: %d \n", instr_left);

  			}

  			break;
  		default:
  			exit(1); //exit with an error. should never hit this

  	}
  	
  }

  printf("\n\n\nSimulation terminates at cycle: %u \n", cycle_number);

  //print the total number of memory accesses
  int total_accesses = I_accesses + D_write_accesses + D_write_accesses;
  printf("Number of memory accesses: %d\n", total_accesses);
  printf("\tI_accesses: %d  D_write_accesses %d  D_read_accesses %d \n", I_accesses, D_write_accesses, D_read_accesses);
  printf("instruction misses: %d\n", I_misses);
  printf("data read misses: %d\n", D_read_misses);
  printf("data write misses: %d\n",D_write_misses);

  int D_misses = D_write_misses+D_read_misses;
  printf("\nL1 Data cache: [%d] accesses, [%d] hits, [%d] misses, [%.2f%] miss rate", D_write_accesses+D_read_accesses, D_hits,D_write_misses+D_read_misses, (double)100*(D_misses)/(D_hits+D_misses));
  printf("\nL1 Instruction cache: [%d] accesses, [%d] hits, [%d] misses, [%.2f%] miss rate", I_accesses, I_hits, I_misses, (double)100*(I_misses)/I_accesses);
  printf("\nL2 cache: [%d] accesses, [%d] hits, [%d] misses, [%.2f%] miss rate\n\n", L2_accesses, L2_hits, L2_misses, (double)100*(L2_misses)/L2_accesses);

  //**********************************************************************
  // debugging helpers
  //printf("\nNumber of branch table misses: %d \n\n", misses);

  // //print the branch predition table
  // printf("Branch Table printed below for debugging\n");
  // int row, col;
  // for (row = 0; row < BP_ENTRIES; row++) {
  //       for (col = 0; col < 4; col++) {
  //           printf("%d \t\t", bp_table[row][col]);
  //       }
  //       printf("\n");
  //   }

  trace_uninit();
  
  end = clock();
  cpu_time_used = (double) (end - start)/ CLOCKS_PER_SEC;
  // printf("\nStart time: %f     ", start);
  // printf("End time: %f        ", end);
  printf("\nTotal Execution Time: %f\n\n", cpu_time_used);

  exit(0);
}

