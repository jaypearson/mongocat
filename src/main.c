#include <stdio.h>
#include "argtable3.h"
#include "getNumCores.h"
#include <mongoc/mongoc.h>
#include <bson.h>
#include <time.h>
#include <pthread.h>
#include "mongocatConfig.h"

/* global arg_xxx structs */
struct arg_lit *verb, *help, *version, *json;
struct arg_str *source, *dest;
struct arg_end *end;
struct arg_file *input, *output;
struct arg_int *batchSize, *numRuns, *numThreads;

bson_t doc = BSON_INITIALIZER;

static void *bulk1(void *data)
{
    mongoc_client_pool_t *pool = (mongoc_client_pool_t *)data;
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    bson_t *command, reply;
    bson_error_t error;
    char *str;
    bool ret;
    mongoc_bulk_operation_t *bulk;
    int i, j;
    clock_t begin_time, end_time;
    double time_spent;

    client = mongoc_client_pool_pop(pool);
    if (!client)
    {
        return NULL;
    }

    collection = mongoc_client_get_collection(client, "db_name", "coll_name");

    for (j = 0; j < *numRuns->ival; j++)
    {
        begin_time = clock();

        bulk = mongoc_collection_create_bulk_operation_with_opts(collection, NULL);

        for (i = 0; i < *batchSize->ival; i++)
        {
            mongoc_bulk_operation_insert(bulk, &doc);
        }

        ret = mongoc_bulk_operation_execute(bulk, &reply, &error);

        /* str = bson_as_canonical_extended_json(&reply, NULL);
        printf("%s\n", str);
        bson_free(str); */

        if (!ret)
        {
            fprintf(stderr, "Error: %s\n", error.message);
        }

        bson_destroy(&reply);
        mongoc_bulk_operation_destroy(bulk);

        end_time = clock();
        time_spent = (double)(end_time - begin_time) / CLOCKS_PER_SEC;
        printf("Bulk insert: %f\n", time_spent);
    }
    printf("Thread exiting...\n");
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    return NULL;
}

int main(int argc, char *argv[])
{
    /* the global arg_xxx structs are initialised within the argtable */
    void *argtable[] = {
        help = arg_litn(NULL, "help", 0, 1, "display this help and exit"),
        version = arg_litn(NULL, "version", 0, 1, "display version info and exit"),
        source = arg_strn("s", "source", "<uri>", 0, 1, "uri for source MongoDB"),
        dest = arg_strn("d", "dest", "<uri>", 0, 1, "uri for destination MongoDB"),
        input = arg_file0("i", "input", "<input filename>", "input filename"),
        output = arg_file0("o", "output", "<output filename>", "output filename"),
        batchSize = arg_int0(NULL, "batchSize", "#", "number of documents to insert"),
        numRuns = arg_int0(NULL, "numRuns", "#", "number of batches to insert"),
        numThreads = arg_int0(NULL, "numThreads", "#", "number of threads to submit batches"),
        verb = arg_litn("v", "verbose", 0, 1, "verbose output"),
        json = arg_litn("j", "json", 0, 1, "output JSON format"),
        end = arg_end(20),
    };

    int exitcode = 0;
    char progname[] = "mongocat";

    int nerrors;
    nerrors = arg_parse(argc, argv, argtable);

    const char *uri_string = source->count > 0 ? *source->sval : "mongodb://localhost:27017";
    mongoc_uri_t *uri;
    mongoc_client_pool_t *pool;

    bson_error_t error;

    bson_json_reader_t *reader;
    const char *filename;
    int i, b = 0;
    pthread_t threads[8];

    void *ret;

    /* special case: '--help' takes precedence over error reporting */
    if (help->count > 0)
    {
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable, "\n");
        printf("\tversion %d.%d.%d\n\n", mongocat_VERSION_MAJOR, mongocat_VERSION_MINOR, mongocat_VERSION_PATCH);
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        exitcode = 0;
        goto exit;
    }

    if (version->count > 0)
    {
        printf("mongocat version %d.%d.%d\n", mongocat_VERSION_MAJOR, mongocat_VERSION_MINOR, mongocat_VERSION_PATCH);
        exitcode = 0;
        goto exit;
    }

    /* If the parser returned any errors then display them and exit */
    if (nerrors > 0)
    {
        /* Display the error details contained in the arg_end struct.*/
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = 1;
        goto exit;
    }

    // Initialize mongoc
    mongoc_init();

    /*
    * Safely create a MongoDB URI object from the given string
    */
    uri = mongoc_uri_new_with_error(uri_string, &error);
    if (!uri)
    {
        fprintf(stderr,
                "failed to parse URI: %s\n"
                "error message:       %s\n",
                uri_string,
                error.message);
        return EXIT_FAILURE;
    }

    /*
    * Create a new client instance
    */
    pool = mongoc_client_pool_new(uri);
    mongoc_client_pool_set_error_api(pool, 2);

    filename = *input->filename;

    if (0 == strcmp(filename, "-"))
    {
        reader = bson_json_reader_new_from_fd(STDIN_FILENO, false);
    }
    else
    {
        if (!(reader = bson_json_reader_new_from_file(filename, &error)))
        {
            fprintf(
                stderr, "Failed to open \"%s\": %s\n", filename, error.message);
        }
        else
        {
            printf("Opened %s\n", filename);
        }
    }
    /*
       * Convert each incoming document to BSON and print to stdout.
       */
    /* while ((b = bson_json_reader_read(reader, &doc, &error)))
    {
        end_time = clock();
        time_spent = (double)(end_time - begin_time) / CLOCKS_PER_SEC;
        printf("JSON parsed: %f\n", time_spent);
        if (b < 0)
        {
            fprintf(stderr, "Error in json parsing:\n%s\n", error.message);
            abort();
        }

        begin_time = clock();
        for (i = 0; i < *batchSize->ival; i++)
        {
            if (!mongoc_collection_insert_one(collection, &doc, NULL, NULL, &error))
            {
                fprintf(stderr, "%s\n", error.message);
            }
            else
            {
                count++;
            }
        }
        end_time = clock();
        time_spent = (double)(end_time - begin_time) / CLOCKS_PER_SEC;
        printf("%d documents inserted: %f\n", count, time_spent);
        bson_reinit(&doc);
        begin_time = clock();
    }*/
    b = bson_json_reader_read(reader, &doc, &error);

    for (i = 0; i < *numThreads->ival; i++)
    {
        printf("Starting thread %d\n", i);
        pthread_create(&threads[i], NULL, bulk1, pool);
    }

    for (i = 0; i < *numThreads->ival; i++)
    {
        pthread_join(threads[i], &ret);
    }

    bson_json_reader_destroy(reader);
    bson_destroy(&doc);

    /*
    * Release our handles and clean up libmongoc
    */
    mongoc_client_pool_destroy(pool);
    mongoc_uri_destroy(uri);
    mongoc_cleanup();

    // printf("Hello, world! %d\n", getNumCores());

exit:
    /* deallocate each non-null entry in argtable[] */
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitcode;
}