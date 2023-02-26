#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <math.h>

#ifndef POSSIBLE_CHARS
#define POSSIBLE_CHARS 37
#endif

typedef struct TrieNode {
    struct TrieNode* child[POSSIBLE_CHARS];
    unsigned count;
    double frequency;
    int endOfWord; // signals end of word (0 not end of word, 1 end of word)
} TrieNode;

typedef struct CombinedTrieNode {
    struct CombinedTrieNode* child[POSSIBLE_CHARS];
    unsigned countA;
    unsigned countB;
    double frequencyA;
    double frequencyB;
    double avgFreq;
    int endOfWord; // signals end of word (0 not end of word, 1 end of word)
} CombinedTrieNode;

typedef struct WFDNode {
    struct TrieNode *trieRoot;
    char* filename;
    int wordCount;
    struct WFDNode *next;
} WFDNode;

typedef struct JSDNode {
    WFDNode* fileA;
    WFDNode* fileB;
    double JSD;
    int combinedWC;
    struct JSDNode* next;
} JSDNode;

/**
 * Initialize the trie alphabet for mapping (saves memory)
 **/
char* initializeAlphabet() {
    char *alphabet = malloc(sizeof(char) * 37);
    if (!alphabet) {
        return NULL;
    }
    
    int i;
    alphabet[0] = '-';

    for (i = 1; i < 11; ++i) {
        alphabet[i] = (char) (i + 47);
    }

    for (i = 11; i < 37; ++i) {
        alphabet[i] = (char) (i + 86);
    }
    return alphabet;
}

/**
 * Initialize Trie node
 **/
TrieNode* initializeTrie(char* alphabet) {
    TrieNode* node = malloc (sizeof(TrieNode));
    if (node == NULL) {
        fprintf(stderr, "Memory could not be allocated\n");
        return NULL;
    }
    node->endOfWord = 0;
    node->count = 0;
    node->frequency = 0;

    int i;

    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        node->child[i] = NULL;
    }
    return node;
}

/**
 * Look up the corresponding index based on a char in the alphabet map.
 **/
int lookupIndex(char c, char* alphabet) {
    int i;
    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        if (c == alphabet[i]) {
            return i;
        }
    }
    return 0;
}

/**
 * Look up a character from a value in the alphabet map.
 **/
char lookupChar(int index, char* alphabet) {
    return alphabet[index];
}

/**
 * Insert word into a trie structure.
 **/
void insert(TrieNode** root, char** alphabet, char* word, const int word_len, int charCounter) {
    if (*root == NULL) {
        *root = initializeTrie(*alphabet);
    }

    if (charCounter < word_len) {
        // Look up in alphabet
        int index = lookupIndex(*word, *alphabet);
        insert(&((*root)->child[index]), alphabet, word + 1, word_len, charCounter+1);
    } else {
        if ( (*root)->endOfWord) {
            ++( (*root)->count);
        } else {
            (*root)->endOfWord = 1;
            (*root)->count = 1;
        }
    }
}

/**
 * Sets the word frequencies of a trie struct.
 */
void setFrequency(TrieNode* root, char* alphabet, char str[], int level, const int wordCount) {
    if ((root->endOfWord) == 1) {
        str[level] = '\0';
        // Add frequency
        if (wordCount == 0) {
            root->frequency = (double) 0;
        } else if (wordCount != 0) {
            root->frequency = (double) (root->count) / (double) wordCount;
        }
    }

    int i;
    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        if (root->child[i]) {
            // Look up in alphabet
            char c = lookupChar(i, alphabet);
            str[level] = c;
            setFrequency(root->child[i], alphabet, str, level+1, wordCount);
        }
    }
}

/**
 * Traverses trie structure
 **/
void traverseTrie(TrieNode* root, char* alphabet, char str[], int level, const int wordCount) {
    if ((root->endOfWord) == 1) {
        str[level] = '\0';
    }

    int i;
    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        if (root->child[i]) {
            // Look up in alphabet
            char c = lookupChar(i, alphabet);
            str[level] = c;
            traverseTrie(root->child[i], alphabet, str, level+1, wordCount);
        }
    }
}

/**
 * Frees trie structure.
 **/
void freeTrie(TrieNode* ptr) {
    int i;
    if (!ptr) {
        return;
    }
    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        freeTrie(ptr->child[i]);
    }
    free(ptr);
}

/**
 * Initialize the Combined Trie Node
 **/
CombinedTrieNode* initializeCombinedTrie(char* alphabet) {
    CombinedTrieNode* node = malloc (sizeof(CombinedTrieNode));
    if (node == NULL) {
        fprintf(stderr, "Memory could not be allocated\n");
        return NULL;
    }
    node->endOfWord = 0;
    node->countA = 0;
    node->countB = 0;
    node->frequencyA = 0;
    node->frequencyB = 0;
    node->avgFreq = 0;

    int i;

    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        node->child[i] = NULL;
    }
    return node;
}

/**
 * Insert trie values into combined trie
 **/
void insertCombinedTrie(CombinedTrieNode** c_root, TrieNode** trieRoot, char** alphabet, char* word, const int word_len, int charCounter, const int file_type) {
    if (*c_root == NULL) {
        *c_root = initializeCombinedTrie(*alphabet);
    }

    if (charCounter < word_len) {
        // Look up in alphabet
        int index = lookupIndex(*word, *alphabet);
        insertCombinedTrie(&((*c_root)->child[index]), trieRoot, alphabet, word + 1, word_len, charCounter+1, file_type);
    } else {
        if ( (*c_root)->endOfWord) {
            if (file_type == 0) {
                (*c_root)->countA = (*trieRoot)->count;
                (*c_root)->frequencyA = (*trieRoot)->frequency;
            } else if (file_type == 1) {
                (*c_root)->countB = (*trieRoot)->count;
                (*c_root)->frequencyB = (*trieRoot)->frequency;
            }
        } else {
            (*c_root)->endOfWord = 1;
            if (file_type == 0) {
                (*c_root)->countA = (*trieRoot)->count;
                (*c_root)->frequencyA = (*trieRoot)->frequency;
            } else if (file_type == 1) {
                (*c_root)->countB = (*trieRoot)->count;
                (*c_root)->frequencyB = (*trieRoot)->frequency;
            }            
        }
    }
}

/**
 * Merge two tries together
 **/
void trieToCombinedTrie(CombinedTrieNode* c_root, TrieNode* root, char* alphabet, char str[], int level, const int file_type) {
    if ((root->endOfWord) == 1) {
        str[level] = '\0';
        if (file_type == 0) {
            insertCombinedTrie(&c_root, &root, &alphabet, str, strlen(str), 0, 0);
        } else if (file_type == 1) {
            insertCombinedTrie(&c_root, &root, &alphabet, str, strlen(str), 0, 1);      
        }
    }

    int i;
    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        if (root->child[i]) {
            // Look up in alphabet
            char c = lookupChar(i, alphabet);
            str[level] = c;
            if (file_type == 0) {
                trieToCombinedTrie(c_root, root->child[i], alphabet, str, level+1, 0);
            } else if (file_type == 1) {
                trieToCombinedTrie(c_root, root->child[i], alphabet, str, level+1, 1);
            }
        }
    }
}

/**
 * Traverses combined trie structure
 **/
void traverseCombinedTrie(CombinedTrieNode* root, char* alphabet, char str[], int level, int combinedWC) {
    if ((root->endOfWord) == 1) {
        str[level] = '\0';
        root->avgFreq = (double) (0.5 * (root->frequencyA + root->frequencyB));
    }

    int i;
    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        if (root->child[i]) {
            // Look up in alphabet
            char c = lookupChar(i, alphabet);
            str[level] = c;
            traverseCombinedTrie(root->child[i], alphabet, str, level+1, combinedWC);
        }
    }
}

/**
 * Frees combinedtrie structure.
 **/
void freeCombinedTrie(CombinedTrieNode* ptr) {
    int i;
    if (!ptr) {
        return;
    }
    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        freeCombinedTrie(ptr->child[i]);
    }
    free(ptr);
}

/**
 * Initialize WFDNode
 **/ 
WFDNode* initializeWFD(TrieNode** root, char* filename, int wordCount) {
    WFDNode* node = malloc (sizeof(WFDNode));
    if (node == NULL) {
        fprintf(stderr, "Memory could not be allocated\n");
        return NULL;
    }
    node->trieRoot = *root;
    node->filename = filename;
    node->wordCount = wordCount;
    node->next = NULL;
    return node;
}

/**
 * Insert a WFD Node into list
 **/
WFDNode* insertWFD(WFDNode* head, WFDNode* new_node) {
    if (new_node != NULL) {
        new_node->next = head;
    }
    head = new_node;
    return head;
}

/**
 * Used to check if char in word is a valid character.
 **/
int checkRegex(char* letter) {
    regex_t regex;
    int val;

    val = regcomp(&regex, "[[:alnum:]|\\-]", 0);
    if (val) {
        fprintf(stderr, "Regex could not be compiled.\n");
        exit(1);
    }

    val = regexec(&regex, letter, 0, NULL, 0);
    regfree(&regex);

    if (val == 0) { return 1; }

    return 0;
}

/**
  * Push word to data structure when whitespace is hit
  * Regex out non-alphanumeric/hyphens
  **/
int tokenize(int input_fd, char** alphabet, TrieNode** root) {

    int bytes, word_len = 0;
    char buf[100];
    char *stash;
    int prevWS = 0;
    int wordCount = 0;
    int notEmpty = 0;

    stash = malloc(sizeof(char));
    if (stash == NULL) {
        return -1;
    }

    while ((bytes = read(input_fd, buf, 100)) > 0) {
        notEmpty = 1;
        int i;
        int buf_position = 0;

        for (i = word_len; buf_position < bytes; ++i) {
            ++word_len;
            char *p = realloc(stash, sizeof(char) * word_len);
            if (!p) return 1;
            stash = p;

            if (isspace(buf[buf_position])) {   // Check for whitespace
                if (prevWS == 0) {
                    insert(root, alphabet, &stash[0], word_len-1, 0);
                    ++wordCount;
                }
                memset(stash, 0, sizeof(char) * word_len); 
                word_len = 0;
                i = word_len-1;
                prevWS = 1;
            } else {
                // Check regex char
                int validChar;

                char tempStr[2];
                tempStr[0] = buf[buf_position];
                tempStr[1] = '\0';
                validChar = checkRegex(tempStr);

                if (validChar == 1) {
                    stash[i] = tolower(buf[buf_position]);
                } else {
                    --word_len;
                    --i;
                }
                prevWS = 0;
            }
            ++buf_position;
        }
    }

    if (notEmpty == 1) {
        if (prevWS == 0) { 
            insert(root, alphabet, &stash[0], word_len-1, 0);
            ++wordCount;
        }
    }

    free(stash);

    if (bytes < 0) {
        perror("Read error");
        return -1;
    }

    return wordCount;
}

/**
 * WFD Driver
 **/
WFDNode* createFileWFD(char* filename, char* alphabet) {
    TrieNode* root = initializeTrie(alphabet);

    // Read file name
    int input_fd;

    input_fd = open(filename, O_RDONLY); 
    if (input_fd == -1) { 
        perror(filename);
        return NULL; 
    }

    int wordCount = tokenize(input_fd, &alphabet, &root);

    close(input_fd);

    if (wordCount == -1) {
        return NULL;
    }

    char str[100];
    int level = 0;

    setFrequency(root, alphabet, str, level, wordCount);

    // Create WFD Struct from Trie
    WFDNode *wfd = initializeWFD(&root, filename, wordCount);
    return wfd;
}

/**
 * Traverses WFD List.
 */
void traverseWFDList(WFDNode* head) {
    WFDNode* ptr;
    if (head == NULL) {
        printf("List is empty\n");
        return;
    }
    ptr = head;
    while (ptr != NULL) {
        printf("WFD: %s\n", ptr->filename);
        ptr = ptr->next;
    }
}

/**
 * Free individual WFD node.
 **/
void freeWFD(WFDNode* ptr) {
    freeTrie(ptr->trieRoot);
    free(ptr->filename);
    free(ptr);
}

/**
 * Free WFD list.
 **/
void freeWFDList(WFDNode* head) {
    WFDNode* ptr;

   while (head != NULL) {
       ptr = head;
       head = head->next;
       freeWFD(ptr);
    }
}

/**
 * Initialize JSD array struct.
 * Array will be iterated through to create ordered LL once JSDs have been calculated.
 **/
void initializeJSDArray(JSDNode** jsdPairList, int index, WFDNode* file1, WFDNode* file2) {
    jsdPairList[index]->fileA = file1;
    jsdPairList[index]->fileB = file2;
    jsdPairList[index]->combinedWC = file1->wordCount + file2->wordCount;
    jsdPairList[index]->JSD = 0;
    jsdPairList[index]->next = NULL;
}

/**
 * Set a JSD node
 **/
JSDNode* setJSD(JSDNode* node, WFDNode* fileA, WFDNode* fileB, double jsd, int combinedWC) {
    node->fileA = fileA;
    node->fileB = fileB;
    node->JSD = jsd;
    node->combinedWC = combinedWC;
    node->next = NULL;
    return node;
}

/**
 * Perform an ordered insertion for a JSDNode (descending order of WC).
 **/
JSDNode* insertInOrder(JSDNode* head, JSDNode* new_node) {
    if (head == NULL) { return new_node; }

    JSDNode* prev = NULL;
    JSDNode* ptr = head;
    while (ptr != NULL) {
        if (prev == NULL) {
            if (ptr->combinedWC <= new_node->combinedWC) {
                new_node->next = ptr;
                return new_node;
            }
        } else {
            if (ptr->combinedWC <= new_node->combinedWC) {
                new_node->next = ptr;
                prev->next = new_node;
                return head;
            }
        }
        prev = ptr;
        ptr = ptr->next;
    }
    prev->next = new_node;
    return head;
}

/**
 * Traverse the JSD list
 **/
void traverseJSDList(JSDNode* head) {
    JSDNode* ptr;
    if (head == NULL) {
        printf("List is empty\n");
        return;
    }
    ptr = head;
    while (ptr != NULL) {
        printf("%f %s %s\n", ptr->JSD, ptr->fileA->filename, ptr->fileB->filename);
        ptr = ptr->next;
    }
}

/**
 * KLD Calculation
 **/
double* KLD(CombinedTrieNode* root, char str[], int level, const int file_type, double* kld) {
    if ((root->endOfWord) == 1) {
        if (file_type == 0) {
            if (root->frequencyA != 0) {
                *kld += (root->frequencyA * log2((root->frequencyA)/(root->avgFreq)));
            }
        } else if (file_type == 1) {
            if (root->frequencyB != 0) {
                *kld += (root->frequencyB * log2((root->frequencyB)/(root->avgFreq)));
            }
        }
    }

    int i;
    for (i = 0; i < POSSIBLE_CHARS; ++i) {
        if (root->child[i]) {
            str[level] = i + '-';
            KLD(root->child[i], str, level+1, file_type, kld);
        }
    }
    return kld;
}

/**
 * JSD Driver
 **/
void createPairJSD(JSDNode* node, char* alphabet, WFDNode* file1, WFDNode* file2) {
    int level = 0;
    char str[100];
    double JSD = 0;
    double* kldA;
    double* kldB;
    //JSDNode* new_node = NULL;
    CombinedTrieNode* ct = initializeCombinedTrie(alphabet);
    int combinedWC = node->combinedWC;
        
    // Combine WFD tries
    trieToCombinedTrie(ct, file1->trieRoot, alphabet, str, level, 0);
    trieToCombinedTrie(ct, file2->trieRoot, alphabet, str, level, 1);

    // Set average frequencies
    traverseCombinedTrie(ct, alphabet, str, level, combinedWC);
        
    // Compute KLDs for JSD Computation
    kldA = malloc(sizeof(double));
    if (kldA == NULL) {
        exit(1);
    }

    kldB = malloc(sizeof(double));
    if (kldB == NULL) {
        exit(1);
    }
    *kldA = 0;
    *kldB = 0;
    if (combinedWC != 0) {
        // Calculate frequencies if non-empty files
        kldA = KLD(ct, str, level, 0, kldA);
        kldB = KLD(ct, str, level, 1, kldB);

        // Compute JSD
        JSD = sqrt((0.5 * (*kldA)) + (0.5 * (*kldB)));
    }
    node = setJSD(node, file1, file2, JSD, combinedWC);
        
    freeCombinedTrie(ct);
    free(kldA);
    free(kldB);
}

/**
 * Frees contents of JSD list
 **/
void freeJSDList(JSDNode* head) {
    JSDNode* ptr;

   while (head != NULL) {
       ptr = head;
       head = head->next;
       free(ptr);
    }
}

/**
 * Frees contents of array containing JSD structs
 **/
void freeJSDListArray(JSDNode** list, int size) {
    int i;
    for (i = 0; i < size; ++i) {
        free(list[i]);
    }
    free(list);
}