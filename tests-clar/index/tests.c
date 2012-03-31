#include "clar_libgit2.h"
#include "index.h"

static const int index_entry_count = 109;
static const int index_entry_count_2 = 1437;
#define TEST_INDEX_PATH cl_fixture("testrepo.git/index")
#define TEST_INDEX2_PATH cl_fixture("gitgit.index")
#define TEST_INDEXBIG_PATH cl_fixture("big.index")


// Suite data
struct test_entry {
   int index;
   char path[128];
   git_off_t file_size;
   git_time_t mtime;
};

static struct test_entry test_entries[] = {
   {4, "Makefile", 5064, 0x4C3F7F33},
   {62, "tests/Makefile", 2631, 0x4C3F7F33},
   {36, "src/index.c", 10014, 0x4C43368D},
   {6, "git.git-authors", 2709, 0x4C3F7F33},
   {48, "src/revobject.h", 1448, 0x4C3F7FE2}
};


// Helpers
static void copy_file(const char *src, const char *dst)
{
	git_buf source_buf = GIT_BUF_INIT;
	git_file dst_fd;
	int error = GIT_ERROR;

	cl_git_pass(git_futils_readbuffer(&source_buf, src));

	dst_fd = git_futils_creat_withpath(dst, 0777, 0666);
	if (dst_fd < 0)
		goto cleanup;

	cl_git_pass(p_write(dst_fd, source_buf.ptr, source_buf.size));

cleanup:
	git_buf_free(&source_buf);
	p_close(dst_fd);
}

static void files_are_equal(const char *a, const char *b)
{
	git_buf buf_a = GIT_BUF_INIT;
	git_buf buf_b = GIT_BUF_INIT;
	int pass;

	if (git_futils_readbuffer(&buf_a, a) < GIT_SUCCESS)
		cl_assert(0);

	if (git_futils_readbuffer(&buf_b, b) < GIT_SUCCESS) {
		git_buf_free(&buf_a);
		cl_assert(0);
	}

	pass = (buf_a.size == buf_b.size && !memcmp(buf_a.ptr, buf_b.ptr, buf_a.size));

	git_buf_free(&buf_a);
	git_buf_free(&buf_b);
}


// Fixture setup and teardown
void test_index_tests__initialize(void)
{
}

void test_index_tests__cleanup(void)
{
}


void test_index_tests__empty_index(void)
{
   git_index *index;

   cl_git_pass(git_index_open(&index, "in-memory-index"));
   cl_assert(index->on_disk == 0);

   cl_assert(git_index_entrycount(index) == 0);
   cl_assert(index->entries.sorted);

   git_index_free(index);
}

void test_index_tests__default_test_index(void)
{
   git_index *index;
   unsigned int i;
   git_index_entry **entries;

   cl_git_pass(git_index_open(&index, TEST_INDEX_PATH));
   cl_assert(index->on_disk);

   cl_assert(git_index_entrycount(index) == (unsigned int)index_entry_count);
   cl_assert(index->entries.sorted);

   entries = (git_index_entry **)index->entries.contents;

   for (i = 0; i < ARRAY_SIZE(test_entries); ++i) {
      git_index_entry *e = entries[test_entries[i].index];

      cl_assert(strcmp(e->path, test_entries[i].path) == 0);
      cl_assert(e->mtime.seconds == test_entries[i].mtime);
      cl_assert(e->file_size == test_entries[i].file_size);
   }

   git_index_free(index);
}

void test_index_tests__gitgit_index(void)
{
   git_index *index;

   cl_git_pass(git_index_open(&index, TEST_INDEX2_PATH));
   cl_assert(index->on_disk);

   cl_assert(git_index_entrycount(index) == (unsigned int)index_entry_count_2);
   cl_assert(index->entries.sorted);
   cl_assert(index->tree != NULL);

   git_index_free(index);
}

void test_index_tests__find_in_existing(void)
{
   git_index *index;
   unsigned int i;

   cl_git_pass(git_index_open(&index, TEST_INDEX_PATH));

   for (i = 0; i < ARRAY_SIZE(test_entries); ++i) {
      int idx = git_index_find(index, test_entries[i].path);
      cl_assert(idx == test_entries[i].index);
   }

   git_index_free(index);
}

void test_index_tests__find_in_empty(void)
{
   git_index *index;
   unsigned int i;

   cl_git_pass(git_index_open(&index, "fake-index"));

   for (i = 0; i < ARRAY_SIZE(test_entries); ++i) {
      int idx = git_index_find(index, test_entries[i].path);
      cl_assert(idx == GIT_ENOTFOUND);
   }

   git_index_free(index);
}

void test_index_tests__write(void)
{
   git_index *index;

   copy_file(TEST_INDEXBIG_PATH, "index_rewrite");

   cl_git_pass(git_index_open(&index, "index_rewrite"));
   cl_assert(index->on_disk);

   cl_git_pass(git_index_write(index));
   files_are_equal(TEST_INDEXBIG_PATH, "index_rewrite");

   git_index_free(index);

   p_unlink("index_rewrite");
}

void test_index_tests__sort0(void)
{
   // sort the entires in an index
   /*
   * TODO: This no longer applies:
   * index sorting in Git uses some specific changes to the way
   * directories are sorted.
   *
   * We need to specificially check for this by creating a new
   * index, adding entries in random order and then
   * checking for consistency
   */
}

void test_index_tests__sort1(void)
{
   // sort the entires in an empty index
   git_index *index;

   cl_git_pass(git_index_open(&index, "fake-index"));

   /* FIXME: this test is slightly dumb */
   cl_assert(index->entries.sorted);

   git_index_free(index);
}

void test_index_tests__add(void)
{
   git_index *index;
   git_filebuf file = GIT_FILEBUF_INIT;
   git_repository *repo;
   git_index_entry *entry;
   git_oid id1;

   /* Intialize a new repository */
   cl_git_pass(git_repository_init(&repo, "./myrepo", 0));

   /* Ensure we're the only guy in the room */
   cl_git_pass(git_repository_index(&index, repo));
   cl_assert(git_index_entrycount(index) == 0);

   /* Create a new file in the working directory */
   cl_git_pass(git_futils_mkpath2file("myrepo/test.txt", 0777));
   cl_git_pass(git_filebuf_open(&file, "myrepo/test.txt", 0));
   cl_git_pass(git_filebuf_write(&file, "hey there\n", 10));
   cl_git_pass(git_filebuf_commit(&file, 0666));

   /* Store the expected hash of the file/blob
   * This has been generated by executing the following
   * $ echo "hey there" | git hash-object --stdin
   */
   cl_git_pass(git_oid_fromstr(&id1, "a8233120f6ad708f843d861ce2b7228ec4e3dec6"));

   /* Add the new file to the index */
   cl_git_pass(git_index_add(index, "test.txt", 0));

   /* Wow... it worked! */
   cl_assert(git_index_entrycount(index) == 1);
   entry = git_index_get(index, 0);

   /* And the built-in hashing mechanism worked as expected */
   cl_assert(git_oid_cmp(&id1, &entry->oid) == 0);

   git_index_free(index);
   git_repository_free(repo);
}

