#include "../util.h"
#include <notmuch.h>

extern const char * notmuch_db_path;

const char * notmuch_query(const char * query) {
	char * error;
	notmuch_database_t *db;
	notmuch_status_t status;

	status = notmuch_database_open_verbose(notmuch_db_path,
			NOTMUCH_DATABASE_MODE_READ_ONLY,
			&db,
			&error);

	if (status) {
		warn("%s", error);
		return NULL;
	}

	notmuch_query_t * unread = notmuch_query_create(db, query);

	unsigned int unread_count;
	status = notmuch_query_count_messages(unread, &unread_count);

	if (status) {
		warn("%s", notmuch_status_to_string(status));
		notmuch_database_close(db);
		return NULL;
	}

	notmuch_database_close(db);
	return bprintf("%d", unread_count);
}
