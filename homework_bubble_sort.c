#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

typedef struct node
{
    struct node *r_node;
    unsigned value;
} node;

typedef struct hash_table
{
    node **list;
    unsigned nof_element;
} hash_table;

typedef struct parameterPass
{
    unsigned threadID;
    hash_table *ht;
    unsigned *numbers;
    unsigned numOfElements;
    unsigned numOfThreads;
    pthread_mutex_t *mutex;
    // gerekli sub-struct'ları buraya ekle.
} parameterPass;

unsigned countNumOfElements(char *filename)
{
    unsigned count = 0;
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Error opening file: %s\n", filename);
        return 0;
    }
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        count++;
    }
    fclose(file);
    return count;
}

unsigned *readNumbers(char *filename, unsigned num_elements)
{
    unsigned *numbers = (unsigned *)malloc(num_elements * sizeof(unsigned));
    if (numbers == NULL)
    {
        printf("Memory allocation failed.\n");
        return NULL;
    }
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Error opening file: %s\n", filename);
        free(numbers);
        return NULL;
    }
    char line[256];
    unsigned index = 0;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token != NULL)
        {
            unsigned number = atoi(token);
            numbers[index] = number;
            index++;
            // printf("%d", number);
        }
    }
    fclose(file);
    return numbers; // list of numbers
}

void swap(node *a, node *b)
{
    unsigned temp = a->value;
    a->value = b->value;
    b->value = temp;
}

void bubbleSort(node *start)
{
    int swapped;
    node *ptr1;
    node *lptr = NULL;

    if (start == NULL)
        return;

    do
    {
        swapped = 0;
        ptr1 = start;

        while (ptr1->r_node != lptr)
        {
            if (ptr1->value > ptr1->r_node->value)
            {
                swap(ptr1, ptr1->r_node);
                swapped = 1;
            }
            ptr1 = ptr1->r_node;
        }
        lptr = ptr1;
        printf("inside bubble sort\n"); // TODO delete
    } while (swapped);
    pthread_exit(NULL);
}

hash_table *initializeHashTable(unsigned numOfThread, unsigned numOfElements)
{
    hash_table *ht = (hash_table *)malloc(sizeof(hash_table));
    if (ht == NULL)
    {
        printf("Memory allocation failed.\n");
        return NULL;
    }

    ht->nof_element = numOfElements;
    unsigned numOfModularResults = (numOfThread * (numOfThread + 1)) / 2;

    ht->list = (node **)calloc(numOfModularResults, sizeof(node *));
    if (ht->list == NULL)
    {
        printf("Memory allocation failed.\n");
        free(ht);
        return NULL;
    }
    for (size_t i = 0; i < numOfModularResults; i++)
    {
        ht->list[i] = NULL;
    }

    return ht;
}

void *insertionFunction(void *parameters)
{
    parameterPass *params = (parameterPass *)parameters;
    unsigned threadID = params->threadID;
    hash_table *ht = params->ht;
    unsigned *numbers = params->numbers;
    unsigned numOfElements = params->numOfElements;
    unsigned numOfThreads = params->numOfThreads;
    pthread_mutex_t *mutex_ptr = params->mutex;

    unsigned interval = numOfElements / numOfThreads + 1;
    unsigned offset = interval * threadID;
    unsigned last_ele = offset + interval;
    if (last_ele > numOfElements)
        last_ele = numOfElements;

    unsigned numberOfBuckets = (numOfThreads * (numOfThreads + 1)) / 2;

    for (unsigned i = offset; i < last_ele; i++)
    {
        unsigned number = numbers[i];

        // Calculate the hash value based on the modular of (thread * (thread + 1)) / 2
        unsigned hash = number % numberOfBuckets;

        // Get the mutex according to the bucket
        pthread_mutex_t *mutex = &mutex_ptr[hash]; // TODO hatırla

        // Create a new node and insert it into the hash table
        node *new_node = (node *)malloc(sizeof(node));
        if (new_node == NULL)
        {
            printf("Memory allocation failed.\n");
            pthread_exit(NULL);
        }

        // Only one thread must be able to add a new node to a linked list at a time
        new_node->value = number;
        pthread_mutex_lock(mutex);
        new_node->r_node = ht->list[hash];
        ht->list[hash] = new_node;
        pthread_mutex_unlock(mutex);
    }

    pthread_exit(NULL);
}

void execute_insertion_function(pthread_t *threads, parameterPass *params, int numOfThreads, hash_table *ht, int *numbers, int numOfElements, pthread_mutex_t *mutex)
{
    for (unsigned i = 0; i < numOfThreads; i++)
    {
        params[i].threadID = i;
        params[i].ht = ht;
        params[i].numbers = numbers;
        params[i].numOfElements = numOfElements;
        params[i].numOfThreads = numOfThreads;
        params[i].mutex = mutex;
    }
    for (unsigned i = 0; i < numOfThreads; i++)
    {
        int result = pthread_create(&threads[i], NULL, insertionFunction, (void *)&params[i]);
        if (result != 0)
        {
            printf("Error creating thread %d\n", i);
        }
    }
}

void *execute_bubble_sort_helper_function(void *param)
{
    node *node_ptr = (node *)param;
    bubbleSort(node_ptr);
}

void execute_bubble_sort(pthread_t *threads, int numOfThreads, hash_table *ht)
{
    //
    for (unsigned i = 0; i < numOfThreads; i++)
    {
        int result = pthread_create(&threads[i], NULL, execute_bubble_sort_helper_function, (void *)(ht->list[i])); // TODO hatırla
        if (result != 0)
        {
            printf("Error creating thread %d\n", i);
        }
    }
}

void free_allocated_memory(pthread_t *threads, parameterPass *params, int *numbers, int numOfThreads, hash_table *ht)
{
    free(threads);
    free(params);
    free(numbers);
    for (unsigned i = 0; i < (numOfThreads * (numOfThreads + 1)) / 2; i++)
    {
        node *curr = ht->list[i];
        while (curr != NULL)
        {
            node *temp = curr;
            curr = curr->r_node;
            free(temp);
        }
    }
    free(ht->list);
    free(ht);
}

int main(int argc, char *argv[])
{
    // if (argc < 3)
    // {
    //     printf("Usage: %s <numbers.csv> <numOfThreads>\n", argv[0]);
    //     return 1;
    // }

    char *filename = "numbers.csv";
    unsigned numOfThreads = 8;

    // Count the number of elements in the file
    unsigned numOfElements = countNumOfElements(filename);
    if (numOfElements == 0)
    {
        printf("Error counting the number of elements.\n");
        return 1;
    }

    // Read the numbers from the file
    unsigned *numbers = readNumbers(filename, numOfElements);
    if (numbers == NULL)
    {
        printf("Error reading numbers from the file.\n");
        return 1;
    }

    // Initialize the hash table
    hash_table *ht = initializeHashTable(numOfThreads, numOfElements);
    if (ht == NULL)
    {
        printf("Error initializing the hash table.\n");
        free(numbers);
        return 1;
    }

    // Create threads and call insertionFunction
    int numberOfBuckets = (numOfThreads * (numOfThreads + 1)) / 2;
    pthread_t *threads1 = (pthread_t *)malloc(numOfThreads * sizeof(pthread_t));
    parameterPass *params = (parameterPass *)malloc(sizeof(parameterPass));
    pthread_mutex_t *mutex_ptr = (pthread_mutex_t *)malloc(numberOfBuckets * sizeof(parameterPass));

    // initialize mutex to be used while adding numbers
    for (size_t i = 0; i < numberOfBuckets; i++)
    {
        // pthread_mutex_t mutex;
        // mutex_ptr[i] = mutex;
        pthread_mutex_init(&(mutex_ptr[i]), NULL);
    }

    execute_insertion_function(threads1, params, numOfThreads, ht, numbers, numOfElements, mutex_ptr);

    printf("Just before joining insertion threads\n");
    // Wait for threads to finish
    for (unsigned i = 0; i < numOfThreads; i++)
    {
        pthread_join(threads1[i], NULL);
    }
    pthread_t *threads2 = (pthread_t *)malloc(numberOfBuckets * sizeof(pthread_t));
    printf("Just after insertion function in main\n");
    // Sort the hash table
    execute_bubble_sort(threads2, numberOfBuckets, ht);

    printf("Just before joining bubble sort threadsn\n");
    for (unsigned i = 0; i < numberOfBuckets; i++)
    {
        pthread_join(threads2[i], NULL);
    }
    printf("Just after joining bubble sort threadsn\n");
    // Print the sorted hash table
    for (unsigned i = 0; i < numberOfBuckets; i++)
    {
        printf("List %u: ", i);
        node *curr = ht->list[i];
        while (curr != NULL)
        {
            printf("%u ", curr->value);
            curr = curr->r_node;
        }
        printf("\n");
    }

    free_allocated_memory(threads1, params, numbers, numOfThreads, ht);
    return 0;
}
