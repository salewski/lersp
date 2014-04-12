/**
 * Lersp.
 *
 * 2014 (c) eddieantonio.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "lersp.h"

static char INTRO_BANNER[] =
    "; Lersp\n"
    "; 2014 (c) eddieantonio.\n"
    "; We may never know why.\n";

/* Points to the next free cell in the free list. */
static sexpr *next_free_cell;
sexpr heap[HEAP_SIZE];

/* Points to the cell where the universe starts.  */
static sexpr *global = NULL; // execution context?

/* List of (identifier . name) pairs. */
static sexpr *name_list = NULL;
/* Amount of symbos left to use. */
static int symbols_left = MAX_NAMES;


/*
 * A lisp interpreter, I guess.
 */
int main(int argc, char *argv[]) {
    init();

    if (argc < 2) {
        puts(INTRO_BANNER);
        repl();
    }

    /* TODO: I guess, interpret a file or something. */

    return 0;
}



void repl(void) {
    result input;
    result evaluation;

    /* Development stuff. */
    printf("; sizeof sexpr: %ld\n", sizeof(sexpr));


    while (1) {
        printf("#=> ");
        input = l_read();

        if (input.status != OKAY) {
            break;
        }

        print(input.expr);

        /* PSYCH! */
        continue;
        evaluation = eval(input.expr);

        if (evaluation.status == OKAY) {
            printf("; ");
            print(evaluation.expr);
            puts("");
        } else {
            break;
        }
    }

}

void display_list(sexpr*);

void print(sexpr *expr) {
    printf(";=> ");
    display(expr);
    puts("");
}

void display(sexpr* expr) {
    if (expr == NULL) {
        printf("NIL");
        return;
    }

    switch (expr->type) {
        case NUMBER:
            printf("%g", expr->number);
            break;

        case SYMBOL:
            printf("%s", lookup(expr->symbol));
            break;

        case FUNCTION:
            /* A function is just a cons-cell. */
            /* FALL THROUGH! */
        case CONS:
            display_list(expr);
            break;
        case WORD:
            /* DEBUG! `word` type is internal and not representable in program
             * text. */
            printf("\033[1;7;43;34m%s\033[0m", expr->word);
            break;
        default:
            assert(0);
    }
}

void display_list(sexpr *head) {
    sexpr *current = head;
    assert(head->type == CONS);

    printf("(");
    do {
        display(current->car);
        if (current->cdr != NULL) {
            printf(" ");
        }

        current = current->cdr;
    } while ((current != NULL) && (current->type == CONS));

    if (current != NULL) {
        /* This must be the end of an improper list. */
        printf(". ");
        display(current);
    }

    printf(")");
}

char *lookup(l_symbol symbol) {
    sexpr *entry = name_list;
    sexpr *name_pair, *identifier, *name;

    assert(name_list != NULL);

    do {
        name_pair = entry->car;
        assert(name_pair->type == CONS);

        identifier = name_pair->car;
        name = name_pair->cdr;

        assert(identifier->type == SYMBOL);

        if (identifier->symbol == symbol) {
            assert(name->type == WORD);
            return name->word;
        }

        entry = entry->cdr;
    } while (entry != NULL);

    printf("Could not find symbol: %d\n", symbol);
    assert(0);

    return NULL; /* For type-checking's sake. */
}




static void prepare_free_list(void);
static void prepare_execution_context(void);

void init(void) {
    prepare_free_list();
    prepare_execution_context();
}

static void prepare_free_list(void) {
    sexpr *last_value = NULL;

    /* Link every cell to the next in the free list. */
    for (int i = HEAP_SIZE; i >= 0; i--) {
        sexpr *cell = heap + i;

        cell->type = CONS;
        cell->car = NULL;
        cell->cdr = last_value;
        cell->reached = false;

        last_value = cell;
    }

    next_free_cell = last_value;
}

static l_symbol insert_symbol(char *name) {
    sexpr *pair, *identifier, *word;

    assert(strlen(name) < NAME_LENGTH);

    identifier = new_cell();
    identifier->type = SYMBOL;
    identifier->symbol = --symbols_left;

    if (!symbols_left) {
        fprintf(stderr, "Ran out of symbol memory. (Sorry %s) :C\n", name);
        fprintf(stderr, "Symbol dump: ");
        print(name_list);
        exit(2);
    }

    word = new_cell();
    word->type = WORD;
    strncpy(word->word, name, NAME_LENGTH);

    pair = cons(identifier, word);

    name_list = cons(pair, name_list);

    return identifier->symbol;
}


static char* INITIAL_SYMBOLS[] = {
    "F", "T", "DEFINE", "LABEL", "LAMBDA",
    "EVAL", "APPLY", "QUOTE",
    "CONS", "CAR", "CDR",
    "EQ", "NULL", "NOT", "AND", "OR",
    "+", "-", "/", "*", "<", ">",
    "MAP", "REDUCE"
};

static void insert_initial_symbols(void) {
    int i;
    for (i = 0; i < sizeof(INITIAL_SYMBOLS)/sizeof(char**); i++) {
        insert_symbol(INITIAL_SYMBOLS[i]);
    }
}

static void prepare_execution_context(void) {
    /* IT BEGINS! */
    //global = cons();

    insert_initial_symbols();
    /* TODO: NOT THIS: */
    global = name_list;
}



static int mark_all_reachable_cells(void) {
    int count = 0;
    sexpr *cell = global;

    assert(global != NULL);

    do {
        cell->reached = true;
        cell = cell->cdr;
        count++;
    } while (cell != NULL);

#if GC_DEBUG
    printf("Reached %d cells (%d total)\n", count, HEAP_SIZE);
#endif

    return count;
}


static int garbage_collect(void) {
    int freed;

#if GC_DEBUG
    puts("Garbage collecting...");
#endif

    mark_all_reachable_cells();

    /* For reached cells, unmark 'em. For unreached cells, return 'em to the
     * free list. */
    for (int i = 0; i < HEAP_SIZE; i++) {
        sexpr *cell = heap + i;

        if (!cell->reached) {
            /* Return the cell to the free list. */
            cell->cdr = next_free_cell;
            next_free_cell = cell->cdr;
            freed++;
        } else {
            cell->reached = false;
        }
    }

#if GC_DEBUG
    printf("Freed %d cells\n", freed);
#endif
    return freed;
}


sexpr *cons(sexpr* car, sexpr* cdr) {
    sexpr *cell = new_cell();
    cell->type = CONS;

    cell->car = car;
    cell->cdr = cdr;

    return cell;
}


sexpr *new_cell(void) {
    sexpr* cell = next_free_cell;

    if (cell == NULL) {
        garbage_collect();
        cell = next_free_cell;
        if (cell == NULL)  {
            fprintf(stderr, "Ran out of cells in free list.\n");
            exit(-1);
        }
    }

    next_free_cell = cell->cdr;

    return cell;
}

static void parse_symbol(char *);

/* TODO: Consider making a tokenizer... */
result l_read(void) {
    static int depth = 0;

    result parse;
    char c;

    parse.status = OKAY;

    /* Skip whitespace. */
    do {
        c = fgetc(stdin);
    } while ((c != EOF) && isspace(c));

    /* Ugh... this is soooo ugly. */

    if (c == EOF) {
        if (depth > 0) {
            parse.status = HALT; // TODO: make better constant name for this
            parse.expr = NULL;
        }
        return parse;
    } else if (c == '(') {
        result inner;
        sexpr *last, *current;

        depth++;

        inner = l_read();

        if (inner.status != OKAY) {
            return inner;
        }

        /* parse.expr is the head of the list. */
        parse.expr = last = new_cell();

        /* Build up the list in order. */
        while ((inner.status == OKAY) && (inner.expr != NULL)) {
            current = new_cell();
            current->car = inner.expr;

            last->cdr = current;
            last = current;

            inner = l_read();
        }

        if (inner.status != OKAY) {
            return inner;
        }

        last->cdr = NULL;
        depth--;

    } else if (c == ')') {
        parse.expr = NULL;
        parse.status = OKAY;

        return parse;
    } else if (isdigit(c)) {
        ungetc(c, stdin);
        
        parse.expr = new_cell();
        parse.expr->type = NUMBER;
        
        scanf("%lf", &parse.expr->number);

        return parse;
    } else {
        /* It's literally anything else, make it a symbol. */
        char buffer[NAME_LENGTH];
        parse_symbol(buffer);

        parse.expr = new_cell();
        parse.expr->type = SYMBOL;

        parse.expr->symbol = insert_symbol(buffer);
    }

    return parse;
}

static void parse_symbol(char *buffer) {
    char c;
    int i;

    for (i = 0; i < NAME_LENGTH - 1; i++) {
        c = fgetc(stdin);

        if ((c == EOF) || isdigit(c) || isspace(c) || (c == ')')) {
            break;
        }
        buffer[i] = c;
    }

    buffer[i] = '\0';

    /* I realize... that I need some more elaborate states... */
    while (!((c == EOF) || isspace(c) || isdigit(c) || (c == ')')));
}

