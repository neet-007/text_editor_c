#define CAPACITY 50000
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    TYPE_BOOL,
    TYPE_INT,
    TYPE_STR,
    TYPE_HASH_TABLE,
} ValueType;

typedef struct Ht_item{
    char *key;
    void *value;
    size_t value_size;
    ValueType value_type;
}Ht_item;

typedef struct LinkedList {
    Ht_item* item;

    struct LinkedList* next;
} LinkedList;;

typedef struct HashTable{
    Ht_item **items;
    LinkedList** overflow_buckets;
    int size;
    int count;
}HashTable;

unsigned long djb2(const char *str);
unsigned long hash_function(char *str);
Ht_item *create_item(char *key, void *value, size_t value_size, ValueType value_type);
HashTable* create_table(int size);
void free_item(Ht_item* item);
void free_table(HashTable* table);
void ht_insert(HashTable *table, char *key, void *value, size_t value_size, ValueType value_type);
void handle_collision(HashTable* table, unsigned long index, Ht_item* item);
Ht_item *ht_search(HashTable *table, char *key);
LinkedList* allocate_list();
LinkedList* linkedlist_insert(LinkedList* list, Ht_item* item);
Ht_item* linkedlist_remove(LinkedList* list);
void free_linkedlist(LinkedList* list);
LinkedList** create_overflow_buckets(HashTable* table);
void free_overflow_buckets(HashTable* table);
void ht_delete(HashTable* table, char* key);

void print_item(Ht_item *item);
void print_table(HashTable *table, int indent);
