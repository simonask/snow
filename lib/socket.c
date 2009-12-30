#include "snow/intern.h"
#include "snow/library.h"
#include "snow/exception.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

SNOW_FUNC(socket_initialize)
{
	REQUIRE_ARGS(2);
	ASSERT_TYPE(ARGS[0], SN_STRING_TYPE);
	ASSERT_TYPE(ARGS[1], SN_INTEGER_TYPE);
	
	const char* host = snow_string_cstr(ARGS[0]);
	        int port = value_to_int(ARGS[1]);
	
	int descriptor = socket(AF_INET, SOCK_STREAM, 0);
	if (descriptor == -1)
		snow_throw_exception_with_description("Socket creation failed.");
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(host);
	
	int success = connect(descriptor, (struct sockaddr*)&addr, sizeof(addr));
	if (success == -1)
		snow_throw_exception_with_description("Socket connection failed.");
	
	snow_set_member(SELF, snow_symbol("_fd"), descriptor);
	snow_set_member(SELF, snow_symbol("host"), ARGS[0]);
	snow_set_member(SELF, snow_symbol("port"), ARGS[1]);
	
	return SELF;
}

SNOW_FUNC(socket_receive) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(ARGS[0], SN_INTEGER_TYPE);
	
	int descriptor = snow_get_member(SELF, snow_symbol("_fd"));
	int buffer_length = value_to_int(ARGS[0]);
	char buffer[buffer_length];
	
	ssize_t read_bytes = recv(descriptor, buffer, buffer_length, 0);
	
	return snow_create_string_from_data(buffer, read_bytes);
}

SNOW_FUNC(socket_send) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(ARGS[0], SN_STRING_TYPE);
	
	int descriptor = snow_get_member(SELF, snow_symbol("_fd"));
	const char* data = snow_string_cstr(ARGS[0]);
	send(descriptor, data, snow_string_length(ARGS[0]), 0);
	
	return SELF;
}

void socket_init(SnContext* global_context)
{
	SnClass* Socket = snow_create_class("Socket");
	snow_set_global(snow_symbol("Socket"), Socket);
	
	snow_define_method(Socket, "initialize", socket_initialize);
	snow_define_method(Socket, "receive", socket_receive);
	snow_define_method(Socket, "send", socket_send);
}

SnLibraryInfo library_info = {
	.name = "Snow Socket I/O Library",
	.version = 1,
	.initialize = socket_init,
	.finalize = NULL
};
