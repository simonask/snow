#include "snow/intern.h"
#include "snow/library.h"
#include "snow/exception.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static inline void throw_errno(const char* msg) {
	char buffer[1024];
	strerror_r(errno, buffer, 1024);
	snow_throw_exception_with_description("%s: %s", msg, buffer);
}

static void socket_gc_free_failsafe(VALUE self) {
	int descriptor = value_to_int(snow_get_member(self, snow_symbol("_fd")));
	close(descriptor); // don't care about failure
}

SNOW_FUNC(socket_initialize)
{
	REQUIRE_ARGS(2);
	VALUE vhost = ARG_BY_NAME_AT("host", 0);
	VALUE vport = ARG_BY_NAME_AT("port", 1);
	ASSERT_TYPE(vhost, SN_STRING_TYPE);
	ASSERT_TYPE(vport, SN_INTEGER_TYPE);
	
	const char* host = snow_string_cstr(vhost);
	        int port = value_to_int(vport);
	
	int descriptor = socket(AF_INET, SOCK_STREAM, 0);
	if (descriptor == -1)
		throw_errno("Socket creation failed");
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	
	// resolve hostname
	struct hostent* he = gethostbyname(host);
	if (!he) throw_errno("Could not resolve hostname");
	// copy resolved address to addr. TODO: try more that one address
	memcpy(&addr.sin_addr.s_addr, *he->h_addr_list, sizeof(addr.sin_addr.s_addr));
	
	int success = connect(descriptor, (struct sockaddr*)&addr, sizeof(addr));
	if (success == -1)
		throw_errno("Socket connection failed");
	
	snow_set_member(SELF, snow_symbol("_fd"), int_to_value(descriptor));
	snow_set_member(SELF, snow_symbol("host"), ARGS[0]);
	snow_set_member(SELF, snow_symbol("port"), ARGS[1]);
	
	snow_gc_set_free_func(SELF, socket_gc_free_failsafe);
	
	return SELF;
}

SNOW_FUNC(socket_receive) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(ARGS[0], SN_INTEGER_TYPE);
	
	int descriptor = value_to_int(snow_get_member(SELF, snow_symbol("_fd")));
	int buffer_length = value_to_int(ARGS[0]);
	char buffer[buffer_length];
	
	ssize_t read_bytes = recv(descriptor, buffer, buffer_length, 0);
	if (read_bytes < 0) throw_errno("Could not receive");
	
	return snow_create_string_from_data((byte*)buffer, read_bytes);
}

SNOW_FUNC(socket_send) {
	REQUIRE_ARGS(1);
	ASSERT_TYPE(ARGS[0], SN_STRING_TYPE);
	
	int descriptor = value_to_int(snow_get_member(SELF, snow_symbol("_fd")));
	const char* data = snow_string_cstr(ARGS[0]);
	if (send(descriptor, data, snow_string_length(ARGS[0]), 0) < 0) throw_errno("Could not send");
	
	return SELF;
}

SNOW_FUNC(socket_close) {
	int descriptor = value_to_int(snow_get_member(SELF, snow_symbol("_fd")));
	if (close(descriptor) != 0) throw_errno("Could not close socket");
	return SN_NIL;
}

SNOW_FUNC(server_initialize) {
	REQUIRE_ARGS(1);
	VALUE vport = ARG_BY_NAME_AT("port", 0);
	VALUE vhost = ARG_BY_NAME_AT("host", 1);
	ASSERT_TYPE(vport, SN_INTEGER_TYPE);
	
	const char* host = vhost ? snow_value_to_cstr(vhost) : "0.0.0.0";
	        int port = value_to_int(vport);
	
	int descriptor = socket(AF_INET, SOCK_STREAM, 0);
	if (descriptor == -1)
		throw_errno("Socket creation failed");
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	
	// resolve hostname
	struct hostent* he = gethostbyname(host);
	if (!he) throw_errno("Could not resolve hostname");
	// copy resolved address to addr. TODO: try more that one address
	memcpy(&addr.sin_addr.s_addr, *he->h_addr_list, sizeof(addr.sin_addr.s_addr));
	
	if (bind(descriptor, (struct sockaddr*)&addr, sizeof(addr)) < 0) throw_errno("Could not bind socket");
	
	if (listen(descriptor, 16)) throw_errno("Could not listen for connections");
	
	snow_set_member(SELF, snow_symbol("_fd"), int_to_value(descriptor));
	snow_set_member(SELF, snow_symbol("host"), ARGS[0]);
	snow_set_member(SELF, snow_symbol("port"), ARGS[1]);
	snow_gc_set_free_func(SELF, socket_gc_free_failsafe); // XXX: free func can be reused because both Socket and Server name their socket descriptor "_fd".
	return SELF;
}

SNOW_FUNC(server_accept) {
	int server_descriptor = value_to_int(snow_get_member(SELF, snow_symbol("_fd")));
	
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	
	int client_descriptor = accept(server_descriptor, (struct sockaddr*)&addr, &addr_len);
	if (client_descriptor < 0) throw_errno("Error accepting connection");
	
	SnClass* Socket = snow_get_global(snow_symbol("Socket"));
	VALUE client = snow_create_object(Socket->instance_prototype);
	snow_set_member(client, snow_symbol("_fd"), int_to_value(client_descriptor));
	snow_set_member(client, snow_symbol("host"), snow_create_string(inet_ntoa(addr.sin_addr)));
	snow_set_member(client, snow_symbol("port"), int_to_value(ntohs(addr.sin_port)));
	snow_gc_set_free_func(client, socket_gc_free_failsafe);
	return client;
}

SNOW_FUNC(server_stop) {
	int server_descriptor = value_to_int(snow_get_member(SELF, snow_symbol("_fd")));
	if (close(server_descriptor)) throw_errno("Could not stop server");
	return SN_NIL;
}

void socket_init(SnContext* global_context)
{
	SnClass* Socket = snow_create_class("Socket");
	snow_set_global(snow_symbol("Socket"), Socket);	
	snow_define_method(Socket, "initialize", socket_initialize);
	snow_define_method(Socket, "receive", socket_receive);
	snow_define_method(Socket, "read", socket_receive);
	snow_define_method(Socket, "send", socket_send);
	snow_define_method(Socket, "write", socket_send);
	snow_define_method(Socket, "<<", socket_send);
	snow_define_method(Socket, "close", socket_close);
	
	SnClass* Server = snow_create_class("Server");
	snow_set_global(snow_symbol("Server"), Server);
	snow_define_method(Server, "initialize", server_initialize);
	snow_define_method(Server, "accept", server_accept);
	snow_define_method(Server, "stop", server_stop);
	// TODO: Nonblocking accept -- depends: support for the select() system call.
	
	snow_set_member(Socket, snow_symbol("listen"), Server);
}

SnLibraryInfo library_info = {
	.name = "Snow Socket I/O Library",
	.version = 1,
	.initialize = socket_init,
	.finalize = NULL
};
