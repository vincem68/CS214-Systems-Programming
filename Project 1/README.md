This is a project I worked with another person on. This is a plagiarism detector that, given an algorithm on how to compute how similar two files were, and
given a file path to a directory, would compare each file in the directory to one another and give a list of how similar each file was to each other. If there
was a subdirectory within the given directory, the program would take all the files in there and compute them as well, and will keep going until all files within
every subdirectory was computed. This project also makes the use of threading to keep track of what files and directories still need to be looked at.

# Plagiarism Detector

By Riyam Zaman (rnz8) and Vincent Mandola (vam91)

## Compilation Instructions
Run `make compare`.
## Algorithm
### Collection Phase
#### Directory and File Queues
We first add every initial file and directory the user inputs and check to make sure they have the correct suffix and add them to their queues. After every argument is examined, we have the file and directory threads run concurrently through unbounded queues. The directory thread repeatedly checks the directory queue to see if there are any nodes in the queue, dequeues one if there are, and then checks if the dequeued directory has other files and directories, and add them to their respective queues, and all waiting threads will terminate once the last thread checks that there is nothing left to dequeue and all other threads are waiting. The file thread also repeatedly checks to see if the file queue has items to get, and stores them in the WFD repository, and also checks the directory queue to make sure if there are any threads running to make sure that no files are missing if all file threads are waiting.
#### Constructing the File Word Frequency Distribution (WFD)
To solve the JSD computation for each pair of files from the input, we first compute each applicable file's word frequency distribution. First, we construct a trie from tokenizing the words of a file. We chose to use a trie data structure because word insertion in such a structure is inherently alphabetized, which greatly simplifies the later JSD calculation. Each trie struct contains the occurrences of a given word (count) for all of the words in the file and the frequency of each word after all of the file's words have been accounted for. Once a file trie is constructed, its WFD is stored in a WFD repository (linked list of WFD structs), which includes the root of the file trie, the file name, and the word count.

#### Analysis Phase
For the analysis phase, we divide the computational work across multiple threads by creating even (or near-even) non-overlapping intervals of indices that correspond to a file pair (which is located in an array of JSD structs to be set). For each interval, a thread iterates through this array of file pair structs that contain the WFD of each file in a given pair. For each pair (A, B), the tries that the WFD structs point to are merged together in the form of a combined trie struct, which holds information about the occurrences of words across both files for each file and the corresponding frequencies. Once the tries are merged, the average frequencies of each word in the combined distribution are calculated. This is a simple computation because the words, by default, have been inserted in lexicographic order. From there, the Kullbeck-Leibler Divergence (KLD) values for each file in the pair are calculated from traversing through the merged trie using equation (2) of the project description. The JSD of the pair is computed using equation (3) of the project description.

#### JSD Storage and Output
The JSD values for each file pair are stored in an array of JSD structs of size n(n-1)/2 to represent all of the file pairs. When a JSD node is created, it is inserted into the list in descending order with respect to the combined word count. Finally, each node contains the name of file A, the name of file B, the JSD, and the combined word count, which form the basis of the output of this program.

## Testing Strategy
Our testing strategy involved breaking the program up into modularized components, testing those components, and iteratively adding components for further testing to ensure that all of the modules functioned cohesively in the full program. We also made sure to error check in the event of process/thread or malloc failures. 

Below we will outline how we tested the program components.

### User Interface
#### Regular Arguments
To ensure baseline functionality in our program, we tested different input arguments, including:
- One file path --> Invalid
- Invalid file/directory path names --> Invalid and exit program
- Two or more regular file paths --> Successfully computed JSD 
- Directory paths that began with a "." --> Ignored
- Directory paths that contained subdirectories
- A combination of file and directory paths

#### Optional Arguments
There were four optional arguments that we tested for, where *N* is the user input argument:
- -d*N* : We checked to make sure the numbers were positive and only contained digits. 
- -s*N* : We looked through all arguments to see if there was a specified suffix, and stored it to use as reference for the rest of the program. We also ensured that there were at least two valid files contained within the arguments that matched the desired suffix before proceeding with the WFD/JSD computations. We also allow for the user to input an empty suffix, which traverses all file types within the regular arguments.
- -f*N* : Like the -d arguments, we checked to make sure that the numbers were positive and only contained digits.
- -a*N* : For the analysis threads, we ensured to spawn threads that had a maximum of the number of file pairs for analysis because the threads were designed to do non-overlapping segments of work, which in this case were the JSD computations represented by the file pairs located in the JSD array struct. We also ensured a relatively even division of work across the valid threads.
With the optional arguments, we ensured that the program could handle high thread counts without deadlocking or interfering with any of the computational processes. Optional arguments may be placed in any order relative to the regular arguments.

### Word Frequency Distribution
#### Tokenization
For tokenization, the testing primarily involved examining different file inputs and ensuring that all words--which were defined as being separated by whitespace and only including alphanumeric characters--were properly parsed, converted to lowercase characters, and inserted into the file trie structure. We ensured that the file word count approximated the word count found by word counting programs.

Sample text file content:
`I 1ike this book.case from 1824. 197-year-old marvel, isn't it?`
would be tokenized like so:
`I`, `like`, `this`, `bookcase`, `from`, `1824`, `197-year-old`, `marvel`, `isnt`, `it`

#### Trie Structure
To test that the trie structure was saving all of the information that we needed, we traversed the trie to print out all of the words, occurrences, and frequencies (occurrence of word / file word count). The occurrence and frequency information were stored in the leaf node for each word.

#### Kullbeck-Leibler Divergence (KLD) --> Jensen-Shannon Distance (JSD)
To test the accuracy of these calculations within the program, we ensured that the variables within the formula were accurate and were accounted for. We tested using two non-empty files, two empty files, and one non-empty and one empty file and verified those results. We also tested with two identical non-empty files.

#### Output
To ensure that our output showed the JSD values and file names in the correct descending total word count order, we checked and printed out the combined word counts.

Sample set of text files with word counts: `ch1.txt: 1714`, `ch2.txt: 4053`, `test.txt: 13`, `full-text.txt: 1`, `subdir1/test1.txt: 0`, `empty.txt: 0`

have the following output:

`0.592883 ch1.txt ch2.txt`

`0.939404 test.txt ch2.txt`

`1.000000 full-text.txt ch2.txt`

`0.707107 ch2.txt empty.txt`

`0.707107 ch2.txt subdir1/test1.txt`

`0.920467 test.txt ch1.txt`

`1.000000 full-text.txt ch1.txt`

`0.707107 ch1.txt empty.txt`

`0.707107 ch1.txt subdir1/test1.txt`

`1.000000 full-text.txt test.txt`

`0.707107 test.txt empty.txt`

`0.707107 test.txt subdir1/test1.txt`

`0.707107 full-text.txt empty.txt`

`0.707107 full-text.txt subdir1/test1.txt`

`0.000000 subdir1/test1.txt empty.txt`
