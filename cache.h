/* This file contains a rough implementation of an L1 cache in the absence of an L2 cache*/
#include <stdlib.h>
#include <stdio.h>

unsigned int L2_accesses = 0;
unsigned int L2_misses = 0;
unsigned int L2_hits = 0;
unsigned int cycle_number;

struct cache_blk_t { // note that no actual data will be stored in the cache 
  unsigned long tag;
  char valid;
  char dirty;
  unsigned LRU; //to be used to build the LRU stack for the blocks in a cache set
};

struct cache_t {
  // The cache is represented by a 2-D array of blocks. 
  // The first dimension of the 2D array is "nsets" which is the number of sets (entries)
  // The second dimension is "assoc", which is the number of blocks in each set.
  int nsets;          // number of sets
  int blocksize;        // block size
  int assoc;          // associativity
  int mem_latency;        // the miss penalty
  struct cache_blk_t **blocks;  // a pointer to the array of cache blocks
};

struct cache_t * cache_L1_create(int size, int blocksize, int assoc, int mem_latency)
{
	printf("\nCreate the L1 cache... ");
	printf("Cache size: %dKB ", size);
	printf("Blocksize: %dB ", blocksize);
	printf("Associativity: %d ", assoc);
	printf("Memory Latency: %d ", mem_latency);
	int i, nblocks , nsets ;
	struct cache_t *C = (struct cache_t *)calloc(1, sizeof(struct cache_t));

	nblocks = size *1024 / blocksize ;// number of blocks in the cache
	nsets = nblocks / assoc ;     // number of sets (entries) in the cache
	C->blocksize = blocksize ;
	C->nsets = nsets  ; 
	C->assoc = assoc;
	C->mem_latency = mem_latency;

	C->blocks= (struct cache_blk_t **)calloc(nsets, sizeof(struct cache_blk_t *));

	for(i = 0; i < nsets; i++) {
		C->blocks[i] = (struct cache_blk_t *)calloc(assoc, sizeof(struct cache_blk_t));
	}
	return C;
}


struct cache_t * cache_L2_create(int size, int blocksize, int assoc, int mem_latency){
  printf("\nCreate the L2 cache ");
  printf("Cache size: %dKB ", size);
  printf("Blocksize: %dB ", blocksize);
  printf("Associativity: %d ", assoc);
  printf("Memory Latency: %d ", mem_latency);
  int i, nblocks , nsets ;
  struct cache_t *C = (struct cache_t *)calloc(1, sizeof(struct cache_t));
  
  if (size == 0) {
    //printf("\nL2 cache created successfully");
    return C;
  }
    
  nblocks = size *1024 / blocksize ;// number of blocks in the cache
  nsets = nblocks / assoc ;     // number of sets (entries) in the cache
  C->blocksize = blocksize ;
  C->nsets = nsets  ; 
  C->assoc = assoc;
  C->mem_latency = mem_latency;

  C->blocks= (struct cache_blk_t **)calloc(nsets, sizeof(struct cache_blk_t *));

  for(i = 0; i < nsets; i++) {
    C->blocks[i] = (struct cache_blk_t *)calloc(assoc, sizeof(struct cache_blk_t));
  }
  //printf("L2 cache created successfully");
  return C;
}

//------------------------------

int updateLRU(struct cache_t *cp ,int index, int way)
{
  int k ;
  for (k=0 ; k< cp->assoc ; k++) {
    if(cp->blocks[index][k].LRU < cp->blocks[index][way].LRU) 
       cp->blocks[index][k].LRU = cp->blocks[index][k].LRU + 1 ;
  }
  cp->blocks[index][way].LRU = 0 ;
}

int cache_access(struct cache_t *L1, struct cache_t *L2, unsigned long address, int access_type)
{
  // printf("\nAccess the cache... ");
  // printf("Address: %lu", address);
  // printf("  Access_type: %d", access_type);
  int i,latency ;
  //L1
  int block_address ;
  int index ;
  int tag ;
  //L2
  int L2_block_address;
  int L2_index;
  int L2_tag;

  int way ;
  int max ;

  block_address = (address / L1->blocksize);
  tag = block_address / L1->nsets;
  index = block_address - (tag * L1->nsets) ;
  // printf(" \nblock_address %d", block_address);
  // printf(" tag %d", tag);
  // printf(" index %d", index);

  if(L2->nsets != 0){
    L2_block_address = (address / L2->blocksize);
    L2_tag = L2_block_address / L2->nsets;
    L2_index = L2_block_address - (L2_tag * L2->nsets) ;

    // printf(" \nL2_block_address %d", L2_block_address);
    // printf(" L2_tag %d", L2_tag);
    // printf(" L2_index %d", L2_index);
  }

  latency = 0;




  //L1 cache hit check
  for (i = 0; i < L1->assoc; i++) { /* look for the requested block */
    if (L1->blocks[index][i].tag == tag && L1->blocks[index][i].valid == 1) {
      updateLRU(L1, index, i);
      if (access_type == 1){ //write
        L1->blocks[index][i].dirty = 1;
      }
       //printf("\nan L1 cache hit");
       //printf(" at index %d with tag %d way %d",  index, tag, i);
      return(latency);          /* a L1 cache hit */
    }
  }




  /* a cache miss */
   //printf("\nan L1 cache miss");
  // printf(" at index %d with tag %d",  index, tag);
  for (way=0 ; way < L1->assoc ; way++){  /* look for an invalid entry */
      if (L1->blocks[index][way].valid == 0) {
      	if(L2->nsets == 0){
			latency = latency + L1->mem_latency;  /* account for reading the block from memory*/
			cycle_number += latency;
      	}else{
      		latency = latency + L2->mem_latency;  /* account for reading the block from L2*/
			cycle_number += latency;
      	}

        L1->blocks[index][way].valid = 1 ;
        L1->blocks[index][way].tag = tag ;
        updateLRU(L1, index, way); 
        L1->blocks[index][way].dirty = 0;
        if(access_type == 1) { //write
          L1->blocks[index][way].dirty = 1;
        }
        //printf("\n\tan invalid L1 entry is available");
	    //printf("\n\tindex %d  tag %d way %d",index, tag, way);
        
        if(L2->nsets == 0){
          //no L2
    	  return(latency);        /* an invalid L1 entry is available*/
    	}
      }
  }




  //cache miss but no empty spots
   // printf("\n\tno invalid L2 entry available");
  max = L1->blocks[index][0].LRU ;  /* find the LRU block */
  way = 0 ;
  for (i=1 ; i< L1->assoc ; i++){
    if (L1->blocks[index][i].LRU > max) {
      //printf("\n\tL1 block: L1_index %d  i %d  L1_tag %d", index, i, tag);
      max = L1->blocks[index][i].LRU ;
      way = i ;
    }
  }
  if (L1->blocks[index][way].dirty == 1){ 
    latency = latency + L1->mem_latency; /* for writing back the evicted block */
	cycle_number += latency;
  } 
  latency = latency + L1->mem_latency;    /* for reading the block from memory*/
  cycle_number += latency;

    L1->blocks[index][way].tag = tag ;
  updateLRU(L1, index, way) ;
  L1->blocks[index][way].dirty = 0 ;
  if(access_type == 1) { //write
    L1->blocks[index][way].dirty = 1 ;
  }
      /* should instead write to and/or read from L2, in case you have an L2 */

  //---------------------------------------------------------
        //look in L2
        if(L2->nsets != 0){
        	L2_accesses++;
        	for (i = 0; i < L2->assoc; i++) { /* look for the requested block in L2 */
            //printf("\n\tL2 block: L2_index %d  i %d  L2_tag %d", L2_index, i, L2_tag);
			      if (L2->blocks[L2_index][i].tag == tag && L2->blocks[L2_index][i].valid == 1) {
			    	  updateLRU(L2, L2_index, i);
			    	  if (access_type == 1){ //write
			        	L2->blocks[L2_index][i].dirty = 1;
			    	  }
			    	  //printf("\nan L2 cache hit");
			    	  //printf(" at index %d with tag %d",  L2_index, L2_tag);
			    	  L2_hits++;
			    	  return(latency);          /* an L2 cache hit */
				  }
			}




			    //a L2 cache miss
			   // printf("\nan L2 cache miss");
	        L2_misses++;
			for(way = 0; way < L2->assoc; way++){
	            if(L2->blocks[L2_index][way].valid == 0){
	              //check the L2 cache
	              L2->blocks[L2_index][way].valid = 1;
	              L2->blocks[L2_index][way].tag = tag;
	              updateLRU(L2, L2_index, way); //do we need this??? I think so
	              if(access_type == 1){
	                L2->blocks[L2_index][way].dirty = 1;
	              }
	              //printf("\n\tan invalid L2 entry is available");
	              //printf("\n\tL2_index %d  L2_tag %d",L2_index, L2_tag);
	              return(latency);
	            }
	        }




	      	//no invalid cache block available
          	//printf("\n\tno invalid L2 entry available");
	      	max = L2->blocks[L2_index][0].LRU; //find the LRU block
			    way = 0;
			    for(i = 1; i<L2->assoc; i++){
            		//printf("\n\tL2 block: L2_index %d  i %d  L2_tag %d", L2_index, i, L2_tag);
				    if(L2->blocks[L2_index][i].LRU > max && L1->blocks[index][i].valid != 1){ //check that L2 is inclusive. Check that evicting L2 doesn't evict something in L1
              			max = L2->blocks[L2_index][i].LRU;
					    way = i;
				    }
			    }
         		//printf("\n\t evict block L2index %d way %d", L2_index, way);

    			//printf("\n way %d", way);
    			if (L2->blocks[L2_index][way].dirty == 1){ 
    			    latency = latency + L2->mem_latency; /* for writing back the evicted block */
					cycle_number += latency;
    			} 
    			latency = latency + L1->mem_latency; //weird, but this should read the block from mem and add the mem latency which is in L1
				cycle_number += latency;
    			L2->blocks[L2_index][way].tag = tag ;
    			updateLRU(L2, L2_index, way) ;
    			L2->blocks[L2_index][way].dirty = 0 ;
    			if(access_type == 1) { //write
    			    L2->blocks[L2_index][way].dirty = 1 ;
    			}
    			return(latency);
    		
    		}
//---------------------------------------------------------



  return(latency);
}
