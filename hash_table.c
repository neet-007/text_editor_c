#include "hash_table.h"

unsigned long djb2(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; 
    }
    return hash;
}

unsigned long hash_function(char* str) {
    unsigned long hash = djb2(str);
    return hash % CAPACITY;
}

LinkedList *allocate_list() {
    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    return list;
}

LinkedList *linkedlist_insert(LinkedList *list, Ht_item *item) {
    if (!list) {
        LinkedList *head = allocate_list();
        head->item = item;
        head->next = NULL;
        list = head;
        return list;
    }
    else if (list->next == NULL) {
        LinkedList *node = allocate_list();
        node->item = item;
        node->next = NULL;
        list->next = node;
        return list;
    }

    LinkedList *temp = list;

    while (temp->next->next) {
        temp = temp->next;
    }

    LinkedList *node = allocate_list();
    node->item = item;
    node->next = NULL;
    temp->next = node;
    return list;
}

Ht_item *linkedlist_remove(LinkedList *list) {
    if (!list){
        return NULL;
    }

    if (!list->next){
        return NULL;
    }

    LinkedList *node = list->next;
    LinkedList *temp = list;
    temp->next = NULL;
    list = node;
    Ht_item *it = NULL;
    memcpy(temp->item, it, sizeof(Ht_item));
    free(temp->item->key);
    free(temp->item->value);
    free(temp->item);
    free(temp);
    return it;
}

void free_linkedlist(LinkedList *list) {
    LinkedList *temp = list;

    while (list) {
        temp = list;
        list = list->next;
        free_item(temp->item);
        free(temp);
    }
}

LinkedList **create_overflow_buckets(HashTable *table) {
    LinkedList **buckets = (LinkedList **)calloc(table->size, sizeof(LinkedList *));

    for (int i = 0; i < table->size; i++){
        buckets[i] = NULL;
    }

    return buckets;
}

void free_overflow_buckets(HashTable *table) {
    LinkedList **buckets = table->overflow_buckets;

    for (int i = 0; i < table->size; i++){
        free_linkedlist(buckets[i]);
    }

    free(buckets);
}

Ht_item *create_item(char *key, void *value, size_t value_size, ValueType value_type) {
    Ht_item *item = (Ht_item *)malloc(sizeof(Ht_item));
    if (!item) {
        perror("Failed to allocate memory for Ht_item");
        exit(EXIT_FAILURE);
    }

    item->key = (char *)malloc(strlen(key) + 1);
    if (!item->key) {
        perror("Failed to allocate memory for key");
        free(item);
        exit(EXIT_FAILURE);
    }
    strcpy(item->key, key);

    item->value = malloc(value_size);
    if (!item->value) {
        perror("Failed to allocate memory for value");
        free(item->key);
        free(item);
        exit(EXIT_FAILURE);
    }
    item->value_size = value_size;
    item->value_type = value_type;
    memcpy(item->value, value, value_size);

    return item;
}

HashTable *create_table(int size) {
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    table->size = size;
    table->count = 0;
    table->items = (Ht_item **)calloc(table->size, sizeof(Ht_item *));

    for (int i = 0; i < table->size; i++){
        table->items[i] = NULL;
    }

    table->overflow_buckets = create_overflow_buckets(table);

    return table;
}

void free_item(Ht_item *item) {
    switch (item->value_type) {
        case TYPE_BOOL:{
            free((bool *)item->value);
            break;
        }
        case TYPE_INT:
            free((int *)item->value);
            break;
        case TYPE_STR:{
            free((char *)item->value);
            break;
        }
        case TYPE_HASH_TABLE:{
            free_table((HashTable *)item->value);
            break;
        }
        default:{
            perror("wrong type for item");
            free(item->key);
            free(item);
            exit(1);
        }
    }
    free(item->key);
    free(item);
}

void free_table(HashTable *table) {
    for (int i = 0; i < table->size; i++) {
        Ht_item *item = table->items[i];

        if (item != NULL){
            free_item(item);
        }
    }

    free_overflow_buckets(table);
    free(table->items);
    free(table);
}

void handle_collision(HashTable *table, unsigned long index, Ht_item *item) {
    LinkedList *head = table->overflow_buckets[index];

    if (head == NULL) {
        head = allocate_list();
        head->item = item;
        table->overflow_buckets[index] = head;
        return;
    }

    table->overflow_buckets[index] = linkedlist_insert(head, item);
}

void ht_insert(HashTable *table, char *key, void *value, size_t value_size, ValueType value_type) {

    int index = hash_function(key);

    Ht_item *current_item = table->items[index];

    if (current_item == NULL) {
        Ht_item *item = create_item(key, value, value_size, value_type);
        if (table->count == table->size) {
            printf("Insert Error: Hash Table is full\n");
            free_item(item);
            return;
        }

        table->items[index] = item;
        table->count++;
    }
    else {
        if (strcmp(current_item->key, key) == 0) {
            if (table->items[index]->value_size < value_size){
                free(table->items[index]->value);

                table->items[index]->value = malloc(value_size);
                if (table->items[index]->value == NULL) {
                    fprintf(stderr, "Memory allocation failed\n");
                    return;
                }
            }
            memcpy(table->items[index]->value, value, value_size);
            table->items[index]->value_size = value_size;
            table->items[index]->value_type = value_type;
            return;
        }
        else {
            // TODO: FIX ME
            Ht_item *item = create_item(key, value, value_size, value_type);
            handle_collision(table, index, item);
            return;
        }
    }
}

Ht_item *ht_search(HashTable *table, char *key) {
    int index = hash_function(key);
    Ht_item *item = table->items[index];
    LinkedList *head = table->overflow_buckets[index];

    while (item != NULL) {
        if (strcmp(item->key, key) == 0){
            return item;
        }

        if (head == NULL){
            return NULL;
        }

        item = head->item;
        head = head->next;
    }

    return NULL;
}

void ht_delete(HashTable *table, char *key) {
    int index = hash_function(key);
    Ht_item *item = table->items[index];
    LinkedList *head = table->overflow_buckets[index];

    if (item == NULL) {
        return;
    }
    else {
        if (head == NULL && strcmp(item->key, key) == 0) {
            table->items[index] = NULL;
            free_item(item);
            table->count--;
            return;
        }
        else if (head != NULL) {
            if (strcmp(item->key, key) == 0) {
                free_item(item);
                LinkedList *node = head;
                head = head->next;
                node->next = NULL;
                table->items[index] = create_item(node->item->key, node->item->value, node->item->value_size, node->item->value_type);
                free_linkedlist(node);
                table->overflow_buckets[index] = head;
                return;
            }

            LinkedList *curr = head;
            LinkedList *prev = NULL;

            while (curr) {
                if (strcmp(curr->item->key, key) == 0) {
                    if (prev == NULL) {
                        free_linkedlist(head);
                        table->overflow_buckets[index] = NULL;
                        return;
                    }
                    else {
                        prev->next = curr->next;
                        curr->next = NULL;
                        free_linkedlist(curr);
                        table->overflow_buckets[index] = head;
                        return;
                    }
                }

                curr = curr->next;
                prev = curr;
            }
        }
    }
}

void print_item(Ht_item *item){
    switch (item->value_type) {
        case TYPE_BOOL:{
            printf("key %s val %d\n", item->key, (*(bool *)item->value));
            break;
        }
        case TYPE_INT:{
            printf("key %s val %d\n", item->key, (*(int *)item->value));
            break;
        }
        case TYPE_STR:{
            printf("key item %s val %s\n", item->key, (char *)item->value);
            break;
        }
        case TYPE_HASH_TABLE:{
            printf("key %s val \n", item->key);
            print_table((HashTable *)item->value, 0);
            break;
        }
        default:{
            printf("unknown type\n");
            break;
        }
    }
}

void print_table(HashTable *table, int indent) {
    char *indent_str = malloc((sizeof(char) * indent) + 1);
    if (indent_str == NULL){
        printf("could not allocate indent\n");
        exit(1);
    }
    int i = 0;
    for (i = 0; i < indent; i++){
        indent_str[i] = ' ';
    }
    indent_str[i] = '\0';

    printf("\n%sHash Table\n%s-------------------\n", indent_str, indent_str);

    for (int i = 0; i < table -> size; i++) {
        if (table -> items[i]) {
            print_item(table->items[i]);
        }
    }

    printf("%s-------------------\n\n", indent_str);
    if (indent_str != NULL){
        free(indent_str);
    }
}
