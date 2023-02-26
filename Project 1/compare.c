#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include "wfd.c"

#ifndef S_ISDIR
#define S_ISDIR
#endif

typedef struct WFDNodeLL {
	WFDNode *head;
	pthread_mutex_t lock;
} WFDNodeLL;

typedef struct JSDListArray {
	JSDNode** pairList;
	pthread_mutex_t lock;
} JSDListArray;

struct direct_arg {
	struct direct_queue *input_Q;
	struct file_queue *second_Q;
	char *specified;
};

struct file_arg{
	struct file_queue *input_Q;
	struct direct_queue *second_Q;
	WFDNodeLL *input_list;
	char* alphabet;
};

struct a_arg{
	unsigned range_start;
	unsigned range_end;
	JSDListArray *jsdPtr;
	char* alphabet;
};

struct direct_queue{
	struct directNode *head;
	struct directNode *end;
	int active_threads;
	int capacity;
	pthread_mutex_t dLock;
	pthread_cond_t ready; 
	
};

struct file_queue{
	struct fileNode *head;
	struct fileNode *end;
	int files_read;
	int active_threads;
	int capacity;
	pthread_mutex_t fLock;
	pthread_cond_t ready;
};

struct directNode {
	char *directName;
	struct directNode *next;
};

struct fileNode {
	char *fileName;
	struct fileNode *next;
};

/**
 * Enqueues directory nodes into the unbounded directory queue.
 **/
void direct_enqueue(struct directNode *node, struct direct_queue *Q){
	if (Q->head == NULL){
		Q->head = node;
		Q->end = node;
		Q->capacity++;
	} else {
		node->next = Q->head;
		Q->end->next = node;
		Q->end = node;
		Q->capacity++;	
	}
}

/**
 * Dequeues directory nodes out of the unbounded directory queue.
 **/
char *direct_dequeue(struct direct_queue *Q){
	char *name = Q->head->directName;
	struct directNode *temp = Q->head;
	if (Q->capacity == 2){
		Q->head = Q->end;
		Q->end = Q->head;
	}
	else if (Q->capacity == 1){
		Q->head = NULL;
		Q->end = NULL;
	}
	else {	
		Q->end->next = Q->head->next;
		Q->head = Q->head->next;
	}
	Q->capacity--;
	free(temp);
	return name;
}

/**
 * Enqueues file nodes into the unbounded file queue.
 **/
void file_enqueue(struct fileNode *node, struct file_queue *Q){
	if (Q->head == NULL){
		Q->head = node;
		Q->end = node;
		Q->capacity++;
	} else {
		node->next = Q->head;
		Q->end->next = node;
		Q->end = node;
		Q->capacity++;	
	}
}

/**
 * Dequeues directory nodes out of the unbounded file queue.
 **/
char *file_dequeue(struct file_queue *Q){
	char *name = Q->head->fileName;
	struct fileNode *temp = Q->head;
	if (Q->capacity == 2){
		Q->head = Q->end;
		Q->end = Q->head;
	}
	else if (Q->capacity == 1){
		Q->head = NULL;
		Q->end = NULL;
	}
	else {	
		Q->end->next = Q->head->next;
		Q->head = Q->head->next;
	}
	Q->capacity--;
	free(temp);
	return name;
}

/**
 * Initializes the unbounded directory queue.
 **/
void direct_queue_init(struct direct_queue *Q){
	Q->head = NULL;
	Q->end = NULL;
	Q->capacity = 0;
	Q->active_threads = 0;
	pthread_mutex_init(&Q->dLock, NULL);
	pthread_cond_init(&Q->ready, NULL);
}

/**
 * Initializes the unbounded file queue.
 **/
void file_queue_init(struct file_queue *Q){
	Q->head = NULL;
	Q->end = NULL;
	Q->active_threads = 0;
	Q->files_read = 0;
	Q->capacity = 0;
	pthread_mutex_init(&Q->fLock, NULL);
	pthread_cond_init(&Q->ready, NULL);
}

/**
 * Initializes the WFD Node linked list.
 **/
void WFDNodeLL_init(WFDNodeLL *list){
	list->head = NULL;
	pthread_mutex_init(&list->lock, NULL);
}

/**
 * Initializes the JSD array struct that contains every pair
 * for JSD computation.
 **/
void JSDListArray_init(JSDListArray *arr, JSDNode** pairList) {
	arr->pairList = pairList;
	pthread_mutex_init(&arr->lock, NULL);
}

/**
 * Inserts a WFD node into the WFD Node linked list.
 **/
void WFDNodeLL_insert(WFDNodeLL *list, WFDNode *node){
	if (list->head == NULL){
		list->head = node;
		node->next = NULL;
	}
	else {
		WFDNode *crnt = list->head;
		while (crnt->next != NULL){
			crnt = crnt->next;
		}
		crnt->next = node;
		node->next = NULL;
	}
}

/**
 * Traverses through the file and directory queues
 * to enqueue and dequeue as needed.
 **/
void *traverse(void *argptr){
	struct direct_arg *args = argptr;
	struct direct_queue *Q = args->input_Q;
	struct file_queue *Q2 = args->second_Q;
	char *suffix = args->specified;
	int go = 1; int active = 0;
	while (go == 1){
		pthread_mutex_lock(&Q->dLock);
		if (Q->capacity != 0){
			if (active == 0){
				active = 1;
				Q->active_threads++;
			}
			char *name = direct_dequeue(Q);
			pthread_mutex_unlock(&Q->dLock);
			DIR *dirp = opendir(name);
			struct dirent *de;
			int fd;
			while ((de = readdir(dirp))){
				char *new_name = malloc(sizeof(char) * (2 + strlen(name) + strlen(de->d_name)));
				if (!new_name) {
					perror("Malloc failed\n");
					exit(1);
				}
				for (int x = 0; x < strlen(name); x++){
					new_name[x] = name[x];
				}
				new_name[strlen(name)] = '/'; new_name[strlen(name) + 1] = '\0';
				for (int x = 0; x < strlen(de->d_name); x++){
					new_name[strlen(name) + 1 + x] = de->d_name[x];
				}
				new_name[strlen(name) + strlen(de->d_name) + 1] = '\0';

				DIR *dirp2 = opendir(new_name); 

				// Enqueues a subdirectory
				if (dirp2 != NULL && de->d_name[0] != '.'){ 
						//printf("enqueuing %s\n", new_name);
						pthread_mutex_lock(&Q->dLock);
						struct directNode *d_node = malloc(sizeof(struct directNode));
						if (!d_node) {
							perror("Malloc failed\n");
							exit(1);
						}
						d_node->directName = new_name;
						direct_enqueue(d_node, Q);
						pthread_mutex_unlock(&Q->dLock);
						//pid_t tid = pthread_self();
						//printf("name of thread is %d\n", tid);
					
					// Enqueues valid files based on matching suffix
				}  else if ((fd = open(new_name, O_RDONLY)) != -1){
					if (new_name[strlen(name) + 1] == '.'){
						free(new_name);
                        if (dirp2 != NULL) {
                            closedir(dirp2);
                        }						
						continue;
					}
					if (suffix[0] == '\0'){
						struct fileNode *f_node = malloc(sizeof(struct fileNode));
						if (!f_node) {
							perror("Malloc failed\n");
							exit(1);
						}
						f_node->fileName = new_name;
						//printf("ENQUEUEING FILE: %s\n", new_name);
						pthread_mutex_lock(&Q2->fLock);
						file_enqueue(f_node, Q2);
						pthread_mutex_unlock(&Q2->fLock);
                        if (dirp2 != NULL) {
                            closedir(dirp2);
                        }						
						continue;
					}
					int diff = strlen(new_name) - strlen(suffix);
					int match = 1;
					for (int z = strlen(suffix) - 1; z >= 0; z--){
						if (suffix[z] != new_name[z + diff]){
							match = 0;
						}
					}
					if (match == 1){
						struct fileNode *f_node = malloc(sizeof(struct fileNode));
						if (!f_node) {
							perror("Malloc failed\n");
							exit(1);
						}
						f_node->fileName = new_name;
						//printf("ENQUEUEING FILE: %s\n", new_name);
						pthread_mutex_lock(&Q2->fLock);
						file_enqueue(f_node, Q2);
						pthread_mutex_unlock(&Q2->fLock);
					} else {
						free(new_name); 
					}
					close(fd);
				} else {
					free(new_name);
				}
				if (dirp2 != NULL){
					closedir(dirp2);
				}
			}
			free(name);
			closedir(dirp);
		} else if (Q->capacity == 0){
			if (active == 1){
				active = 0;
				Q->active_threads--;
			}
			if (Q->active_threads > 0){
				pthread_cond_wait(&Q->ready, &Q->dLock);
				pthread_mutex_unlock(&Q->dLock);
				return NULL;
			}
			else {
				pthread_cond_broadcast(&Q->ready);
				pthread_mutex_unlock(&Q->dLock);
				go = 0;
				return NULL;
			}
		}
			
	}
	return NULL;
}

/**
 * Performs the WFD computation for each file.
 **/
void *computeWFD(void *argptr){
	struct file_arg *args = argptr;
	struct file_queue *Q = args->input_Q;
	struct direct_queue *Q2 = args->second_Q;	
	WFDNodeLL *list = args->input_list;
	char* alphabet = args->alphabet;
	int go = 1; int active = 0;
	while (go == 1){
		pthread_mutex_lock(&Q->fLock);
		if (Q->capacity != 0){
			if (active == 0){
				Q->active_threads++;
				active = 1;
			}
			char *name = file_dequeue(Q);
			Q->files_read++;
			pthread_mutex_unlock(&Q->fLock);
			WFDNode *new_node = createFileWFD(name, alphabet); 
			pthread_mutex_lock(&list->lock);
			WFDNodeLL_insert(list, new_node);
			pthread_mutex_unlock(&list->lock);
			
		} 
		else if (Q->capacity == 0){
			Q->active_threads--;
			if (Q->active_threads > 0){
				pthread_cond_wait(&Q->ready, &Q->fLock);
				pthread_mutex_lock(&Q2->dLock);
				if (Q2->capacity == 0){
					pthread_mutex_unlock(&Q->fLock);
					pthread_mutex_unlock(&Q2->dLock);
					return NULL;
				}	
				pthread_mutex_unlock(&Q2->dLock);
				Q->active_threads++;
				pthread_mutex_unlock(&Q->fLock);
				continue;
			}
			else {
				pthread_mutex_lock(&Q2->dLock);
				if (Q2->active_threads <= 0){
					pthread_cond_broadcast(&Q->ready);
					pthread_mutex_unlock(&Q->fLock);
					if (Q2->capacity == 0){
						pthread_mutex_unlock(&Q2->dLock);
						return NULL;
					}
					pthread_mutex_unlock(&Q2->dLock);
					pthread_mutex_lock(&Q->fLock);
					Q->active_threads++;
					pthread_mutex_unlock(&Q->fLock);
					continue;
				}
				pthread_mutex_unlock(&Q->fLock);
				pthread_mutex_unlock(&Q2->dLock);
				continue;
			}
		} 
	}
	return NULL;
}

/**
 * Computes the JSD values for each JSD value pairs.
 **/
void* computeJSD(void *argPtr) {
	struct a_arg *args = argPtr;

	unsigned range_start = args->range_start;
	unsigned range_end = args->range_end;
	char* alphabet = args->alphabet;
	JSDListArray* arr = args->jsdPtr;
	int m;
	for (m = range_start; m <= range_end; ++m) {
		pthread_mutex_lock(&arr->lock);
		createPairJSD(arr->pairList[m], alphabet, arr->pairList[m]->fileA, arr->pairList[m]->fileB);
		pthread_mutex_unlock(&arr->lock);
	}
	return NULL;

}
	
int main(int argc, char **argv){
	struct direct_queue *direct_Q = malloc(sizeof(struct direct_queue));
	if (!direct_Q) {
		perror("Malloc failed\n");
		exit(1);
	}

	direct_queue_init(direct_Q);
	struct file_queue *file_Q = malloc(sizeof(struct file_queue));
	if (!file_Q) {
		perror("Malloc failed\n");
		exit(1);
	}

	WFDNodeLL *list = malloc(sizeof(WFDNodeLL));
	if (!list) {
		perror("Malloc failed\n");
		exit(1);
	}

	char* alphabet = initializeAlphabet();

	WFDNodeLL_init(list);
	file_queue_init(file_Q);

	// Default arguments
	char *suffix = NULL;
	int fthreads = 1;
	int dthreads = 1;
	int athreads = 1;

	for (int i = 1; i < argc; i++){
		// Check for optional suffix argument
		if (strlen(argv[i]) > 2){
			if (argv[i][0] == '-' && argv[i][1] == 's'){
				int diff = strlen(argv[i]) - 2;
				suffix = malloc(diff + 1);
				for (int a = 2; a < strlen(argv[i]); a++){
					suffix[a - 2] = argv[i][a];
				}
				suffix[diff] = '\0';
			}
		}
		if (strlen(argv[i]) == 2){
			if (argv[i][0] == '-' && argv[i][1] == 's'){
				suffix = malloc(sizeof(char) * 1);
				suffix[0] = '\0';
			}
		}
	}
	if (suffix == NULL) {
		// Set default to .txt
		suffix = malloc(sizeof(char) * 5);
		//suffix = ".txt"; 
		suffix[0] = '.';
		suffix[1] = 't';
		suffix[2] = 'x';
		suffix[3] = 't';
		suffix[4] = '\0';
	}
	for (int i = 1; i < argc; i++){
		if (argv[i][0] == '.'){
			continue;
		}
		DIR *dirp = opendir(argv[i]);
		if (dirp != NULL){
			//printf("Found directory. Add in queue\n");
			struct directNode *new_direct = malloc(sizeof(struct directNode));
			if (!new_direct) {
				perror("Malloc failed\n");
				exit(1);
			}
			new_direct->next = NULL;

			new_direct->directName = malloc(sizeof(char)*(strlen(argv[i])+1));
			if (!new_direct->directName) {
				perror("Malloc failed\n");
				exit(1);
			}
			strcpy(new_direct->directName, argv[i]);
			direct_enqueue(new_direct, direct_Q); 
			closedir(dirp);
			continue;
		}
		// Check for optional thread arguments
		if (argv[i][0] == '-'){
			if (strlen(argv[i]) == 1){
				perror("invalid argument");
				abort();
			}
			else {
					int space;
					char* number = NULL;
					if (argv[i][1] == 'd'){
						space = strlen(argv[i]) - 2;
						if (space == 0){
							perror("Invalid\n");
							abort();
						}
						number = malloc(sizeof(char) * (space + 1));
						if (!number){
							perror("Malloc failed\n");
							exit(1);
						}
						for (int h = 0; h < space; h++){
							if (isdigit(argv[i][h + 2])){
								number[h] = argv[i][h + 2];
							}
							else { perror("Invalid\n"); abort(); }
						}
						number[space] = '\0';
						dthreads = atoi(number);
						if (dthreads <= 0){
							perror("Invalid argument\n");
							exit(1);
						}
						free(number);
					} else if (argv[i][1] == 'f'){
						space = strlen(argv[i]) - 2;
						if (space == 0){
							perror("Invalid\n");
							abort();
						}
						number = malloc(sizeof(char) * (space + 1));
						if (!number){
							perror("Malloc failed\n");
							exit(1);
						}
						for (int h = 0; h < space; h++){
							if (isdigit(argv[i][h + 2])){
								number[h] = argv[i][h + 2];
							}
							else { perror("Invalid\n"); abort(); }
						}
						number[space] = '\0';
						fthreads = atoi(number);
						if (fthreads <= 0){
							perror("Invalid argument\n");
							exit(1);
						}
						free(number);
					} else if (argv[i][1] == 'a'){
						space = strlen(argv[i]) - 2;
						if (space == 0){
							perror("Invalid\n");
							abort();
						}
						number = malloc(sizeof(char) * (space + 1));
						if (!number){
							perror("Malloc failed\n");
							exit(1);
						}
						for (int h = 0; h < space; h++){
							if (isdigit(argv[i][h + 2])){
								number[h] = argv[i][h + 2];
							}
							else { perror("Invalid\n"); abort(); }
						}
						number[space] = '\0';
						athreads = atoi(number);
						if (athreads <= 0){
							perror("Invalid argument\n");
							exit(1);
						}
						free(number);
					} else if (argv[i][1] == 's'){
						continue;
					} else { 
						perror("invalid optional argument");
						abort();
					}
				}
		}
		// Traverse through file and directory arguments. 
		else {
			int fd = open(argv[i], O_RDONLY);
			if (fd == -1){
				printf("invalid input\n");
				close(fd);
				continue;
			}	
			else { 
				if (suffix[0] == '\0'){
					//printf("add to queue\n");
					struct fileNode *new_file = malloc(sizeof(struct fileNode));
					if (!new_file) {
						perror("Malloc failed\n");
						exit(1);
					}
					new_file->fileName = malloc(sizeof(char)*(strlen(argv[i])+1));
					if (!new_file->fileName) {
						perror("Malloc failed\n");
						exit(1);
					}
					strcpy(new_file->fileName, argv[i]);
					new_file->next = NULL;
					file_enqueue(new_file, file_Q);
					continue;
				}
				int no_match = 0;
				int diff = strlen(argv[i]) - strlen(suffix);
				for (int z = strlen(suffix); z >= 0; z--){
					if (suffix[z] != argv[i][z + diff]){
						no_match = 1;
					}
				}
				if (no_match == 0){
					//printf("add to queue\n");
					struct fileNode *new_file = malloc(sizeof(struct fileNode));
					if (!new_file) {
						perror("Malloc failed\n");
						exit(1);
					}
					new_file->fileName = malloc(sizeof(char)*(strlen(argv[i])+1));
					if (!new_file->fileName) {
						perror("Malloc failed\n");
						exit(1);
					}
					strcpy(new_file->fileName, argv[i]);
					new_file->next = NULL;
					file_enqueue(new_file, file_Q);
					} else {
						//printf("Wrong suffix\n");
					} 
				close(fd);
			}	
		}
	}
	
	struct direct_arg *direct_args = malloc(sizeof(struct direct_arg) * dthreads);
	if (!direct_args) {
		perror("Malloc failed\n");
		exit(1);
	}

	pthread_t* dthreadIDs;
	dthreadIDs = malloc(sizeof(pthread_t)*dthreads);
	if (!dthreadIDs) {
		perror("Malloc failed\n");
		exit(1);
	}

	pthread_t* fthreadIDs;
	fthreadIDs = malloc(sizeof(pthread_t)*fthreads);
	if (!fthreadIDs) {
		perror("Malloc failed\n");
		exit(1);
	}

	// Start directory threads
	for (int i = 0; i < dthreads; i++){
		direct_args[i].input_Q = direct_Q;
		direct_args[i].second_Q = file_Q;
		direct_args[i].specified = suffix;
		pthread_create(&dthreadIDs[i], NULL, traverse, &direct_args[i]);
	}
	struct file_arg *file_args = malloc(sizeof(struct file_arg) * fthreads);
	if (!file_args) {
		perror("Malloc failed\n");
		exit(1);
	}
	// Start file threads
	for (int i = 0; i < fthreads; i++){
		file_args[i].input_Q = file_Q;
		file_args[i].second_Q = direct_Q;
		file_args[i].input_list = list;
		file_args[i].alphabet = alphabet;
		pthread_create(&fthreadIDs[i], NULL, computeWFD, &file_args[i]);
	} 
	// Join directory threads; join file threads
	for (int i = 0; i < dthreads; i++){
		pthread_join(dthreadIDs[i], NULL);	
	}
	for (int i = 0; i < fthreads; i++){
		pthread_join(fthreadIDs[i], NULL);
	}
	// Exit if there are not enough valid files (with the appropriate suffix) to compare
	if (file_Q->files_read < 2){
		perror("Not enough files\n");
		exit(1);
	}
	free(file_args);
	free(direct_args); 
	//printf("dthreads: %d\n", dthreads);
	//printf("athreads: %d\n", athreads);
	//printf("fthreads: %d\n", fthreads);

	pthread_mutex_destroy(&file_Q->fLock);
	pthread_cond_destroy(&file_Q->ready);

	unsigned numFiles = file_Q->files_read;

	free(file_Q);

	pthread_mutex_destroy(&direct_Q->dLock);
	pthread_cond_destroy(&direct_Q->ready);
	pthread_mutex_destroy(&list->lock);

	free(direct_Q);
	free(dthreadIDs);
	free(fthreadIDs);

	// Calculate number of pairs to initialize JSD array
	const unsigned numPairs = (numFiles * (numFiles - 1)) / 2;
	unsigned listIndex = 0;

	WFDNode *file1 = list->head;

	// ANALYSIS THREAD ACTIONS
   JSDNode** jsdPairList;
   jsdPairList = malloc(sizeof(JSDNode*) * numPairs);
   if (!jsdPairList) {
	   perror("Malloc failed\n");
	   exit(1);
   }

   int k;
   for (k = 0; k < numPairs; ++k) {
       jsdPairList[k] = malloc(sizeof(JSDNode));
	   if (!jsdPairList[k]) {
		   perror("Malloc failed\n");
		   exit(1);
	   }
   }
   
    // Initialize array struct
    // Each element points to a JSDNode ptr
   while (file1 != NULL) {
       WFDNode* file2 = file1->next;
       while (file2 != NULL) {
           initializeJSDArray(jsdPairList, listIndex, file1, file2);
           file2 = file2->next;
           ++listIndex;
       }
       file1 = file1->next;
   }

   JSDListArray* arr = malloc(sizeof(JSDListArray));
   if (!arr) {
	   perror("Malloc failed\n");
	   exit(1);
   }
   JSDListArray_init(arr, jsdPairList);

   struct a_arg *a_args;
   a_args = malloc(sizeof(struct a_arg) * athreads);
	if (!a_args) {
		perror("Malloc failed\n");
		exit(1);
	}
	pthread_t* athreadIDs;
	athreadIDs = malloc(sizeof(pthread_t)*athreads);
	if (!athreadIDs) {
		perror("Malloc failed\n");
		exit(1);
	}

	// Assign threads to non-overlapping intervals of comparison segments
	int quotient = numPairs / athreads;
	int remainder = numPairs % athreads;
	int isDivisible = 0;

	if (remainder == 0) { isDivisible = 1; }

	int p;
	int marker = 0;
	for (p = 0; p < athreads; ++p) {
		a_args[p].range_start = marker;
		a_args[p].range_end = marker + quotient - 1;
		a_args[p].alphabet = alphabet;
		a_args[p].jsdPtr = arr;
		if (isDivisible == 0) {
			if (p < remainder) {
				++a_args[p].range_end;
			}
		}
		marker = a_args[p].range_end + 1;
	}

	int q;
	int athread_counter = 0;
	for (q = 0; q < athreads; ++q) {
		if (a_args[q].range_end >= a_args[q].range_start) {
			++athread_counter;
			pthread_create(&athreadIDs[q], NULL, computeJSD, &a_args[q]);
		}
	}

	int i;
	for (i = 0; i < athread_counter; i++){
		pthread_join(athreadIDs[i], NULL);
	}

	// Insert array struct values into an ordered linked list for 
	// traversal
	JSDNode* jsdList = NULL;
	for (i = 0; i < numPairs; ++i) {
		jsdList = insertInOrder(jsdList, jsdPairList[i]);
	}
	traverseJSDList(jsdList);

	pthread_mutex_destroy(&arr->lock);
	free(a_args);
	free(athreadIDs);
	freeWFDList(list->head);
	free(list);
	free(alphabet);
    freeJSDListArray(jsdPairList, numPairs);
	free(arr);

	if (suffix[0] != '\0'){
		//printf("suffix is: %s\n", suffix);
	} else { 
		//printf("no suffix\n"); 
	}
	if (suffix != NULL){
		free(suffix);
	}
	return EXIT_SUCCESS;
}
