#include <nuttx/config.h>
#include <stdio.h>
#include <sqlite3.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * sqlite3_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
    sqlite3 *db;
    char *err_msg = NULL;
    int last_id;

    //int rc = sqlite3_open("/tmp/hogehoge.db", &db);
    int rc = sqlite3_open(":memory:", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    char *sql = "CREATE TABLE Friends(Id INTEGER PRIMARY KEY, Name TEXT);"
      "INSERT INTO Friends(Name) VALUES ('Tom');"
      "INSERT INTO Friends(Name) VALUES ('Rebecca');"
      "INSERT INTO Friends(Name) VALUES ('Jim');"
      "INSERT INTO Friends(Name) VALUES ('Roger');"
      "INSERT INTO Friends(Name) VALUES ('Robert');";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK)
      {
        fprintf(stderr, "Failed to create table\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
      }
    else
      {
        fprintf(stdout, "Table Friends created successfully\n");

        last_id = sqlite3_last_insert_rowid(db);
        printf("The last Id of the inserted row is %d\n", last_id);
      }

    sqlite3_close(db);
    return 0;
}
