#include "snow/snow.h"
#include "snow/library.h"
#include "snow/intern.h"
#include "snow/pointer.h"
#include "snow/class.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

typedef struct FilePrivate {
	FILE* fp;
} FilePrivate;

static inline void throw_errno() {
	char buffer[1024];
	strerror_r(errno, buffer, 1024);
	snow_throw_exception_with_description("File error: %s", buffer);
}

static void file_free(void* _priv)
{
	FilePrivate* priv = (FilePrivate*)_priv;
	if (priv->fp) fclose(priv->fp);
}

static VALUE create_object_for_global_file_pointer(FILE* fp, SnClass* file_class)
{
	SnObject* object = snow_create_wrap_object(file_class);
	FilePrivate* priv = snow_unwrap_struct(object, FilePrivate);
	priv->fp = fp;
	return object;
}

SNOW_FUNC(file_initialize) {
	REQUIRE_ARGS(1);
	SnString* mode = NULL;
	if (NUM_ARGS > 1)
	{
		ASSERT_TYPE(ARGS[1], SnStringType);
		mode = (SnString*)ARGS[1];
	}
	else
	{
		mode = snow_create_string("r");
	}
	
	ASSERT_TYPE(ARGS[0], SnStringType);
	SnString* filename = (SnString*)ARGS[0];
	
	FILE* fp = fopen(snow_string_cstr(filename), snow_string_cstr(mode));
	if (!fp) throw_errno();
	
	FilePrivate* priv = snow_unwrap_struct(SELF, FilePrivate);
	priv->fp = fp;

	return SELF;
}

SNOW_FUNC(file_open) {
	return snow_call_with_args(SELF, SELF, _context->args);
}

SNOW_FUNC(file_close) {
	FilePrivate* priv = snow_unwrap_struct(SELF, FilePrivate);
	if (priv->fp) {
		fclose(priv->fp);
		priv->fp = NULL;
		return SN_TRUE;
	}
	return SN_FALSE;
}

SNOW_FUNC(file_is_eof) {
	FilePrivate* priv = snow_unwrap_struct(SELF, FilePrivate);
	if (priv->fp)
		return snow_boolean_to_value(feof(priv->fp));
	return SN_TRUE; // no stream == EOF
}

SNOW_FUNC(file_read) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(ARGS[0], SnIntegerType);
	FilePrivate* priv = snow_unwrap_struct(SELF, FilePrivate);
	if (priv->fp && !feof(priv->fp)) {
		intx num_bytes = snow_value_to_int(ARGS[0]);
		byte buffer[num_bytes+1];
		size_t n = fread(buffer, 1, num_bytes, priv->fp);
		if (!n && !feof(priv->fp)) throw_errno();
		buffer[n] = '\0';
		return snow_create_string((char*)buffer);
	}
	return SN_NIL;
}

SNOW_FUNC(file_read_bytes) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(ARGS[0], SnIntegerType);
	FilePrivate* priv = snow_unwrap_struct(SELF, FilePrivate);
	if (priv->fp && !feof(priv->fp)) {
		intx num_bytes = snow_value_to_int(ARGS[0]);
		byte buffer[num_bytes];
		size_t n = fread(buffer, 1, num_bytes, priv->fp);
		if (!n && !feof(priv->fp)) throw_errno();
		SnArray* array = snow_create_array_with_size(n);
		for (size_t i = 0; i < n; ++i) {
			snow_array_push(array, snow_int_to_value(buffer[i]));
		}
		return array;
	}
	return SN_NIL;
}

static inline size_t file_write_string(FILE* fp, SnString* str) {
	size_t len = snow_string_length(str);
	if (len) {
		const char* data = snow_string_cstr(str);
		size_t n = fwrite(data, 1, len, fp);
		if (!n) throw_errno();
		return n;
	}
	return 0;
}

SNOW_FUNC(file_write) {
	// TODO: Expand with support for byte arrays
	REQUIRE_ARGS(1);
	SnString* str = (SnString*)snow_call_method(ARGS[0], snow_symbol("to_string"), 0);
	ASSERT_TYPE(str, SnStringType);
	FilePrivate* priv = snow_unwrap_struct(SELF, FilePrivate);
	if (priv->fp) {
		size_t n = file_write_string(priv->fp, str);
		return snow_int_to_value(n);
	}
	return SN_NIL;
}

SNOW_FUNC(file_stream_write) {
	REQUIRE_ARGS(1);
	SnString* str = (SnString*)snow_call_method(ARGS[0], snow_symbol("to_string"), 0);
	ASSERT_TYPE(str, SnStringType);
	FilePrivate* priv = snow_unwrap_struct(SELF, FilePrivate);
	if (priv->fp) {
		file_write_string(priv->fp, str);
		return SELF;
	}
	return SN_NIL;
}

SNOW_FUNC(file_flush) {
	FilePrivate* priv = snow_unwrap_struct(SELF, FilePrivate);
	if (priv->fp) {
		if (!fflush(priv->fp))
			return SN_TRUE;
		throw_errno();
	}
	return SN_FALSE;
}

static void file_init(SnContext* global_context)
{
	SnClass* File = snow_create_class_wrap_struct("File", FilePrivate, NULL, NULL);
	snow_define_method(File, "initialize", file_initialize);
	snow_define_class_method(File, "open", file_open);
	snow_define_method(File, "close", file_close);
	snow_define_property(File, "eof?", file_is_eof, NULL);
	snow_define_method(File, "read", file_read);
	snow_define_method(File, "read_bytes", file_read_bytes);
	snow_define_method(File, "write", file_write);
	snow_define_method(File, "<<", file_stream_write);
	snow_define_method(File, "flush", file_flush);
	
	snow_set_member(File, snow_symbol("stdout"), create_object_for_global_file_pointer(stdout, File));
	snow_set_member(File, snow_symbol("stderr"), create_object_for_global_file_pointer(stderr, File));
	snow_set_member(File, snow_symbol("stdin"), create_object_for_global_file_pointer(stdin, File));
	
	snow_set_global(snow_symbol("File"), File);
}

SnLibraryInfo library_info = {
	.name = "Snow File I/O Library",
	.version = 1,
	.initialize = file_init,
	.finalize = NULL
};