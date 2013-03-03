#include <fd.h>

struct lock *file_table_lock;
struct fd *file_table[MAX_FILE_HANDLES];

static int file_table_initialized = 0;

// TODO move this to a bootstrap process?
// (this may be unnecessary once processes are implemented)
void initialize_file_table_if_necessary() {
	if (!file_table_initialized) {
		DEBUG(DB_FSYSCALL, "Initializing file table\n");

		file_table_lock = lock_create("file_table_lock");

		lock_acquire(file_table_lock);

		int i;
		for (i = 0; i < MAX_FILE_HANDLES; i++) {
			file_table[i] = NULL;
		}

		file_table_initialized = 1;

		lock_release(file_table_lock);
	}
}
